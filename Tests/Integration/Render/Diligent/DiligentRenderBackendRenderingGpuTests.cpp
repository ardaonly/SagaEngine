#include "DiligentGpuTestFixture.h"

#include <SDL.h>

using namespace SagaEngine::Render::Backend;
using namespace SagaEngine::Render::Scene;


namespace
{

SagaEngine::Render::MeshAsset BuildIndexedCubeMeshAsset()
{
    SagaEngine::Render::MeshAsset asset{};
    asset.meshId = 1;
    asset.debugName = "DiligentGPUIndexedCube";
    asset.lodCount = 1;
    asset.rootBounds.centre = {0.0f, 0.0f, 0.0f};
    asset.rootBounds.halfExtents = {0.5f, 0.5f, 0.5f};

    auto& lod = asset.lods[0];

    auto pushFace = [&](SagaEngine::Render::MeshVec3 p0,
                        SagaEngine::Render::MeshVec3 p1,
                        SagaEngine::Render::MeshVec3 p2,
                        SagaEngine::Render::MeshVec3 p3,
                        SagaEngine::Render::MeshVec3 normal,
                        SagaEngine::Render::MeshVec3 tangent)
    {
        const auto base = static_cast<std::uint32_t>(lod.vertices.size());

        auto makeVertex = [&](SagaEngine::Render::MeshVec3 position,
                              SagaEngine::Render::MeshVec2 uv)
        {
            SagaEngine::Render::MeshVertex vertex{};
            vertex.position = position;
            vertex.normal = normal;
            vertex.tangent = tangent;
            vertex.handedness = 1.0f;
            vertex.uv0 = uv;
            return vertex;
        };

        lod.vertices.push_back(makeVertex(p0, {0.0f, 1.0f}));
        lod.vertices.push_back(makeVertex(p1, {1.0f, 1.0f}));
        lod.vertices.push_back(makeVertex(p2, {1.0f, 0.0f}));
        lod.vertices.push_back(makeVertex(p3, {0.0f, 0.0f}));

        lod.indices.push_back(base);
        lod.indices.push_back(base + 1u);
        lod.indices.push_back(base + 2u);
        lod.indices.push_back(base);
        lod.indices.push_back(base + 2u);
        lod.indices.push_back(base + 3u);
    };

    constexpr float h = 0.5f;
    pushFace({-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h},
             {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f});
    pushFace({h, -h, -h}, {-h, -h, -h}, {-h, h, -h}, {h, h, -h},
             {0.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, 0.0f});
    pushFace({h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h},
             {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f});
    pushFace({-h, -h, -h}, {-h, -h, h}, {-h, h, h}, {-h, h, -h},
             {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
    pushFace({-h, h, h}, {h, h, h}, {h, h, -h}, {-h, h, -h},
             {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f});
    pushFace({-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h},
             {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f});

    lod.vertexCountHint = static_cast<std::uint32_t>(lod.vertices.size());
    lod.indexCountHint = static_cast<std::uint32_t>(lod.indices.size());
    lod.approxGpuBytes =
        static_cast<std::uint64_t>(lod.vertices.size()) *
            sizeof(SagaEngine::Render::MeshVertex) +
        static_cast<std::uint64_t>(lod.indices.size()) *
            sizeof(std::uint32_t);

    return asset;
}

SagaEngine::Render::Scene::Camera MakeIndexedCubeCamera()
{
    SagaEngine::Render::Scene::Camera camera{};
    camera.position = {0.0f, 0.0f, 3.0f};
    camera.view = SagaEngine::Math::Mat4::LookAtRH(
        camera.position,
        {0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f});
    camera.projection = SagaEngine::Math::Mat4::PerspectiveRH_ZO(
        1.0472f,
        16.0f / 9.0f,
        0.1f,
        100.0f);
    camera.RecomputeViewProj();
    camera.RecomputeFrustum();
    return camera;
}

} // namespace

TEST_F(DiligentGPU, IndexedCubeSubmitsAndPresents)
{
    const auto cube = BuildIndexedCubeMeshAsset();
    ASSERT_EQ(cube.lodCount, 1u);
    ASSERT_EQ(cube.lods[0].vertices.size(), 24u)
        << "deterministic indexed cube must provide 24 vertices";
    ASSERT_EQ(cube.lods[0].indices.size(), 36u)
        << "deterministic indexed cube must provide 36 indices";

    const auto mesh = m_Backend.CreateMesh(cube);
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid)
        << "mesh handle valid";

    SagaEngine::Render::MaterialRuntime material{};
    material.materialId = 1u;
    material.renderQueue = SagaEngine::Render::MaterialRenderQueue::Opaque;
    material.cullMode = SagaEngine::Render::MaterialCullMode::Back;
    material.writesDepth = true;

    const auto materialHandle = m_Backend.CreateMaterial(material);
    ASSERT_NE(materialHandle, SagaEngine::Render::World::MaterialId::kInvalid)
        << "material handle valid";

    const auto frameBefore = m_Backend.FrameIndex();

    RenderView view{};
    DrawItem item{};
    item.mesh = mesh;
    item.material = materialHandle;
    item.model = SagaEngine::Math::Mat4::Identity();
    view.drawItems.push_back(item);

    m_Backend.BeginFrame();
    m_Backend.Submit(MakeIndexedCubeCamera(), view);
    m_Backend.EndFrame();

    const auto diagnostics = m_Backend.LastFrameDiagnostics();
    EXPECT_EQ(m_Backend.FrameIndex(), frameBefore + 1u);
    EXPECT_EQ(diagnostics.submittedDrawItems, 1u)
        << "submittedDrawItems == 1";
    EXPECT_EQ(diagnostics.indexedDrawCalls, 1u)
        << "indexedDrawCalls == 1";
    EXPECT_EQ(diagnostics.nonIndexedDrawCalls, 0u)
        << "nonIndexedDrawCalls == 0";
    EXPECT_EQ(diagnostics.lastIndexedIndexCount, 36u)
        << "lastIndexedIndexCount == 36";
    EXPECT_EQ(diagnostics.presentCalls, 1u)
        << "presentCalls == 1";

    m_Backend.DestroyMaterial(materialHandle);
    m_Backend.DestroyMesh(mesh);
}

TEST_F(DiligentGPU, ClearFrameCanBeReadBack)
{
    m_Backend.BeginFrame();
    RenderFrameCapture capture{};
    const auto result = m_Backend.CaptureCurrentColorFrame(capture);
    m_Backend.EndFrame();

    ASSERT_EQ(result, RenderCaptureResult::kSuccess);
    ASSERT_EQ(capture.width, kWidth);
    ASSERT_EQ(capture.height, kHeight);
    ASSERT_EQ(capture.rowPitch, kWidth * 4u);
    ASSERT_EQ(capture.format, RenderPixelFormat::RGBA8_UNORM);
    ASSERT_EQ(capture.pixels.size(),
              static_cast<std::size_t>(kWidth) * kHeight * 4u);

    constexpr SagaTests::Render::Rgba8 kClear{0u, 0u, 0u, 255u};
    EXPECT_TRUE(SagaTests::Render::ColorNear(
        SagaTests::Render::PixelAt(capture, kWidth / 2u, kHeight / 2u),
        kClear, 3));
    EXPECT_TRUE(SagaTests::Render::ColorNear(
        SagaTests::Render::PixelAt(capture, 0u, 0u), kClear, 3));
}

TEST_F(DiligentGPU, IndexedCubeProducesNonClearPixels)
{
    const auto cube = SagaTests::Render::BuildIndexedCubeMeshAsset();
    const auto mesh = m_Backend.CreateMesh(cube);
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle blueTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        10u, {40u, 80u, 255u, 255u}, &blueTex);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    const auto capture = RenderAndCapture(
        SagaTests::Render::MakeCamera(),
        MakeSingleDrawView(mesh, material));

    constexpr SagaTests::Render::Rgba8 kClear{0u, 0u, 0u, 255u};
    EXPECT_GT(SagaTests::Render::CountPixelsNotNear(capture, kClear, 8), 500u);
    EXPECT_GT(SagaTests::Render::CountRegionMatching(
                  capture, capture.width / 2u, capture.height / 2u, 8u,
                  [](SagaTests::Render::Rgba8 c)
                  {
                      return SagaTests::Render::IsDominantBlue(c);
                  }),
              20u);

    const auto diagnostics = m_Backend.LastFrameDiagnostics();
    EXPECT_EQ(diagnostics.submittedDrawItems, 1u);
    EXPECT_EQ(diagnostics.indexedDrawCalls, 1u);
    EXPECT_EQ(diagnostics.presentCalls, 1u);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(blueTex);
    m_Backend.DestroyMesh(mesh);
}

TEST_F(DiligentGPU, UploadedTextureAffectsRenderedPixels)
{
    const auto quad = SagaTests::Render::BuildQuadMeshAsset(1.0f, 0.0f);
    const auto mesh = m_Backend.CreateMesh(quad);
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle redTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        11u, {255u, 20u, 20u, 255u}, &redTex);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    const auto capture = RenderAndCapture(
        SagaTests::Render::MakeCamera(),
        MakeSingleDrawView(mesh, material));

    EXPECT_GT(SagaTests::Render::CountRegionMatching(
                  capture, capture.width / 2u, capture.height / 2u, 16u,
                  [](SagaTests::Render::Rgba8 c)
                  {
                      return SagaTests::Render::IsDominantRed(c);
                  }),
              100u);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(redTex);
    m_Backend.DestroyMesh(mesh);
}

TEST_F(DiligentGPU, NearGeometryOccludesFarGeometry)
{
    const auto nearQuad = SagaTests::Render::BuildQuadMeshAsset(
        1.1f, 0.5f, "NearDepthQuad");
    const auto farQuad = SagaTests::Render::BuildQuadMeshAsset(
        1.1f, 0.0f, "FarDepthQuad");
    const auto nearMesh = m_Backend.CreateMesh(nearQuad);
    const auto farMesh = m_Backend.CreateMesh(farQuad);
    ASSERT_NE(nearMesh, SagaEngine::Render::World::MeshId::kInvalid);
    ASSERT_NE(farMesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle nearTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    SagaEngine::Render::TextureHandle farTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto nearMaterial = CreateSolidMaterial(
        12u, {30u, 255u, 30u, 255u}, &nearTex);
    const auto farMaterial = CreateSolidMaterial(
        13u, {255u, 30u, 30u, 255u}, &farTex);
    ASSERT_NE(nearMaterial, SagaEngine::Render::World::MaterialId::kInvalid);
    ASSERT_NE(farMaterial, SagaEngine::Render::World::MaterialId::kInvalid);

    RenderView view{};
    DrawItem nearItem{};
    nearItem.mesh = nearMesh;
    nearItem.material = nearMaterial;
    view.drawItems.push_back(nearItem);
    DrawItem farItem{};
    farItem.mesh = farMesh;
    farItem.material = farMaterial;
    view.drawItems.push_back(farItem);

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

TEST_F(DiligentGPU, ModelTransformChangesScreenCoverage)
{
    const auto cube = SagaTests::Render::BuildIndexedCubeMeshAsset();
    const auto mesh = m_Backend.CreateMesh(cube);
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle blueTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        14u, {60u, 90u, 255u, 255u}, &blueTex);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    const auto camera = SagaTests::Render::MakeCamera();
    constexpr SagaTests::Render::Rgba8 kClear{0u, 0u, 0u, 255u};
    const auto smallCapture = RenderAndCapture(
        camera,
        MakeSingleDrawView(
            mesh, material,
            SagaEngine::Math::Mat4::FromScale({0.55f, 0.55f, 0.55f})));
    const auto largeCapture = RenderAndCapture(
        camera,
        MakeSingleDrawView(
            mesh, material,
            SagaEngine::Math::Mat4::FromScale({1.25f, 1.25f, 1.25f})));

    const auto smallBounds =
        SagaTests::Render::FindNonClearBounds(smallCapture, kClear, 8);
    const auto largeBounds =
        SagaTests::Render::FindNonClearBounds(largeCapture, kClear, 8);

    ASSERT_TRUE(smallBounds.valid);
    ASSERT_TRUE(largeBounds.valid);
    EXPECT_GT(largeBounds.Area(), smallBounds.Area() * 2u);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(blueTex);
    m_Backend.DestroyMesh(mesh);
}

TEST_F(DiligentGPU, IndexedCubeRendersAcrossMultipleFrames)
{
    const auto cube = SagaTests::Render::BuildIndexedCubeMeshAsset();
    const auto mesh = m_Backend.CreateMesh(cube);
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle blueTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        15u, {40u, 80u, 255u, 255u}, &blueTex);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    const auto camera = SagaTests::Render::MakeCamera();
    const auto view = MakeSingleDrawView(mesh, material);
    constexpr SagaTests::Render::Rgba8 kClear{0u, 0u, 0u, 255u};

    for (int i = 0; i < 3; ++i)
    {
        const auto capture = RenderAndCapture(camera, view);
        EXPECT_GT(SagaTests::Render::CountPixelsNotNear(capture, kClear, 8),
                  500u);

        const auto diagnostics = m_Backend.LastFrameDiagnostics();
        EXPECT_EQ(diagnostics.submittedDrawItems, 1u);
        EXPECT_EQ(diagnostics.indexedDrawCalls, 1u);
        EXPECT_EQ(diagnostics.presentCalls, 1u);
    }

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(blueTex);
    m_Backend.DestroyMesh(mesh);
}

TEST_F(DiligentGPU, ResizeContinuesProducingValidPixels)
{
    SDL_SetWindowSize(m_Window, 400, 300);
    m_Backend.OnResize(400u, 300u);

    const auto cube = SagaTests::Render::BuildIndexedCubeMeshAsset();
    const auto mesh = m_Backend.CreateMesh(cube);
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle blueTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        16u, {50u, 80u, 255u, 255u}, &blueTex);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    const auto capture = RenderAndCapture(
        SagaTests::Render::MakeCamera(400.0f / 300.0f),
        MakeSingleDrawView(mesh, material));

    constexpr SagaTests::Render::Rgba8 kClear{0u, 0u, 0u, 255u};
    EXPECT_EQ(capture.width, 400u);
    EXPECT_EQ(capture.height, 300u);
    EXPECT_GT(SagaTests::Render::CountPixelsNotNear(capture, kClear, 8),
              500u);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyTexture(blueTex);
    m_Backend.DestroyMesh(mesh);
}

TEST_F(DiligentGPU, DestroyedResourcesAreRejectedBySubmission)
{
    const auto cube = SagaTests::Render::BuildIndexedCubeMeshAsset();
    const auto mesh = m_Backend.CreateMesh(cube);
    ASSERT_NE(mesh, SagaEngine::Render::World::MeshId::kInvalid);

    SagaEngine::Render::TextureHandle blueTex =
        SagaEngine::Render::TextureHandle::kInvalid;
    const auto material = CreateSolidMaterial(
        17u, {50u, 80u, 255u, 255u}, &blueTex);
    ASSERT_NE(material, SagaEngine::Render::World::MaterialId::kInvalid);

    m_Backend.DestroyMaterial(material);
    m_Backend.DestroyMesh(mesh);
    m_Backend.DestroyTexture(blueTex);

    const auto capture = RenderAndCapture(
        SagaTests::Render::MakeCamera(),
        MakeSingleDrawView(mesh, material));

    constexpr SagaTests::Render::Rgba8 kClear{0u, 0u, 0u, 255u};
    EXPECT_EQ(SagaTests::Render::CountPixelsNotNear(capture, kClear, 8), 0u);

    const auto diagnostics = m_Backend.LastFrameDiagnostics();
    EXPECT_EQ(diagnostics.submittedDrawItems, 0u);
    EXPECT_EQ(diagnostics.rejectedDrawItems, 1u);
    EXPECT_EQ(diagnostics.indexedDrawCalls, 0u);
    EXPECT_EQ(diagnostics.presentCalls, 1u);
}
