/// @file FileAssetSource.h
/// @brief Filesystem-backed asset source implementation.
///
/// Layer  : SagaEngine / Resources
/// Purpose: Concrete `IAssetSource` that reads assets from the local
///          filesystem.  Used in development builds (loose files),
///          shipped builds (package files or mounted archives), and
///          test harnesses (temporary directories).  The streaming
///          manager never knows or cares where the bytes come from.
///
/// Design rules:
///   - Asset id to path mapping is supplied at construction via a
///     callback, so the caller can use a hash table, a database,
///     or a simple "append to root directory" rule without changing
///     this class.
///   - Reads are synchronous on the streaming worker thread.  The
///     source does not spin up its own threads or use overlapped I/O
///     — the streaming manager already owns the worker pool.
///   - Cooperative cancellation.  Large files are read in chunks;
///     between chunks the source checks `CancelRequested()` and
///     bails out early if the caller has moved on.
///
/// What this header is NOT:
///   - Not a virtual filesystem.  It reads files from the native OS
///     filesystem; package-file support is a separate backend.
///   - Not a decoder.  Bytes are returned exactly as they appear on
///     disk — decoding is the consumer's job.

#pragma once

#include "SagaEngine/Resources/IAssetSource.h"

#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>

namespace SagaEngine::Resources {

// ─── Path resolution callback ──────────────────────────────────────────────

/// Signature for the path resolver.  The caller provides this at
/// construction so the source can turn an `AssetId` into an
/// on-disk path.  Returning an empty string signals "not found".
///
/// Threading: reentrant — the streaming manager calls `LoadBlocking`
/// from several workers concurrently, so the resolver must not
/// mutate shared state without its own synchronisation.
using PathResolverFn = std::function<std::string(AssetId, AssetKind)>;

// ─── FileAssetSource ───────────────────────────────────────────────────────

/// Filesystem-backed asset source.  Owns a root directory and a
/// path resolver; the resolver turns an id into a relative path,
/// which is then appended to the root to produce an absolute path.
///
/// Example:
///   root = "C:/game/assets"
///   resolver(12345, Mesh) → "meshes/hero.glb"
///   final path           → "C:/game/assets/meshes/hero.glb"
class FileAssetSource final : public IAssetSource
{
public:
    /// Build a source rooted at `rootDirectory`.  The `pathResolver`
    /// must not be null — callers that want a trivial "id.bin"
    /// mapping can supply a lambda that returns `std::to_string(id)`.
    explicit FileAssetSource(std::string       rootDirectory,
                             PathResolverFn    pathResolver) noexcept;

    ~FileAssetSource() override = default;

    FileAssetSource(const FileAssetSource&)            = delete;
    FileAssetSource& operator=(const FileAssetSource&) = delete;
    FileAssetSource(FileAssetSource&&)                 = delete;
    FileAssetSource& operator=(FileAssetSource&&)      = delete;

    // ── IAssetSource ──────────────────────────────────────────────────────

    [[nodiscard]] AssetLoadResult LoadBlocking(
        const StreamingRequest&       request,
        const StreamingRequestHandle& handle) override;

    [[nodiscard]] const char* DebugName() const noexcept override { return "FileAssetSource"; }

private:
    /// Read one file into `out`.  Returns true on success, false on
    /// any I/O error.  Checks `handle.CancelRequested()` between
    /// chunks so large files can be aborted without reading the
    /// entire payload into memory first.
    ///
    /// The caller owns `file` and is responsible for closing it.
    [[nodiscard]] bool ReadFileChunks(std::FILE*                    file,
                                      std::vector<std::uint8_t>&   out,
                                      const StreamingRequestHandle& handle) const;

    std::string      rootDirectory_;
    PathResolverFn   pathResolver_;

    /// Read buffer size.  64 KiB is a comfortable trade-off between
    /// syscall overhead and memory footprint — large enough to amortise
    /// disk latency, small enough that a dozen concurrent workers do not
    /// blow the page cache.
    static inline constexpr std::size_t kChunkSize = 64 * 1024;
};

} // namespace SagaEngine::Resources
