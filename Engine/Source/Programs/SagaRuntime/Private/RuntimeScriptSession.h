// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <SagaEngine/Scripting/CSharpScriptHost.hpp>
#include <SagaRuntime/Play/RuntimePlayContracts.hpp>

#include <map>
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

    SagaEngine::Scripting::ScriptEntityCreateResult CreateEntity(const std::string& name) override;
    SagaEngine::Scripting::ScriptHostOperationResult SetPosition(
        SagaEngine::Scripting::ScriptEntityHandle entity,
        SagaEngine::Scripting::ScriptVector3 position) override;
    SagaEngine::Scripting::ScriptEntityPositionResult GetPosition(
        SagaEngine::Scripting::ScriptEntityHandle entity) const override;

private:
    SagaRuntime::Play::RuntimeSceneArtifact scene_;
    std::map<std::string, std::size_t> byId_;
};

class RuntimeNamedActionTarget final : public SagaRuntime::Play::INamedActionTarget
{
public:
    RuntimeNamedActionTarget(SagaEngine::Scripting::CSharpScriptHost& host,
        std::map<std::pair<std::string, std::string>,
                 SagaEngine::Scripting::ScriptInstanceHandle> instances,
        std::map<std::pair<std::string, std::string>, std::string> methods);
    bool InvokeNamedAction(std::string_view behaviorTypeId,
                           std::string_view namedActionId,
                           std::string_view entityId) override;

private:
    SagaEngine::Scripting::CSharpScriptHost& host_;
    std::map<std::pair<std::string, std::string>,
             SagaEngine::Scripting::ScriptInstanceHandle> instances_;
    std::map<std::pair<std::string, std::string>, std::string> methods_;
};

} // namespace SagaRuntimeApp
