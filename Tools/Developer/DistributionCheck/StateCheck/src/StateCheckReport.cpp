/// @file StateCheckReport.cpp
/// @brief Implements deterministic SagaStateCheck report JSON.

#include "SagaStateCheck/StateCheckReport.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <system_error>
#include <utility>

namespace SagaStateCheck
{
namespace
{

using Json = nlohmann::ordered_json;

[[nodiscard]] StateCheckReportWriteDiagnostic MakeDiagnostic(
    std::string diagnosticId,
    std::string message,
    std::filesystem::path outputPath = {})
{
    StateCheckReportWriteDiagnostic diagnostic;
    diagnostic.diagnosticId = std::move(diagnosticId);
    diagnostic.message = std::move(message);
    diagnostic.outputPath = std::move(outputPath);
    return diagnostic;
}

[[nodiscard]] Json BuildVectorJson(const StateVector3& vector)
{
    return Json{
        {"x", vector.x},
        {"y", vector.y},
        {"z", vector.z},
    };
}

[[nodiscard]] Json BuildEntityJson(
    const EntitySnapshot& entity,
    const StateCheckOptions& options)
{
    return Json{
        {"entityId", entity.entityId},
        {"ownerClientId", entity.ownerClientId},
        {"position", BuildVectorJson(entity.position)},
        {"hasVelocity", entity.hasVelocity},
        {"velocity", BuildVectorJson(entity.velocity)},
        {"checksum", ComputeEntityChecksum(entity, options).value},
    };
}

[[nodiscard]] Json BuildSnapshotJson(
    StateSnapshot snapshot,
    const StateCheckOptions& options)
{
    snapshot = CanonicalizeSnapshot(std::move(snapshot));

    Json entities = Json::array();
    for (const auto& entity : snapshot.entities)
    {
        entities.push_back(BuildEntityJson(entity, options));
    }

    return Json{
        {"schemaVersion", snapshot.schemaVersion},
        {"snapshotId", snapshot.snapshotId},
        {"tick", snapshot.tick},
        {"entities", std::move(entities)},
    };
}

[[nodiscard]] Json BuildOwnershipMismatchJson(
    const OwnershipMismatch& mismatch)
{
    return Json{
        {"entityId", mismatch.entityId},
        {"expectedOwnerClientId", mismatch.expectedOwnerClientId},
        {"actualOwnerClientId", mismatch.actualOwnerClientId},
    };
}

[[nodiscard]] Json BuildPositionMismatchJson(
    const PositionMismatch& mismatch)
{
    return Json{
        {"entityId", mismatch.entityId},
        {"expectedPosition", BuildVectorJson(mismatch.expectedPosition)},
        {"actualPosition", BuildVectorJson(mismatch.actualPosition)},
    };
}

[[nodiscard]] Json BuildChecksumMismatchJson(
    const ChecksumMismatch& mismatch)
{
    return Json{
        {"entityId", mismatch.entityId},
        {"expectedChecksum", mismatch.expectedChecksum.value},
        {"actualChecksum", mismatch.actualChecksum.value},
    };
}

[[nodiscard]] Json BuildDesyncJson(const DesyncReport& report)
{
    Json missing = Json::array();
    for (const auto entityId : report.missingEntities)
    {
        missing.push_back(entityId);
    }

    Json extra = Json::array();
    for (const auto entityId : report.extraEntities)
    {
        extra.push_back(entityId);
    }

    Json ownership = Json::array();
    for (const auto& mismatch : report.ownershipMismatches)
    {
        ownership.push_back(BuildOwnershipMismatchJson(mismatch));
    }

    Json positions = Json::array();
    for (const auto& mismatch : report.positionMismatches)
    {
        positions.push_back(BuildPositionMismatchJson(mismatch));
    }

    Json checksums = Json::array();
    for (const auto& mismatch : report.checksumMismatches)
    {
        checksums.push_back(BuildChecksumMismatchJson(mismatch));
    }

    Json diagnostics = Json::array();
    for (const auto& diagnostic : report.diagnostics)
    {
        diagnostics.push_back(diagnostic);
    }

    return Json{
        {"status", ToString(report.status)},
        {"mismatchCount", report.mismatchCount},
        {"missingEntities", std::move(missing)},
        {"extraEntities", std::move(extra)},
        {"ownershipMismatches", std::move(ownership)},
        {"positionMismatches", std::move(positions)},
        {"checksumMismatches", std::move(checksums)},
        {"diagnostics", std::move(diagnostics)},
    };
}

[[nodiscard]] Json BuildReportJson(const StateCheckReportDocument& document)
{
    return Json{
        {"schemaVersion", document.schemaVersion},
        {"reportKind", document.reportKind},
        {"options", Json{{"quantizationScale", document.options.quantizationScale}}},
        {"expected", BuildSnapshotJson(document.expected, document.options)},
        {"actual", BuildSnapshotJson(document.actual, document.options)},
        {"desync", BuildDesyncJson(document.desync)},
    };
}

} // namespace

StateCheckReportDocument BuildStateCheckReport(
    StateSnapshot expected,
    StateSnapshot actual,
    const StateCheckOptions& options)
{
    StateCheckReportDocument document;
    document.expected = CanonicalizeSnapshot(std::move(expected));
    document.actual = CanonicalizeSnapshot(std::move(actual));
    document.options = options;
    document.desync = CompareSnapshots(document.expected, document.actual, options);
    return document;
}

StateCheckReportWriteResult WriteStateCheckReportJson(
    const StateCheckReportDocument& document,
    std::ostream& output)
{
    StateCheckReportWriteResult result;
    if (!output.good())
    {
        result.diagnostics.push_back(MakeDiagnostic(
            "SagaStateCheck.Report.OutputUnavailable",
            "state check report output stream is not writable"));
        return result;
    }

    output << BuildReportJson(document).dump(2) << '\n';
    if (!output.good())
    {
        result.diagnostics.push_back(MakeDiagnostic(
            "SagaStateCheck.Report.WriteFailed",
            "state check report output stream write failed"));
    }
    return result;
}

StateCheckReportWriteResult WriteStateCheckReportJsonToFile(
    const StateCheckReportDocument& document,
    const std::filesystem::path& outputPath)
{
    StateCheckReportWriteResult result;
    if (outputPath.empty())
    {
        result.diagnostics.push_back(MakeDiagnostic(
            "SagaStateCheck.Report.OutputPathEmpty",
            "state check report output path is empty"));
        return result;
    }

    std::error_code error;
    const auto parent = outputPath.parent_path();
    if (!parent.empty())
    {
        std::filesystem::create_directories(parent, error);
        if (error)
        {
            result.diagnostics.push_back(MakeDiagnostic(
                "SagaStateCheck.Report.DirectoryCreateFailed",
                error.message(),
                outputPath));
            return result;
        }
    }

    std::ofstream output(outputPath);
    if (!output.is_open())
    {
        result.diagnostics.push_back(MakeDiagnostic(
            "SagaStateCheck.Report.FileOpenFailed",
            "failed to open state check report output file",
            outputPath));
        return result;
    }

    result = WriteStateCheckReportJson(document, output);
    for (auto& diagnostic : result.diagnostics)
    {
        diagnostic.outputPath = outputPath;
    }
    return result;
}

} // namespace SagaStateCheck
