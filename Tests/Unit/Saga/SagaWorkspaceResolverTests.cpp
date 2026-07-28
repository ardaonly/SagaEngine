// SPDX-License-Identifier: MPL-2.0
#include "SagaProductTestSupport.hpp"

using namespace SagaProduct;
using namespace SagaProductTestSupport;

namespace
{
TEST(SagaProductInfoTest, VersionInfoResolvesFromDistributionJson)
{
    const fs::path root = MakeTempDir("saga_product_info_test");
    const fs::path versionJson = root / "version.json";
    WriteFile(versionJson, R"({
        "productName": "SAGA",
        "distributionVersion": "0.0.8-test",
        "buildVersion": "0.0.8",
        "gitCommit": "abc123",
        "platform": "Linux",
        "binaries": [ "Saga" ]
    })");

    SagaAppConfig config;
    config.versionInfoPath = versionJson;

    std::string error;
    const SagaProductInfo info = LoadProductInfo(config, error);

    EXPECT_TRUE(error.empty());
    EXPECT_EQ(info.productName, "SAGA");
    EXPECT_EQ(info.distributionVersion, "0.0.8-test");
    EXPECT_EQ(info.buildVersion, "0.0.8");
    EXPECT_EQ(info.gitCommit, "abc123");
    EXPECT_EQ(info.binaries.size(), 1u);
    EXPECT_EQ(info.binaries.front(), "Saga");
}

TEST(SagaWorkspaceResolverTest, BuiltInProductWorkspaceResolvesFromProductDefaults)
{
    SagaWorkspaceResolver resolver;
    SagaWorkspaceResolveRequest request;
    request.selector = "builtin:basic";
    request.builtInBasicRoot = BuiltInProductWorkspaceRoot();

    const SagaWorkspaceResolveResult result = resolver.Resolve(request);

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.workspace.id, "builtin.basic");
    EXPECT_EQ(result.workspace.runtimeRole, "SagaRuntime");
    EXPECT_TRUE(result.workspace.serverRole.empty());
    EXPECT_TRUE(result.workspace.artifactHash.empty());
}

TEST(SagaWorkspaceResolverTest, MissingProjectRootFailsDeterministically)
{
    SagaWorkspaceResolver resolver;
    SagaWorkspaceResolveRequest request;
    request.selector =
        (MakeTempDir("saga_missing_project_root_test") / "missing").string();
    request.builtInBasicRoot = BuiltInProductWorkspaceRoot();

    const SagaWorkspaceResolveResult result = resolver.Resolve(request);

    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.error.find("missing"), std::string::npos);
}

TEST(SagaWorkspaceResolverTest, ProjectManifestResolvesWorkspaceDefaults)
{
    const fs::path root = MakeTempDir("saga_project_manifest_workspace_test");
    WriteFile(root / "Project" / "Project.sagaproj", R"({
        "schemaVersion": 0,
        "projectId": "manifest-project",
        "displayName": "Manifest Project"
    })");

    SagaWorkspaceResolver resolver;
    SagaWorkspaceResolveRequest request;
    request.selector = (root / "Project").string();
    request.builtInBasicRoot = BuiltInProductWorkspaceRoot();

    const SagaWorkspaceResolveResult result = resolver.Resolve(request);

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.workspace.id, "manifest-project");
    EXPECT_EQ(result.workspace.displayName, "Manifest Project");
    EXPECT_EQ(result.workspace.runtimeRole, "SagaRuntime");
    EXPECT_TRUE(result.workspace.serverRole.empty());
    EXPECT_TRUE(result.workspace.artifactHash.empty());
}

TEST(SagaWorkspaceResolverTest, ExplicitFutureServerMetadataRemainsDeclarative)
{
    const fs::path root = MakeTempDir("saga_server_metadata_workspace_test");
    WriteFile(root / "Server.sagaproj", R"({
        "schemaVersion": 0,
        "projectId": "server-metadata-project",
        "displayName": "Server Metadata Project",
        "serverRole": "future.dedicated.metadata"
    })");

    SagaWorkspaceResolver resolver;
    SagaWorkspaceResolveRequest request;
    request.selector = root.string();

    const SagaWorkspaceResolveResult result = resolver.Resolve(request);

    ASSERT_TRUE(result.ok) << result.error;
    EXPECT_EQ(result.workspace.serverRole, "future.dedicated.metadata");
}

} // namespace
