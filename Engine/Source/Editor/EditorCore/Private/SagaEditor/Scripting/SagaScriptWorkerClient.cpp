// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Scripting/SagaScriptWorkerClient.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <set>

namespace SagaEditor
{
namespace
{
using Json = nlohmann::json;

bool WriteJson(const std::filesystem::path& path, const Json& value,
               std::error_code& error)
{
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) return false;
    return AtomicWriteDocument(path, value.dump(2) + "\n", "worker", error);
}

Json InputJson(const ScriptWorkerInput& input)
{ return {{"hash", input.hash}, {"path", input.path.string()}}; }

std::vector<std::string> ReadProgress(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    std::vector<std::string> stages;
    std::string line;
    while (std::getline(input, line))
    {
        try
        {
            const auto record = Json::parse(line);
            if (record.value("schemaVersion", -1) == 1 && record.contains("stage"))
                stages.push_back(record.value("stage", std::string{}));
        }
        catch (const Json::exception&) {}
    }
    return stages;
}
}

SagaScriptWorkerClient::SagaScriptWorkerClient(std::filesystem::path executable,
                                               std::chrono::milliseconds timeout)
    : executable_(std::move(executable)), timeout_(timeout) {}

ScriptWorkerResult SagaScriptWorkerClient::Run(const ScriptWorkerRequest& request) const
{
    ScriptWorkerResult result;
    if (request.operationId.empty() || request.sourceRevision.empty() ||
        request.projectId.empty() || request.transportRoot.empty())
    {
        result.diagnostics.push_back({"Script.Worker.InvalidRequest",
            "Operation, revision, project, and transport identities are required."});
        return result;
    }
    std::filesystem::create_directories(request.transportRoot);
    const auto requestPath = request.transportRoot / (request.operationId + ".request.json");
    const auto resultPath = request.transportRoot / (request.operationId + ".result.json");
    const auto progressPath = request.transportRoot / (request.operationId + ".progress.ndjson");
    const auto cancellationPath = request.transportRoot / (request.operationId + ".cancel.json");
    Json sources = Json::array();
    for (const auto& source : request.sources)
        sources.push_back({{"hash", source.hash}, {"path", source.relativePath.generic_string()}});
    const Json document = {
        {"apiSurfaceManifest", InputJson(request.apiSurfaceManifest)},
        {"cancellation", {{"operationId", request.operationId}, {"path", cancellationPath.string()}}},
        {"operationId", request.operationId},
        {"operations", request.operations},
        {"outputRoot", request.outputRoot.string()},
        {"packageLock", InputJson(request.packageLock)},
        {"packageGenerationId", request.packageGenerationId},
        {"packageGenerationHash", request.packageGenerationHash},
        {"packageGenerationManifestHash", request.packageGenerationManifestHash},
        {"packageGraphHash", request.packageGraphHash},
        {"project", {{"hash", request.project.hash}, {"path", request.project.path.string()},
                     {"projectId", request.projectId}}},
        {"referenceManifest", InputJson(request.referenceManifest)},
        {"runtimeBridge", {
            {"assemblyHash", request.runtimeBridge.assembly.hash},
            {"assemblyPath", request.runtimeBridge.assembly.path.string()},
            {"descriptorHash", request.runtimeBridge.descriptor.hash},
            {"descriptorPath", request.runtimeBridge.descriptor.path.string()},
            {"logicalReferenceId", request.runtimeBridge.logicalReferenceId},
            {"runtimeCompatibility", request.runtimeBridge.runtimeCompatibility},
            {"targetFramework", request.runtimeBridge.targetFramework},
            {"trust", request.runtimeBridge.trust}}},
        {"schemaVersion", 1},
        {"sourceRevision", request.sourceRevision},
        {"sources", std::move(sources)}};
    std::error_code writeError;
    if (!WriteJson(requestPath, document, writeError))
    {
        result.diagnostics.push_back({"Script.Worker.RequestWriteFailed",
            writeError.message(), requestPath});
        return result;
    }

    std::set<std::string> reportedStages;
    ToolProcessRequest processRequest;
    processRequest.executable = executable_;
    processRequest.arguments = {"worker", "--request", requestPath.string(),
        "--result", resultPath.string(), "--progress", progressPath.string()};
    processRequest.timeout = timeout_;
    processRequest.cancellationRequested = request.cancellationRequested;
    processRequest.requestCancellation = [cancellationPath, operationId = request.operationId]
    {
        std::error_code ignored;
        (void)WriteJson(cancellationPath,
            {{"operationId", operationId}, {"schemaVersion", 1}}, ignored);
    };
    processRequest.onPoll = [&]
    {
        for (const auto& stage : ReadProgress(progressPath))
            if (reportedStages.insert(stage).second && request.progress) request.progress(stage);
    };
    result.process = ToolProcessRunner{}.Run(processRequest);
    result.progressStages = ReadProgress(progressPath);
    std::ifstream input(resultPath, std::ios::binary);
    if (!input)
    {
        result.diagnostics.push_back({"Script.Worker.ResultMissing",
            "SagaScript worker did not publish a structured result.", resultPath});
        return result;
    }
    try
    {
        const auto report = Json::parse(input);
        if (!report.is_object() || report.value("schemaVersion", -1) != 1 ||
            report.value("tool", std::string{}) != "sagascript")
        {
            result.diagnostics.push_back({"Script.Worker.ResultMalformed",
                "SagaScript worker result schema is invalid.", resultPath});
            return result;
        }
        result.operationId = report.value("operationId", std::string{});
        result.sourceRevision = report.value("sourceRevision", std::string{});
        result.status = report.value("status", std::string{});
        result.failureClass = report.value("failureClass", std::string{});
        result.cancelled = report.value("cancelled", false);
        result.stale = report.value("stale", false);
        result.currentManifestHash = report.value("currentManifestHash", std::string{});
        result.packageGenerationId = report.value("packageGenerationId", std::string{});
        result.packageGenerationHash = report.value("packageGenerationHash", std::string{});
        result.packageGenerationManifestHash = report.value("packageGenerationManifestHash", std::string{});
        result.packageGraphHash = report.value("packageGraphHash", std::string{});
        for (const auto& diagnostic : report.value("diagnostics", Json::array()))
            result.diagnostics.push_back({diagnostic.value("code", std::string{}),
                diagnostic.value("message", std::string{}),
                diagnostic.value("path", std::string{})});
        if (result.operationId != request.operationId ||
            result.sourceRevision != request.sourceRevision ||
            result.packageGenerationId != request.packageGenerationId ||
            result.packageGenerationHash != request.packageGenerationHash ||
            result.packageGenerationManifestHash != request.packageGenerationManifestHash ||
            result.packageGraphHash != request.packageGraphHash)
        {
            result.stale = true;
            result.status = "StaleResult";
            result.failureClass = "StaleResult";
            result.diagnostics.push_back({"Script.Worker.ResultIdentityMismatch",
                "Worker result identity does not match the requested operation."});
        }
        result.passed = result.process.Succeeded() && result.status == "Passed" &&
            !result.cancelled && !result.stale;
    }
    catch (const Json::exception& error)
    {
        result.diagnostics.push_back({"Script.Worker.ResultMalformed", error.what(), resultPath});
    }
    return result;
}

} // namespace SagaEditor
