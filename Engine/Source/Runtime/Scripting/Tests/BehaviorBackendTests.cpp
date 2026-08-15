// SPDX-License-Identifier: MPL-2.0

#include "SagaEngine/Scripting/BehaviorBackend.hpp"
#include "SagaEngine/Scripting/OperationAdmission.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace SagaEngine::Scripting
{
namespace
{

class TestBackend final : public IBehaviorBackend
{
public:
    TestBackend(std::string language, bool available = true)
        : language_{std::move(language)}, available_(available) {}
    SagaShared::Behavior::BehaviorLanguageId Language() const override
    { return language_; }
    bool IsAvailable() const noexcept override { return available_; }
    BehaviorBackendResult ValidateArtifact(
        const SagaShared::Behavior::BehaviorArtifactDescriptor&) const override
    { return {true, BehaviorBackendFailure::None, {}}; }
private:
    SagaShared::Behavior::BehaviorLanguageId language_;
    bool available_;
};

TEST(BehaviorBackendCatalogTests, RoutesOnlyExplicitRegisteredLanguage)
{
    BehaviorBackendCatalog catalog;
    auto backend = std::make_shared<TestBackend>("csharp");
    EXPECT_TRUE(catalog.Register(backend).succeeded);
    const auto resolved = catalog.Resolve({"csharp"});
    ASSERT_TRUE(resolved.succeeded);
    EXPECT_EQ(resolved.backend, backend.get());
    const auto unknown = catalog.Resolve({"unknown"});
    EXPECT_FALSE(unknown.succeeded);
    EXPECT_EQ(unknown.failure, BehaviorBackendFailure::BackendUnknown);
    EXPECT_EQ(unknown.backend, nullptr);
}

TEST(BehaviorBackendCatalogTests, RejectsDuplicateAndUnavailableBackends)
{
    BehaviorBackendCatalog catalog;
    const auto invalid = catalog.Register(std::make_shared<TestBackend>(""));
    EXPECT_FALSE(invalid.succeeded);
    EXPECT_EQ(invalid.failure, BehaviorBackendFailure::InvalidLanguage);
    EXPECT_TRUE(catalog.Register(std::make_shared<TestBackend>("csharp")).succeeded);
    const auto duplicate = catalog.Register(std::make_shared<TestBackend>("csharp"));
    EXPECT_FALSE(duplicate.succeeded);
    EXPECT_EQ(duplicate.failure, BehaviorBackendFailure::DuplicateRegistration);
    EXPECT_TRUE(catalog.Register(std::make_shared<TestBackend>("disabled", false)).succeeded);
    const auto unavailable = catalog.Resolve({"disabled"});
    EXPECT_FALSE(unavailable.succeeded);
    EXPECT_EQ(unavailable.failure, BehaviorBackendFailure::BackendUnavailable);
}

TEST(BehaviorIdentityTests, ModuleAttachmentAndInstanceIdentitiesRemainDistinct)
{
    SagaShared::Behavior::BehaviorModuleId module{"behavior://sample/controller"};
    SagaShared::Behavior::BehaviorAttachmentDescriptor attachment;
    attachment.attachmentId = "attachment://sample/entity/controller";
    attachment.behaviorModuleId = module;
    SagaShared::Behavior::BehaviorInstanceIdentity instance;
    instance.value = "behavior-instance://launch/attachment";
    instance.behaviorModuleId = module;
    instance.attachmentId = attachment.attachmentId;
    instance.worldGenerationId = "launch";
    EXPECT_TRUE(module.IsValid());
    EXPECT_NE(module.value, attachment.attachmentId);
    EXPECT_NE(module.value, instance.value);
    EXPECT_NE(attachment.attachmentId, instance.value);
}

TEST(BehaviorBackendCatalogTests, CSharpArtifactFormatIsBackendSpecific)
{
    CSharpScriptHost host;
    CSharpBehaviorBackend backend(host);
    SagaShared::Behavior::BehaviorArtifactDescriptor artifact;
    artifact.language = SagaShared::Behavior::CSharpBehaviorLanguage;
    artifact.format = "saga.csharp.assembly-generation.v1";
    artifact.artifactHash = "sha256:payload";
    artifact.payloadArtifacts.emplace("scripts.assembly", artifact.artifactHash);
    EXPECT_TRUE(backend.ValidateArtifact(artifact).succeeded);
    artifact.format = "unknown";
    EXPECT_FALSE(backend.ValidateArtifact(artifact).succeeded);
}

TEST(OperationAdmissionTests, StableOpaqueIdentityAndEffectsRemainIndependent)
{
    SagaShared::Api::SagaOperationDescriptor get;
    get.operationId.value = "operation://stable/get-position";
    get.apiId.value = "api://stable/world";
    get.packageId = "package";
    get.moduleId = "world";
    get.effect = SagaShared::Scripting::ScriptSideEffect::ReadAuthoritativeState;
    get.requiredCapabilities = {"World.Transform.ReadWrite"};
    get.runtimeBinding = "RenamedClrType.RenamedMethod";
    get.csharpBinding = "Another.Symbol";
    get.packageGenerationId = "generation";
    get.packageGenerationHash = "sha256:generation";
    SagaOperationCatalog catalog;
    EXPECT_TRUE(catalog.Register(get));
    EXPECT_EQ(catalog.Find("operation://stable/get-position")->operationId.value,
        "operation://stable/get-position");
    EXPECT_EQ(catalog.Find("operation://stable/get-position")->effect,
        SagaShared::Scripting::ScriptSideEffect::ReadAuthoritativeState);
}

TEST(OperationAdmissionTests, DuplicateAndUnknownOperationsFailClosed)
{
    SagaShared::Api::SagaOperationDescriptor operation;
    operation.operationId.value = "operation://stable/set-position";
    operation.apiId.value = "api://stable/world";
    operation.packageId = "package";
    operation.moduleId = "world";
    operation.effect = SagaShared::Scripting::ScriptSideEffect::WriteAuthoritativeState;
    operation.requiredCapabilities = {"World.Transform.ReadWrite"};
    operation.runtimeBinding = "Entity.SetPosition";
    operation.csharpAvailable = true;
    operation.packageGenerationId = "generation";
    operation.packageGenerationHash = "sha256:generation";
    SagaOperationCatalog catalog;
    EXPECT_TRUE(catalog.Register(operation));
    EXPECT_FALSE(catalog.Register(operation));
    const std::unordered_set<std::string> effective{"World.Transform.ReadWrite"};
    const OperationAdmissionContext context{
        catalog, effective, "generation", "sha256:generation", "Entity.SetPosition"};
    const auto unknown = OperationAdmissionEvaluator::Evaluate(
        context, SagaShared::Api::SagaOperationId{"operation://unknown"});
    EXPECT_FALSE(unknown.allowed);
    EXPECT_EQ(unknown.failure, OperationAdmissionFailure::UnknownOperation);
}

TEST(OperationAdmissionTests, CapabilityAndGenerationAreRequiredForAdmission)
{
    SagaShared::Api::SagaOperationDescriptor operation;
    operation.operationId.value = "operation://stable/set-position";
    operation.apiId.value = "api://stable/world";
    operation.packageId = "package";
    operation.moduleId = "world";
    operation.effect = SagaShared::Scripting::ScriptSideEffect::WriteAuthoritativeState;
    operation.requiredCapabilities = {"World.Transform.ReadWrite"};
    operation.runtimeBinding = "Entity.SetPosition";
    operation.csharpAvailable = true;
    operation.packageGenerationId = "generation";
    operation.packageGenerationHash = "sha256:generation";
    SagaOperationCatalog catalog;
    ASSERT_TRUE(catalog.Register(operation));
    const std::unordered_set<std::string> none;
    auto denied = OperationAdmissionEvaluator::Evaluate(
        {catalog, none, "generation", "sha256:generation", "Entity.SetPosition"},
        operation.operationId);
    EXPECT_EQ(denied.failure, OperationAdmissionFailure::MissingCapability);
    const std::unordered_set<std::string> effective{"World.Transform.ReadWrite"};
    denied = OperationAdmissionEvaluator::Evaluate(
        {catalog, effective, "other", "sha256:generation", "Entity.SetPosition"},
        operation.operationId);
    EXPECT_EQ(denied.failure, OperationAdmissionFailure::PackageGenerationMismatch);
    denied = OperationAdmissionEvaluator::Evaluate(
        {catalog, effective, "generation", "sha256:generation", "Entity.GetPosition"},
        operation.operationId);
    EXPECT_EQ(denied.failure, OperationAdmissionFailure::UnavailableOperation);
    const auto allowed = OperationAdmissionEvaluator::Evaluate(
        {catalog, effective, "generation", "sha256:generation", "Entity.SetPosition"},
        operation.operationId);
    EXPECT_TRUE(allowed.allowed);
    EXPECT_EQ(allowed.failure, OperationAdmissionFailure::None);
}

TEST(OperationAdmissionTests, RequestedCapabilityIsNotEffectiveAuthority)
{
    SagaShared::Api::SagaOperationDescriptor operation;
    operation.operationId.value = "operation://stable/set-position";
    operation.apiId.value = "api://stable/world";
    operation.packageId = "package";
    operation.moduleId = "world";
    operation.effect = SagaShared::Scripting::ScriptSideEffect::WriteAuthoritativeState;
    operation.requiredCapabilities = {"World.Transform.ReadWrite"};
    operation.runtimeBinding = "Entity.SetPosition";
    operation.csharpAvailable = true;
    operation.packageGenerationId = "generation";
    operation.packageGenerationHash = "sha256:generation";
    SagaOperationCatalog catalog;
    ASSERT_TRUE(catalog.Register(operation));
    const std::vector<std::string> requested{"World.Transform.ReadWrite"};
    const std::unordered_set<std::string> effective;
    EXPECT_FALSE(requested.empty());
    const auto denied = OperationAdmissionEvaluator::Evaluate(
        {catalog, effective, "generation", "sha256:generation", "Entity.SetPosition"},
        operation.operationId);
    EXPECT_EQ(denied.failure, OperationAdmissionFailure::MissingCapability);
}

TEST(BehaviorBackendTests, AttachmentRequirementCannotElevateModuleAuthority)
{
    CSharpScriptHost host;
    CSharpBehaviorBackend backend(host);
    CSharpBehaviorInstanceCreateRequest request;
    request.module.moduleId.value = "behavior://sample/controller";
    request.module.source.language = SagaShared::Behavior::CSharpBehaviorLanguage;
    request.module.source.sourceSymbolId = "Game.Controller";
    request.attachment.attachmentId = "attachment://sample/controller";
    request.attachment.behaviorModuleId = request.module.moduleId;
    request.attachment.requiredCapabilities = {"World.Transform.ReadWrite"};
    request.instanceIdentity.value = "behavior-instance://sample/controller";
    request.instanceIdentity.behaviorModuleId = request.module.moduleId;
    request.instanceIdentity.attachmentId = request.attachment.attachmentId;
    const auto result = backend.CreateInstance(request);
    EXPECT_FALSE(result.Succeeded());
}

} // namespace
} // namespace SagaEngine::Scripting
