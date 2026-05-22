/// @file SagaPipelineTests.cpp
/// @brief Tests for Saga-specific pipeline orchestration.

#include "SagaPipeline/EditorCompositionStep.h"
#include "SagaPipeline/SagaPipelineConfig.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{

namespace fs = std::filesystem;

class FakeRunner final : public SagaPipeline::IExternalToolRunner
{
public:
    [[nodiscard]] SagaPipeline::ExternalToolResult Run(
        const SagaPipeline::ExternalToolInvocation& invocation) const override
    {
        invoked = true;
        lastInvocation = invocation;
        return result;
    }

    mutable bool invoked = false;
    mutable SagaPipeline::ExternalToolInvocation lastInvocation;
    SagaPipeline::ExternalToolResult result;
};

[[nodiscard]] bool ContainsOrderedPair(const std::vector<std::string>& values,
                                       const std::string& key,
                                       const std::string& value)
{
    for (std::size_t index = 0; index + 1 < values.size(); ++index)
    {
        if (values[index] == key && values[index + 1] == value)
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool FileExists(const fs::path& path)
{
    std::error_code ec;
    return fs::exists(path, ec) && fs::is_regular_file(path, ec);
}

} // namespace

TEST(SagaPipelineTest, DefaultsResolveProductEditorCompositionInputs)
{
    SagaPipeline::EditorCompositionBuildRequest request;
    request.projectRoot = "Game";

    const SagaPipeline::EditorCompositionBuildResult result =
        SagaPipeline::EditorCompositionStep::Plan(request);

    EXPECT_EQ(result.workspaceRoot.generic_string(),
              "Game/Editor/CompositionSources");
    EXPECT_EQ(result.buildRoot.generic_string(), "Game/Build");
    EXPECT_EQ(result.invocation.executable,
              "saga-editor-composition-compiler");
    EXPECT_TRUE(ContainsOrderedPair(result.invocation.arguments,
                                    "--composition-id",
                                    "saga.editor.default"));
}

TEST(SagaPipelineTest, BuildsExpectedCompilerInvocation)
{
    SagaPipeline::EditorCompositionBuildRequest request;
    request.projectRoot = "Game";
    request.toolExecutable = "custom-editor-composition";
    request.compositionId = "studio.editor.main";

    const SagaPipeline::EditorCompositionBuildResult result =
        SagaPipeline::EditorCompositionStep::Plan(request);

    ASSERT_GE(result.invocation.arguments.size(), 7u);
    EXPECT_EQ(result.invocation.executable, "custom-editor-composition");
    EXPECT_EQ(result.invocation.arguments[0], "compile");
    EXPECT_TRUE(ContainsOrderedPair(result.invocation.arguments,
                                    "--workspace",
                                    "Game/Editor/CompositionSources"));
    EXPECT_TRUE(ContainsOrderedPair(result.invocation.arguments,
                                    "--build-root",
                                    "Game/Build"));
    EXPECT_TRUE(ContainsOrderedPair(result.invocation.arguments,
                                    "--composition-id",
                                    "studio.editor.main"));
}

TEST(SagaPipelineTest, ExplicitWorkspaceAndBuildRootOverrideDefaults)
{
    SagaPipeline::EditorCompositionBuildRequest request;
    request.projectRoot = "Game";
    request.workspaceRoot = "Game/CustomComposition";
    request.buildRoot = "Game/CustomBuild";

    const SagaPipeline::EditorCompositionBuildResult result =
        SagaPipeline::EditorCompositionStep::Plan(request);

    EXPECT_EQ(result.workspaceRoot.generic_string(), "Game/CustomComposition");
    EXPECT_EQ(result.buildRoot.generic_string(), "Game/CustomBuild");
    EXPECT_TRUE(ContainsOrderedPair(result.invocation.arguments,
                                    "--workspace",
                                    "Game/CustomComposition"));
    EXPECT_TRUE(ContainsOrderedPair(result.invocation.arguments,
                                    "--build-root",
                                    "Game/CustomBuild"));
}

TEST(SagaPipelineTest, ExplainDoesNotInvokeChildProcess)
{
    SagaPipeline::EditorCompositionBuildRequest request;
    request.projectRoot = "Game";
    request.explain = true;

    FakeRunner runner;
    const SagaPipeline::EditorCompositionBuildResult result =
        SagaPipeline::EditorCompositionStep::Run(request, runner);

    EXPECT_FALSE(runner.invoked);
    EXPECT_EQ(result.pipeline.exitCode, 0);
    EXPECT_TRUE(result.pipeline.diagnostics.empty());
}

TEST(SagaPipelineTest, MissingToolProducesLaunchDiagnostic)
{
    SagaPipeline::EditorCompositionBuildRequest request;
    request.toolExecutable = "definitely-not-a-real-saga-pipeline-tool";

    SagaPipeline::ProcessExternalToolRunner runner;
    const SagaPipeline::EditorCompositionBuildResult result =
        SagaPipeline::EditorCompositionStep::Run(request, runner);

    EXPECT_NE(result.pipeline.exitCode, 0);
    ASSERT_FALSE(result.pipeline.diagnostics.empty());
    EXPECT_EQ(result.pipeline.diagnostics.front().code,
              "SAGA_PIPELINE_TOOL_EXIT_FAILED");
}

#ifdef SAGA_EDITOR_COMPOSITION_COMPILER
TEST(SagaPipelineTest, InvokesCheckedInEditorCompositionCompilerWhenAvailable)
{
    const fs::path compiler = SAGA_EDITOR_COMPOSITION_COMPILER;
    if (!FileExists(compiler))
    {
        GTEST_SKIP() << "saga-editor-composition-compiler is not available";
    }

    const fs::path projectRoot =
        fs::temp_directory_path() / "saga_pipeline_editor_composition_actual";
    fs::remove_all(projectRoot);
    fs::create_directories(projectRoot / "Editor");
    fs::copy(fs::path(SAGA_SOURCE_ROOT) / "Editor" / "CompositionSources",
             projectRoot / "Editor" / "CompositionSources",
             fs::copy_options::recursive);

    SagaPipeline::EditorCompositionBuildRequest request;
    request.projectRoot = projectRoot;
    request.toolExecutable = compiler.string();

    SagaPipeline::ProcessExternalToolRunner runner;
    const SagaPipeline::EditorCompositionBuildResult result =
        SagaPipeline::EditorCompositionStep::Run(request, runner);

    EXPECT_EQ(result.pipeline.exitCode, 0);
    EXPECT_TRUE(result.pipeline.diagnostics.empty());
    EXPECT_TRUE(FileExists(projectRoot / "Build" / "Manifests" /
                           "saga.editor.composition.manifest.json"));
}
#endif
