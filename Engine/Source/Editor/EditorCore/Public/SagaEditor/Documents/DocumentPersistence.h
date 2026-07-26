// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>

namespace SagaEditor
{
enum class DocumentPersistenceFaultPoint
{
    TemporaryOpen,
    TemporaryWrite,
    TemporaryFlush,
    FileSync,
    AtomicRename,
    ParentDirectorySync,
    TemporaryCleanup,
    RecoveryCleanup,
};

struct DocumentPublicationResult final
{
    bool renamed = false;
    std::error_code error;
};

class IDocumentPersistence
{
public:
    virtual ~IDocumentPersistence() = default;
    [[nodiscard]] virtual DocumentPublicationResult Publish(
        const std::filesystem::path& path, const std::string& bytes,
        std::string_view operationId) = 0;
    [[nodiscard]] virtual bool Remove(const std::filesystem::path& path,
        std::error_code& error) = 0;
};

using DocumentPersistenceFaultInjector = std::function<std::error_code(
    DocumentPersistenceFaultPoint, const std::filesystem::path&)>;

class AtomicDocumentPersistence final : public IDocumentPersistence
{
public:
    explicit AtomicDocumentPersistence(DocumentPersistenceFaultInjector injector = {});
    [[nodiscard]] DocumentPublicationResult Publish(
        const std::filesystem::path& path, const std::string& bytes,
        std::string_view operationId) override;
    [[nodiscard]] bool Remove(const std::filesystem::path& path,
        std::error_code& error) override;

private:
    [[nodiscard]] std::error_code Fault(DocumentPersistenceFaultPoint point,
        const std::filesystem::path& path) const;
    DocumentPersistenceFaultInjector injector_;
};

[[nodiscard]] std::shared_ptr<IDocumentPersistence> DefaultDocumentPersistence();
}
