#include "DiligentGpuTestFixture.h"

using namespace SagaEngine::Render::Backend;
using namespace SagaEngine::Render::Scene;

#include <cstddef>
#include <vector>

namespace
{

SagaEngine::Render::MeshAsset BuildCoordinateTriangleMeshAsset(bool clockwise)
{
    SagaEngine::Render::MeshAsset asset{};
    asset.meshId = clockwise ? 301u : 300u;
    asset.debugName = clockwise
        ? "CoordinateClockwiseTriangle"
        : "CoordinateCounterClockwiseTriangle";
    asset.lodCount = 1u;
    asset.rootBounds.centre = {0.0f, 0.0f, 0.0f};
    asset.rootBounds.halfExtents = {0.85f, 0.85f, 0.0f};

    auto& lod = asset.lods[0];
    SagaEngine::Render::MeshVertex v0{}, v1{}, v2{};
    v0.position = {-0.8f, -0.8f, 0.0f};
    v1.position = {0.8f, -0.8f, 0.0f};
    v2.position = {0.0f, 0.8f, 0.0f};
    v0.normal = v1.normal = v2.normal = {0.0f, 0.0f, 1.0f};
    v0.tangent = v1.tangent = v2.tangent = {1.0f, 0.0f, 0.0f};
    v0.handedness = v1.handedness = v2.handedness = 1.0f;
    v0.uv0 = {0.0f, 1.0f};
    v1.uv0 = {1.0f, 1.0f};
    v2.uv0 = {0.5f, 0.0f};
    lod.vertices = {v0, v1, v2};
    lod.indices = clockwise
        ? std::vector<std::uint32_t>{0u, 2u, 1u}
        : std::vector<std::uint32_t>{0u, 1u, 2u};
    lod.vertexCountHint = 3u;
    lod.indexCountHint = 3u;
    lod.approxGpuBytes =
        sizeof(SagaEngine::Render::MeshVertex) * 3u +
        sizeof(std::uint32_t) * 3u;
    return asset;
}

std::vector<std::uint8_t> QuadrantTexturePixels()
{
    constexpr SagaTests::Render::Rgba8 kRed{230u, 20u, 20u, 255u};
    constexpr SagaTests::Render::Rgba8 kGreen{20u, 230u, 20u, 255u};
    constexpr SagaTests::Render::Rgba8 kBlue{20u, 20u, 230u, 255u};
    constexpr SagaTests::Render::Rgba8 kYellow{230u, 230u, 20u, 255u};

    std::vector<std::uint8_t> pixels(4u * 4u * 4u);
    auto write = [&](std::uint32_t x, std::uint32_t y,
                     SagaTests::Render::Rgba8 color)
    {
        const auto idx = (static_cast<std::size_t>(y) * 4u + x) * 4u;
        pixels[idx + 0u] = color.r;
        pixels[idx + 1u] = color.g;
        pixels[idx + 2u] = color.b;
        pixels[idx + 3u] = color.a;
    };

    for (std::uint32_t y = 0; y < 4u; ++y)
    {
        for (std::uint32_t x = 0; x < 4u; ++x)
        {
            if (y < 2u && x < 2u)
                write(x, y, kRed);
            else if (y < 2u)
                write(x, y, kGreen);
            else if (x < 2u)
                write(x, y, kBlue);
            else
                write(x, y, kYellow);
        }
    }
    return pixels;
}

bool IsDominantYellow(SagaTests::Render::Rgba8 c) noexcept
{
    return c.r >= 90u && c.g >= 90u && c.b < c.r - 25u && c.b < c.g - 25u;
}

} // namespace

TEST_F(CoordinateGPU, CounterClockwiseTriangleDraws)
{
    const auto mesh = m_Backend.CreateMesh(
        BuildCoordinateTriangleMeshAsset(false));
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle greenTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        60u, {20u, 230u, 20u, 255u}, &greenTex);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    const auto capture = RenderAndCapture(
        SagaTests::Render::MakeCamera(),
        MakeSingleDrawView(mesh, material));

    EXPECT_GT(SagaTests::Render::CountRegionMatching(
                  capture, capture.width / 2u, capture.height / 2u, 18u,
                  [](SagaTests::Render::Rgba8 c)
                  {
                      return SagaTests::Render::IsDominantGreen(c);
                  }),
              40u);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(greenTex);
    m_Backend.DestroyMesh(mesh);
}

TEST_F(CoordinateGPU, ClockwiseTriangleIsCulled)
{
    const auto mesh = m_Backend.CreateMesh(
        BuildCoordinateTriangleMeshAsset(true));
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle greenTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        61u, {20u, 230u, 20u, 255u}, &greenTex);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    const auto capture = RenderAndCapture(
        SagaTests::Render::MakeCamera(),
        MakeSingleDrawView(mesh, material));

    constexpr SagaTests::Render::Rgba8 kClear{0u, 0u, 0u, 255u};
    EXPECT_EQ(SagaTests::Render::CountPixelsNotNear(capture, kClear, 8), 0u);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(greenTex);
    m_Backend.DestroyMesh(mesh);
}

TEST_F(CoordinateGPU, NearGeometryOccludesFarGeometry)
{
    const auto nearQuad = SagaTests::Render::BuildQuadMeshAsset(
        1.0f, 0.5f, "CoordinateNearQuad");
    const auto farQuad = SagaTests::Render::BuildQuadMeshAsset(
        1.0f, 0.0f, "CoordinateFarQuad");
    const auto nearMesh = m_Backend.CreateMesh(nearQuad);
    const auto farMesh = m_Backend.CreateMesh(farQuad);
    ASSERT_NE(nearMesh, SagaEngine::Render::World::MeshId::kInvalid);
    ASSERT_NE(farMesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle nearTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    SagaEngine::Render::TextureHandle farTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto nearMaterial = CreateSolidMaterial(
        62u, {20u, 230u, 20u, 255u}, &nearTex);
    const auto farMaterial = CreateSolidMaterial(
        63u, {230u, 20u, 20u, 255u}, &farTex);
    ASSERT_NE(nearMaterial, SagaEngine::Render::World::MaterialId::kInvalid);
    ASSERT_NE(farMaterial, SagaEngine::Render::World::MaterialId::kInvalid);

    RenderView view{};
    DrawItem far{};
    far.mesh = farMesh;
    far.material = farMaterial;
    view.drawItems.push_back(far);
    DrawItem near{};
    near.mesh = nearMesh;
    near.material = nearMaterial;
    view.drawItems.push_back(near);

    const auto capture = RenderAndCapture(SagaTests::Render::MakeCamera(), view);
    EXPECT_GT(SagaTests::Render::CountRegionMatching(
                  capture, capture.width / 2u, capture.height / 2u, 10u,
                  [](SagaTests::Render::Rgba8 c)
                  {
                      return SagaTests::Render::IsDominantGreen(c);
                  }),
              50u);
    EXPECT_LT(SagaTests::Render::CountRegionMatching(
                  capture, capture.width / 2u, capture.height / 2u, 10u,
                  [](SagaTests::Render::Rgba8 c)
                  {
                      return SagaTests::Render::IsDominantRed(c);
                  }),
              5u);

    m_Backend.DestroyMaterial(nearMaterial);
    m_Backend.DestroyMaterial(farMaterial);
    m_Backend.DestroyTexture(nearTex);
    m_Backend.DestroyTexture(farTex);
    m_Backend.DestroyMesh(nearMesh);
    m_Backend.DestroyMesh(farMesh);
}

TEST_F(CoordinateGPU, TextureCornersMatchCanonicalUv)
{
    const auto quad = SagaTests::Render::BuildQuadMeshAsset(
        1.0f, 0.0f, "CoordinateUvQuad");
    const auto mesh = m_Backend.CreateMesh(quad);
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid);

    const auto pixels = QuadrantTexturePixels();
    const auto texture = m_Backend.CreateTexture(4u, 4u, pixels.data());
    ASSERT_NE(texture, SagaEngine::Render::TextureHandle::kInvalid);
    const auto material = m_Backend.CreateMaterial(
        SagaTests::Render::MakeMaterial(
            64u,
            texture,
            SagaEngine::Render::MaterialCullMode::None));
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    const auto capture = RenderAndCapture(
        SagaTests::Render::MakeCamera(),
        MakeSingleDrawView(mesh, material));

    const auto cx = capture.width / 2u;
    const auto cy = capture.height / 2u;
    EXPECT_GT(SagaTests::Render::CountRegionMatching(
                  capture, cx - 35u, cy - 35u, 8u,
                  [](SagaTests::Render::Rgba8 c)
                  {
                      return SagaTests::Render::IsDominantRed(c);
                  }),
              20u);
    EXPECT_GT(SagaTests::Render::CountRegionMatching(
                  capture, cx + 35u, cy - 35u, 8u,
                  [](SagaTests::Render::Rgba8 c)
                  {
                      return SagaTests::Render::IsDominantGreen(c);
                  }),
              20u);
    EXPECT_GT(SagaTests::Render::CountRegionMatching(
                  capture, cx - 35u, cy + 35u, 8u,
                  [](SagaTests::Render::Rgba8 c)
                  {
                      return SagaTests::Render::IsDominantBlue(c);
                  }),
              20u);
    EXPECT_GT(SagaTests::Render::CountRegionMatching(
                  capture, cx + 35u, cy + 35u, 8u,
                  [](SagaTests::Render::Rgba8 c)
                  {
                      return IsDominantYellow(c);
                  }),
              20u);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(texture);
    m_Backend.DestroyMesh(mesh);
}

TEST_F(CoordinateGPU, ShadowProjectionMatchesMainConvention)
{
    const auto receiverMesh = m_Backend.CreateMesh(
        SagaTests::Render::BuildQuadMeshAsset(
            1.7f, 0.0f, "CoordinateShadowReceiver"));
    const auto occluderMesh = m_Backend.CreateMesh(
        SagaTests::Render::BuildIndexedCubeMeshAsset(
            "CoordinateShadowOccluder"));
    ASSERT_NE(receiverMesh, SagaEngine::Render::World::MeshId::kInvalid);
    ASSERT_NE(occluderMesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle tex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        65u, {215u, 215u, 215u, 255u}, &tex,
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
    const auto shadowCapture = RenderAndCapture(camera, makeView(true));
    const auto darker = SagaTests::Render::FindDarkerPixelCentroid(
        litCapture, shadowCapture, 18.0f);

    EXPECT_GT(darker.count, 80u);
    const auto diagnostics = m_Backend.LastFrameDiagnostics();
    EXPECT_EQ(diagnostics.shadowPassExecuted, 1u);
    EXPECT_EQ(diagnostics.shadowPassDrawCalls, 2u);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(tex);
    m_Backend.DestroyMesh(receiverMesh);
    m_Backend.DestroyMesh(occluderMesh);
}

TEST_F(CoordinateGPU, BackendAdjustmentIsNotDoubleApplied)
{
    const auto quad = SagaTests::Render::BuildQuadMeshAsset(
        0.32f, 0.0f, "CoordinateVerticalQuad");
    const auto mesh = m_Backend.CreateMesh(quad);
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle redTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    SagaEngine::Render::TextureHandle blueTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto redMaterial = CreateSolidMaterial(
        66u, {230u, 20u, 20u, 255u}, &redTex);
    const auto blueMaterial = CreateSolidMaterial(
        67u, {20u, 20u, 230u, 255u}, &blueTex);
    ASSERT_NE(redMaterial, SagaEngine::Render::World::MaterialId::kInvalid);
    ASSERT_NE(blueMaterial, SagaEngine::Render::World::MaterialId::kInvalid);

    RenderView view{};
    DrawItem top{};
    top.mesh = mesh;
    top.material = redMaterial;
    top.model = SagaEngine::Math::Mat4::FromTranslation(
        {0.0f, 0.55f, 0.0f});
    view.drawItems.push_back(top);

    DrawItem bottom{};
    bottom.mesh = mesh;
    bottom.material = blueMaterial;
    bottom.model = SagaEngine::Math::Mat4::FromTranslation(
        {0.0f, -0.55f, 0.0f});
    view.drawItems.push_back(bottom);

    const auto capture = RenderAndCapture(SagaTests::Render::MakeCamera(), view);
    EXPECT_GT(SagaTests::Render::CountRegionMatching(
                  capture, capture.width / 2u, capture.height / 2u - 38u, 10u,
                  [](SagaTests::Render::Rgba8 c)
                  {
                      return SagaTests::Render::IsDominantRed(c);
                  }),
              30u);
    EXPECT_GT(SagaTests::Render::CountRegionMatching(
                  capture, capture.width / 2u, capture.height / 2u + 38u, 10u,
                  [](SagaTests::Render::Rgba8 c)
                  {
                      return SagaTests::Render::IsDominantBlue(c);
                  }),
              30u);

    m_Backend.DestroyMaterial(redMaterial);
    m_Backend.DestroyMaterial(blueMaterial);
    m_Backend.DestroyTexture(redTex);
    m_Backend.DestroyTexture(blueTex);
    m_Backend.DestroyMesh(mesh);
}

// ═══════════════════════════════════════════════════════════════════════
//  1. Init verification
// ═══════════════════════════════════════════════════════════════════════

