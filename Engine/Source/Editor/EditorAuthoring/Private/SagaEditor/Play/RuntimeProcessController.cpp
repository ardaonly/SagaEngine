// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Play/RuntimeProcessController.h"

#include "SagaEditor/Documents/Document.h"
#include "SagaEditor/Processes/ToolProcessRunner.h"
#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"
#include "SagaRuntime/Play/RuntimePlayContracts.hpp"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <fstream>
#include <sstream>

namespace SagaEditor
{
namespace
{
using Json = nlohmann::json;

std::map<std::string, std::string> ReadEnvelopeEnvironment(
    const std::filesystem::path& envelopePath)
{
    std::ifstream input(envelopePath, std::ios::binary);
    Json root;
    try { if (input) input >> root; }
    catch (const Json::exception&) { return {}; }
    std::map<std::string, std::string> result;
    if (root.contains("environment") && root["environment"].is_object())
        for (const auto& [name, value] : root["environment"].items())
            if (value.is_string()) result.emplace(name, value.get<std::string>());
    for (const char* name : {"DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY", "XDG_RUNTIME_DIR",
                             "DOTNET_ROOT", "DOTNET_ROOT_X64",
                             "DOTNET_SYSTEM_GLOBALIZATION_INVARIANT", "LD_LIBRARY_PATH"})
        if (const char* value = std::getenv(name); value && *value) result[name] = value;
    return result;
}

std::string ReadBytes(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    std::ostringstream bytes;
    if (input) bytes << input.rdbuf();
    return bytes.str();
}
}

RuntimeProcessController::~RuntimeProcessController() { Shutdown(); }

bool RuntimeProcessController::Start(EditorDocumentWorkspace& workspace,
                                     RuntimeProcessLaunchRequest request)
{
    {
        std::scoped_lock lock(mutex_);
        if (snapshot_.state == RuntimePlayState::Preparing ||
            snapshot_.state == RuntimePlayState::Starting ||
            snapshot_.state == RuntimePlayState::Running ||
            snapshot_.state == RuntimePlayState::Stopping || request.playSessionId.empty() ||
            request.runtimeExecutable.empty()) return false;
    }
    const auto captured = CapturePlaySourceSnapshot(workspace, request.playSessionId);
    if (!captured.Succeeded())
    {
        std::scoped_lock lock(mutex_);
        snapshot_ = {};
        snapshot_.state = RuntimePlayState::Failed;
        snapshot_.playSessionId = request.playSessionId;
        snapshot_.diagnosticCode = captured.diagnostics.empty() ?
            "Play.Snapshot.Failed" : captured.diagnostics.front().code;
        snapshot_.message = captured.diagnostics.empty() ?
            "Saved-source snapshot failed." : captured.diagnostics.front().message;
        return false;
    }
    if (worker_.joinable()) worker_.join();
    std::scoped_lock lock(mutex_);
    stopRequested_ = false;
    stopPath_.clear();
    snapshot_ = {};
    snapshot_.state = RuntimePlayState::Preparing;
    snapshot_.playSessionId = request.playSessionId;
    worker_ = std::thread([this, source = *captured.snapshot,
                           request = std::move(request)]() mutable
    { Run(std::move(source), std::move(request)); });
    return true;
}

void RuntimeProcessController::Stop()
{
    stopRequested_ = true;
    std::filesystem::path stopPath;
    std::string session;
    {
        std::scoped_lock lock(mutex_);
        if (snapshot_.state != RuntimePlayState::Preparing &&
            snapshot_.state != RuntimePlayState::Starting &&
            snapshot_.state != RuntimePlayState::Running) return;
        snapshot_.state = RuntimePlayState::Stopping;
        stopPath = stopPath_;
        session = snapshot_.playSessionId;
    }
    if (!stopPath.empty())
    {
        std::error_code error;
        (void)AtomicWriteDocument(stopPath,
            Json({{"operation", "Stop"}, {"playSessionId", session},
                  {"schemaVersion", 1}}).dump(2) + "\n", session, error);
    }
}

void RuntimeProcessController::Shutdown()
{
    Stop();
    if (worker_.joinable()) worker_.join();
}

RuntimeProcessSnapshot RuntimeProcessController::Snapshot() const
{
    std::scoped_lock lock(mutex_);
    return snapshot_;
}

void RuntimeProcessController::SetState(RuntimePlayState state, std::string code,
                                        std::string message)
{
    std::scoped_lock lock(mutex_);
    snapshot_.state = state;
    snapshot_.diagnosticCode = std::move(code);
    snapshot_.message = std::move(message);
}

void RuntimeProcessController::Run(PlaySourceSnapshot sourceSnapshot,
                                   RuntimeProcessLaunchRequest request)
{
    auto prepared = PlayPreparationPipeline{}.Prepare(sourceSnapshot,
        {request.playSessionId, request.runtimeExecutable,
         [this] { return stopRequested_.load(); }, request.progress, {}});
    if (!prepared.passed)
    {
        const auto code = prepared.diagnostics.empty() ? "Play.Prepare.Failed" :
            prepared.diagnostics.front().code;
        const auto message = prepared.diagnostics.empty() ? "Play preparation failed." :
            prepared.diagnostics.front().message;
        SetState(stopRequested_ ? RuntimePlayState::Completed : RuntimePlayState::Failed,
                 code, message);
        return;
    }
    const auto freshness = VerifyPlaySourceSnapshot(sourceSnapshot);
    if (!freshness.empty())
    {
        SetState(RuntimePlayState::Failed, "Play.Prepare.StaleSource",
                 freshness.front().message);
        return;
    }
    std::error_code fileError;
    auto removeStale = [&](const std::filesystem::path& path)
    {
        if (!std::filesystem::exists(path, fileError)) return !fileError;
        return std::filesystem::remove(path, fileError) && !fileError;
    };
    const auto startupReadyPath = prepared.buildRoot / "startup-ready-v1.json";
    if (!removeStale(prepared.resultPath) || !removeStale(prepared.stopPath) ||
        !removeStale(startupReadyPath) || !removeStale(startupReadyPath.string() + ".tmp"))
    {
        SetState(RuntimePlayState::Failed, "Play.Runtime.StaleTransportRemoveFailed",
                 "A stale runtime result, startup-ready record, or Stop marker could not be removed before launch.");
        return;
    }
    if (fileError)
    {
        SetState(RuntimePlayState::Failed, "Play.Runtime.ResultPathInvalid", fileError.message());
        return;
    }
    {
        std::scoped_lock lock(mutex_);
        stopPath_ = prepared.stopPath;
        snapshot_.resultPath = prepared.resultPath;
        snapshot_.startupReadyPath = startupReadyPath;
        snapshot_.state = RuntimePlayState::Starting;
    }
    const auto launchedAt = std::chrono::steady_clock::now();
    auto readyAt = launchedAt;
    std::atomic_bool internalCancellation{false};
    bool startupReady = false;
    bool startupTimedOut = false;
    bool runtimeTimedOut = false;
    bool startupInvalid = false;
    std::string startupDiagnosticCode;
    std::string startupDiagnosticMessage;
    ToolProcessRequest process;
    process.executable = request.runtimeExecutable;
    process.arguments = {"--play-envelope", prepared.localEnvelopePath.string()};
    if (request.headless) process.arguments.emplace_back("--headless");
    if (request.smokeInteractions) process.arguments.emplace_back("--smoke-interactions");
    process.workingDirectory = prepared.buildRoot;
    process.inheritEnvironment = false;
    process.environment = ReadEnvelopeEnvironment(prepared.localEnvelopePath);
    process.timeout = request.startupTimeout + request.runtimeTimeout +
        request.gracefulStopTimeout + request.sigtermGrace + request.sigkillGrace +
        std::chrono::seconds(1);
    process.cancellationRequested = [this, &internalCancellation]
    { return stopRequested_.load() || internalCancellation.load(); };
    process.requestCancellation = [this]
    { if (stopRequested_.load()) Stop(); };
    process.cancellationGraceOverride = [this, &request]
    { return stopRequested_.load() ? request.gracefulStopTimeout :
                                     std::chrono::milliseconds::zero(); };
    process.cancellationGrace = request.gracefulStopTimeout;
    process.terminationGrace = request.sigtermGrace;
    process.forceKillGrace = request.sigkillGrace;
    process.onPoll = [&, this]
    {
        const auto now = std::chrono::steady_clock::now();
        if (!startupReady)
        {
            std::error_code existsError;
            if (std::filesystem::exists(startupReadyPath, existsError) && !existsError)
            {
                const auto parsed = SagaRuntime::Play::ParseRuntimeStartupReadyV1(
                    ReadBytes(startupReadyPath), request.playSessionId,
                    prepared.canonicalManifestHash);
                if (!parsed.Valid())
                {
                    startupInvalid = true;
                    startupDiagnosticCode = parsed.diagnostics.empty() ?
                        "Play.Runtime.StartupFailure" : parsed.diagnostics.front().code;
                    startupDiagnosticMessage = parsed.diagnostics.empty() ?
                        "Runtime startup-ready validation failed." :
                        parsed.diagnostics.front().message;
                    internalCancellation = true;
                    return;
                }
                startupReady = true;
                readyAt = now;
                std::scoped_lock lock(mutex_);
                if (snapshot_.state == RuntimePlayState::Starting)
                    snapshot_.state = RuntimePlayState::Running;
            }
            else if (now - launchedAt >= request.startupTimeout)
            {
                startupTimedOut = true;
                internalCancellation = true;
            }
        }
        else if (now - readyAt >= request.runtimeTimeout)
        {
            runtimeTimedOut = true;
            internalCancellation = true;
        }
    };
    const auto result = ToolProcessRunner{}.Run(process);
    {
        std::ofstream log(prepared.buildRoot / "SagaRuntime.log",
                          std::ios::binary | std::ios::trunc);
        log << result.standardOutput;
        if (!result.standardError.empty())
            log << "\n[stderr]\n" << result.standardError;
    }
    {
        std::scoped_lock lock(mutex_);
        snapshot_.exitCode = result.exitCode;
        snapshot_.shutdownClassification = result.shutdownClassification;
    }
    const bool userStopRequested = stopRequested_.load();
    if (userStopRequested &&
        (result.shutdownClassification == ToolProcessShutdownClassification::Terminated ||
         result.shutdownClassification == ToolProcessShutdownClassification::Forced))
    {
        SetState(RuntimePlayState::Completed, "Play.Runtime.ForcedStop",
                 "SagaRuntime exceeded graceful Stop and was force-terminated.");
        return;
    }
    if (!userStopRequested && (runtimeTimedOut ||
        (startupReady && result.classification == ToolProcessExitClassification::TimedOut)))
    {
        SetState(RuntimePlayState::Failed, "Play.Runtime.RuntimeTimeout",
                 "SagaRuntime exceeded the normal runtime deadline.");
        return;
    }
    if (!startupReady)
    {
        if (startupInvalid)
            SetState(RuntimePlayState::Failed, std::move(startupDiagnosticCode),
                     std::move(startupDiagnosticMessage));
        else if (startupTimedOut || result.classification == ToolProcessExitClassification::TimedOut)
            SetState(RuntimePlayState::Failed, "Play.Runtime.StartupTimeout",
                     "SagaRuntime did not publish a valid startup-ready record before the deadline.");
        else
            SetState(RuntimePlayState::Failed, "Play.Runtime.StartupFailure",
                     "SagaRuntime exited before publishing a valid startup-ready record.");
        return;
    }
    if (!std::filesystem::is_regular_file(prepared.resultPath))
    {
        if (result.classification == ToolProcessExitClassification::Crashed ||
            (result.classification == ToolProcessExitClassification::Failed && result.exitCode != 0))
            SetState(RuntimePlayState::Failed, "Play.Runtime.Crash",
                     "SagaRuntime crashed after startup readiness without a structured result.");
        else
            SetState(RuntimePlayState::Failed, "Play.Runtime.ResultMissing",
                     "SagaRuntime exited without a structured runtime result.");
        return;
    }
    const auto parsed = SagaRuntime::Play::ParseRuntimeResultV1(
        ReadBytes(prepared.resultPath), request.playSessionId,
        prepared.canonicalManifestHash);
    if (!parsed.Valid())
    {
        SetState(RuntimePlayState::Failed,
            parsed.diagnostics.empty() ? "Play.Runtime.ResultInvalid" : parsed.diagnostics.front().code,
            parsed.diagnostics.empty() ? "Runtime result validation failed." :
                parsed.diagnostics.front().message);
        return;
    }
    {
        std::scoped_lock lock(mutex_);
        snapshot_.runtimeResult = parsed.result;
    }
    if (!parsed.result->Successful())
    {
        SetState(RuntimePlayState::Failed, "Play.Runtime.ReportedFailure",
                 "SagaRuntime returned a valid structured failure result.");
        return;
    }
    if (userStopRequested && parsed.result->shutdownClassification == "GracefulStop")
    {
        SetState(RuntimePlayState::Completed, "Play.Runtime.GracefulStop",
                 "SagaRuntime acknowledged Stop and returned a validated graceful result.");
        return;
    }
    if (!result.Succeeded() && result.shutdownClassification !=
        ToolProcessShutdownClassification::Graceful)
    {
        SetState(RuntimePlayState::Failed,
            result.diagnosticCode.empty() ? "Play.Runtime.ProcessFailed" : result.diagnosticCode,
            result.standardError.empty() ? result.diagnosticMessage : result.standardError);
        return;
    }
    SetState(RuntimePlayState::Completed, {}, "SagaRuntime returned a validated structured result.");
}
} // namespace SagaEditor
