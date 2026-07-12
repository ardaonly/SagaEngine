/// @file FirstPlayableManualEvidence.cpp
#include "FirstPlayableManualEvidence.h"

#include <QCryptographicHash>
#include <nlohmann/json.hpp>

#include <cmath>
#include <fstream>

namespace SagaProduct
{
namespace
{
using Json = nlohmann::json;
void Fail(FirstPlayableManualEvidenceResult& result, std::string code,
          std::string message, const std::filesystem::path& path)
{
    FirstPlayableDiagnostic diagnostic;
    diagnostic.code = std::move(code); diagnostic.message = std::move(message);
    diagnostic.path = path;
    diagnostic.actionHint = "Supply an unmodified report from a real manual keyboard run.";
    result.diagnostics.push_back(std::move(diagnostic));
    result.status = EvidenceStatus::Failed;
}
bool PrivatePath(const std::filesystem::path& path)
{
    for (const auto& part : path) if (part == ".saga-private") return true;
    return false;
}
} // namespace

FirstPlayableManualEvidenceResult FirstPlayableManualEvidence::Validate(
    const std::optional<std::filesystem::path>& requested)
{
    FirstPlayableManualEvidenceResult result;
    if (!requested) return result;
    result.reportPath = std::filesystem::absolute(*requested);
    if (PrivatePath(*result.reportPath) || !std::filesystem::is_regular_file(*result.reportPath))
    {
        Fail(result, "ProductShell.FirstPlayable.KeyboardReportMissing",
             "supplied keyboard report is missing or uses a private path", *result.reportPath);
        return result;
    }
    std::ifstream input(*result.reportPath, std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(input)), {});
    result.reportSha256 = QCryptographicHash::hash(
        QByteArray(bytes.data(), static_cast<qsizetype>(bytes.size())),
        QCryptographicHash::Sha256).toHex().toStdString();
    Json root;
    try { root = Json::parse(bytes); }
    catch (const std::exception& error)
    {
        Fail(result, "ProductShell.FirstPlayable.KeyboardReportMalformed",
             std::string("keyboard report is not valid JSON: ") + error.what(),
             *result.reportPath);
        return result;
    }
    if (!root.is_object())
    {
        Fail(result, "ProductShell.FirstPlayable.KeyboardReportMalformed",
             "keyboard report root must be an object", *result.reportPath);
        return result;
    }
    if (!root.contains("diagnostics") ||
        !root["diagnostics"].is_array())
        Fail(result, "ProductShell.FirstPlayable.KeyboardReportMalformed",
             "keyboard report requires a diagnostics array", *result.reportPath);
    else
        for (const Json& diagnostic : root["diagnostics"])
            if (!diagnostic.is_object() || diagnostic.value("severity", "Error") == "Error")
                Fail(result, "ProductShell.FirstPlayable.GateManualKeyboardInvalid",
                     "keyboard report contains an error diagnostic", *result.reportPath);
    const Json inputSection = root.value("input", Json::object());
    if (inputSection.value("source", "") != "keyboard" ||
        inputSection.value("status", "") != "Passed")
        Fail(result, "ProductShell.FirstPlayable.KeyboardReportSourceMismatch",
             "keyboard report must record a Passed keyboard source", *result.reportPath);
    result.realDeviceObserved = inputSection.value("realDeviceObserved", false);
    if (!result.realDeviceObserved)
        Fail(result, "ProductShell.FirstPlayable.KeyboardReportNoRealDevice",
             "keyboard report did not observe a real device", *result.reportPath);
    if (inputSection.contains("framesWithInput"))
    {
        const Json& frames = inputSection["framesWithInput"];
        if (frames.is_number_unsigned()) result.framesWithInput = frames.get<std::uint64_t>();
        else if (frames.is_number_integer())
        {
            const auto signedFrames = frames.get<std::int64_t>();
            if (signedFrames > 0)
                result.framesWithInput = static_cast<std::uint64_t>(signedFrames);
        }
    }
    if (result.framesWithInput == 0)
        Fail(result, "ProductShell.FirstPlayable.KeyboardReportNoInputFrames",
             "keyboard report has no input frames", *result.reportPath);
    const Json simulation = root.value("simulation", Json::object());
    const Json initial = simulation.value("initialPosition", Json::object());
    const Json final = simulation.value("finalPosition", Json::object());
    const auto coordinate = [](const Json& value, const char* key) {
        return value.contains(key) && value[key].is_number() &&
            std::isfinite(value[key].get<double>());
    };
    if (!coordinate(initial, "x") || !coordinate(initial, "y") ||
        !coordinate(final, "x") || !coordinate(final, "y"))
        Fail(result, "ProductShell.FirstPlayable.KeyboardReportMalformed",
             "keyboard report requires finite initial and final positions", *result.reportPath);
    else if (std::abs(initial["x"].get<double>() - final["x"].get<double>()) <= 1e-6 &&
             std::abs(initial["y"].get<double>() - final["y"].get<double>()) <= 1e-6)
        Fail(result, "ProductShell.FirstPlayable.KeyboardReportNoMovement",
             "keyboard report records no player movement", *result.reportPath);
    if (result.diagnostics.empty()) result.status = EvidenceStatus::Passed;
    return result;
}
} // namespace SagaProduct
