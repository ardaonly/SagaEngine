// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Documents/DocumentPersistence.h"

#include <cerrno>
#include <cstdio>
#include <fstream>
#include <utility>

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#else
#include <windows.h>
#endif

namespace SagaEditor
{
namespace
{
std::string SafeOperation(std::string_view operation)
{
    std::string safe;
    for (const auto character : operation)
        if ((character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9')) safe.push_back(character);
    return safe.empty() ? "document" : safe.substr(0, 32);
}
}

AtomicDocumentPersistence::AtomicDocumentPersistence(DocumentPersistenceFaultInjector injector)
    : injector_(std::move(injector)) {}

std::error_code AtomicDocumentPersistence::Fault(DocumentPersistenceFaultPoint point,
    const std::filesystem::path& path) const
{ return injector_ ? injector_(point, path) : std::error_code{}; }

DocumentPublicationResult AtomicDocumentPersistence::Publish(
    const std::filesystem::path& path, const std::string& bytes,
    std::string_view operationId)
{
    DocumentPublicationResult result;
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) { result.error = error; return result; }
    const auto temporary = path.parent_path() /
        (path.filename().string() + ".saga-tmp-" + SafeOperation(operationId));
    auto cleanup = [&]
    {
        if (const auto injected = Fault(DocumentPersistenceFaultPoint::TemporaryCleanup, temporary))
            return injected;
        std::error_code cleanupError;
        std::filesystem::remove(temporary, cleanupError);
        return cleanupError;
    };
    if (const auto injected = Fault(DocumentPersistenceFaultPoint::TemporaryOpen, temporary))
    { result.error = injected; return result; }
#ifdef _WIN32
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) { result.error = std::make_error_code(std::errc::io_error); return result; }
        if (const auto injected = Fault(DocumentPersistenceFaultPoint::TemporaryWrite, temporary))
        { result.error = injected; output.close(); (void)cleanup(); return result; }
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (const auto injected = Fault(DocumentPersistenceFaultPoint::TemporaryFlush, temporary))
        { result.error = injected; output.close(); (void)cleanup(); return result; }
        output.flush();
        if (!output) { result.error = std::make_error_code(std::errc::io_error); output.close(); (void)cleanup(); return result; }
    }
    if (const auto injected = Fault(DocumentPersistenceFaultPoint::FileSync, temporary))
    { result.error = injected; (void)cleanup(); return result; }
    if (const auto injected = Fault(DocumentPersistenceFaultPoint::AtomicRename, path))
    { result.error = injected; (void)cleanup(); return result; }
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    { result.error = std::error_code(static_cast<int>(GetLastError()), std::system_category()); (void)cleanup(); return result; }
#else
    const auto descriptor = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (descriptor < 0) { result.error = std::error_code(errno, std::generic_category()); return result; }
    if (const auto injected = Fault(DocumentPersistenceFaultPoint::TemporaryWrite, temporary))
    { result.error = injected; ::close(descriptor); (void)cleanup(); return result; }
    std::size_t offset = 0;
    while (offset < bytes.size())
    {
        const auto written = ::write(descriptor, bytes.data() + offset, bytes.size() - offset);
        if (written <= 0)
        { result.error = std::error_code(errno, std::generic_category()); ::close(descriptor); (void)cleanup(); return result; }
        offset += static_cast<std::size_t>(written);
    }
    if (const auto injected = Fault(DocumentPersistenceFaultPoint::TemporaryFlush, temporary))
    { result.error = injected; ::close(descriptor); (void)cleanup(); return result; }
    if (const auto injected = Fault(DocumentPersistenceFaultPoint::FileSync, temporary))
    { result.error = injected; ::close(descriptor); (void)cleanup(); return result; }
    if (::fsync(descriptor) != 0)
    { result.error = std::error_code(errno, std::generic_category()); ::close(descriptor); (void)cleanup(); return result; }
    if (::close(descriptor) != 0)
    { result.error = std::error_code(errno, std::generic_category()); (void)cleanup(); return result; }
    if (const auto injected = Fault(DocumentPersistenceFaultPoint::AtomicRename, path))
    { result.error = injected; (void)cleanup(); return result; }
    if (::rename(temporary.c_str(), path.c_str()) != 0)
    { result.error = std::error_code(errno, std::generic_category()); (void)cleanup(); return result; }
#endif
    result.renamed = true;
    if (const auto injected = Fault(DocumentPersistenceFaultPoint::ParentDirectorySync, path.parent_path()))
    { result.error = injected; return result; }
#ifndef _WIN32
    const auto directory = ::open(path.parent_path().c_str(), O_RDONLY | O_DIRECTORY);
    if (directory < 0) { result.error = std::error_code(errno, std::generic_category()); return result; }
    if (::fsync(directory) != 0) result.error = std::error_code(errno, std::generic_category());
    if (::close(directory) != 0 && !result.error)
        result.error = std::error_code(errno, std::generic_category());
#endif
    return result;
}

bool AtomicDocumentPersistence::Remove(const std::filesystem::path& path,
    std::error_code& error)
{
    error.clear();
    if (const auto injected = Fault(DocumentPersistenceFaultPoint::RecoveryCleanup, path))
    { error = injected; return false; }
    if (!std::filesystem::exists(path, error)) return !error;
    return std::filesystem::remove(path, error) && !error;
}

std::shared_ptr<IDocumentPersistence> DefaultDocumentPersistence()
{
    static auto persistence = std::make_shared<AtomicDocumentPersistence>();
    return persistence;
}
}
