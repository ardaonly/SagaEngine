// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "SagaEditor/Scripting/SagaScriptLanguageServiceClient.h"
#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"

#include <map>
#include <mutex>

namespace SagaEditor
{
class IEditorDiagnosticsService;

/// One isolated language-service process for one canonical open project.
class SagaScriptLanguageServiceSession final
{
public:
    using PublicationSubscription = std::uint64_t;
    using PublicationCallback = std::function<void(LanguageServiceResponse)>;
    static constexpr PublicationSubscription InvalidPublicationSubscription = 0;

    SagaScriptLanguageServiceSession(EditorDocumentWorkspace& workspace,
        IEditorDiagnosticsService& diagnostics,
        ManagedToolRuntimeOptions runtime = {});
    ~SagaScriptLanguageServiceSession();
    SagaScriptLanguageServiceSession(const SagaScriptLanguageServiceSession&) = delete;
    SagaScriptLanguageServiceSession& operator=(const SagaScriptLanguageServiceSession&) = delete;

    void Refresh();
    void Restart();
    void Shutdown();
    [[nodiscard]] LanguageServiceState State() const noexcept;
    [[nodiscard]] std::string LastDiagnostic() const;
    [[nodiscard]] std::uint64_t ProcessId() const noexcept;
    [[nodiscard]] const LanguageServiceProjectSnapshot& Snapshot() const noexcept;
    [[nodiscard]] std::future<LanguageServiceResponse> Request(
        std::string method, const CSharpSourceDocument& document,
        std::optional<LanguageServicePosition> position = std::nullopt,
        std::string payloadJson = "{}");
    [[nodiscard]] std::future<LanguageServiceResponse> Request(
        std::string method, const DocumentSnapshot& document,
        std::optional<LanguageServicePosition> position = std::nullopt,
        std::string payloadJson = "{}");
    [[nodiscard]] std::future<LanguageServiceResponse> Preflight(
        const CSharpSourceDocument& document, std::string candidateText);
    [[nodiscard]] PublicationSubscription Subscribe(PublicationCallback callback);
    void Unsubscribe(PublicationSubscription subscription);

private:
    void Publish(LanguageServiceResponse publication);
    [[nodiscard]] LanguageServiceDocumentIdentity Identity(
        const CSharpSourceDocument& document) const;
    [[nodiscard]] LanguageServiceDocumentIdentity Identity(
        const DocumentSnapshot& document) const;

    EditorDocumentWorkspace& workspace_;
    IEditorDiagnosticsService& diagnostics_;
    SagaScriptLanguageServiceClient client_;
    EditorDocumentWorkspace::ChangeSubscription workspaceSubscription_ =
        EditorDocumentWorkspace::InvalidChangeSubscription;
    LanguageServiceProjectSnapshot snapshot_;
    std::map<std::string, std::uint64_t> synchronizedVersions_;
    std::map<PublicationSubscription, PublicationCallback> callbacks_;
    PublicationSubscription nextSubscription_ = 1;
    mutable std::mutex mutex_;
    bool refreshing_ = false;
};

} // namespace SagaEditor
