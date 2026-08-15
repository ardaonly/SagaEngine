// SPDX-License-Identifier: MPL-2.0
/// @file CSharpScriptHostSemanticOperationTests.hpp
/// @brief Verified semantic-operation catalog host fixtures and rejection tests.

#pragma once

namespace
{
[[nodiscard]] nlohmann::json VerifiedOperationCatalog(
    const VerifiedScriptGenerationLoadRequest& request)
{
    const auto operation = [](const char* operationId, const char* effect,
                              const char* runtimeBinding, const char* csharpBinding)
    {
        return nlohmann::json{{"operationId", operationId},
            {"apiId", "com.saga.gameplay.foundation/gameplay.highlevel"},
            {"packageId", "com.saga.gameplay.foundation"},
            {"module", "gameplay.highlevel"}, {"apiLevel", "High"},
            {"effect", effect},
            {"requiredCapabilities", nlohmann::json::array({kTransformReadWriteCapability})},
            {"runtimeBinding", runtimeBinding}, {"csharpBinding", csharpBinding},
            {"csharp", true}, {"blocks", true}};
    };
    return nlohmann::json{{"schemaVersion", 1}, {"projectId", request.projectId},
        {"packageGenerationId", request.packageGenerationId},
        {"packageGenerationHash", request.packageGenerationHash},
        {"packageGraphHash", request.packageGraphHash},
        {"operations", nlohmann::json::array({
            operation("com.saga.gameplay.foundation/gameplay.highlevel/ScriptEntity.TryGetPosition",
                "ReadAuthoritativeState", "Entity.GetPosition", "ScriptEntity.TryGetPosition"),
            operation("com.saga.gameplay.foundation/gameplay.highlevel/ScriptEntity.TrySetPosition",
                "WriteAuthoritativeState", "Entity.SetPosition", "ScriptEntity.TrySetPosition")})}};
}
} // namespace

TEST(CSharpScriptHostTests, VerifiedGenerationRejectsDuplicateOperationIdentity)
{
    auto fixture = CreateVerifiedGeneration("saga_verified_generation_duplicate_operation");
    auto catalog = ReadJson(fixture.request.operationCatalog);
    catalog["operations"].push_back(catalog["operations"][0]);
    RewriteVerifiedJson(fixture.request.operationCatalog, catalog);
    CSharpScriptHost host(HostOptions());
    const auto result = host.LoadVerifiedGeneration(fixture.request);
    ASSERT_FALSE(result.Succeeded());
    EXPECT_EQ(result.diagnostics.front().diagnostic.code.value,
        SagaEngine::Scripting::ScriptHostDiagnostics::ManifestInvalid);
}

TEST(CSharpScriptHostTests, VerifiedGenerationRejectsInvalidOperationIdentity)
{
    auto fixture = CreateVerifiedGeneration("saga_verified_generation_invalid_operation");
    auto catalog = ReadJson(fixture.request.operationCatalog);
    catalog["operations"][0]["operationId"] = "operation identity with spaces";
    RewriteVerifiedJson(fixture.request.operationCatalog, catalog);
    CSharpScriptHost host(HostOptions());
    const auto result = host.LoadVerifiedGeneration(fixture.request);
    ASSERT_FALSE(result.Succeeded());
    EXPECT_EQ(result.diagnostics.front().diagnostic.code.value,
        SagaEngine::Scripting::ScriptHostDiagnostics::ManifestInvalid);
}
