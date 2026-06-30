/// @file GraphicsDiligentBackendNativeShaderPipelineTests.cpp
/// @brief Focused Diligent graphics adapter tests.

#include "GraphicsDiligentBackendTestHelpers.h"

#include <gtest/gtest.h>

TEST(GraphicsDiligentBackendAdapter, NativeShaderRejectsEmptySource)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    Graphics::ShaderDesc shader{};
    shader.stage = Graphics::ShaderStage::Vertex;
    shader.entryPoint = "main";

    EXPECT_FALSE(backend->CreateShader(shader).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidShaderDesc);
}

TEST(GraphicsDiligentBackendAdapter, NativeShaderRejectsMissingEntryPoint)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    auto shader = MakeNativeShaderDesc();
    shader.entryPoint.clear();

    EXPECT_FALSE(backend->CreateShader(shader).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidShaderDesc);
}

TEST(GraphicsDiligentBackendAdapter, NativeShaderRejectsUnsupportedStage)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    auto shader = MakeNativeShaderDesc();
    shader.stage = Graphics::ShaderStage::Compute;

    EXPECT_FALSE(backend->CreateShader(shader).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidShaderDesc);
}

TEST(GraphicsDiligentBackendAdapter, NativeShaderQueryReportsNativeGpu)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    auto shaderDesc = MakeNativeShaderDesc();
    shaderDesc.debugName = "query-native-shader";
    const auto shader = backend->CreateShader(shaderDesc);
    ASSERT_TRUE(shader.IsValid());
    ASSERT_TRUE(backend->MarkShaderNativeForTesting(shader, 99u));

    const auto query = backend->QueryShaderForTesting(shader);
    EXPECT_TRUE(query.valid);
    EXPECT_EQ(query.lifecycle, Graphics::GraphicsResourceLifecycle::Ready);
    EXPECT_EQ(query.backing, Graphics::GraphicsResourceBacking::NativeGpu);
    EXPECT_TRUE(query.nativeBacked);
    EXPECT_TRUE(query.resident);
    EXPECT_EQ(query.debugName, "query-native-shader");
}

TEST(GraphicsDiligentBackendAdapter, NativePipelineRejectsMissingVertexShader)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));
    backend->BindNativeDeviceServicesForTesting(MakeFakeDeviceServices(), true);

    Graphics::PipelineDesc pipeline{};
    pipeline.fragmentShader.index = 1u;
    pipeline.fragmentShader.generation = 1u;

    EXPECT_FALSE(backend->CreatePipeline(pipeline).IsValid());
    EXPECT_EQ(
        backend->GetLastResourceFailure(),
        Graphics::GraphicsResourceFailure::InvalidPipelineDesc);
}

TEST(GraphicsDiligentBackendAdapter, NativePipelineQueryReportsNativeGpu)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    Graphics::PipelineDesc desc{};
    desc.debugName = "query-native-pipeline";
    const auto pipeline = backend->CreatePipeline(desc);
    ASSERT_TRUE(pipeline.IsValid());
    ASSERT_TRUE(backend->MarkPipelineNativeForTesting(pipeline, 100u));

    const auto query = backend->QueryPipelineForTesting(pipeline);
    EXPECT_TRUE(query.valid);
    EXPECT_EQ(query.lifecycle, Graphics::GraphicsResourceLifecycle::Ready);
    EXPECT_EQ(query.backing, Graphics::GraphicsResourceBacking::NativeGpu);
    EXPECT_TRUE(query.nativeBacked);
    EXPECT_TRUE(query.resident);
    EXPECT_EQ(query.debugName, "query-native-pipeline");
}

TEST(GraphicsDiligentBackendAdapter, DestroyedPipelineCannotResolve)
{
    FakeRenderState state;
    auto backend = MakeConcreteBackend(state);
    EXPECT_TRUE(backend->Initialize({}, MakeSwapchain()));

    const auto pipeline = backend->CreatePipeline({});
    ASSERT_TRUE(pipeline.IsValid());
    ASSERT_TRUE(backend->MarkPipelineNativeForTesting(pipeline, 101u));
    backend->DestroyPipeline(pipeline);

    EXPECT_FALSE(backend->QueryPipelineForTesting(pipeline).valid);
    EXPECT_EQ(backend->ResolveNativePipelineForTesting(pipeline), nullptr);
}
