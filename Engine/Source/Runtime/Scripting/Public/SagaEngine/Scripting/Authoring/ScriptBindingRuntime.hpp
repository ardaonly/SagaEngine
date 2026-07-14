/// @file ScriptBindingRuntime.hpp
/// @brief SagaScript binding runtime authoring workflow helpers.

#pragma once

#include "SagaEngine/Scripting/Authoring/ScriptBindingManifestLoader.hpp"
#include "SagaEngine/Scripting/LowLevel/ScriptLifecycleService.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace SagaEngine::Scripting
{

struct ScriptBindingRuntimeCreateRequest
{
    ScriptPackageLoadRequest packageRequest;
    ScriptBindingManifestLoadRequest manifestRequest;
    std::unordered_map<std::string, ScriptEntityHandle> entities;
};

struct ScriptBindingRuntimeInstance
{
    std::string bindingId;
    ScriptInstanceHandle instance;
};

class ScriptBindingRuntime
{
public:
    ScriptBindingRuntime(
        ScriptLifecycleService& lifecycle,
        ISagaScriptWorld& world);

    [[nodiscard]] ScriptHostOperationResult LoadAndCreate(
        const ScriptBindingRuntimeCreateRequest& request);

    [[nodiscard]] ScriptHostOperationResult StartAll() const;

    [[nodiscard]] ScriptHostOperationResult UpdateAll(
        double deltaTimeSeconds) const;

    [[nodiscard]] ScriptHostOperationResult Shutdown();

    [[nodiscard]] const std::vector<ScriptBindingRuntimeInstance>& Instances()
        const noexcept;

    [[nodiscard]] const std::unordered_map<std::string, ScriptEntityHandle>&
    Entities() const noexcept;

private:
    [[nodiscard]] ScriptHostOperationResult ResolveSelfEntities(
        const std::vector<ScriptBindingRuntimeEntry>& bindings);

    [[nodiscard]] ScriptHostOperationResult DestroyCreatedInstances();

    ScriptLifecycleService& lifecycle_;
    ISagaScriptWorld& world_;
    ScriptPackageHandle package_;
    std::unordered_map<std::string, ScriptEntityHandle> entities_;
    std::vector<ScriptBindingRuntimeEntry> bindings_;
    std::vector<ScriptBindingRuntimeInstance> instances_;
};

} // namespace SagaEngine::Scripting
