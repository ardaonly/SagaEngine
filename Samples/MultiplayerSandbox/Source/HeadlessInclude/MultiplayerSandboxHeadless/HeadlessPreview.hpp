/// @file HeadlessPreview.hpp
/// @brief Bounded server-side MultiplayerSandbox headless preview proof.

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace MultiplayerSandboxHeadless
{

struct Vec3
{
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
};

struct HeadlessPreviewOptions
{
    std::filesystem::path projectPath;
    std::filesystem::path reportOut;
    std::filesystem::path diagnosticsOut;
    bool starterArenaServerSmoke{false};
    int ticks{1};
    float fixedDtSeconds{1.0f};
};

struct HeadlessPreviewDiagnostic
{
    std::string severity{"Error"};
    std::string code;
    std::string message;
    std::filesystem::path path;
};

struct HeadlessPreviewReport
{
    int schemaVersion{1};
    std::string tool{"MultiplayerSandboxHeadless"};
    std::string status{"Failed"};
    std::string projectId;
    std::filesystem::path projectPath;
    int tickCount{0};
    int entityCount{0};
    int inputQueuedCount{0};
    int inputAcceptedCount{0};
    int inputRejectedCount{0};
    int snapshotCount{0};
    bool serverAuthority{false};
    std::string networkMode;
    std::vector<unsigned int> dirtyEntityIds;
    Vec3 initialPosition{};
    Vec3 beforeTickPosition{};
    Vec3 finalPosition{};
    Vec3 authoritativeInitialState{};
    Vec3 authoritativeFinalState{};
    std::vector<HeadlessPreviewDiagnostic> invalidInputDiagnostics;
    std::vector<std::string> nonClaims;
    std::vector<HeadlessPreviewDiagnostic> diagnostics;
};

[[nodiscard]] HeadlessPreviewReport RunHeadlessPreview(
    const HeadlessPreviewOptions& options);

[[nodiscard]] bool WriteHeadlessPreviewReportJson(
    const HeadlessPreviewReport& report,
    const std::filesystem::path& outputPath,
    std::string& error);

} // namespace MultiplayerSandboxHeadless
