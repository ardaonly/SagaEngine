#include "DiligentGpuTestFixture.h"

#include <SDL.h>

using namespace SagaEngine::Render::Backend;
using namespace SagaEngine::Render::Scene;

TEST_F(DiligentGPU, OccluderCastsShadowOnReceiver)
{
    const auto receiverMesh = m_Backend.CreateMesh(
        SagaTests::Render::BuildQuadMeshAsset(1.7f, 0.0f, "ShadowReceiver"));
    const auto occluderMesh = m_Backend.CreateMesh(
        SagaTests::Render::BuildIndexedCubeMeshAsset("ShadowOccluder"));
    ASSERT_NE(receiverMesh, SagaEngine::Render::World::MeshId::kInvalid);
    ASSERT_NE(occluderMesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle groundTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    SagaEngine::Render::TextureHandle cubeTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto groundMat = CreateSolidMaterial(
        44u, {210u, 210u, 210u, 255u}, &groundTex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    const auto cubeMat = CreateSolidMaterial(
        45u, {180u, 180u, 180u, 255u}, &cubeTex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    ASSERT_NE(groundMat, SagaEngine::Render::World::MaterialId::kInvalid);
    ASSERT_NE(cubeMat, SagaEngine::Render::World::MaterialId::kInvalid);

    RenderView view{};
    view.lighting.directionalEnabled = true;
    view.lighting.shadowsEnabled = true;
    view.lighting.directional.direction = {0.35f, -0.15f, -1.0f};
    view.lighting.directional.intensity = 1.0f;
    view.lighting.ambient.intensity = 0.25f;

    DrawItem receiver{};
    receiver.mesh = receiverMesh;
    receiver.material = groundMat;
    view.drawItems.push_back(receiver);

    DrawItem occluder{};
    occluder.mesh = occluderMesh;
    occluder.material = cubeMat;
    occluder.model =
        SagaEngine::Math::Mat4::FromTranslation({0.0f, 0.0f, 0.65f}) *
        SagaEngine::Math::Mat4::FromScale({0.35f, 0.35f, 0.35f});
    view.drawItems.push_back(occluder);

    auto noShadowView = view;
    noShadowView.lighting.shadowsEnabled = false;

    const auto camera = SagaTests::Render::MakeCamera();
    const auto litCapture = RenderAndCapture(camera, noShadowView);
    const auto shadowCapture = RenderAndCapture(camera, view);
    const auto darker = SagaTests::Render::FindDarkerPixelCentroid(
        litCapture, shadowCapture, 18.0f);

    EXPECT_GT(darker.count, 80u);
    const auto diagnostics = m_Backend.LastFrameDiagnostics();
    EXPECT_EQ(diagnostics.shadowPassExecuted, 1u);
    EXPECT_EQ(diagnostics.shadowPassDrawCalls, 2u);

    m_Backend.DestroyMaterial(groundMat);
    m_Backend.DestroyMaterial(cubeMat);
    m_Backend.DestroyTexture(groundTex);
    m_Backend.DestroyTexture(cubeTex);
    m_Backend.DestroyMesh(receiverMesh);
    m_Backend.DestroyMesh(occluderMesh);
}

TEST_F(DiligentGPU, DisablingShadowsRestoresReceiverLuminance)
{
    const auto receiverMesh = m_Backend.CreateMesh(
        SagaTests::Render::BuildQuadMeshAsset(1.7f, 0.0f, "ShadowReceiverToggle"));
    const auto occluderMesh = m_Backend.CreateMesh(
        SagaTests::Render::BuildIndexedCubeMeshAsset("ShadowOccluderToggle"));
    ASSERT_NE(receiverMesh, SagaEngine::Render::World::MeshId::kInvalid);
    ASSERT_NE(occluderMesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle groundTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    SagaEngine::Render::TextureHandle cubeTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto groundMat = CreateSolidMaterial(
        46u, {210u, 210u, 210u, 255u}, &groundTex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    const auto cubeMat = CreateSolidMaterial(
        146u, {180u, 180u, 180u, 255u}, &cubeTex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    ASSERT_NE(groundMat, SagaEngine::Render::World::MaterialId::kInvalid);
    ASSERT_NE(cubeMat, SagaEngine::Render::World::MaterialId::kInvalid);

    RenderView view{};
    view.lighting.directionalEnabled = true;
    view.lighting.shadowsEnabled = true;
    view.lighting.directional.direction = {0.35f, -0.15f, -1.0f};
    view.lighting.directional.intensity = 1.0f;
    view.lighting.ambient.intensity = 0.25f;
    DrawItem receiver{};
    receiver.mesh = receiverMesh;
    receiver.material = groundMat;
    view.drawItems.push_back(receiver);
    DrawItem occluder{};
    occluder.mesh = occluderMesh;
    occluder.material = cubeMat;
    occluder.model =
        SagaEngine::Math::Mat4::FromTranslation({0.0f, 0.0f, 0.65f}) *
        SagaEngine::Math::Mat4::FromScale({0.35f, 0.35f, 0.35f});
    view.drawItems.push_back(occluder);

    auto noShadowView = view;
    noShadowView.lighting.shadowsEnabled = false;
    const auto camera = SagaTests::Render::MakeCamera();
    const auto litCapture = RenderAndCapture(camera, noShadowView);
    const auto noShadowDiagnostics = m_Backend.LastFrameDiagnostics();
    const auto shadowCapture = RenderAndCapture(camera, view);
    const auto shadowDiagnostics = m_Backend.LastFrameDiagnostics();
    const auto darker = SagaTests::Render::FindDarkerPixelCentroid(
        litCapture, shadowCapture, 18.0f);
    EXPECT_GT(darker.count, 80u);
    EXPECT_EQ(noShadowDiagnostics.shadowPassExecuted, 0u);
    EXPECT_EQ(shadowDiagnostics.shadowPassExecuted, 1u);

    m_Backend.DestroyMaterial(groundMat);
    m_Backend.DestroyMaterial(cubeMat);
    m_Backend.DestroyTexture(groundTex);
    m_Backend.DestroyTexture(cubeTex);
    m_Backend.DestroyMesh(receiverMesh);
    m_Backend.DestroyMesh(occluderMesh);
}

TEST_F(DiligentGPU, ShadowResultIsIndependentOfMainPassDrawOrder)
{
    const auto receiverMesh = m_Backend.CreateMesh(
        SagaTests::Render::BuildQuadMeshAsset(1.7f, 0.0f, "ShadowReceiverOrder"));
    const auto occluderMesh = m_Backend.CreateMesh(
        SagaTests::Render::BuildIndexedCubeMeshAsset("ShadowOccluderOrder"));
    ASSERT_NE(receiverMesh, SagaEngine::Render::World::MeshId::kInvalid);
    ASSERT_NE(occluderMesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle tex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        47u, {210u, 210u, 210u, 255u}, &tex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    auto makeView = [&](bool cubeFirst, bool shadows)
    {
        RenderView view{};
        view.lighting.directionalEnabled = true;
        view.lighting.shadowsEnabled = shadows;
        view.lighting.directional.direction = {0.35f, -0.15f, -1.0f};
        view.lighting.directional.intensity = 1.0f;
        view.lighting.ambient.intensity = 0.25f;

        DrawItem receiver{};
        receiver.mesh = receiverMesh;
        receiver.material = material;
        DrawItem occluder{};
        occluder.mesh = occluderMesh;
        occluder.material = material;
        occluder.model =
            SagaEngine::Math::Mat4::FromTranslation({0.0f, 0.0f, 0.65f}) *
            SagaEngine::Math::Mat4::FromScale({0.35f, 0.35f, 0.35f});

        if (cubeFirst)
        {
            view.drawItems.push_back(occluder);
            view.drawItems.push_back(receiver);
        }
        else
        {
            view.drawItems.push_back(receiver);
            view.drawItems.push_back(occluder);
        }
        return view;
    };

    const auto camera = SagaTests::Render::MakeCamera();
    const auto litA = RenderAndCapture(camera, makeView(false, false));
    const auto shadowA = RenderAndCapture(camera, makeView(false, true));
    const auto litB = RenderAndCapture(camera, makeView(true, false));
    const auto shadowB = RenderAndCapture(camera, makeView(true, true));
    const auto darkerA = SagaTests::Render::FindDarkerPixelCentroid(
        litA, shadowA, 18.0f);
    const auto darkerB = SagaTests::Render::FindDarkerPixelCentroid(
        litB, shadowB, 18.0f);
    EXPECT_GT(darkerA.count, 50u);
    EXPECT_GT(darkerB.count, 50u);
    EXPECT_NEAR(static_cast<float>(darkerA.count),
                static_cast<float>(darkerB.count), 1200.0f);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(tex);
    m_Backend.DestroyMesh(receiverMesh);
    m_Backend.DestroyMesh(occluderMesh);
}

TEST_F(DiligentGPU, MovingOccluderMovesProjectedShadow)
{
    const auto receiverMesh = m_Backend.CreateMesh(
        SagaTests::Render::BuildQuadMeshAsset(1.9f, 0.0f, "ShadowReceiverMoving"));
    const auto occluderMesh = m_Backend.CreateMesh(
        SagaTests::Render::BuildIndexedCubeMeshAsset("ShadowOccluderMoving"));
    ASSERT_NE(receiverMesh, SagaEngine::Render::World::MeshId::kInvalid);
    ASSERT_NE(occluderMesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle tex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        48u, {215u, 215u, 215u, 255u}, &tex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    auto makeView = [&](float x, bool shadows)
    {
        RenderView view{};
        view.lighting.directionalEnabled = true;
        view.lighting.shadowsEnabled = shadows;
        view.lighting.directional.direction = {0.35f, -0.15f, -1.0f};
        view.lighting.directional.intensity = 1.0f;
        view.lighting.ambient.intensity = 0.25f;
        DrawItem receiver{};
        receiver.mesh = receiverMesh;
        receiver.material = material;
        view.drawItems.push_back(receiver);
        DrawItem occluder{};
        occluder.mesh = occluderMesh;
        occluder.material = material;
        occluder.model =
            SagaEngine::Math::Mat4::FromTranslation({x, 0.0f, 0.65f}) *
            SagaEngine::Math::Mat4::FromScale({0.35f, 0.35f, 0.35f});
        view.drawItems.push_back(occluder);
        return view;
    };

    const auto camera = SagaTests::Render::MakeCamera();
    const auto leftLit = RenderAndCapture(camera, makeView(-0.45f, false));
    const auto leftShadow = RenderAndCapture(camera, makeView(-0.45f, true));
    const auto rightLit = RenderAndCapture(camera, makeView(0.45f, false));
    const auto rightShadow = RenderAndCapture(camera, makeView(0.45f, true));

    const auto leftCentroid = SagaTests::Render::FindDarkerPixelCentroid(
        leftLit, leftShadow, 18.0f);
    const auto rightCentroid = SagaTests::Render::FindDarkerPixelCentroid(
        rightLit, rightShadow, 18.0f);
    EXPECT_GT(leftCentroid.count, 50u);
    EXPECT_GT(rightCentroid.count, 50u);
    EXPECT_GT(rightCentroid.x, leftCentroid.x + 20.0f);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(tex);
    m_Backend.DestroyMesh(receiverMesh);
    m_Backend.DestroyMesh(occluderMesh);
}

TEST_F(DiligentGPU, ShadowMapOutsideProjectionRemainsLit)
{
    const auto quad = SagaTests::Render::BuildQuadMeshAsset(1.0f, 0.0f);
    const auto mesh = m_Backend.CreateMesh(quad);
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle tex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        49u, {220u, 220u, 220u, 255u}, &tex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    auto view = MakeSingleDrawView(mesh, material);
    view.lighting.directionalEnabled = true;
    view.lighting.shadowsEnabled = true;
    view.lighting.directional.direction = {0.0f, 0.0f, -1.0f};
    view.lighting.directional.intensity = 1.0f;
    view.lighting.ambient.intensity = 0.15f;

    const auto capture = RenderAndCapture(SagaTests::Render::MakeCamera(), view);
    EXPECT_GT(SagaTests::Render::AverageRegionLuminance(
                  capture, capture.width / 2u, capture.height / 2u, 18u),
              120.0f);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(tex);
    m_Backend.DestroyMesh(mesh);
}

TEST_F(DiligentGPU, ShadowPcfConfigPropagatesConfiguredResolution)
{
    const auto cube = SagaTests::Render::BuildIndexedCubeMeshAsset();
    const auto mesh = m_Backend.CreateMesh(cube);
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle tex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        50u, {220u, 220u, 220u, 255u}, &tex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    auto view = MakeSingleDrawView(mesh, material);
    view.lighting.directionalEnabled = true;
    view.lighting.shadowsEnabled = true;
    view.lighting.directional.direction = {0.2f, -0.4f, -1.0f};
    view.lighting.ambient.intensity = 0.2f;

    (void)RenderAndCapture(SagaTests::Render::MakeCamera(), view);
    const auto diagnostics = m_Backend.LastFrameDiagnostics();
    EXPECT_EQ(diagnostics.shadowMapWidth, 1024u);
    EXPECT_EQ(diagnostics.shadowMapHeight, 1024u);
    EXPECT_EQ(diagnostics.shadowSamplingEnabled, 1u);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(tex);
    m_Backend.DestroyMesh(mesh);
}

TEST_F(DiligentGPU, LightingAndShadowsRemainValidAcrossMultipleFrames)
{
    const auto receiverMesh = m_Backend.CreateMesh(
        SagaTests::Render::BuildQuadMeshAsset(1.7f, 0.0f, "ShadowReceiverMulti"));
    const auto occluderMesh = m_Backend.CreateMesh(
        SagaTests::Render::BuildIndexedCubeMeshAsset("ShadowOccluderMulti"));
    ASSERT_NE(receiverMesh, SagaEngine::Render::World::MeshId::kInvalid);
    ASSERT_NE(occluderMesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle tex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        51u, {210u, 210u, 210u, 255u}, &tex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    auto makeView = [&](bool shadows)
    {
        RenderView view{};
        view.lighting.directionalEnabled = true;
        view.lighting.shadowsEnabled = shadows;
        view.lighting.directional.direction = {0.35f, -0.15f, -1.0f};
        view.lighting.directional.intensity = 1.0f;
        view.lighting.ambient.intensity = 0.25f;

        DrawItem receiver{};
        receiver.mesh = receiverMesh;
        receiver.material = material;
        view.drawItems.push_back(receiver);

        DrawItem occluder{};
        occluder.mesh = occluderMesh;
        occluder.material = material;
        occluder.model =
            SagaEngine::Math::Mat4::FromTranslation({0.0f, 0.0f, 0.65f}) *
            SagaEngine::Math::Mat4::FromScale({0.35f, 0.35f, 0.35f});
        view.drawItems.push_back(occluder);
        return view;
    };

    const auto camera = SagaTests::Render::MakeCamera();
    const auto litCapture = RenderAndCapture(camera, makeView(false));

    for (int frame = 0; frame < 3; ++frame)
    {
        const auto shadowCapture = RenderAndCapture(camera, makeView(true));
        const auto darker = SagaTests::Render::FindDarkerPixelCentroid(
            litCapture, shadowCapture, 18.0f);
        EXPECT_GT(darker.count, 80u);

        const auto diagnostics = m_Backend.LastFrameDiagnostics();
        EXPECT_EQ(diagnostics.shadowPassExecuted, 1u);
        EXPECT_EQ(diagnostics.shadowPassDrawCalls, 2u);
        EXPECT_EQ(diagnostics.mainPassDrawCalls, 2u);
        EXPECT_EQ(diagnostics.presentCalls, 1u);
        EXPECT_EQ(diagnostics.shadowResourceRecreationCount, 0u);
        if (frame == 0)
            EXPECT_EQ(diagnostics.shadowResourceCreationCount, 1u);
        else
            EXPECT_EQ(diagnostics.shadowResourceCreationCount, 0u);
    }

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(tex);
    m_Backend.DestroyMesh(receiverMesh);
    m_Backend.DestroyMesh(occluderMesh);
}

TEST_F(DiligentGPU, ResizePreservesLightingAndShadows)
{
    const auto receiverMesh = m_Backend.CreateMesh(
        SagaTests::Render::BuildQuadMeshAsset(1.7f, 0.0f, "ShadowReceiverResize"));
    const auto occluderMesh = m_Backend.CreateMesh(
        SagaTests::Render::BuildIndexedCubeMeshAsset("ShadowOccluderResize"));
    ASSERT_NE(receiverMesh, SagaEngine::Render::World::MeshId::kInvalid);
    ASSERT_NE(occluderMesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle tex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        52u, {210u, 210u, 210u, 255u}, &tex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    auto makeView = [&](bool shadows)
    {
        RenderView view{};
        view.lighting.directionalEnabled = true;
        view.lighting.shadowsEnabled = shadows;
        view.lighting.directional.direction = {0.35f, -0.15f, -1.0f};
        view.lighting.directional.intensity = 1.0f;
        view.lighting.ambient.intensity = 0.25f;

        DrawItem receiver{};
        receiver.mesh = receiverMesh;
        receiver.material = material;
        view.drawItems.push_back(receiver);

        DrawItem occluder{};
        occluder.mesh = occluderMesh;
        occluder.material = material;
        occluder.model =
            SagaEngine::Math::Mat4::FromTranslation({0.0f, 0.0f, 0.65f}) *
            SagaEngine::Math::Mat4::FromScale({0.35f, 0.35f, 0.35f});
        view.drawItems.push_back(occluder);
        return view;
    };

    const auto camera = SagaTests::Render::MakeCamera();
    (void)RenderAndCapture(camera, makeView(true));
    auto firstDiagnostics = m_Backend.LastFrameDiagnostics();
    EXPECT_EQ(firstDiagnostics.shadowResourceCreationCount, 1u);

    SDL_SetWindowSize(m_Window, 400, 300);
    m_Backend.OnResize(400u, 300u);

    const auto resizedCamera = SagaTests::Render::MakeCamera(400.0f / 300.0f);
    const auto litCapture = RenderAndCapture(resizedCamera, makeView(false));
    const auto shadowCapture = RenderAndCapture(resizedCamera, makeView(true));
    EXPECT_EQ(shadowCapture.width, 400u);
    EXPECT_EQ(shadowCapture.height, 300u);

    const auto darker = SagaTests::Render::FindDarkerPixelCentroid(
        litCapture, shadowCapture, 18.0f);
    EXPECT_GT(darker.count, 80u);

    const auto diagnostics = m_Backend.LastFrameDiagnostics();
    EXPECT_EQ(diagnostics.shadowPassExecuted, 1u);
    EXPECT_EQ(diagnostics.shadowResourceCreationCount, 0u);
    EXPECT_EQ(diagnostics.shadowResourceRecreationCount, 0u);
    EXPECT_EQ(diagnostics.shadowMapWidth, 1024u);
    EXPECT_EQ(diagnostics.shadowMapHeight, 1024u);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(tex);
    m_Backend.DestroyMesh(receiverMesh);
    m_Backend.DestroyMesh(occluderMesh);
}

TEST_F(DiligentGPU, ShadowToggleDoesNotLeakOrUseStaleBindings)
{
    const auto receiverMesh = m_Backend.CreateMesh(
        SagaTests::Render::BuildQuadMeshAsset(1.7f, 0.0f, "ShadowReceiverToggleStale"));
    const auto occluderMesh = m_Backend.CreateMesh(
        SagaTests::Render::BuildIndexedCubeMeshAsset("ShadowOccluderToggleStale"));
    ASSERT_NE(receiverMesh, SagaEngine::Render::World::MeshId::kInvalid);
    ASSERT_NE(occluderMesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle tex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        53u, {210u, 210u, 210u, 255u}, &tex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    auto makeView = [&](bool shadows)
    {
        RenderView view{};
        view.lighting.directionalEnabled = true;
        view.lighting.shadowsEnabled = shadows;
        view.lighting.directional.direction = {0.35f, -0.15f, -1.0f};
        view.lighting.directional.intensity = 1.0f;
        view.lighting.ambient.intensity = 0.25f;

        DrawItem receiver{};
        receiver.mesh = receiverMesh;
        receiver.material = material;
        view.drawItems.push_back(receiver);

        DrawItem occluder{};
        occluder.mesh = occluderMesh;
        occluder.material = material;
        occluder.model =
            SagaEngine::Math::Mat4::FromTranslation({0.0f, 0.0f, 0.65f}) *
            SagaEngine::Math::Mat4::FromScale({0.35f, 0.35f, 0.35f});
        view.drawItems.push_back(occluder);
        return view;
    };

    const auto camera = SagaTests::Render::MakeCamera();
    const auto enabledA = RenderAndCapture(camera, makeView(true));
    (void)enabledA;
    const auto disabled = RenderAndCapture(camera, makeView(false));
    const auto disabledDiagnostics = m_Backend.LastFrameDiagnostics();
    const auto enabledB = RenderAndCapture(camera, makeView(true));
    const auto enabledDiagnostics = m_Backend.LastFrameDiagnostics();

    const auto disabledVsShadowAgain = SagaTests::Render::FindDarkerPixelCentroid(
        disabled, enabledB, 18.0f);
    EXPECT_GT(disabledVsShadowAgain.count, 80u);
    EXPECT_EQ(disabledDiagnostics.shadowPassExecuted, 0u);
    EXPECT_EQ(disabledDiagnostics.shadowSamplingEnabled, 0u);
    EXPECT_EQ(enabledDiagnostics.shadowPassExecuted, 1u);
    EXPECT_EQ(enabledDiagnostics.shadowSamplingEnabled, 2u);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(tex);
    m_Backend.DestroyMesh(receiverMesh);
    m_Backend.DestroyMesh(occluderMesh);
}
