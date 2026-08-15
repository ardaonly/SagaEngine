// SPDX-License-Identifier: MPL-2.0

#include "SagaEngine/Scripting/BehaviorBackend.hpp"

#include <gtest/gtest.h>

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

} // namespace
} // namespace SagaEngine::Scripting
