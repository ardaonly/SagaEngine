// SPDX-License-Identifier: MPL-2.0
/// @file BehaviorBackend.hpp
/// @brief Explicit language-to-runtime behavior backend routing.

#pragma once

#include "SagaEngine/Scripting/CSharpScriptHost.hpp"
#include "SagaShared/Behavior/BehaviorArtifactDescriptor.hpp"
#include "SagaShared/Behavior/BehaviorAttachmentDescriptor.hpp"
#include "SagaShared/Behavior/BehaviorInstanceIdentity.hpp"
#include "SagaShared/Behavior/BehaviorLanguageId.hpp"
#include "SagaShared/Behavior/BehaviorModuleDescriptor.hpp"

#include <map>
#include <memory>
#include <string>

namespace SagaEngine::Scripting
{

enum class BehaviorBackendFailure
{
    None,
    InvalidLanguage,
    DuplicateRegistration,
    BackendUnknown,
    BackendUnavailable,
    InvalidModuleDescriptor,
    InvalidArtifactDescriptor,
};

struct BehaviorBackendResult
{
    bool succeeded = false;
    BehaviorBackendFailure failure = BehaviorBackendFailure::None;
    std::string message;
};

class IBehaviorBackend
{
public:
    virtual ~IBehaviorBackend() = default;
    [[nodiscard]] virtual SagaShared::Behavior::BehaviorLanguageId Language() const = 0;
    [[nodiscard]] virtual bool IsAvailable() const noexcept = 0;
    [[nodiscard]] virtual BehaviorBackendResult ValidateArtifact(
        const SagaShared::Behavior::BehaviorArtifactDescriptor& artifact) const = 0;
};

struct BehaviorBackendResolution : BehaviorBackendResult
{
    IBehaviorBackend* backend = nullptr;
};

class BehaviorBackendCatalog
{
public:
    [[nodiscard]] BehaviorBackendResult Register(std::shared_ptr<IBehaviorBackend> backend);
    [[nodiscard]] BehaviorBackendResolution Resolve(
        const SagaShared::Behavior::BehaviorLanguageId& language) const;
    [[nodiscard]] std::size_t Size() const noexcept { return backends_.size(); }

private:
    std::map<std::string, std::shared_ptr<IBehaviorBackend>, std::less<>> backends_;
};

struct CSharpBehaviorInstanceCreateRequest
{
    ScriptPackageHandle package;
    SagaShared::Behavior::BehaviorModuleDescriptor module;
    SagaShared::Behavior::BehaviorAttachmentDescriptor attachment;
    SagaShared::Behavior::BehaviorInstanceIdentity instanceIdentity;
    ScriptEntityHandle selfEntity;
};

/// Adapter over the one existing CSharpScriptHost pipeline.
class CSharpBehaviorBackend final : public IBehaviorBackend
{
public:
    explicit CSharpBehaviorBackend(CSharpScriptHost& host) : host_(host) {}

    [[nodiscard]] SagaShared::Behavior::BehaviorLanguageId Language() const override;
    [[nodiscard]] bool IsAvailable() const noexcept override { return true; }
    [[nodiscard]] BehaviorBackendResult ValidateArtifact(
        const SagaShared::Behavior::BehaviorArtifactDescriptor& artifact) const override;
    [[nodiscard]] ScriptPackageLoadResult LoadVerifiedGeneration(
        const VerifiedScriptGenerationLoadRequest& request);
    [[nodiscard]] ScriptInstanceCreateResult CreateInstance(
        const CSharpBehaviorInstanceCreateRequest& request);

private:
    CSharpScriptHost& host_;
};

} // namespace SagaEngine::Scripting
