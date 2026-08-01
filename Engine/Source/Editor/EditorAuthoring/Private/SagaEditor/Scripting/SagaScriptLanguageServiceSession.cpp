// SPDX-License-Identifier: MPL-2.0
#include "SagaEditor/Scripting/SagaScriptLanguageServiceSession.h"

#include "SagaEditor/Diagnostics/IEditorDiagnosticsService.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace SagaEditor
{
namespace
{
std::string ReadHash(const std::filesystem::path& path)
{
    std::error_code error;
    const auto bytes = ReadDocumentBytes(path, error);
    return error ? std::string{} : HashDocumentBytes(bytes);
}

bool IsContained(const std::filesystem::path& root, const std::filesystem::path& path)
{
    const auto relative = path.lexically_relative(root);
    return !relative.empty() && *relative.begin() != "..";
}

void LoadPackageSnapshot(const std::filesystem::path& root,
    LanguageServiceProjectSnapshot& snapshot)
{
    const auto packages = root / "Build" / "Packages";
    const auto currentPath = packages / "manifests" / "current.json";
    std::ifstream currentInput(currentPath);
    if (!currentInput) return;
    const auto current = nlohmann::json::parse(currentInput, nullptr, false);
    if (current.is_discarded()) return;
    const auto generationId = current.value("generationId", std::string{});
    if (generationId.empty() || generationId.find_first_not_of("0123456789abcdef") !=
        std::string::npos) return;
    const auto generation = std::filesystem::weakly_canonical(
        packages / "generations" / generationId);
    if (!IsContained(root, generation)) return;
    const auto referencesPath = generation / "csharp_references_v1.json";
    const auto ownershipPath = generation / "api_ownership_v1.json";
    snapshot.packageGeneration = current.value("generationHash", generationId);
    snapshot.referenceManifestHash = ReadHash(referencesPath);
    snapshot.ownershipManifestHash = ReadHash(ownershipPath);
    std::ifstream referencesInput(referencesPath);
    if (!referencesInput || snapshot.referenceManifestHash.empty()) return;
    const auto manifest = nlohmann::json::parse(referencesInput, nullptr, false);
    if (manifest.is_discarded() || manifest.value("projectId", std::string{}) !=
        snapshot.projectId) return;
    for (const auto& value : manifest.value("references", nlohmann::json::array()))
    {
        if (value.value("trust", std::string{}) != "TrustedEngineDistribution") continue;
        const auto relative = std::filesystem::path(
            value.value("resolvedAssemblyLogicalPath", std::string{}));
        const auto path = std::filesystem::weakly_canonical(generation / relative);
        if (!IsContained(generation, path) || !std::filesystem::is_regular_file(path))
            continue;
        const auto hash = ReadHash(path);
        if (hash.empty() || hash != value.value("resolvedAssemblyHash", std::string{}))
            continue;
        snapshot.metadataReferences.push_back({path, hash});
        if (value.value("artifactId", std::string{}) ==
            "com.saga.gameplay.foundation.managed-reference")
            snapshot.runtimeBridgeIdentity = hash;
    }
}
}

SagaScriptLanguageServiceSession::SagaScriptLanguageServiceSession(
    EditorDocumentWorkspace& workspace, IEditorDiagnosticsService& diagnostics,
    ManagedToolRuntimeOptions runtime)
    : workspace_(workspace), diagnostics_(diagnostics), client_(std::move(runtime))
{
    client_.SetPublicationCallback([this](LanguageServiceResponse value)
    { Publish(std::move(value)); });
    workspaceSubscription_ = workspace_.SubscribeChanges([this] { Refresh(); });
}

SagaScriptLanguageServiceSession::~SagaScriptLanguageServiceSession()
{
    if (workspaceSubscription_ != EditorDocumentWorkspace::InvalidChangeSubscription)
        workspace_.UnsubscribeChanges(workspaceSubscription_);
    Shutdown();
}

void SagaScriptLanguageServiceSession::Refresh()
{
    if (refreshing_) return;
    refreshing_ = true;
    if (!workspace_.IsOpen() || !workspace_.Project())
    {
        Shutdown(); refreshing_ = false; return;
    }
    const auto& project = *workspace_.Project();
    const auto root = std::filesystem::weakly_canonical(project.rootPath);
    const auto manifestHash = workspace_.ProjectSource() ?
        HashDocumentBytes(workspace_.ProjectSource()->Text()) : std::string{};
    const auto lockHash = ReadHash(root / "SagaPackage.lock");
    LanguageServiceProjectSnapshot nextSnapshot;
    nextSnapshot.projectSessionId = project.projectId;
    nextSnapshot.canonicalRoot = root;
    nextSnapshot.projectId = project.projectId;
    nextSnapshot.manifestHash = manifestHash;
    nextSnapshot.lockHash = lockHash;
    nextSnapshot.packageGeneration = lockHash;
    LoadPackageSnapshot(root, nextSnapshot);
    nextSnapshot.projectSnapshotIdentity = HashDocumentBytes(project.projectId + "\n" +
        manifestHash + "\n" + lockHash + "\n" + nextSnapshot.packageGeneration + "\n" +
        nextSnapshot.referenceManifestHash + "\n" + nextSnapshot.ownershipManifestHash);
    const auto& identity = nextSnapshot.projectSnapshotIdentity;
    if (client_.State() != LanguageServiceState::Ready ||
        snapshot_.projectSessionId != project.projectId)
    {
        snapshot_ = std::move(nextSnapshot);
        synchronizedVersions_.clear();
        if (!client_.Start(snapshot_))
        {
            EditorDiagnostic diagnostic;
            diagnostic.severity = EditorDiagnosticSeverity::Error;
            diagnostic.source = "editor.language-service";
            diagnostic.code = "LanguageService.Unavailable";
            diagnostic.message = client_.LastDiagnostic();
            diagnostics_.ReplaceSource("editor.language-service", {std::move(diagnostic)});
            refreshing_ = false; return;
        }
        diagnostics_.ClearSource("editor.language-service");
    }
    else if (snapshot_.projectSnapshotIdentity != identity)
    {
        snapshot_ = std::move(nextSnapshot);
        if (!client_.Reconfigure(snapshot_))
        { refreshing_ = false; return; }
        synchronizedVersions_.clear();
    }
    for (const auto& script : workspace_.Scripts())
    {
        const auto document = Identity(*script);
        const auto found = synchronizedVersions_.find(document.documentId);
        if (found == synchronizedVersions_.end())
        {
            (void)client_.OpenDocument(document, script->Text());
            synchronizedVersions_[document.documentId] = document.version;
        }
        else if (found->second < document.version)
        {
            (void)client_.ChangeDocument(document, script->Text());
            found->second = document.version;
        }
    }
    refreshing_ = false;
}

void SagaScriptLanguageServiceSession::Restart()
{
    client_.Stop(); synchronizedVersions_.clear(); Refresh();
}

void SagaScriptLanguageServiceSession::Shutdown()
{
    client_.Stop(); synchronizedVersions_.clear();
    diagnostics_.ClearSource("editor.language-service");
}

LanguageServiceState SagaScriptLanguageServiceSession::State() const noexcept
{ return client_.State(); }
std::string SagaScriptLanguageServiceSession::LastDiagnostic() const
{ return client_.LastDiagnostic(); }
const LanguageServiceProjectSnapshot& SagaScriptLanguageServiceSession::Snapshot() const noexcept
{ return snapshot_; }

std::future<LanguageServiceResponse> SagaScriptLanguageServiceSession::Request(
    std::string method, const CSharpSourceDocument& document,
    std::optional<LanguageServicePosition> position, std::string payloadJson)
{
    return client_.Request(std::move(method), Identity(document), position,
        std::move(payloadJson));
}

std::future<LanguageServiceResponse> SagaScriptLanguageServiceSession::Preflight(
    const CSharpSourceDocument& document, std::string candidateText)
{ return client_.Preflight(Identity(document), std::move(candidateText)); }

SagaScriptLanguageServiceSession::PublicationSubscription
SagaScriptLanguageServiceSession::Subscribe(PublicationCallback callback)
{
    if (!callback) return InvalidPublicationSubscription;
    std::scoped_lock lock(mutex_); const auto id = nextSubscription_++;
    callbacks_.emplace(id, std::move(callback)); return id;
}

void SagaScriptLanguageServiceSession::Unsubscribe(PublicationSubscription subscription)
{ std::scoped_lock lock(mutex_); callbacks_.erase(subscription); }

void SagaScriptLanguageServiceSession::Publish(LanguageServiceResponse publication)
{
    if (publication.projectSessionId != snapshot_.projectSessionId ||
        publication.projectSnapshotIdentity != snapshot_.projectSnapshotIdentity) return;
    std::vector<PublicationCallback> callbacks;
    { std::scoped_lock lock(mutex_); for (const auto& [id, callback] : callbacks_) { (void)id; callbacks.push_back(callback); } }
    for (const auto& callback : callbacks) callback(publication);

    if (publication.method != "analysis/publish" || !publication.documentId ||
        !publication.documentVersion) return;
    std::vector<EditorDiagnostic> diagnostics;
    try
    {
        const auto payload = nlohmann::json::parse(publication.payloadJson);
        for (const auto& value : payload.value("diagnostics", nlohmann::json::array()))
        {
            EditorDiagnostic diagnostic;
            diagnostic.severity = value.value("severity", "") == "Error" ?
                EditorDiagnosticSeverity::Error : value.value("severity", "") == "Warning" ?
                EditorDiagnosticSeverity::Warning : EditorDiagnosticSeverity::Info;
            diagnostic.source = "editor.language-service";
            diagnostic.code = value.value("id", "LanguageService.Diagnostic");
            diagnostic.message = value.value("message", "");
            diagnostic.location.resource = *publication.documentId;
            const auto range = value.value("range", nlohmann::json::object());
            const auto start = range.value("start", nlohmann::json::object());
            diagnostic.location.line = start.value("line", 0) + 1;
            diagnostic.location.column = start.value("character", 0) + 1;
            diagnostic.publishBlocking = diagnostic.severity == EditorDiagnosticSeverity::Error;
            diagnostics.push_back(std::move(diagnostic));
        }
    }
    catch (...) { return; }
    diagnostics_.ReplaceSource("editor.language-service", std::move(diagnostics));
}

LanguageServiceDocumentIdentity SagaScriptLanguageServiceSession::Identity(
    const CSharpSourceDocument& document) const
{
    return {document.Snapshot().documentId,
        document.Snapshot().canonicalSourcePath,
        document.Snapshot().inMemoryRevision};
}

} // namespace SagaEditor
