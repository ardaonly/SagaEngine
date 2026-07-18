// SPDX-FileCopyrightText: 2026 Arda Koyuncu
// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Documents/Document.h"

#include <openssl/evp.h>

#include <array>
#include <cerrno>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace SagaEditor
{

std::string HashDocumentBytes(const std::string& bytes)
{
    std::array<unsigned char, 32> digest{};
    unsigned int size = 0;
    if (EVP_Digest(bytes.data(), bytes.size(), digest.data(), &size, EVP_sha256(), nullptr) != 1)
        return {};
    std::ostringstream out;
    out << "sha256:" << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < size; ++i)
        out << std::setw(2) << static_cast<unsigned int>(digest[i]);
    return out.str();
}

std::string ReadDocumentBytes(const std::filesystem::path& path,
                              std::error_code& error)
{
    error.clear();
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        error = std::make_error_code(std::errc::no_such_file_or_directory);
        return {};
    }
    std::ostringstream bytes;
    bytes << input.rdbuf();
    if (input.bad())
        error = std::make_error_code(std::errc::io_error);
    return bytes.str();
}

bool AtomicWriteDocument(const std::filesystem::path& path,
                         const std::string& bytes,
                         const std::string& operationId,
                         std::error_code& error)
{
    error.clear();
    const auto temporary = path.parent_path() /
        (path.filename().string() + ".saga-tmp-" + operationId);
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            error = std::make_error_code(std::errc::io_error);
            return false;
        }
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        output.flush();
        if (!output)
        {
            output.close();
            std::filesystem::remove(temporary);
            error = std::make_error_code(std::errc::io_error);
            return false;
        }
    }
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        error = std::error_code(static_cast<int>(GetLastError()), std::system_category());
        std::filesystem::remove(temporary);
        return false;
    }
#else
    if (std::rename(temporary.c_str(), path.c_str()) != 0)
    {
        error = std::error_code(errno, std::generic_category());
        std::filesystem::remove(temporary);
        return false;
    }
#endif
    return true;
}

} // namespace SagaEditor
