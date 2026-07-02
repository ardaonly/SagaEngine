#include "DiligentGpuTestFixture.h"

using namespace SagaEngine::Render::Backend;
using namespace SagaEngine::Render::Scene;

TEST_F(DiligentGPU, ShadowPassSubmitsDepthDraws)
{
    const auto cube = SagaTests::Render::BuildIndexedCubeMeshAsset();
    const auto mesh = m_Backend.CreateMesh(cube);
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle tex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        40u, {220u, 220u, 220u, 255u}, &tex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    auto view = MakeSingleDrawView(mesh, material);
    view.lighting.directionalEnabled = true;
    view.lighting.shadowsEnabled = true;
    view.lighting.directional.direction = {0.25f, -0.35f, -1.0f};
    view.lighting.directional.intensity = 1.0f;
    view.lighting.ambient.intensity = 0.2f;

    const auto capture = RenderAndCapture(SagaTests::Render::MakeCamera(), view);
    EXPECT_GT(SagaTests::Render::CountPixelsNotNear(
                  capture, {0u, 0u, 0u, 255u}, 8), 500u);

    const auto diagnostics = m_Backend.LastFrameDiagnostics();
    EXPECT_EQ(diagnostics.shadowPassExecuted, 1u);
    EXPECT_EQ(diagnostics.shadowPassDrawCalls, 1u);
    EXPECT_EQ(diagnostics.mainPassDrawCalls, 1u);
    EXPECT_EQ(diagnostics.shadowMapWidth, 1024u);
    EXPECT_EQ(diagnostics.shadowMapHeight, 1024u);
    EXPECT_EQ(diagnostics.shadowSamplingEnabled, 1u);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(tex);
    m_Backend.DestroyMesh(mesh);
}

TEST_F(DiligentGPU, ShadowMapResourceIsPersistentAcrossFrames)
{
    const auto cube = SagaTests::Render::BuildIndexedCubeMeshAsset();
    const auto mesh = m_Backend.CreateMesh(cube);
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle tex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        41u, {220u, 220u, 220u, 255u}, &tex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    auto view = MakeSingleDrawView(mesh, material);
    view.lighting.directionalEnabled = true;
    view.lighting.shadowsEnabled = true;
    view.lighting.directional.direction = {0.25f, -0.35f, -1.0f};
    view.lighting.ambient.intensity = 0.2f;

    (void)RenderAndCapture(SagaTests::Render::MakeCamera(), view);
    auto first = m_Backend.LastFrameDiagnostics();
    EXPECT_EQ(first.shadowResourceCreationCount, 1u);
    EXPECT_EQ(first.shadowResourceRecreationCount, 0u);

    (void)RenderAndCapture(SagaTests::Render::MakeCamera(), view);
    auto second = m_Backend.LastFrameDiagnostics();
    EXPECT_EQ(second.shadowResourceCreationCount, 0u);
    EXPECT_EQ(second.shadowResourceRecreationCount, 0u);
    EXPECT_EQ(second.shadowPassExecuted, 1u);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(tex);
    m_Backend.DestroyMesh(mesh);
}

TEST_F(DiligentGPU, ShadowEnabledFrameRejectsAdditionalSubmit)
{
    const auto cube = SagaTests::Render::BuildIndexedCubeMeshAsset();
    const auto mesh = m_Backend.CreateMesh(cube);
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle tex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        42u, {220u, 220u, 220u, 255u}, &tex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    auto view = MakeSingleDrawView(mesh, material);
    view.lighting.directionalEnabled = true;
    view.lighting.shadowsEnabled = true;
    view.lighting.directional.direction = {0.25f, -0.35f, -1.0f};
    view.lighting.ambient.intensity = 0.2f;

    m_Backend.BeginFrame();
    m_Backend.Submit(SagaTests::Render::MakeCamera(), view);
    m_Backend.Submit(SagaTests::Render::MakeCamera(), view);
    m_Backend.EndFrame();

    const auto diagnostics = m_Backend.LastFrameDiagnostics();
    EXPECT_EQ(diagnostics.shadowPassExecuted, 1u);
    EXPECT_EQ(diagnostics.additionalFrameSubmitsRejected, 1u);
    EXPECT_EQ(diagnostics.mainPassDrawCalls, 1u);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(tex);
    m_Backend.DestroyMesh(mesh);
}

TEST_F(DiligentGPU, ShadowSamplingProducesValidatedPixels)
{
    const auto cube = SagaTests::Render::BuildIndexedCubeMeshAsset();
    const auto mesh = m_Backend.CreateMesh(cube);
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle tex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        43u, {220u, 220u, 220u, 255u}, &tex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    auto view = MakeSingleDrawView(mesh, material);
    view.lighting.directionalEnabled = true;
    view.lighting.shadowsEnabled = true;
    view.lighting.directional.direction = {0.2f, -0.4f, -1.0f};
    view.lighting.directional.intensity = 1.0f;
    view.lighting.ambient.intensity = 0.25f;

    const auto capture = RenderAndCapture(SagaTests::Render::MakeCamera(), view);
    EXPECT_GT(SagaTests::Render::AverageRegionLuminance(
                  capture, capture.width / 2u, capture.height / 2u, 18u),
              20.0f);
    const auto diagnostics = m_Backend.LastFrameDiagnostics();
    EXPECT_EQ(diagnostics.shadowSamplingEnabled, 1u);
    EXPECT_EQ(diagnostics.shadowPassDrawCalls, 1u);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(tex);
    m_Backend.DestroyMesh(mesh);
}

