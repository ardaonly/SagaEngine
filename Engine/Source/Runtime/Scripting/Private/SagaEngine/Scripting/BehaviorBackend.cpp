// SPDX-License-Identifier: MPL-2.0
/// @file BehaviorBackend.cpp

#include "SagaEngine/Scripting/BehaviorBackend.hpp"

#include <utility>

namespace SagaEngine::Scripting
{

BehaviorBackendResult BehaviorBackendCatalog::Register(
    std::shared_ptr<IBehaviorBackend> backend)
{
    if (!backend || !backend->Language().IsValid())
        return {false, BehaviorBackendFailure::InvalidLanguage,
                "Behavior backend language identity is required."};
    const auto language = backend->Language().value;
    if (backends_.contains(language))
        return {false, BehaviorBackendFailure::DuplicateRegistration,
                "A behavior backend is already registered for language '" + language + "'."};
    backends_.emplace(language, std::move(backend));
    return {true, BehaviorBackendFailure::None, {}};
}

BehaviorBackendResolution BehaviorBackendCatalog::Resolve(
    const SagaShared::Behavior::BehaviorLanguageId& language) const
{
    if (!language.IsValid())
        return {{false, BehaviorBackendFailure::InvalidLanguage,
                 "Behavior language identity is required."}, nullptr};
    const auto found = backends_.find(language.value);
    if (found == backends_.end())
        return {{false, BehaviorBackendFailure::BackendUnknown,
                 "No behavior backend is registered for language '" + language.value + "'."},
                nullptr};
    if (!found->second->IsAvailable())
        return {{false, BehaviorBackendFailure::BackendUnavailable,
                 "The registered behavior backend is unavailable for language '" +
                     language.value + "'."},
                nullptr};
    return {{true, BehaviorBackendFailure::None, {}}, found->second.get()};
}

SagaShared::Behavior::BehaviorLanguageId CSharpBehaviorBackend::Language() const
{
    return SagaShared::Behavior::CSharpBehaviorLanguage;
}

BehaviorBackendResult CSharpBehaviorBackend::ValidateArtifact(
    const SagaShared::Behavior::BehaviorArtifactDescriptor& artifact) const
{
    const auto assembly = artifact.payloadArtifacts.find("scripts.assembly");
    if (artifact.language != SagaShared::Behavior::CSharpBehaviorLanguage ||
        artifact.format != "saga.csharp.assembly-generation.v1" ||
        artifact.artifactHash.empty() || assembly == artifact.payloadArtifacts.end() ||
        assembly->second != artifact.artifactHash)
        return {false, BehaviorBackendFailure::InvalidArtifactDescriptor,
            "C# behavior artifact does not pin its verified assembly payload."};
    return {true, BehaviorBackendFailure::None, {}};
}

ScriptPackageLoadResult CSharpBehaviorBackend::LoadVerifiedGeneration(
    const VerifiedScriptGenerationLoadRequest& request)
{
    return host_.LoadVerifiedGeneration(request);
}

ScriptInstanceCreateResult CSharpBehaviorBackend::CreateInstance(
    const CSharpBehaviorInstanceCreateRequest& request)
{
    if (!request.module.moduleId.IsValid() ||
        request.module.source.language != SagaShared::Behavior::CSharpBehaviorLanguage ||
        request.module.source.sourceSymbolId.empty() ||
        request.attachment.behaviorModuleId != request.module.moduleId ||
        request.attachment.attachmentId.empty() || request.instanceIdentity.value.empty() ||
        request.instanceIdentity.behaviorModuleId != request.module.moduleId ||
        request.instanceIdentity.attachmentId != request.attachment.attachmentId)
    {
        ScriptInstanceCreateResult result;
        return result;
    }

    ScriptInstanceCreateRequest create;
    create.package = request.package;
    create.classId.value = request.module.source.sourceSymbolId;
    create.scriptId = request.module.moduleId.value;
    create.selfEntity = request.selfEntity;
    create.instanceConfigurationCanonicalJson =
        request.attachment.instanceConfigurationCanonicalJson;
    return host_.CreateInstance(create);
}

} // namespace SagaEngine::Scripting
