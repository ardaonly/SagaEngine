// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <SagaEngine/Scripting/CSharpScriptHost.hpp>
#include <SagaRuntime/Play/RuntimePlayContracts.hpp>

#include <map>
#include <set>
#include <string>

namespace SagaRuntimeApp
{

class RuntimeScriptWorld final : public SagaEngine::Scripting::ISagaScriptWorld
{
public:
    explicit RuntimeScriptWorld(SagaRuntime::Play::RuntimeSceneArtifact scene);
    [[nodiscard]] SagaEngine::Scripting::ScriptEntityHandle Handle(std::string_view id) const;
    [[nodiscard]] const SagaRuntime::Play::RuntimeSceneArtifact& Scene() const noexcept
    { return scene_; }
    [[nodiscard]] std::size_t EntityCount() const noexcept
    { return scene_.entities.size(); }

    SagaEngine::Scripting::ScriptEntityCreateResult CreateEntity(const std::string& name) override;
    SagaEngine::Scripting::ScriptHostOperationResult SetPosition(
        SagaEngine::Scripting::ScriptEntityHandle entity,
        SagaEngine::Scripting::ScriptVector3 position) override;
    SagaEngine::Scripting::ScriptEntityPositionResult GetPosition(
        SagaEngine::Scripting::ScriptEntityHandle entity) const override;
    SagaEngine::Scripting::ScriptEntityCreateResult FindEntity(
        const std::string& stableId) const override;
    SagaEngine::Scripting::ScriptHostOperationResult SetActive(
        SagaEngine::Scripting::ScriptEntityHandle entity, bool active) override;
    SagaEngine::Scripting::ScriptStateBoolResult GetActive(
        SagaEngine::Scripting::ScriptEntityHandle entity) const override;

private:
    SagaRuntime::Play::RuntimeSceneArtifact scene_;
    std::map<std::string, std::size_t> byId_;
};

class RuntimeScriptInputPort final : public SagaEngine::Scripting::ISagaScriptInputPort
{
public:
    using Policy = std::map<std::string, std::set<std::string>>;
    explicit RuntimeScriptInputPort(Policy policy) : policy_(std::move(policy)) {}
    void Advance(std::set<std::string> pressed, std::set<std::string> held);
    void Clear();
    SagaEngine::Scripting::ScriptStateBoolResult Query(
        const SagaEngine::Scripting::ScriptStateContext&, const std::string&,
        SagaEngine::Scripting::ScriptLogicalInputState) const override;
    SagaEngine::Scripting::ScriptStateDoubleResult GetValue(
        const SagaEngine::Scripting::ScriptStateContext&,
        const std::string&) const override;
private:
    [[nodiscard]] bool Allowed(
        const SagaEngine::Scripting::ScriptStateContext&, const std::string&) const;
    Policy policy_;
    std::set<std::string> pressed_;
    std::set<std::string> held_;
    std::set<std::string> released_;
};

class RuntimeExitPort final : public SagaEngine::Scripting::ISagaScriptRuntimePort
{
public:
    SagaEngine::Scripting::ScriptHostOperationResult RequestExit(
        const SagaEngine::Scripting::ScriptStateContext&) override;
    [[nodiscard]] bool Requested() const noexcept { return requested_; }
    void Reset() noexcept { requested_ = false; }
private:
    bool requested_ = false;
};

class RuntimeNamedActionTarget final : public SagaRuntime::Play::INamedActionTarget
{
public:
    RuntimeNamedActionTarget(SagaEngine::Scripting::CSharpScriptHost& host,
        std::map<std::pair<std::string, std::string>,
                 SagaEngine::Scripting::ScriptInstanceHandle> instances,
        std::map<std::pair<std::string, std::string>, std::string> methods);
    bool InvokeNamedAction(std::string_view behaviorModuleId,
                           std::string_view namedActionId,
                           std::string_view entityId) override;

private:
    SagaEngine::Scripting::CSharpScriptHost& host_;
    std::map<std::pair<std::string, std::string>,
             SagaEngine::Scripting::ScriptInstanceHandle> instances_;
    std::map<std::pair<std::string, std::string>, std::string> methods_;
};

} // namespace SagaRuntimeApp
