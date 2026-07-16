// SPDX-License-Identifier: Apache-2.0

/// @file StarterArenaSmokeReport.h
/// @brief StarterArena smoke JSON report helpers.

#pragma once

#include "StarterArenaSmoke.h"
#include "StarterArenaGameplayState.h"

#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace SagaRuntimeApp
{

using StarterArenaSmokeJson = nlohmann::ordered_json;

struct StarterArenaSmokeReportInput
{
    const RuntimeCommandLine& commandLine;
    const StarterArenaProject* project = nullptr;
    const StarterArenaScene* scene = nullptr;
    const StarterArenaScriptBindingMetadata& scriptBinding;
    const StarterArenaScriptInvocationResult& scriptInvocation;
    const StarterArenaScriptLifecycleResult& scriptLifecycle;
    const StarterArenaGameplayState& gameplay;
    const StarterArenaLoopResult& loop;
    const std::vector<StarterArenaDiagnostic>& diagnostics;
    bool canRun = false;
    std::string sceneSource;
};

[[nodiscard]] StarterArenaSmokeJson BuildStarterArenaSmokeReport(
    const StarterArenaSmokeReportInput& input);

[[nodiscard]] bool WriteJsonReport(
    const std::filesystem::path& outputPath,
    const StarterArenaSmokeJson& report,
    std::string& error);

} // namespace SagaRuntimeApp
