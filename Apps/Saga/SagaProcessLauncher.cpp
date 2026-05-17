/// @file SagaProcessLauncher.cpp
/// @brief Qt-backed process launch boundary for Saga role targets.

#include "SagaProcessLauncher.h"

#include <QProcess>
#include <QString>
#include <QStringList>

#include <ostream>
#include <string>
#include <utility>

namespace SagaProduct
{
namespace
{

[[nodiscard]] SagaProductDiagnostic MakeLaunchDiagnostic(
    SagaProductTargetKind target,
    const char* diagnosticId,
    std::string message,
    std::filesystem::path path)
{
    SagaProductDiagnostic diagnostic;
    diagnostic.target = target;
    diagnostic.phase = SagaProductDiagnosticPhase::StartupHandoff;
    diagnostic.diagnosticId = diagnosticId;
    diagnostic.message = std::move(message);
    diagnostic.path = std::move(path);
    return diagnostic;
}

[[nodiscard]] QStringList ToQStringList(
    const std::vector<std::string>& arguments)
{
    QStringList result;
    for (const std::string& argument : arguments)
    {
        result.push_back(QString::fromStdString(argument));
    }
    return result;
}

} // namespace

SagaProcessLaunchResult SagaProcessLauncher::Launch(
    const SagaProcessLaunchRequest& request,
    std::ostream& out,
    std::ostream& err)
{
    SagaProcessLaunchResult result;

    QProcess process;
    process.setProgram(QString::fromStdString(request.executablePath.string()));
    process.setArguments(ToQStringList(request.arguments));
    process.setProcessChannelMode(QProcess::SeparateChannels);
    if (!request.workingDirectory.empty())
    {
        process.setWorkingDirectory(
            QString::fromStdString(request.workingDirectory.string()));
    }

    process.start();
    if (!process.waitForStarted())
    {
        result.diagnostics.push_back(MakeLaunchDiagnostic(
            request.target,
            SagaProductDiagnostics::ProcessStartFailed,
            std::string(ToString(request.target)) +
                " target process failed to start: " +
                process.errorString().toStdString(),
            request.executablePath));
        return result;
    }

    result.started = true;
    process.waitForFinished(-1);

    out << process.readAllStandardOutput().toStdString();
    err << process.readAllStandardError().toStdString();

    result.exitCode = process.exitCode();
    if (process.exitStatus() != QProcess::NormalExit)
    {
        result.diagnostics.push_back(MakeLaunchDiagnostic(
            request.target,
            SagaProductDiagnostics::ProcessExitedWithFailure,
            std::string(ToString(request.target)) + " target process crashed",
            request.executablePath));
        return result;
    }

    if (result.exitCode != 0)
    {
        result.diagnostics.push_back(MakeLaunchDiagnostic(
            request.target,
            SagaProductDiagnostics::ProcessExitedWithFailure,
            std::string(ToString(request.target)) +
                " target process exited with code " +
                std::to_string(result.exitCode),
            request.executablePath));
        return result;
    }

    result.ok = true;
    return result;
}

} // namespace SagaProduct
