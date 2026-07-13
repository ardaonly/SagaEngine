/// @file VisualBlocksDescriptor.cpp
/// @brief Read-only Product Shell generation from C# and generated evidence.

#include "VisualBlocks/VisualBlocksDescriptor.h"

#include <QCryptographicHash>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <fstream>
#include <set>
#include <utility>

namespace SagaProduct
{
namespace
{

using Json = nlohmann::json;

constexpr const char* kProjectId = "starter-arena";
constexpr const char* kScriptId = "script://starter-arena/game-rules";
constexpr const char* kTypeName = "StarterArena.Scripts.GameRules";
constexpr const char* kGameplayCapability =
    "Sample.StarterArena.GameplayState";

const std::array<std::string, 8> kNonClaims = {
    "No Visual Blocks canvas claim",
    "No Visual Blocks runtime execution claim",
    "No generated C# from blocks claim",
    "No editor graph editing claim",
    "No production block library claim",
    "No package install or distribution claim",
    "No networking or multiplayer claim",
    "No production readiness claim",
};

[[nodiscard]] Json DiagnosticJson(const FirstPlayableDiagnostic& diagnostic)
{
    Json value = {
        {"severity", ToString(diagnostic.severity)},
        {"code", diagnostic.code},
        {"message", diagnostic.message},
    };
    if (diagnostic.profileId) value["profileId"] = *diagnostic.profileId;
    if (diagnostic.path) value["path"] = diagnostic.path->string();
    if (diagnostic.actionHint) value["actionHint"] = *diagnostic.actionHint;
    if (diagnostic.blockId) value["blockId"] = *diagnostic.blockId;
    if (diagnostic.fieldPath) value["fieldPath"] = *diagnostic.fieldPath;
    return value;
}

void AddFailure(VisualBlocksDescriptorGenerationResult& result,
                std::string code,
                std::string message,
                const std::filesystem::path& path,
                std::optional<std::string> blockId = std::nullopt,
                std::optional<std::string> fieldPath = std::nullopt,
                std::optional<std::string> actionHint = std::nullopt)
{
    FirstPlayableDiagnostic diagnostic;
    diagnostic.code = std::move(code);
    diagnostic.message = std::move(message);
    diagnostic.profileId = kVisualBlocksDescriptorProfileId;
    if (!path.empty()) diagnostic.path = path;
    diagnostic.blockId = std::move(blockId);
    diagnostic.fieldPath = std::move(fieldPath);
    diagnostic.actionHint = std::move(actionHint);
    result.diagnostics.push_back(std::move(diagnostic));
    result.status = EvidenceStatus::Failed;
}

void AddIncomplete(VisualBlocksDescriptorGenerationResult& result,
                   std::string message,
                   const std::filesystem::path& path,
                   std::optional<std::string> fieldPath = std::nullopt)
{
    FirstPlayableDiagnostic diagnostic;
    diagnostic.code = "ProductShell.VisualBlocks.RuntimeEvidenceMismatch";
    diagnostic.message = std::move(message);
    diagnostic.profileId = kVisualBlocksDescriptorProfileId;
    diagnostic.path = path;
    diagnostic.fieldPath = std::move(fieldPath);
    diagnostic.actionHint =
        "Run all first-playable runtime profiles successfully and retry.";
    result.diagnostics.push_back(std::move(diagnostic));
    if (result.status != EvidenceStatus::Failed)
        result.status = EvidenceStatus::Incomplete;
}

[[nodiscard]] bool IsWithin(const std::filesystem::path& child,
                            const std::filesystem::path& parent)
{
    std::error_code ec;
    const auto canonicalChild = std::filesystem::weakly_canonical(child, ec);
    if (ec) return false;
    const auto canonicalParent = std::filesystem::weakly_canonical(parent, ec);
    if (ec) return false;
    auto childIt = canonicalChild.begin();
    for (auto parentIt = canonicalParent.begin();
         parentIt != canonicalParent.end(); ++parentIt, ++childIt)
    {
        if (childIt == canonicalChild.end() || *childIt != *parentIt)
            return false;
    }
    return true;
}

[[nodiscard]] std::string FileSha256(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    std::array<char, 16384> buffer{};
    while (input)
    {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        hash.addData(QByteArrayView(
            buffer.data(), static_cast<qsizetype>(input.gcount())));
    }
    return hash.result().toHex().toStdString();
}

[[nodiscard]] std::optional<Json> LoadJson(
    const std::filesystem::path& path,
    VisualBlocksDescriptorGenerationResult& result,
    const std::string& code,
    const std::string& label)
{
    std::ifstream input(path);
    if (!input)
    {
        AddFailure(result, code, label + " is missing", path);
        return std::nullopt;
    }
    Json value;
    try { input >> value; }
    catch (const std::exception& error)
    {
        AddFailure(result, code, label + " is not valid JSON: " + error.what(),
                   path);
        return std::nullopt;
    }
    if (!value.is_object())
    {
        AddFailure(result, code, label + " must be a JSON object", path);
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] bool HasBlockingDiagnostics(const Json& root)
{
    const Json summary = root.value("diagnosticSummary", Json::object());
    return !summary.is_object() ||
        summary.value("hasBlockingDiagnostics", false) ||
        summary.value("errorCount", 0) > 0;
}

[[nodiscard]] bool ContainsString(const Json& array, const std::string& value)
{
    return array.is_array() && std::any_of(array.begin(), array.end(),
        [&](const Json& item) { return item.is_string() && item == value; });
}

[[nodiscard]] std::optional<Json> LoadEvidence(
    const VisualBlocksEvidenceInput& evidence,
    const char* label,
    VisualBlocksDescriptorGenerationResult& result)
{
    if (evidence.status != EvidenceStatus::Passed ||
        !std::filesystem::is_regular_file(evidence.reportPath))
    {
        AddIncomplete(result, std::string(label) +
            " runtime evidence is not available as Passed",
            evidence.reportPath);
        return std::nullopt;
    }
    std::ifstream input(evidence.reportPath);
    Json value;
    try { input >> value; }
    catch (const std::exception& error)
    {
        AddIncomplete(result, std::string(label) +
            " runtime evidence is malformed: " + error.what(),
            evidence.reportPath);
        return std::nullopt;
    }
    if (!value.is_object() || value.value("status", "") != "Passed")
    {
        AddIncomplete(result, std::string(label) +
            " runtime evidence top-level status is not Passed",
            evidence.reportPath, "status");
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] VisualBlocksPort Port(std::string id,
                                    std::string name,
                                    std::string type)
{
    return {std::move(id), std::move(name), std::move(type), true};
}

[[nodiscard]] VisualBlocksBlock BasicBlock(std::string id,
                                           std::string kind,
                                           std::string displayName,
                                           std::string sourceSymbol)
{
    VisualBlocksBlock block;
    block.id = std::move(id);
    block.kind = std::move(kind);
    block.displayName = std::move(displayName);
    block.sourceSymbol = std::move(sourceSymbol);
    return block;
}

[[nodiscard]] VisualBlocksBlock StateBlock(std::string id,
                                           std::string kind,
                                           std::string displayName,
                                           std::string sourceSymbol,
                                           std::string operation,
                                           std::string target,
                                           std::string type)
{
    VisualBlocksBlock block = BasicBlock(std::move(id), std::move(kind),
        std::move(displayName), std::move(sourceSymbol));
    const bool read = block.kind == "StateRead";
    if (read) block.outputs.push_back(Port("value", "value", type));
    else
    {
        block.inputs.push_back(Port("value", "value", type));
        block.outputs.push_back(Port("completed", "completed", "void"));
    }
    block.stateAccess = VisualBlocksStateAccess{
        std::move(operation), std::move(target), std::move(type)};
    block.requiredCapabilities.push_back(kGameplayCapability);
    return block;
}

[[nodiscard]] VisualBlocksDescriptor GenerateDescriptor(
    const Json& binding,
    const std::vector<std::string>& callbacks)
{
    VisualBlocksDescriptor descriptor;
    descriptor.projectId = kProjectId;
    descriptor.scriptId = kScriptId;
    descriptor.typeName = kTypeName;
    descriptor.source = "Scripts/GameRules.cs";
    descriptor.capabilities.push_back(kGameplayCapability);
    descriptor.nonClaims.assign(kNonClaims.begin(), kNonClaims.end());

    const std::array<std::pair<const char*, const char*>, 4> lifecycle = {{
        {"lifecycle.on-create", "On Create"},
        {"lifecycle.on-start", "On Start"},
        {"lifecycle.on-update", "On Update"},
        {"lifecycle.on-destroy", "On Destroy"},
    }};
    for (std::size_t index = 0; index < lifecycle.size(); ++index)
    {
        auto block = BasicBlock(lifecycle[index].first, "LifecycleEvent",
                                lifecycle[index].second, callbacks[index]);
        block.outputs.push_back(Port("event", "Event", "void"));
        descriptor.blocks.push_back(std::move(block));
    }

    auto pure = BasicBlock("pure.add-pickup-score", "PureFunction",
                           "Add Pickup Score", binding.value("methodName", ""));
    for (const Json& parameter : binding["parameters"])
    {
        const std::string name = parameter.value("name", "");
        pure.inputs.push_back(Port(name, name, "int32"));
    }
    pure.outputs.push_back(Port("result", "result", "int32"));
    descriptor.blocks.push_back(std::move(pure));

    descriptor.blocks.push_back(StateBlock("state.read-pickup-reachable",
        "StateRead", "Read Pickup Reachable", "GetPickupReachable", "GetBool",
        "starter.pickup.0.reachable", "bool"));
    descriptor.blocks.push_back(StateBlock("state.read-pickup-collected",
        "StateRead", "Read Pickup Collected", "GetPickupCollected", "GetBool",
        "starter.pickup.0.collected", "bool"));
    descriptor.blocks.push_back(StateBlock("state.set-pickup-collected",
        "StateWrite", "Set Pickup Collected", "SetPickupCollected", "SetBool",
        "starter.pickup.0.collected", "bool"));
    descriptor.blocks.push_back(StateBlock("state.add-score", "StateWrite",
        "Add Score", "AddScore", "AddInt32", "starter.score", "int32"));
    descriptor.blocks.push_back(StateBlock("state.set-player-powered",
        "StateWrite", "Set Player Powered", "SetPlayerPowered", "SetString",
        "starter.player.state", "string"));

    auto branch = BasicBlock("control.branch-pickup-available", "Branch",
        "Pickup Available", "PickupAvailable");
    branch.inputs.push_back(Port("condition", "condition", "bool"));
    branch.outputs.push_back(Port("true", "true", "void"));
    branch.outputs.push_back(Port("false", "false", "void"));
    descriptor.blocks.push_back(std::move(branch));
    auto returnBlock = BasicBlock("control.return-noop", "Return",
                                  "Return No-op", "NoOp");
    returnBlock.outputs.push_back(Port("result", "result", "void"));
    descriptor.blocks.push_back(std::move(returnBlock));
    return descriptor;
}

[[nodiscard]] Json PortJson(const VisualBlocksPort& port)
{
    return {{"id", port.id}, {"name", port.name}, {"type", port.type},
            {"required", port.required}};
}

[[nodiscard]] Json BlockJson(const VisualBlocksBlock& block)
{
    Json inputs = Json::array();
    Json outputs = Json::array();
    for (const auto& port : block.inputs) inputs.push_back(PortJson(port));
    for (const auto& port : block.outputs) outputs.push_back(PortJson(port));
    Json value = {{"id", block.id}, {"kind", block.kind},
        {"displayName", block.displayName}, {"sourceSymbol", block.sourceSymbol},
        {"inputs", inputs}, {"outputs", outputs},
        {"requiredCapabilities", block.requiredCapabilities}};
    if (block.stateAccess)
        value["stateAccess"] = {{"operation", block.stateAccess->operation},
            {"target", block.stateAccess->target},
            {"valueType", block.stateAccess->valueType}};
    return value;
}

void WriteReport(const VisualBlocksDescriptorGenerationRequest& request,
                 VisualBlocksDescriptorGenerationResult& result)
{
    Json diagnostics = Json::array();
    for (const auto& diagnostic : result.diagnostics)
        diagnostics.push_back(DiagnosticJson(diagnostic));
    Json blocks = Json::array();
    if (result.descriptor)
        for (const auto& block : result.descriptor->blocks)
            blocks.push_back(BlockJson(block));
    const Json report = {
        {"schemaVersion", 1},
        {"tool", "Saga"},
        {"command", "first-playable-check"},
        {"profileId", kVisualBlocksDescriptorProfileId},
        {"status", ToString(result.status)},
        {"sourceOfTruth", "CSharpAndGeneratedEvidence"},
        {"generation", "InProcessReadOnlyGeneration"},
        {"originalCSharpSourcePath", result.sourcePath.string()},
        {"originalCSharpSourceHash", result.sourceHash},
        {"bindingManifestPath", request.bindingManifestPath.string()},
        {"artifactManifestPath", request.artifactManifestPath.string()},
        {"lifecycleEvidenceReportPath", request.lifecycleEvidence.reportPath.string()},
        {"gameplayEvidenceReportPath", request.gameplayEvidence.reportPath.string()},
        {"projectId", result.projectId},
        {"scriptId", result.scriptId},
        {"typeName", result.typeName},
        {"blocks", blocks},
        {"blockCount", blocks.size()},
        {"generatedBlockIds", result.generatedBlockIds},
        {"observedCallbacks", result.observedCallbacks},
        {"matchedGameplayMutations", result.matchedGameplayMutations},
        {"requiredCapabilities", result.requiredCapabilities},
        {"processLaunched", false},
        {"csharpExecuted", false},
        {"nonClaims", result.nonClaims},
        {"diagnostics", diagnostics},
    };
    std::error_code ec;
    std::filesystem::create_directories(request.reportPath.parent_path(), ec);
    std::ofstream output(request.reportPath, std::ios::trunc);
    if (ec || !output)
    {
        AddFailure(result, "ProductShell.VisualBlocks.ReportWriteFailed",
            "could not write Visual Blocks descriptor report", request.reportPath);
        return;
    }
    output << report.dump(2) << '\n';
    if (!output)
        AddFailure(result, "ProductShell.VisualBlocks.ReportWriteFailed",
            "failed while writing Visual Blocks descriptor report", request.reportPath);
}

} // namespace

VisualBlocksDescriptorGenerationResult GenerateVisualBlocksDescriptorReport(
    const VisualBlocksDescriptorGenerationRequest& request)
{
    VisualBlocksDescriptorGenerationResult result;
    result.status = EvidenceStatus::Passed;
    result.reportPath = request.reportPath;
    result.projectId = kProjectId;
    result.scriptId = kScriptId;
    result.typeName = kTypeName;
    result.requiredCapabilities = {kGameplayCapability};
    result.nonClaims.assign(kNonClaims.begin(), kNonClaims.end());

    const auto projectManifest = std::filesystem::absolute(
        request.originalProjectManifest);
    const auto projectRoot = projectManifest.parent_path();
    result.sourcePath = projectRoot / "Scripts" / "GameRules.cs";
    const auto workspaceSource = request.generatedWorkspace / "Scripts" /
        "GameRules.cs";

    auto project = LoadJson(projectManifest, result,
        "ProductShell.VisualBlocks.ProjectMismatch", "original project manifest");
    if (project && project->value("projectId", "") != kProjectId)
        AddFailure(result, "ProductShell.VisualBlocks.ProjectMismatch",
            "original project manifest projectId must be starter-arena",
            projectManifest, std::nullopt, "projectId");

    const bool originalValid = IsWithin(result.sourcePath, projectRoot) &&
        std::filesystem::is_regular_file(result.sourcePath);
    if (!originalValid)
        AddFailure(result, IsWithin(result.sourcePath, projectRoot) ?
            "ProductShell.VisualBlocks.SourceMissing" :
            "ProductShell.VisualBlocks.SourceOutsideProject",
            "original Scripts/GameRules.cs must be a regular file inside the project root",
            result.sourcePath);
    const bool workspaceValid = IsWithin(workspaceSource,
        request.generatedWorkspace) && std::filesystem::is_regular_file(workspaceSource);
    if (!workspaceValid)
        AddFailure(result, IsWithin(workspaceSource, request.generatedWorkspace) ?
            "ProductShell.VisualBlocks.SourceMissing" :
            "ProductShell.VisualBlocks.SourceOutsideProject",
            "generated Scripts/GameRules.cs must be a regular file inside the generated workspace",
            workspaceSource);
    const std::string originalHash = originalValid ? FileSha256(result.sourcePath) : "";
    const std::string workspaceHash = workspaceValid ? FileSha256(workspaceSource) : "";
    result.sourceHash = originalHash;
    if (originalValid && workspaceValid &&
        (originalHash.empty() || originalHash != workspaceHash))
        AddFailure(result, "ProductShell.VisualBlocks.SourceHashMismatch",
            "original and generated GameRules.cs content hashes must match",
            workspaceSource);

    auto bindings = LoadJson(request.bindingManifestPath, result,
        "ProductShell.VisualBlocks.MetadataMissing", "script binding manifest");
    auto artifacts = LoadJson(request.artifactManifestPath, result,
        "ProductShell.VisualBlocks.ArtifactMismatch", "script artifact manifest");
    Json callable;
    std::string bindingId;
    if (bindings)
    {
        if (HasBlockingDiagnostics(*bindings))
            AddFailure(result, "ProductShell.VisualBlocks.MetadataMissing",
                "script binding manifest contains blocking or missing diagnostic summary",
                request.bindingManifestPath, std::nullopt, "diagnosticSummary");
        if (bindings->value("sourceHash", "").empty())
            AddFailure(result, "ProductShell.VisualBlocks.SourceHashMismatch",
                "script binding manifest requires a non-empty sourceHash",
                request.bindingManifestPath, std::nullopt, "sourceHash");
        const Json entries = bindings->value("bindings", Json::array());
        std::vector<Json> matches;
        if (entries.is_array())
            for (const Json& binding : entries)
                if (binding.is_object() && binding.value("scriptId", "") == kScriptId &&
                    binding.value("declaringType", "") == kTypeName &&
                    binding.value("methodName", "") == "AddPickupScore")
                    matches.push_back(binding);
        if (matches.size() != 1)
            AddFailure(result, "ProductShell.VisualBlocks.MetadataMissing",
                "exactly one AddPickupScore binding must match GameRules",
                request.bindingManifestPath, "pure.add-pickup-score", "bindings");
        else
        {
            callable = matches.front();
            bindingId = callable.value("bindingId", "");
            const Json parameters = callable.value("parameters", Json::array());
            const Json returnType = callable.value("returnType", Json::object());
            const bool signature = callable.value("authority", "") == "SharedPure" &&
                parameters.is_array() && parameters.size() == 2 &&
                parameters[0].is_object() && parameters[1].is_object() &&
                parameters[0].value("name", "") == "currentScore" &&
                parameters[1].value("name", "") == "pickupValue" &&
                parameters[0].value("type", "") == "int" &&
                parameters[1].value("type", "") == "int" &&
                parameters[0].value("supported", false) &&
                parameters[1].value("supported", false) &&
                returnType.is_object() && returnType.value("type", "") == "int" &&
                returnType.value("supported", false);
            if (!signature)
                AddFailure(result, "ProductShell.VisualBlocks.MethodSignatureMismatch",
                    "AddPickupScore must be SharedPure with supported currentScore: int and pickupValue: int parameters and an int return",
                    request.bindingManifestPath, "pure.add-pickup-score", "bindings");
            const std::filesystem::path declaredSource = callable.value("sourceFile", "");
            std::error_code ec;
            if (bindingId.empty() || callable.value("sourceHash", "") != originalHash ||
                !IsWithin(declaredSource, request.generatedWorkspace) ||
                std::filesystem::weakly_canonical(declaredSource, ec) !=
                    std::filesystem::weakly_canonical(workspaceSource, ec))
                AddFailure(result, "ProductShell.VisualBlocks.SourceHashMismatch",
                    "callable binding source path and hash must match both GameRules.cs copies",
                    request.bindingManifestPath, "pure.add-pickup-score",
                    "bindings[].sourceHash");
        }
    }

    if (artifacts)
    {
        if (HasBlockingDiagnostics(*artifacts))
            AddFailure(result, "ProductShell.VisualBlocks.ArtifactMismatch",
                "script artifact manifest contains blocking or missing diagnostic summary",
                request.artifactManifestPath, std::nullopt, "diagnosticSummary");
        if (!bindings || artifacts->value("sourceHash", "") !=
                bindings->value("sourceHash", ""))
            AddFailure(result, "ProductShell.VisualBlocks.SourceHashMismatch",
                "binding and artifact manifest source hashes must agree",
                request.artifactManifestPath, std::nullopt, "sourceHash");
        const Json entries = artifacts->value("artifacts", Json::array());
        std::vector<Json> matches;
        if (entries.is_array())
            for (const Json& artifact : entries)
                if (artifact.is_object() && artifact.value("scriptId", "") == kScriptId)
                    matches.push_back(artifact);
        if (matches.size() != 1)
            AddFailure(result, "ProductShell.VisualBlocks.ArtifactMismatch",
                "exactly one GameRules artifact must exist",
                request.artifactManifestPath, std::nullopt, "artifacts");
        else
        {
            const Json& artifact = matches.front();
            const Json bindingIds = artifact.value("bindingIds", Json::array());
            const Json sourceFiles = artifact.value("sourceFiles", Json::array());
            if (bindingId.empty() || !bindingIds.is_array() ||
                bindingIds.size() != 1 || !ContainsString(bindingIds, bindingId) ||
                !sourceFiles.is_array() || sourceFiles.size() != 1 ||
                !ContainsString(sourceFiles, "Scripts/GameRules.cs"))
                AddFailure(result, "ProductShell.VisualBlocks.ArtifactMismatch",
                    "GameRules artifact must reference exactly its callable binding and source file",
                    request.artifactManifestPath, std::nullopt, "artifacts");
            for (const char* key : {"assemblyPath", "runtimeConfigPath"})
            {
                const std::filesystem::path declared = artifact.value(key, "");
                const auto resolved = declared.is_absolute() ? declared :
                    request.generatedWorkspace / declared;
                if (declared.empty() || !IsWithin(resolved, request.generatedWorkspace) ||
                    !std::filesystem::is_regular_file(resolved))
                    AddFailure(result, "ProductShell.VisualBlocks.ArtifactMismatch",
                        std::string(key) +
                            " must resolve to a regular file inside the generated workspace",
                        resolved, std::nullopt, std::string("artifacts[].") + key);
            }
        }
    }

    const auto lifecycle = LoadEvidence(request.lifecycleEvidence, "lifecycle", result);
    if (lifecycle)
    {
        const Json section = lifecycle->value("scriptLifecycle", Json::object());
        const Json callbacks = section.value("callbacksObserved", Json::array());
        const std::array<std::string, 4> expected = {
            "OnCreate", "OnStart", "OnUpdate", "OnDestroy"};
        bool valid = section.value("status", "") == "Passed" &&
            callbacks.is_array() && callbacks.size() == expected.size();
        for (const auto& callback : expected)
            valid = valid && ContainsString(callbacks, callback);
        if (!valid)
            AddIncomplete(result,
                "lifecycle report must observe exactly OnCreate, OnStart, OnUpdate, and OnDestroy",
                request.lifecycleEvidence.reportPath,
                "scriptLifecycle.callbacksObserved");
        else result.observedCallbacks.assign(expected.begin(), expected.end());
    }

    const auto gameplay = LoadEvidence(request.gameplayEvidence, "gameplay", result);
    if (gameplay)
    {
        const Json section = gameplay->value("gameplay", Json::object());
        if (section.value("status", "") != "Passed" ||
            section.value("capability", "") != kGameplayCapability)
            AddFailure(result, "ProductShell.VisualBlocks.CapabilityMissing",
                "passed gameplay evidence must expose Sample.StarterArena.GameplayState",
                request.gameplayEvidence.reportPath, std::nullopt,
                "gameplay.capability");
        const Json mutations = section.value("mutations", Json::array());
        struct ExpectedMutation { const char* operation; const char* target; const char* type; };
        const std::array<ExpectedMutation, 3> expected = {{
            {"SetBool", "starter.pickup.0.collected", "bool"},
            {"AddInt32", "starter.score", "int32"},
            {"SetString", "starter.player.state", "string"},
        }};
        bool valid = mutations.is_array() && mutations.size() == expected.size();
        for (const auto& item : expected)
        {
            bool found = false;
            if (mutations.is_array())
                for (const Json& mutation : mutations)
                {
                    const auto typedValue = [&](const Json& value) {
                        if (std::string(item.type) == "bool")
                            return value.is_boolean();
                        if (std::string(item.type) == "int32")
                            return value.is_number_integer();
                        return value.is_string();
                    };
                    found = found || (mutation.is_object() &&
                        mutation.value("operation", "") == item.operation &&
                        mutation.value("target", "") == item.target &&
                        mutation.contains("before") && mutation.contains("after") &&
                        typedValue(mutation["before"]) &&
                        typedValue(mutation["after"]));
                }
            valid = valid && found;
        }
        if (!valid)
            AddFailure(result, "ProductShell.VisualBlocks.RuntimeEvidenceMismatch",
                "gameplay evidence must contain exactly the three expected typed mutations",
                request.gameplayEvidence.reportPath, std::nullopt,
                "gameplay.mutations");
        else
            for (const auto& item : expected)
                result.matchedGameplayMutations.push_back(
                    std::string(item.operation) + " -> " + item.target);
    }

    if (result.diagnostics.empty())
    {
        auto descriptor = GenerateDescriptor(callable, result.observedCallbacks);
        descriptor.sourceHash = result.sourceHash;
        for (const auto& block : descriptor.blocks)
            result.generatedBlockIds.push_back(block.id);
        result.descriptor = std::move(descriptor);
        result.status = EvidenceStatus::Passed;
    }
    WriteReport(request, result);
    return result;
}

} // namespace SagaProduct
