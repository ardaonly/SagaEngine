/// @file ScriptBindingRuntime.cpp
/// @brief Implements runtime-owned SagaScript binding lifecycle orchestration.

#include <SagaEngine/Scripting/ScriptBindingRuntime.hpp>

#include "SagaShared/Diagnostics/DiagnosticCategory.hpp"
#include "SagaShared/Diagnostics/DiagnosticSeverity.hpp"
#include "SagaShared/Diagnostics/DiagnosticSource.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace SagaEngine::Scripting
{

namespace
{

[[nodiscard]] ScriptDiagnostic MakeDiagnostic(
    std::string code,
    std::string title,
    std::string message)
{
    ScriptDiagnostic diagnostic;
    diagnostic.diagnostic.severity =
        SagaShared::Diagnostics::DiagnosticSeverity::Error;
    diagnostic.diagnostic.category =
        SagaShared::Diagnostics::DiagnosticCategory::Script;
    diagnostic.diagnostic.source =
        SagaShared::Diagnostics::DiagnosticSource::Runtime;
    diagnostic.diagnostic.code.value = std::move(code);
    diagnostic.diagnostic.title = std::move(title);
    diagnostic.diagnostic.message = std::move(message);
    return diagnostic;
}

[[nodiscard]] ScriptHostOperationResult FailedOperation(
    ScriptDiagnostic diagnostic)
{
    ScriptHostOperationResult result;
    result.diagnostics.push_back(std::move(diagnostic));
    return result;
}

void AppendDiagnostics(
    ScriptHostOperationResult& target,
    std::vector<ScriptDiagnostic> diagnostics)
{
    for (auto& diagnostic : diagnostics)
    {
        target.diagnostics.push_back(std::move(diagnostic));
    }
}

} // namespace

ScriptBindingRuntime::ScriptBindingRuntime(
    ScriptLifecycleService& lifecycle,
    ISagaScriptWorld& world)
    : lifecycle_(lifecycle)
    , world_(world)
{
}

ScriptHostOperationResult ScriptBindingRuntime::LoadAndCreate(
    const ScriptBindingRuntimeCreateRequest& request)
{
    static_cast<void>(Shutdown());
    entities_ = request.entities;

    auto manifest = ScriptBindingManifestLoader::Load(request.manifestRequest);
    if (!manifest.Succeeded())
    {
        ScriptHostOperationResult result;
        result.diagnostics = std::move(manifest.diagnostics);
        return result;
    }

    auto selfResolution = ResolveSelfEntities(manifest.bindings);
    if (!selfResolution.Succeeded())
    {
        return selfResolution;
    }

    auto load = lifecycle_.LoadPackage(request.packageRequest);
    if (!load.Succeeded())
    {
        ScriptHostOperationResult result;
        result.diagnostics = std::move(load.diagnostics);
        return result;
    }
    package_ = load.package;
    bindings_ = std::move(manifest.bindings);

    for (const auto& binding : bindings_)
    {
        ScriptInstanceCreateRequest createRequest;
        createRequest.package = package_;
        createRequest.classId = ScriptClassId{binding.binding.declaringType};
        createRequest.scriptId = binding.binding.scriptId;
        createRequest.selfEntity = entities_.at(binding.self.entityKey);

        auto created = lifecycle_.CreateInstance(createRequest);
        if (!created.Succeeded())
        {
            ScriptHostOperationResult result;
            result.diagnostics = std::move(created.diagnostics);
            const auto cleanup = DestroyCreatedInstances();
            AppendDiagnostics(result, cleanup.diagnostics);
            return result;
        }

        instances_.push_back(ScriptBindingRuntimeInstance{
            binding.binding.bindingId,
            created.instance,
        });
    }

    ScriptHostOperationResult result;
    result.succeeded = true;
    return result;
}

ScriptHostOperationResult ScriptBindingRuntime::StartAll() const
{
    ScriptHostOperationResult result;
    result.succeeded = true;
    for (const auto& instance : instances_)
    {
        auto started = lifecycle_.StartInstance(instance.instance);
        if (!started.Succeeded())
        {
            result.succeeded = false;
            AppendDiagnostics(result, std::move(started.diagnostics));
            return result;
        }
    }
    return result;
}

ScriptHostOperationResult ScriptBindingRuntime::UpdateAll(
    const double deltaTimeSeconds) const
{
    ScriptHostOperationResult result;
    result.succeeded = true;
    for (const auto& instance : instances_)
    {
        auto updated =
            lifecycle_.UpdateInstance(instance.instance, deltaTimeSeconds);
        if (!updated.Succeeded())
        {
            result.succeeded = false;
            AppendDiagnostics(result, std::move(updated.diagnostics));
            return result;
        }
    }
    return result;
}

ScriptHostOperationResult ScriptBindingRuntime::Shutdown()
{
    auto result = DestroyCreatedInstances();
    bindings_.clear();
    package_ = {};
    entities_.clear();
    return result;
}

const std::vector<ScriptBindingRuntimeInstance>&
ScriptBindingRuntime::Instances() const noexcept
{
    return instances_;
}

const std::unordered_map<std::string, ScriptEntityHandle>&
ScriptBindingRuntime::Entities() const noexcept
{
    return entities_;
}

ScriptHostOperationResult ScriptBindingRuntime::ResolveSelfEntities(
    const std::vector<ScriptBindingRuntimeEntry>& bindings)
{
    for (const auto& binding : bindings)
    {
        if (binding.self.policy == ScriptBindingSelfPolicy::Existing)
        {
            const auto iterator = entities_.find(binding.self.entityKey);
            if (iterator == entities_.end() || !iterator->second.IsValid())
            {
                auto diagnostic = MakeDiagnostic(
                    ScriptBindingDiagnostics::MissingExistingEntityKey,
                    "Script self entity key missing",
                    "Runtime self policy references an entityKey that does not exist.");
                diagnostic.bindingId = binding.binding.bindingId;
                diagnostic.scriptId = binding.binding.scriptId;
                diagnostic.metadata["entityKey"] = binding.self.entityKey;
                return FailedOperation(std::move(diagnostic));
            }
            continue;
        }

        if (entities_.find(binding.self.entityKey) != entities_.end())
        {
            auto diagnostic = MakeDiagnostic(
                ScriptBindingDiagnostics::DuplicateCreateEntityKey,
                "Script self create entity key already exists",
                "Runtime create self policy cannot replace an existing entityKey.");
            diagnostic.bindingId = binding.binding.bindingId;
            diagnostic.scriptId = binding.binding.scriptId;
            diagnostic.metadata["entityKey"] = binding.self.entityKey;
            return FailedOperation(std::move(diagnostic));
        }

        auto created = world_.CreateEntity(binding.self.name);
        if (!created.Succeeded())
        {
            ScriptHostOperationResult result;
            auto diagnostic = MakeDiagnostic(
                ScriptBindingDiagnostics::CreateSelfFailed,
                "Script self entity creation failed",
                "Runtime create self policy could not create the self entity.");
            diagnostic.bindingId = binding.binding.bindingId;
            diagnostic.scriptId = binding.binding.scriptId;
            diagnostic.metadata["entityKey"] = binding.self.entityKey;
            diagnostic.metadata["name"] = binding.self.name;
            result.diagnostics.push_back(std::move(diagnostic));
            AppendDiagnostics(result, std::move(created.diagnostics));
            return result;
        }

        entities_.emplace(binding.self.entityKey, created.entity);
    }

    ScriptHostOperationResult result;
    result.succeeded = true;
    return result;
}

ScriptHostOperationResult ScriptBindingRuntime::DestroyCreatedInstances()
{
    ScriptHostOperationResult result;
    result.succeeded = true;

    for (auto iterator = instances_.rbegin(); iterator != instances_.rend();
         ++iterator)
    {
        auto destroyed = lifecycle_.DestroyInstance(iterator->instance);
        if (!destroyed.Succeeded())
        {
            result.succeeded = false;
            AppendDiagnostics(result, std::move(destroyed.diagnostics));
        }
    }

    instances_.clear();
    return result;
}

} // namespace SagaEngine::Scripting
