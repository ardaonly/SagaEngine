#include "DiligentGpuTestFixture.h"

using namespace SagaEngine::Render::Backend;
using namespace SagaEngine::Render::Scene;

TEST_F(DiligentGPU, DirectionalLightChangesSurfaceLuminance)
{
    const auto quad = SagaTests::Render::BuildQuadMeshAsset(1.0f, 0.0f);
    const auto mesh = m_Backend.CreateMesh(quad);
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle whiteTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        30u, {220u, 220u, 220u, 255u}, &whiteTex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    auto litView = MakeSingleDrawView(mesh, material);
    litView.lighting.directionalEnabled = true;
    litView.lighting.directional.direction = {0.0f, 0.0f, -1.0f};
    litView.lighting.directional.color = {1.0f, 1.0f, 1.0f};
    litView.lighting.directional.intensity = 1.0f;
    litView.lighting.ambient.intensity = 0.02f;

    auto darkView = litView;
    darkView.lighting.directional.direction = {0.0f, 0.0f, 1.0f};

    const auto camera = SagaTests::Render::MakeCamera();
    const auto litCapture = RenderAndCapture(camera, litView);
    const auto darkCapture = RenderAndCapture(camera, darkView);

    const float litLum = SagaTests::Render::AverageRegionLuminance(
        litCapture, litCapture.width / 2u, litCapture.height / 2u, 18u);
    const float darkLum = SagaTests::Render::AverageRegionLuminance(
        darkCapture, darkCapture.width / 2u, darkCapture.height / 2u, 18u);
    EXPECT_GT(litLum, darkLum + 80.0f);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(whiteTex);
    m_Backend.DestroyMesh(mesh);
}

TEST_F(DiligentGPU, VertexNormalsAffectLighting)
{
    auto facingQuad = SagaTests::Render::BuildQuadMeshAsset(
        0.32f, 0.0f, "NormalFacingQuad");
    auto grazingQuad = SagaTests::Render::BuildQuadMeshAsset(
        0.32f, 0.0f, "NormalGrazingQuad");
    for (auto& vertex : grazingQuad.lods[0].vertices)
        vertex.normal = {1.0f, 0.0f, 0.0f};

    const auto facingMesh = m_Backend.CreateMesh(facingQuad);
    const auto grazingMesh = m_Backend.CreateMesh(grazingQuad);
    ASSERT_NE(facingMesh, SagaEngine::Render::World::MeshId::kInvalid);
    ASSERT_NE(grazingMesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle whiteTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        31u, {230u, 230u, 230u, 255u}, &whiteTex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    RenderView view{};
    DrawItem facing{};
    facing.mesh = facingMesh;
    facing.material = material;
    facing.model = SagaEngine::Math::Mat4::FromTranslation({-0.45f, 0.0f, 0.0f});
    view.drawItems.push_back(facing);

    DrawItem grazing{};
    grazing.mesh = grazingMesh;
    grazing.material = material;
    grazing.model = SagaEngine::Math::Mat4::FromTranslation({0.45f, 0.0f, 0.0f});
    view.drawItems.push_back(grazing);

    view.lighting.directionalEnabled = true;
    view.lighting.directional.direction = {0.0f, 0.0f, -1.0f};
    view.lighting.directional.color = {1.0f, 1.0f, 1.0f};
    view.lighting.directional.intensity = 1.0f;
    view.lighting.ambient.intensity = 0.04f;

    const auto capture = RenderAndCapture(SagaTests::Render::MakeCamera(), view);

    const float facingLum = SagaTests::Render::AverageRegionLuminance(
        capture, capture.width / 2u - 36u, capture.height / 2u, 12u);
    const float grazingLum = SagaTests::Render::AverageRegionLuminance(
        capture, capture.width / 2u + 36u, capture.height / 2u, 12u);
    EXPECT_GT(facingLum, grazingLum + 80.0f);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(whiteTex);
    m_Backend.DestroyMesh(facingMesh);
    m_Backend.DestroyMesh(grazingMesh);
}

TEST_F(DiligentGPU, AmbientLightKeepsUnlitFacingSurfaceVisible)
{
    const auto quad = SagaTests::Render::BuildQuadMeshAsset(1.0f, 0.0f);
    const auto mesh = m_Backend.CreateMesh(quad);
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle whiteTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        32u, {200u, 200u, 200u, 255u}, &whiteTex,
        SagaEngine::Render::OpaqueShadingModel::LitDiffuse);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    auto noAmbientView = MakeSingleDrawView(mesh, material);
    noAmbientView.lighting.directionalEnabled = true;
    noAmbientView.lighting.directional.direction = {0.0f, 0.0f, 1.0f};
    noAmbientView.lighting.directional.intensity = 1.0f;
    noAmbientView.lighting.ambient.intensity = 0.0f;

    auto ambientView = noAmbientView;
    ambientView.lighting.ambient.color = {1.0f, 1.0f, 1.0f};
    ambientView.lighting.ambient.intensity = 0.35f;

    const auto camera = SagaTests::Render::MakeCamera();
    const auto noAmbientCapture = RenderAndCapture(camera, noAmbientView);
    const auto ambientCapture = RenderAndCapture(camera, ambientView);

    const float noAmbientLum = SagaTests::Render::AverageRegionLuminance(
        noAmbientCapture, noAmbientCapture.width / 2u, noAmbientCapture.height / 2u, 18u);
    const float ambientLum = SagaTests::Render::AverageRegionLuminance(
        ambientCapture, ambientCapture.width / 2u, ambientCapture.height / 2u, 18u);
    EXPECT_GT(ambientLum, noAmbientLum + 45.0f);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(whiteTex);
    m_Backend.DestroyMesh(mesh);
}

TEST_F(DiligentGPU, UnlitMaterialIgnoresDirectionalLight)
{
    const auto quad = SagaTests::Render::BuildQuadMeshAsset(1.0f, 0.0f);
    const auto mesh = m_Backend.CreateMesh(quad);
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle blueTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        33u, {50u, 80u, 240u, 255u}, &blueTex,
        SagaEngine::Render::OpaqueShadingModel::Unlit);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    auto viewA = MakeSingleDrawView(mesh, material);
    viewA.lighting.directionalEnabled = true;
    viewA.lighting.directional.direction = {0.0f, 0.0f, -1.0f};
    viewA.lighting.directional.intensity = 2.0f;
    viewA.lighting.ambient.intensity = 0.0f;

    auto viewB = viewA;
    viewB.lighting.directional.direction = {0.0f, 0.0f, 1.0f};
    viewB.lighting.ambient.intensity = 1.0f;

    const auto camera = SagaTests::Render::MakeCamera();
    const auto captureA = RenderAndCapture(camera, viewA);
    const auto captureB = RenderAndCapture(camera, viewB);

    const float lumA = SagaTests::Render::AverageRegionLuminance(
        captureA, captureA.width / 2u, captureA.height / 2u, 18u);
    const float lumB = SagaTests::Render::AverageRegionLuminance(
        captureB, captureB.width / 2u, captureB.height / 2u, 18u);
    EXPECT_NEAR(lumA, lumB, 4.0f);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(blueTex);
    m_Backend.DestroyMesh(mesh);
}

