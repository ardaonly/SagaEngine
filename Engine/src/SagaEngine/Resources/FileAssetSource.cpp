/// @file FileAssetSource.cpp
/// @brief Filesystem-backed asset source implementation.

#include "SagaEngine/Resources/FileAssetSource.h"

#include "SagaEngine/Core/Log/Log.h"

#include <array>
#include <cstdio>
#include <cstring>

#if defined(_WIN32)
#   include <windows.h>
#else
#   include <climits>
#   include <unistd.h>
#endif

namespace SagaEngine::Resources {

namespace {

constexpr const char* kLogTag = "Resources";

/// Normalise a path separator.  Accepts both forward and backward
/// slashes and converts everything to the platform-native separator
/// so a content manifest authored on Linux still works on Windows.
[[nodiscard]] std::string NormalisePath(std::string path) noexcept
{
#if defined(_WIN32)
    for (char& c : path)
        if (c == '/')
            c = '\\';
#else
    for (char& c : path)
        if (c == '\\')
            c = '/';
#endif
    return path;
}

} // namespace

// ─── Construction ──────────────────────────────────────────────────────────

FileAssetSource::FileAssetSource(std::string    rootDirectory,
                                 PathResolverFn pathResolver) noexcept
    : rootDirectory_(std::move(rootDirectory))
    , pathResolver_(std::move(pathResolver))
{
    // Normalise the root so concatenations with resolver output
    // never mix slash styles.
    if (!rootDirectory_.empty())
    {
        char trailing = rootDirectory_.back();
        if (trailing != '/' && trailing != '\\')
#if defined(_WIN32)
            rootDirectory_ += '\\';
#else
            rootDirectory_ += '/';
#endif
    }

    rootDirectory_ = NormalisePath(rootDirectory_);

    LOG_INFO(kLogTag,
             "FileAssetSource: root='%s'",
             rootDirectory_.c_str());
}

// ─── LoadBlocking ──────────────────────────────────────────────────────────

AssetLoadResult FileAssetSource::LoadBlocking(
    const StreamingRequest&       request,
    const StreamingRequestHandle& handle)
{
    AssetLoadResult result;

    if (!pathResolver_)
    {
        result.status     = StreamingStatus::SourceError;
        result.diagnostic = "FileAssetSource: null path resolver";
        return result;
    }

    // Resolve the asset id to a relative path.
    std::string relativePath = pathResolver_(request.assetId, request.kind);
    if (relativePath.empty())
    {
        result.status     = StreamingStatus::AssetNotFound;
        result.diagnostic = "FileAssetSource: resolver returned empty path for assetId=" +
                            std::to_string(request.assetId);
        return result;
    }

    // Build the absolute path.
    std::string absolutePath = rootDirectory_ + NormalisePath(std::move(relativePath));

    // Open the file using the CRT for maximum portability.
    // We cannot rely on C++ iostreams giving us binary mode on every
    // platform, so we use fopen with "rb".
#if defined(_WIN32)
    // On Windows, fopen expects narrow characters but the path may
    // contain Unicode.  For a production build we would switch to
    // _wfopen, but the narrow version works for ASCII development paths.
    std::FILE* file = std::fopen(absolutePath.c_str(), "rb");
#else
    std::FILE* file = std::fopen(absolutePath.c_str(), "rb");
#endif

    if (!file)
    {
        result.status     = StreamingStatus::AssetNotFound;
        result.diagnostic = "FileAssetSource: cannot open '" + absolutePath + "'";
        return result;
    }

    // Read the file in chunks so we can honour cancellation midway
    // rather than blocking the worker for the entire payload.
    // Pass the already-open FILE* to avoid a second open.
    bool ok = ReadFileChunks(file, result.payload.bytes, handle);

    std::fclose(file);

    if (!ok)
    {
        if (handle.CancelRequested())
        {
            result.status     = StreamingStatus::Cancelled;
            result.diagnostic = "FileAssetSource: read cancelled for '" + absolutePath + "'";
        }
        else
        {
            result.status     = StreamingStatus::SourceError;
            result.diagnostic = "FileAssetSource: read failed for '" + absolutePath + "'";
        }
        result.payload.bytes.clear();
        return result;
    }

    // Publish the payload.
    result.status            = StreamingStatus::Ok;
    result.payload.kind      = request.kind;
    result.payload.lod       = request.requestedLod;
    result.payload.sizeBytes = static_cast<std::uint64_t>(result.payload.bytes.size());

    return result;
}

// ─── ReadFileChunks ────────────────────────────────────────────────────────

bool FileAssetSource::ReadFileChunks(std::FILE*                    file,
                                     std::vector<std::uint8_t>&   out,
                                     const StreamingRequestHandle& handle) const
{
    if (!file)
        return false;

    // Determine file size so we can reserve the vector up front.
    if (std::fseek(file, 0, SEEK_END) != 0)
        return false;

    const long fileSize = std::ftell(file);
    if (fileSize < 0)
        return false;

    std::rewind(file);

    // Guard against a runaway file that exceeds the streaming cap.
    const auto maxBytes = static_cast<long>(kMaxStreamingPayloadBytes);
    if (fileSize > maxBytes)
    {
        LOG_ERROR(kLogTag,
                  "FileAssetSource: file exceeds max payload (%ld > %ld bytes)",
                  static_cast<long>(fileSize),
                  static_cast<long>(maxBytes));
        return false;
    }

    // Reserve space.  A single allocation is cheaper than letting
    // the vector grow geometrically.
    out.reserve(static_cast<std::size_t>(fileSize));

    // Read in chunks, checking cancellation between each block.
    std::array<std::uint8_t, kChunkSize> buffer{};
    std::size_t                            totalRead = 0;

    while (totalRead < static_cast<std::size_t>(fileSize))
    {
        if (handle.CancelRequested())
            return false;

        const std::size_t remaining = static_cast<std::size_t>(fileSize) - totalRead;
        const std::size_t toRead    = (remaining < kChunkSize) ? remaining : kChunkSize;

        const std::size_t actuallyRead = std::fread(buffer.data(), 1, toRead, file);
        if (actuallyRead == 0 && remaining > 0)
            return false;

        out.insert(out.end(), buffer.data(), buffer.data() + actuallyRead);
        totalRead += actuallyRead;
    }

    return true;
}

} // namespace SagaEngine::Resources
