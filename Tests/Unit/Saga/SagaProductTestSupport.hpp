// SPDX-License-Identifier: MPL-2.0
/// @file SagaProductTests.cpp
/// @brief Tests for Saga product orchestration startup boundaries.

#include "App/SagaAppConfig.h"
#include "App/SagaApp.h"
#include "ProductIntegration/SagaLocalCollaborationMetadataReports.h"
#include "ProductIntegration/SagaLocalWorkspaceTransactionReport.h"
#include "Processes/SagaProcessService.h"
#include "App/SagaProductHost.h"
#include "Reports/SagaProductWorkflowSmokeReport.h"
#include "Projects/SagaWorkspaceResolver.h"
#include "Workflows/SagaScriptGate.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifndef SAGA_SOURCE_ROOT
#define SAGA_SOURCE_ROOT "."
#endif

namespace SagaProductTestSupport
{

namespace fs = std::filesystem;
using namespace SagaProduct;

[[nodiscard]] inline fs::path SourceRoot()
{
    return fs::path(SAGA_SOURCE_ROOT);
}

[[nodiscard]] inline fs::path BuiltInProductWorkspaceRoot()
{
    return SourceRoot() / "Engine" / "Source" / "Programs" / "SagaLauncher";
}

[[nodiscard]] inline fs::path MakeTempDir(const std::string& name)
{
    fs::path root = fs::temp_directory_path() / name;
    fs::remove_all(root);
    fs::create_directories(root);
    return root;
}

inline void WriteFile(const fs::path& path, const std::string& text)
{
    fs::create_directories(path.parent_path());
    std::ofstream out(path);
    out << text;
}

[[nodiscard]] inline std::string ReadFile(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

[[nodiscard]] inline std::string ProjectIdFromManifest(const fs::path& path)
{
    return nlohmann::json::parse(ReadFile(path)).at("projectId").get<std::string>();
}

[[nodiscard]] inline fs::path WriteProject(const fs::path& root,
                                    const std::string& projectId,
                                    const std::string& displayName,
                                    bool createScripts = false)
{
    const fs::path manifest = root / (displayName + ".sagaproj");
    WriteFile(manifest,
              nlohmann::json({{"schemaVersion", 0},
                              {"projectId", projectId},
                              {"displayName", displayName}})
                  .dump(2));
    if (createScripts)
        fs::create_directories(root / "Scripts");
    return manifest;
}

class FakeProcessLauncher final : public ISagaProcessLauncher
{
public:
    SagaProcessLaunchResult result;
    std::vector<SagaProcessLaunchRequest> requests;

    SagaProcessLaunchResult Launch(const SagaProcessLaunchRequest& request,
                                   std::ostream& out,
                                   std::ostream& err) override
    {
        requests.push_back(request);
        out << childStdout;
        err << childStderr;
        return result;
    }

    std::string childStdout;
    std::string childStderr;
};

class FakeToolProcessRunner final : public ISagaToolProcessRunner
{
public:
    SagaToolProcessResult result;
    std::vector<SagaToolProcessRequest> requests;

    SagaToolProcessResult Run(const SagaToolProcessRequest& request) override
    {
        requests.push_back(request);
        return result;
    }
};

[[nodiscard]] inline SagaAppConfig MakeBasicTargetConfig(SagaProductTargetKind target)
{
    SagaAppConfig config;
    config.target = target;
    config.workspaceSelector = "builtin:basic";
    config.builtInWorkspaceRoot = BuiltInProductWorkspaceRoot();
    config.executablePath = fs::temp_directory_path() / "saga_bin" / "Saga";
    return config;
}

[[nodiscard]] inline SagaProductDiagnostic MakeTestLaunchDiagnostic(
    SagaProductTargetKind target,
    const char* diagnosticId,
    std::string message,
    fs::path path)
{
    SagaProductDiagnostic diagnostic;
    diagnostic.target = target;
    diagnostic.stage = SagaProductDiagnosticStage::StartupHandoff;
    diagnostic.diagnosticId = diagnosticId;
    diagnostic.message = std::move(message);
    diagnostic.path = std::move(path);
    return diagnostic;
}

[[nodiscard]] inline const nlohmann::json* FindWorkflowAction(
    const nlohmann::json& report,
    const std::string& actionId)
{
    if (!report.contains("workflowActions") ||
        !report["workflowActions"].is_array())
    {
        return nullptr;
    }
    for (const nlohmann::json& action : report["workflowActions"])
    {
        if (action.value("id", std::string{}) == actionId)
        {
            return &action;
        }
    }
    return nullptr;
}

[[nodiscard]] inline bool ContainsJsonKeyRecursively(const nlohmann::json& value,
                                              const std::string& key)
{
    if (value.is_object())
    {
        if (value.contains(key)) return true;
        for (const auto& [name, child] : value.items())
        {
            (void)name;
            if (ContainsJsonKeyRecursively(child, key)) return true;
        }
    }
    else if (value.is_array())
    {
        for (const auto& child : value)
        {
            if (ContainsJsonKeyRecursively(child, key)) return true;
        }
    }
    return false;
}

[[nodiscard]] inline bool JsonArrayContainsString(const nlohmann::json& array,
                                           const std::string& value)
{
    if (!array.is_array())
    {
        return false;
    }
    return std::any_of(array.begin(), array.end(),
                       [&](const nlohmann::json& item)
                       {
                           return item.is_string() &&
                               item.get<std::string>() == value;
                       });
}

} // namespace SagaProductTestSupport
