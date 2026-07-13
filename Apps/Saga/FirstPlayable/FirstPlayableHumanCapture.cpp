/// @file FirstPlayableHumanCapture.cpp
/// @brief Bounded real-keyboard capture and RC-gate import orchestration.

#include "FirstPlayable/FirstPlayableHumanCapture.h"

#include "FirstPlayable/FirstPlayableEvidenceBundle.h"

#include <nlohmann/json.hpp>

#include <atomic>
#include <fstream>
#include <ostream>

namespace SagaProduct
{
namespace
{
using Json = nlohmann::json;

void AddDiagnostic(std::vector<FirstPlayableDiagnostic>& diagnostics,
                   std::string code,
                   std::string message,
                   const std::filesystem::path& path,
                   std::string hint)
{
    FirstPlayableDiagnostic diagnostic;
    diagnostic.code = std::move(code);
    diagnostic.message = std::move(message);
    if (!path.empty()) diagnostic.path = path;
    diagnostic.actionHint = std::move(hint);
    diagnostics.push_back(std::move(diagnostic));
}

Json DiagnosticJson(const FirstPlayableDiagnostic& diagnostic)
{
    Json value = {{"severity", ToString(diagnostic.severity)},
        {"code", diagnostic.code}, {"message", diagnostic.message}};
    if (diagnostic.path) value["path"] = diagnostic.path->string();
    if (diagnostic.actionHint) value["actionHint"] = *diagnostic.actionHint;
    return value;
}

std::filesystem::path CreateStagingRoot()
{
    static std::atomic_uint64_t sequence{0};
    const auto stamp = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    for (int attempt = 0; attempt < 32; ++attempt)
    {
        const auto path = std::filesystem::temp_directory_path() /
            ("saga-first-playable-human-staging-" + std::to_string(stamp) + "-" +
             std::to_string(sequence.fetch_add(1)));
        std::error_code ec;
        if (std::filesystem::create_directory(path, ec)) return path;
    }
    return {};
}

void WriteText(const std::filesystem::path& path, const std::string& value)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << value;
}

void Cleanup(const std::filesystem::path& path)
{
    if (path.empty()) return;
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
}

bool WriteIncompleteReport(FirstPlayableHumanCaptureResult& result,
                           const FirstPlayableHumanCaptureRequest& request,
                           const FirstPlayableWorkspacePolicySession& session,
                           const std::filesystem::path& captureReport = {})
{
    if (session.status != EvidenceStatus::Passed) return false;
    const auto manual = std::filesystem::absolute(
        request.workflow.runtime.outputDirectory) / "manual";
    std::error_code ec;
    std::filesystem::create_directories(manual, ec);
    if (!ec)
    {
        WriteText(manual / "keyboard_capture_stdout.txt", result.process.standardOutput);
        WriteText(manual / "keyboard_capture_stderr.txt", result.process.standardError);
        if (std::filesystem::is_regular_file(captureReport))
            std::filesystem::copy_file(captureReport,
                manual / "real_keyboard_capture_report.json",
                std::filesystem::copy_options::overwrite_existing, ec);
    }
    Json diagnostics = Json::array();
    for (const auto& diagnostic : result.diagnostics)
        diagnostics.push_back(DiagnosticJson(diagnostic));
    const Json report = {{"schemaVersion", 1}, {"tool", "Saga"},
        {"command", "first-playable-human-capture"}, {"status", "Incomplete"},
        {"mode", "Capture"},
        {"runtime", {{"started", result.process.started},
            {"timedOut", result.process.timedOut},
            {"exitCode", result.process.started ? Json(result.process.exitCode) : Json(nullptr)},
            {"durationMs", result.process.duration.count()},
            {"stdoutPath", "manual/keyboard_capture_stdout.txt"},
            {"stderrPath", "manual/keyboard_capture_stderr.txt"},
            {"reportPath", std::filesystem::is_regular_file(captureReport) ?
                Json("manual/real_keyboard_capture_report.json") : Json(nullptr)}}},
        {"keyboardEvidence", {{"status", "Incomplete"}}},
        {"gateStatus", "Incomplete"},
        {"nonClaims", Json::array({"No real keyboard evidence claim",
            "No broad interactive gameplay claim", "No production input UX claim"})},
        {"diagnostics", std::move(diagnostics)}};
    result.reportPath = manual / "human_capture_report.json";
    return FirstPlayableEvidenceBundle::WriteJson(result.reportPath, report,
        result.diagnostics, "ProductShell.HumanCapture.ReportImportFailed");
}

void EmitDiagnostics(const std::vector<FirstPlayableDiagnostic>& diagnostics,
                     std::ostream& err)
{
    for (const auto& diagnostic : diagnostics)
        err << diagnostic.code << ": " << diagnostic.message << '\n';
}

void AddHumanPolicyDiagnostics(
    const FirstPlayableWorkspacePolicySession& session,
    std::vector<FirstPlayableDiagnostic>& diagnostics)
{
    for (const auto& diagnostic : session.diagnostics)
    {
        if (diagnostic.code == "ProductShell.FirstPlayable.SourceFileMissing")
            AddDiagnostic(diagnostics, "ProductShell.HumanCapture.ProjectMissing",
                "the StarterArena project or a required source file is missing",
                diagnostic.path.value_or(session.projectManifest),
                "Restore the existing StarterArena sample and retry.");
        else if (diagnostic.code == "ProductShell.FirstPlayable.OutputRootNotEmpty")
            AddDiagnostic(diagnostics, "ProductShell.HumanCapture.OutputRootNotEmpty",
                "human-capture output must be absent or empty", session.outputRoot,
                "Choose a fresh external evidence output root.");
        else if (diagnostic.code == "ProductShell.FirstPlayable.SummaryOutsideOutputRoot")
            AddDiagnostic(diagnostics, "ProductShell.HumanCapture.SummaryOutsideOutputRoot",
                "the first-playable summary must remain inside the output root",
                session.summaryPath, "Choose a summary path below the evidence output root.");
        else if (diagnostic.code ==
                     "ProductShell.FirstPlayable.GeneratedOutputInsideSample" ||
                 diagnostic.code == "ProductShell.FirstPlayable.SagaPrivateTouched")
            AddDiagnostic(diagnostics, "ProductShell.HumanCapture.OutputRootInvalid",
                "human-capture output must be outside source and .saga-private",
                session.outputRoot, "Choose a fresh external evidence output root.");
    }
}

EvidenceStatus OperatorStatus(const EvidenceProcessResult& process,
                              const FirstPlayableManualEvidenceResult& manual)
{
    if (!process.started || process.timedOut) return EvidenceStatus::Incomplete;
    if (process.exitCode != 0 || manual.status != EvidenceStatus::Passed)
        return EvidenceStatus::Failed;
    return EvidenceStatus::Passed;
}
} // namespace

FirstPlayableHumanCapture::FirstPlayableHumanCapture()
    : FirstPlayableHumanCapture(std::make_unique<QtEvidenceProcessRunner>())
{}

FirstPlayableHumanCapture::FirstPlayableHumanCapture(
    std::unique_ptr<IEvidenceProcessRunner> runner)
    : m_runner(runner ? std::move(runner) :
        std::make_unique<QtEvidenceProcessRunner>())
{}

EvidenceProcessRequest FirstPlayableHumanCapture::BuildProcessRequest(
    const FirstPlayableHumanCaptureRequest& request,
    const std::filesystem::path& reportPath)
{
    EvidenceProcessRequest process;
    process.executable = std::filesystem::absolute(
        request.workflow.runtime.runtimeExecutable);
    process.workingDirectory = process.executable.parent_path();
    process.timeout = request.timeout;
    process.arguments = {"--project",
        std::filesystem::absolute(request.workflow.runtime.projectManifest).string(),
        "--starter-arena-playable", "--playable-input-source", "keyboard",
        "--playable-frames", std::to_string(request.frames),
        "--playable-report-out", reportPath.string(), "--fixed-dt", "0.016"};
    return process;
}

FirstPlayableHumanCaptureResult FirstPlayableHumanCapture::Run(
    const FirstPlayableHumanCaptureRequest& request,
    std::ostream& out,
    std::ostream& err)
{
    FirstPlayableHumanCaptureResult result;
    result.mode = request.workflow.keyboardReportPath ?
        FirstPlayableOperatorMode::Import : FirstPlayableOperatorMode::Capture;
    const auto outputRoot = std::filesystem::absolute(
        request.workflow.runtime.outputDirectory);
    const auto summaryPath = std::filesystem::absolute(
        request.workflow.summaryPath.empty() ? outputRoot /
            "first_playable_summary.json" : request.workflow.summaryPath);
    const auto session = FirstPlayableWorkspacePolicy::Begin(
        request.workflow.runtime.projectManifest, outputRoot, summaryPath);
    result.reportPath = outputRoot / "manual" / "human_capture_report.json";
    if (session.status != EvidenceStatus::Passed)
    {
        result.diagnostics = session.diagnostics;
        AddHumanPolicyDiagnostics(session, result.diagnostics);
        result.status = EvidenceStatus::Incomplete;
        EmitDiagnostics(result.diagnostics, err);
        return result;
    }
    if (!std::filesystem::is_regular_file(request.workflow.runtime.runtimeExecutable))
    {
        AddDiagnostic(result.diagnostics,
            "ProductShell.HumanCapture.RuntimeExecutableMissing",
            "SagaRuntime executable is missing",
            request.workflow.runtime.runtimeExecutable,
            "Build SagaRuntime or pass --runtime-executable with a valid path.");
        result.status = EvidenceStatus::Incomplete;
        WriteIncompleteReport(result, request, session);
        EmitDiagnostics(result.diagnostics, err);
        return result;
    }

    FirstPlayableWorkflowRequest workflow = request.workflow;
    workflow.workspaceSession = session;
    FirstPlayableOperatorEvidenceInput operatorEvidence;
    operatorEvidence.mode = result.mode;

    std::filesystem::path staging;
    if (result.mode == FirstPlayableOperatorMode::Import)
    {
        result.keyboardEvidence = FirstPlayableManualEvidence::Validate(
            workflow.keyboardReportPath);
        operatorEvidence.status = result.keyboardEvidence.status;
        operatorEvidence.originalReportPath = *workflow.keyboardReportPath;
        if (result.keyboardEvidence.status != EvidenceStatus::Passed)
            AddDiagnostic(operatorEvidence.diagnostics,
                "ProductShell.HumanCapture.KeyboardEvidenceInvalid",
                "the supplied real-keyboard report did not validate",
                *workflow.keyboardReportPath,
                "Supply an unmodified report from a real keyboard run.");
    }
    else
    {
        staging = CreateStagingRoot();
        if (staging.empty())
        {
            AddDiagnostic(result.diagnostics,
                "ProductShell.HumanCapture.OutputRootInvalid",
                "could not create an external human-capture staging directory", {},
                "Check temporary-directory permissions and retry.");
            result.status = EvidenceStatus::Incomplete;
            WriteIncompleteReport(result, request, session);
            EmitDiagnostics(result.diagnostics, err);
            return result;
        }
        const auto captureReport = staging / "real_keyboard_capture_report.json";
        out << "Saga First Playable Human Capture\n\n"
            << "A visible StarterArena window will open.\n"
            << "Use a real keyboard:\n  W / A / S / D: move\n"
            << "  Escape: quit early\n\n"
            << "The window must be focused and measurable movement must occur.\n"
            << "Synthetic input is not accepted.\n\nCapture report: "
            << captureReport << '\n';
        const auto processRequest = BuildProcessRequest(request, captureReport);
        result.process = m_runner->Run(processRequest);
        WriteText(staging / "keyboard_capture_stdout.txt", result.process.standardOutput);
        WriteText(staging / "keyboard_capture_stderr.txt", result.process.standardError);
        if (!result.process.started)
            AddDiagnostic(result.diagnostics,
                "ProductShell.HumanCapture.ProcessLaunchFailed",
                "SagaRuntime keyboard capture could not start: " +
                    result.process.startError,
                processRequest.executable,
                "Check the Runtime executable and graphics environment, then retry.");
        else if (result.process.timedOut)
            AddDiagnostic(result.diagnostics,
                "ProductShell.HumanCapture.ProcessTimedOut",
                "SagaRuntime keyboard capture exceeded its bounded timeout",
                captureReport,
                "Focus the window, press W/A/S/D, and press Escape before the timeout.");
        else if (result.process.exitCode != 0)
            AddDiagnostic(result.diagnostics,
                "ProductShell.HumanCapture.ProcessExitFailed",
                "SagaRuntime keyboard capture exited with code " +
                    std::to_string(result.process.exitCode),
                staging / "keyboard_capture_stderr.txt",
                "Inspect the capture stderr and retry in a supported display environment.");
        result.keyboardEvidence = FirstPlayableManualEvidence::Validate(captureReport);
        if (!std::filesystem::is_regular_file(captureReport))
            AddDiagnostic(result.diagnostics,
                "ProductShell.HumanCapture.ReportMissing",
                "SagaRuntime did not write the keyboard capture report", captureReport,
                "Inspect Runtime stderr and retry the capture.");
        else if (result.keyboardEvidence.status != EvidenceStatus::Passed)
            AddDiagnostic(result.diagnostics,
                "ProductShell.HumanCapture.KeyboardEvidenceInvalid",
                "captured real-keyboard evidence did not validate", captureReport,
                "Focus the window and press W/A/S/D long enough to move the player.");
        operatorEvidence.status = OperatorStatus(result.process, result.keyboardEvidence);
        operatorEvidence.process = result.process;
        operatorEvidence.originalReportPath = captureReport;
        operatorEvidence.captureReportPath = captureReport;
        operatorEvidence.stdoutPath = staging / "keyboard_capture_stdout.txt";
        operatorEvidence.stderrPath = staging / "keyboard_capture_stderr.txt";
        operatorEvidence.diagnostics = result.diagnostics;
        workflow.keyboardReportPath = captureReport;
        if (operatorEvidence.status == EvidenceStatus::Incomplete)
        {
            auto policy = FirstPlayableWorkspacePolicy::Complete(session);
            if (policy.status != EvidenceStatus::Passed)
                AddDiagnostic(result.diagnostics,
                    "ProductShell.HumanCapture.SourceIntegrityFailed",
                    "StarterArena source changed during human capture",
                    session.projectRoot,
                    "Restore the sample source and retry from a clean output root.");
            WriteIncompleteReport(result, request, session, captureReport);
            Cleanup(staging);
            result.status = EvidenceStatus::Incomplete;
            EmitDiagnostics(result.diagnostics, err);
            return result;
        }
    }

    workflow.operatorEvidence = operatorEvidence;
    result.workflow = RunFirstPlayableWorkflow(workflow, out, err);
    result.gateStatus = result.workflow.gate.status;
    result.keyboardEvidence = result.workflow.gate.manualEvidence;
    result.reportPath = result.workflow.runtime.outputDirectory / "manual" /
        "human_capture_report.json";
    result.diagnostics = result.workflow.gate.diagnostics;
    result.status = result.gateStatus == FirstPlayableGateStatus::Accepted ?
        EvidenceStatus::Passed : (result.gateStatus == FirstPlayableGateStatus::Incomplete ?
            EvidenceStatus::Incomplete : EvidenceStatus::Failed);
    Cleanup(staging);
    out << "\nHuman Keyboard Capture: " << ToString(result.keyboardEvidence.status)
        << "\nReal Keyboard Evidence: " << ToString(result.keyboardEvidence.status)
        << "\nGate Status: " << ToString(result.gateStatus) << '\n';
    return result;
}

} // namespace SagaProduct
