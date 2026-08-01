// SPDX-License-Identifier: MPL-2.0
#include "SagaEditor/VisualBlocks/ControlledBlockEditController.h"

#include "SagaEditor/Commands/UndoRedoStack.h"
#include "SagaEditor/Documents/SourceDocumentTransaction.h"
#include "SagaEditor/Scripting/SagaScriptLanguageServiceSession.h"

#include <nlohmann/json.hpp>

#include <algorithm>

namespace SagaEditor
{
using Json = nlohmann::json;

ControlledBlockEditController::ControlledBlockEditController(
    SagaScriptLanguageServiceSession& languageService, UndoRedoStack& journal)
    : languageService_(languageService), journal_(journal) {}

ControlledBlockEditPlan ControlledBlockEditController::Plan(
    CSharpSourceDocument& document, std::string fieldId, std::string value)
{
    ControlledBlockEditPlan plan;
    state_ = ControlledBlockEditState::Planning;
    diagnostic_.clear();
    if (languageService_.State() != LanguageServiceState::Ready)
    {
        state_ = ControlledBlockEditState::ServiceRecovering;
        diagnostic_ = "Language service is not ready.";
        return plan;
    }
    const auto source = document.Snapshot();
    const auto project = languageService_.Snapshot();
    plan.operationId = "controlled-edit-" + std::to_string(nextOperation_++);
    plan.projectSessionId = project.projectSessionId;
    plan.projectSnapshotIdentity = project.projectSnapshotIdentity;
    plan.packageGeneration = project.packageGeneration;
    plan.documentId = source.documentId;
    plan.documentVersion = source.inMemoryRevision;
    plan.documentHash = HashDocumentBytes(document.Text());
    plan.fieldId = std::move(fieldId);
    const Json payload{{"fieldId", plan.fieldId}, {"value", value}};
    auto response = languageService_.Request(
        "controlled-edit/plan", document, std::nullopt, payload.dump()).get();
    if (!response.Succeeded())
    {
        state_ = response.errorCode == "LanguageService.OpaqueField" ?
            ControlledBlockEditState::OpaqueReadOnly :
            ControlledBlockEditState::SemanticRejected;
        diagnostic_ = response.errorCode + ": " + response.errorMessage;
        plan.diagnostics.push_back(diagnostic_);
        return plan;
    }
    try
    {
        const auto result = Json::parse(response.payloadJson);
        plan.startByte = result.at("startByte").get<std::uint64_t>();
        plan.endByte = result.at("endByte").get<std::uint64_t>();
        plan.oldChecksum = result.at("oldChecksum").get<std::string>();
        plan.oldValue = result.at("oldValue").get<std::string>();
        plan.replacement = result.at("replacement").get<std::string>();
        plan.candidateText = result.at("candidateText").get<std::string>();
        plan.candidateHash = result.at("candidateHash").get<std::string>();
        plan.minimalDiff = result.at("minimalDiff").get<std::string>();
        plan.parsePassed = result.value("parsePassed", false);
        plan.semanticPassed = result.value("semanticPassed", false);
        plan.analyzersPassed = result.value("analyzersPassed", false);
        plan.packageVisibilityPassed = result.value("packageVisibilityPassed", false);
        plan.accepted = result.value("accepted", false) && plan.parsePassed &&
            plan.semanticPassed && plan.analyzersPassed && plan.packageVisibilityPassed;
        for (const auto& item : result.value("diagnostics", Json::array()))
            plan.diagnostics.push_back(item.value("id", "") + ": " +
                item.value("message", ""));
    }
    catch (const Json::exception& error)
    {
        diagnostic_ = std::string("Controlled edit response is malformed: ") + error.what();
        plan.diagnostics.push_back(diagnostic_);
    }
    state_ = plan.accepted ? ControlledBlockEditState::AwaitingReview :
        ControlledBlockEditState::SemanticRejected;
    return plan;
}

bool ControlledBlockEditController::Apply(CSharpSourceDocument& document,
    const ControlledBlockEditPlan& plan)
{
    state_ = ControlledBlockEditState::Applying;
    diagnostic_.clear();
    const auto source = document.Snapshot();
    const auto project = languageService_.Snapshot();
    if (!plan.accepted || source.documentId != plan.documentId ||
        source.inMemoryRevision != plan.documentVersion ||
        HashDocumentBytes(document.Text()) != plan.documentHash)
    {
        state_ = ControlledBlockEditState::StaleDocument;
        diagnostic_ = "Document changed after controlled edit planning.";
        return false;
    }
    if (project.projectSessionId != plan.projectSessionId ||
        project.projectSnapshotIdentity != plan.projectSnapshotIdentity ||
        project.packageGeneration != plan.packageGeneration)
    {
        state_ = ControlledBlockEditState::GenerationChanged;
        diagnostic_ = "Project or package generation changed after planning.";
        return false;
    }
    if (document.CheckExternalChange() != DocumentExternalChangeState::Unchanged ||
        plan.startByte > plan.endByte || plan.endByte > document.Text().size())
    {
        state_ = ControlledBlockEditState::Conflict;
        diagnostic_ = "External source or controlled span changed.";
        return false;
    }
    const auto oldBytes = document.Text().substr(
        static_cast<std::size_t>(plan.startByte),
        static_cast<std::size_t>(plan.endByte - plan.startByte));
    if (HashDocumentBytes(oldBytes) != plan.oldChecksum)
    {
        state_ = ControlledBlockEditState::SpanConflict;
        diagnostic_ = "Controlled initializer checksum changed.";
        return false;
    }
    auto candidate = document.Text();
    candidate.replace(static_cast<std::size_t>(plan.startByte),
        static_cast<std::size_t>(plan.endByte - plan.startByte), plan.replacement);
    if (candidate != plan.candidateText || HashDocumentBytes(candidate) != plan.candidateHash)
    {
        state_ = ControlledBlockEditState::SpanConflict;
        diagnostic_ = "Controlled edit candidate identity changed.";
        return false;
    }
    CSharpDocumentTransaction transaction(document, journal_);
    if (!transaction.ReplaceTextProvisional(std::move(candidate),
        [&plan](const auto& current)
        { return HashDocumentBytes(current.Text()) == plan.candidateHash; },
        "Edit Controlled Block Field"))
    {
        state_ = ControlledBlockEditState::Conflict;
        diagnostic_ = "Controlled edit provisional validation failed.";
        return false;
    }
    state_ = ControlledBlockEditState::Refreshing;
    return true;
}

ControlledProjectionResult ControlledBlockEditController::AnalyzeProjection(
    const CSharpSourceDocument& document)
{
    ControlledProjectionResult result;
    const auto source = document.Snapshot();
    result.documentId = source.documentId;
    result.documentVersion = source.inMemoryRevision;
    if (languageService_.State() != LanguageServiceState::Ready)
    {
        result.diagnostic = "Language service is not ready.";
        return result;
    }
    const auto response = languageService_.Request(
        "controlled-projection", document).get();
    if (!response.Succeeded())
    {
        result.diagnostic = response.errorCode + ": " + response.errorMessage;
        return result;
    }
    try
    {
        const auto payload = Json::parse(response.payloadJson);
        if (response.documentId != source.documentId ||
            response.documentVersion != source.inMemoryRevision ||
            payload.value("documentId", std::string{}) != source.documentId ||
            payload.value("documentVersion", 0ull) != source.inMemoryRevision)
        {
            result.diagnostic = "Controlled projection response is stale.";
            return result;
        }
        result.sourceHash = payload.value("sourceHash", std::string{});
        if (result.sourceHash != HashDocumentBytes(document.Text()))
        {
            result.diagnostic = "Controlled projection source identity changed.";
            return result;
        }
        const auto project = languageService_.Snapshot();
        std::vector<VisualBlocks::EditableProjectionField> fields;
        std::uint64_t firstByte = document.Text().size();
        std::uint64_t lastByte = 0;
        for (const auto& field : payload.value("editableFields", Json::array()))
        {
            const auto startByte = field.value<std::uint64_t>("startByte", 0);
            const auto endByte = field.value<std::uint64_t>("endByte", 0);
            const auto checksum = field.value("checksum", std::string{});
            if (endByte <= startByte || endByte > document.Text().size() ||
                HashDocumentBytes(document.Text().substr(
                    static_cast<std::size_t>(startByte),
                    static_cast<std::size_t>(endByte - startByte))) != checksum)
            {
                result.diagnostic = "Controlled projection span identity is invalid.";
                return result;
            }
            firstByte = std::min(firstByte, startByte);
            lastByte = std::max(lastByte, endByte);
            VisualBlocks::ProjectionSourceSpan span{
                source.canonicalSourcePath.filename().generic_string(), result.sourceHash,
                startByte, endByte};
            fields.push_back({field.value("fieldId", std::string{}),
                field.value("valueType", std::string{}),
                field.value("minimum", std::string{}),
                field.value("containingSymbol", std::string{}),
                field.value("initializerRole", std::string{}), std::move(span),
                std::to_string(source.inMemoryRevision), project.packageGeneration,
                project.packageGeneration, checksum,
                field.value("value", std::string{})});
        }
        std::vector<VisualBlocks::ProjectionCard> roots;
        std::vector<VisualBlocks::ProjectionCard> opaque;
        if (fields.size() == 2)
        {
            VisualBlocks::ProjectionSourceSpan span{
                source.canonicalSourcePath.filename().generic_string(), result.sourceHash,
                firstByte, lastByte};
            roots.emplace_back("controlled://ArenaRush/EnergyCore", "ControlledFields",
                "Energy Core", project.packageGeneration, std::move(span));
        }
        else
        {
            VisualBlocks::ProjectionSourceSpan span{
                source.canonicalSourcePath.filename().generic_string(), result.sourceHash,
                0, static_cast<std::uint64_t>(document.Text().size())};
            opaque.emplace_back("opaque://ArenaRush/CustomSource", "OpaqueCSharp",
                payload.value("opaqueReason", "Unsupported source shape"),
                project.packageGeneration, std::move(span));
            fields.clear();
        }
        result.artifactHash = HashDocumentBytes("controlled-projection\n" +
            result.sourceHash + "\n" + std::to_string(source.inMemoryRevision) + "\n" +
            project.projectSnapshotIdentity);
        result.projection = std::make_shared<const VisualBlocks::ReadOnlyScriptProjection>(
            project.projectId, project.projectSnapshotIdentity,
            std::to_string(source.inMemoryRevision), project.packageGeneration,
            project.packageGeneration, result.artifactHash, std::move(roots),
            std::move(opaque), std::move(fields));
    }
    catch (const Json::exception& error)
    {
        result.diagnostic = std::string("Controlled projection response is malformed: ") +
            error.what();
    }
    return result;
}

void ControlledBlockEditController::Cancel()
{
    diagnostic_.clear();
    state_ = ControlledBlockEditState::Ready;
}
}
