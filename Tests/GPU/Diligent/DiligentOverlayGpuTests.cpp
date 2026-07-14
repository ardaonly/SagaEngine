#include "DiligentGpuTestFixture.h"

#include <limits>
#include <thread>
#include <utility>
#include <vector>

using namespace SagaEngine::Render::Backend;

class OverlayGPU : public DiligentGPU
{
protected:
    [[nodiscard]] RenderOverlayTextureHandle CreateOverlayTexture(
        SagaTests::Render::Rgba8 color)
    {
        const auto pixels = SagaTests::Render::SolidTexturePixels(color);
        RenderOverlayTextureDesc desc{};
        desc.width = 1u;
        desc.height = 1u;
        desc.rowPitchBytes = 4u;
        return CreateBackendOverlayTexture(
            m_Backend.BackendInterface(), desc, pixels.data());
    }

    [[nodiscard]] static RenderOverlayFrame MakeQuadFrame(
        RenderOverlayTextureHandle texture,
        float left,
        float top,
        float right,
        float bottom,
        std::uint32_t color = 0xFFFFFFFFu,
        RenderOverlayClipRect clip = {})
    {
        if (clip.right <= clip.left || clip.bottom <= clip.top)
        {
            clip.left = left;
            clip.top = top;
            clip.right = right;
            clip.bottom = bottom;
        }

        RenderOverlayDrawList list{};
        list.vertices = {
            {{left, top}, {0.0f, 0.0f}, color},
            {{right, top}, {1.0f, 0.0f}, color},
            {{right, bottom}, {1.0f, 1.0f}, color},
            {{left, bottom}, {0.0f, 1.0f}, color},
        };
        list.indices = {0u, 1u, 2u, 0u, 2u, 3u};
        RenderOverlayDrawCommand cmd{};
        cmd.texture = texture;
        cmd.clipRect = clip;
        cmd.elementCount = 6u;
        list.commands.push_back(cmd);

        RenderOverlayFrame frame{};
        frame.displaySize[0] = static_cast<float>(kWidth);
        frame.displaySize[1] = static_cast<float>(kHeight);
        frame.framebufferScale[0] = 1.0f;
        frame.framebufferScale[1] = 1.0f;
        frame.drawLists.push_back(std::move(list));
        return frame;
    }

    [[nodiscard]] RenderFrameCapture RenderOverlayAndCapture(
        const RenderOverlayFrame& frame)
    {
        RenderFrameCapture capture{};
        m_Backend.BeginFrame();
        RenderBackendOverlayFrame(m_Backend.BackendInterface(), frame);
        const auto result = m_Backend.CaptureCurrentColorFrame(capture);
        m_Backend.EndFrame();
        EXPECT_EQ(result, RenderCaptureResult::kSuccess);
        return capture;
    }

    [[nodiscard]] RenderFrameCapture RenderEmptyOverlayAndCapture()
    {
        RenderOverlayFrame frame{};
        frame.displaySize[0] = static_cast<float>(kWidth);
        frame.displaySize[1] = static_cast<float>(kHeight);
        frame.framebufferScale[0] = 1.0f;
        frame.framebufferScale[1] = 1.0f;
        return RenderOverlayAndCapture(frame);
    }
};

TEST_F(OverlayGPU, ColoredQuadProducesPixels)
{
    ASSERT_TRUE(InitBackendOverlayRendering(m_Backend.BackendInterface()));
    const auto white = CreateOverlayTexture({255u, 255u, 255u, 255u});
    ASSERT_TRUE(white);

    const auto frame = MakeQuadFrame(
        white, 80.0f, 60.0f, 240.0f, 180.0f, 0xFF0000FFu);
    const auto capture = RenderOverlayAndCapture(frame);

    EXPECT_GT(SagaTests::Render::CountRegionMatching(
                  capture, capture.width / 2u, capture.height / 2u, 24u,
                  [](SagaTests::Render::Rgba8 c)
                  {
                      return SagaTests::Render::IsDominantRed(c);
                  }),
              100u);
    DestroyBackendOverlayTexture(m_Backend.BackendInterface(), white);
}

TEST_F(OverlayGPU, TextureSampleProducesExpectedPixels)
{
    ASSERT_TRUE(InitBackendOverlayRendering(m_Backend.BackendInterface()));
    const auto green = CreateOverlayTexture({0u, 255u, 0u, 255u});
    ASSERT_TRUE(green);

    const auto frame = MakeQuadFrame(green, 96.0f, 72.0f, 224.0f, 168.0f);
    const auto capture = RenderOverlayAndCapture(frame);

    EXPECT_GT(SagaTests::Render::CountRegionMatching(
                  capture, capture.width / 2u, capture.height / 2u, 24u,
                  [](SagaTests::Render::Rgba8 c)
                  {
                      return SagaTests::Render::IsDominantGreen(c);
                  }),
              100u);
    DestroyBackendOverlayTexture(m_Backend.BackendInterface(), green);
}

TEST_F(OverlayGPU, AlphaBlendProducesExpectedPixels)
{
    ASSERT_TRUE(InitBackendOverlayRendering(m_Backend.BackendInterface()));
    const auto white = CreateOverlayTexture({255u, 255u, 255u, 255u});
    ASSERT_TRUE(white);

    const auto frame = MakeQuadFrame(
        white, 96.0f, 72.0f, 224.0f, 168.0f, 0x800000FFu);
    const auto capture = RenderOverlayAndCapture(frame);
    const auto center = SagaTests::Render::PixelAt(
        capture, capture.width / 2u, capture.height / 2u);

    EXPECT_GT(center.r, 80u);
    EXPECT_LT(center.r, 220u);
    EXPECT_LT(center.g, 40u);
    EXPECT_LT(center.b, 40u);
    DestroyBackendOverlayTexture(m_Backend.BackendInterface(), white);
}

TEST_F(OverlayGPU, ScissorClipsPixels)
{
    ASSERT_TRUE(InitBackendOverlayRendering(m_Backend.BackendInterface()));
    const auto white = CreateOverlayTexture({255u, 255u, 255u, 255u});
    ASSERT_TRUE(white);

    RenderOverlayClipRect clip{};
    clip.left = 80.0f;
    clip.top = 60.0f;
    clip.right = 160.0f;
    clip.bottom = 180.0f;
    const auto frame = MakeQuadFrame(
        white, 80.0f, 60.0f, 240.0f, 180.0f, 0xFFFF0000u, clip);
    const auto capture = RenderOverlayAndCapture(frame);

    EXPECT_TRUE(SagaTests::Render::IsDominantBlue(
        SagaTests::Render::PixelAt(capture, 120u, 120u)));
    EXPECT_TRUE(SagaTests::Render::ColorNear(
        SagaTests::Render::PixelAt(capture, 200u, 120u),
        {0u, 0u, 0u, 255u},
        8));
    DestroyBackendOverlayTexture(m_Backend.BackendInterface(), white);
}

TEST_F(OverlayGPU, MultipleDrawListsPreserveOrder)
{
    ASSERT_TRUE(InitBackendOverlayRendering(m_Backend.BackendInterface()));
    const auto white = CreateOverlayTexture({255u, 255u, 255u, 255u});
    ASSERT_TRUE(white);

    auto frame = MakeQuadFrame(
        white, 80.0f, 60.0f, 240.0f, 180.0f, 0xFF0000FFu);
    auto top = MakeQuadFrame(
        white, 112.0f, 84.0f, 208.0f, 156.0f, 0xFF00FF00u);
    frame.drawLists.push_back(std::move(top.drawLists.front()));
    const auto capture = RenderOverlayAndCapture(frame);

    EXPECT_TRUE(SagaTests::Render::IsDominantGreen(
        SagaTests::Render::PixelAt(capture, capture.width / 2u, capture.height / 2u)));
    DestroyBackendOverlayTexture(m_Backend.BackendInterface(), white);
}

TEST_F(OverlayGPU, VertexAndIndexOffsetsWork)
{
    ASSERT_TRUE(InitBackendOverlayRendering(m_Backend.BackendInterface()));
    const auto white = CreateOverlayTexture({255u, 255u, 255u, 255u});
    ASSERT_TRUE(white);

    auto frame = MakeQuadFrame(
        white, 90.0f, 70.0f, 230.0f, 170.0f, 0xFF00FF00u);
    auto& list = frame.drawLists.front();
    list.vertices.insert(
        list.vertices.begin(),
        RenderOverlayVertex{{0.0f, 0.0f}, {0.0f, 0.0f}, 0xFFFFFFFFu});
    list.indices.insert(list.indices.begin(), {0u, 0u, 0u});
    list.commands.front().indexOffset = 3u;
    list.commands.front().vertexOffset = 1u;
    const auto capture = RenderOverlayAndCapture(frame);

    EXPECT_GT(SagaTests::Render::CountRegionMatching(
                  capture, capture.width / 2u, capture.height / 2u, 20u,
                  [](SagaTests::Render::Rgba8 c)
                  {
                      return SagaTests::Render::IsDominantGreen(c);
                  }),
              80u);
    DestroyBackendOverlayTexture(m_Backend.BackendInterface(), white);
}

TEST_F(OverlayGPU, FramebufferScaleTransformsClipRect)
{
    ASSERT_TRUE(InitBackendOverlayRendering(m_Backend.BackendInterface()));
    const auto white = CreateOverlayTexture({255u, 255u, 255u, 255u});
    ASSERT_TRUE(white);

    auto frame = MakeQuadFrame(
        white, 40.0f, 30.0f, 120.0f, 90.0f, 0xFF0000FFu);
    frame.displaySize[0] = static_cast<float>(kWidth) / 2.0f;
    frame.displaySize[1] = static_cast<float>(kHeight) / 2.0f;
    frame.framebufferScale[0] = 2.0f;
    frame.framebufferScale[1] = 2.0f;
    const auto capture = RenderOverlayAndCapture(frame);

    EXPECT_TRUE(SagaTests::Render::IsDominantRed(
        SagaTests::Render::PixelAt(capture, 80u, 60u)));
    DestroyBackendOverlayTexture(m_Backend.BackendInterface(), white);
}

TEST_F(OverlayGPU, ResizePreservesOverlayRendering)
{
    ASSERT_TRUE(InitBackendOverlayRendering(m_Backend.BackendInterface()));
    const auto white = CreateOverlayTexture({255u, 255u, 255u, 255u});
    ASSERT_TRUE(white);

    m_Backend.OnResize(256u, 192u);
    auto frame = MakeQuadFrame(
        white, 64.0f, 48.0f, 192.0f, 144.0f, 0xFF0000FFu);
    frame.displaySize[0] = 256.0f;
    frame.displaySize[1] = 192.0f;
    const auto capture = RenderOverlayAndCapture(frame);

    EXPECT_TRUE(SagaTests::Render::IsDominantRed(
        SagaTests::Render::PixelAt(capture, capture.width / 2u, capture.height / 2u)));
    DestroyBackendOverlayTexture(m_Backend.BackendInterface(), white);
}

TEST_F(OverlayGPU, InvalidTextureHandleIsRejected)
{
    ASSERT_TRUE(InitBackendOverlayRendering(m_Backend.BackendInterface()));
    const auto frame = MakeQuadFrame(
        {}, 80.0f, 60.0f, 240.0f, 180.0f, 0xFF0000FFu);
    const auto capture = RenderOverlayAndCapture(frame);

    EXPECT_EQ(SagaTests::Render::CountPixelsNotNear(
                  capture, {0u, 0u, 0u, 255u}, 8),
              0u);
}

TEST_F(OverlayGPU, DestroyedTextureHandleIsRejected)
{
    ASSERT_TRUE(InitBackendOverlayRendering(m_Backend.BackendInterface()));
    const auto white = CreateOverlayTexture({255u, 255u, 255u, 255u});
    ASSERT_TRUE(white);
    DestroyBackendOverlayTexture(m_Backend.BackendInterface(), white);

    const auto frame = MakeQuadFrame(
        white, 80.0f, 60.0f, 240.0f, 180.0f, 0xFF0000FFu);
    const auto capture = RenderOverlayAndCapture(frame);

    EXPECT_EQ(SagaTests::Render::CountPixelsNotNear(
                  capture, {0u, 0u, 0u, 255u}, 8),
              0u);
}

TEST_F(OverlayGPU, StaleTextureHandleIsRejected)
{
    ASSERT_TRUE(InitBackendOverlayRendering(m_Backend.BackendInterface()));
    const auto stale = CreateOverlayTexture({255u, 255u, 255u, 255u});
    ASSERT_TRUE(stale);
    DestroyBackendOverlayTexture(m_Backend.BackendInterface(), stale);
    const auto replacement = CreateOverlayTexture({255u, 255u, 255u, 255u});
    ASSERT_TRUE(replacement);
    EXPECT_EQ(replacement.index, stale.index);
    EXPECT_NE(replacement.generation, stale.generation);

    const auto frame = MakeQuadFrame(
        stale, 80.0f, 60.0f, 240.0f, 180.0f, 0xFF0000FFu);
    const auto capture = RenderOverlayAndCapture(frame);

    EXPECT_EQ(SagaTests::Render::CountPixelsNotNear(
                  capture, {0u, 0u, 0u, 255u}, 8),
              0u);
    DestroyBackendOverlayTexture(m_Backend.BackendInterface(), replacement);
}

TEST_F(OverlayGPU, NonFiniteClipRectIsRejected)
{
    ASSERT_TRUE(InitBackendOverlayRendering(m_Backend.BackendInterface()));
    const auto white = CreateOverlayTexture({255u, 255u, 255u, 255u});
    ASSERT_TRUE(white);

    RenderOverlayClipRect clip{};
    clip.left = 80.0f;
    clip.top = 60.0f;
    clip.right = std::numeric_limits<float>::infinity();
    clip.bottom = std::numeric_limits<float>::quiet_NaN();
    const auto frame = MakeQuadFrame(
        white, 80.0f, 60.0f, 240.0f, 180.0f, 0xFF0000FFu, clip);
    const auto capture = RenderOverlayAndCapture(frame);

    EXPECT_EQ(SagaTests::Render::CountPixelsNotNear(
                  capture, {0u, 0u, 0u, 255u}, 8),
              0u);
    DestroyBackendOverlayTexture(m_Backend.BackendInterface(), white);
}

TEST_F(OverlayGPU, ScissorFramebufferEdgesAreInclusiveEnough)
{
    ASSERT_TRUE(InitBackendOverlayRendering(m_Backend.BackendInterface()));
    const auto white = CreateOverlayTexture({255u, 255u, 255u, 255u});
    ASSERT_TRUE(white);

    RenderOverlayClipRect clip{};
    clip.left = 0.0f;
    clip.top = 0.0f;
    clip.right = static_cast<float>(kWidth);
    clip.bottom = static_cast<float>(kHeight);
    const auto frame = MakeQuadFrame(
        white, 0.0f, 0.0f, static_cast<float>(kWidth),
        static_cast<float>(kHeight), 0xFF0000FFu, clip);
    const auto capture = RenderOverlayAndCapture(frame);

    EXPECT_TRUE(SagaTests::Render::IsDominantRed(
        SagaTests::Render::PixelAt(capture, 1u, 1u)));
    EXPECT_TRUE(SagaTests::Render::IsDominantRed(
        SagaTests::Render::PixelAt(capture, kWidth - 2u, kHeight - 2u)));
    DestroyBackendOverlayTexture(m_Backend.BackendInterface(), white);
}

TEST_F(OverlayGPU, FractionalDpiScaledClipBoundariesRenderExpectedPixels)
{
    ASSERT_TRUE(InitBackendOverlayRendering(m_Backend.BackendInterface()));
    const auto white = CreateOverlayTexture({255u, 255u, 255u, 255u});
    ASSERT_TRUE(white);

    RenderOverlayClipRect clip{};
    clip.left = 10.25f;
    clip.top = 8.5f;
    clip.right = 40.75f;
    clip.bottom = 32.5f;
    auto frame = MakeQuadFrame(
        white, 10.0f, 8.0f, 42.0f, 34.0f, 0xFF00FF00u, clip);
    frame.displaySize[0] = static_cast<float>(kWidth) / 1.5f;
    frame.displaySize[1] = static_cast<float>(kHeight) / 1.5f;
    frame.framebufferScale[0] = 1.5f;
    frame.framebufferScale[1] = 1.5f;
    const auto capture = RenderOverlayAndCapture(frame);

    EXPECT_TRUE(SagaTests::Render::IsDominantGreen(
        SagaTests::Render::PixelAt(capture, 30u, 24u)));
    EXPECT_TRUE(SagaTests::Render::ColorNear(
        SagaTests::Render::PixelAt(capture, 70u, 55u),
        {0u, 0u, 0u, 255u},
        8));
    DestroyBackendOverlayTexture(m_Backend.BackendInterface(), white);
}

TEST_F(OverlayGPU, MultiFrameTextureLifetimeStress)
{
    ASSERT_TRUE(InitBackendOverlayRendering(m_Backend.BackendInterface()));

    std::vector<RenderOverlayTextureHandle> staleHandles;
    staleHandles.reserve(9u);
    for (std::uint32_t i = 0u; i < 9u; ++i)
    {
        const SagaTests::Render::Rgba8 color{
            static_cast<std::uint8_t>(40u + i * 20u),
            static_cast<std::uint8_t>(220u - i * 12u),
            40u,
            255u,
        };
        const auto texture = CreateOverlayTexture(color);
        ASSERT_TRUE(texture);

        const auto frame = MakeQuadFrame(
            texture, 96.0f, 72.0f, 224.0f, 168.0f);
        const auto capture = RenderOverlayAndCapture(frame);
        EXPECT_GT(SagaTests::Render::CountPixelsNotNear(
                      capture, {0u, 0u, 0u, 255u}, 8),
                  500u);

        DestroyBackendOverlayTexture(m_Backend.BackendInterface(), texture);
        staleHandles.push_back(texture);

        const auto replacement =
            CreateOverlayTexture({255u, 255u, 255u, 255u});
        ASSERT_TRUE(replacement);
        auto staleFrame = MakeQuadFrame(
            texture, 96.0f, 72.0f, 224.0f, 168.0f, 0xFF0000FFu);
        const auto staleCapture = RenderOverlayAndCapture(staleFrame);
        EXPECT_EQ(SagaTests::Render::CountPixelsNotNear(
                      staleCapture, {0u, 0u, 0u, 255u}, 8),
                  0u);

        const auto replacementFrame = MakeQuadFrame(
            replacement, 96.0f, 72.0f, 224.0f, 168.0f, 0xFF0000FFu);
        const auto replacementCapture =
            RenderOverlayAndCapture(replacementFrame);
        EXPECT_TRUE(SagaTests::Render::IsDominantRed(
            SagaTests::Render::PixelAt(
                replacementCapture,
                replacementCapture.width / 2u,
                replacementCapture.height / 2u)));
        DestroyBackendOverlayTexture(
            m_Backend.BackendInterface(), replacement);
    }

    for (const auto stale : staleHandles)
    {
        const auto frame = MakeQuadFrame(
            stale, 96.0f, 72.0f, 224.0f, 168.0f, 0xFF0000FFu);
        const auto capture = RenderOverlayAndCapture(frame);
        EXPECT_EQ(SagaTests::Render::CountPixelsNotNear(
                      capture, {0u, 0u, 0u, 255u}, 8),
                  0u);
    }
}

TEST_F(OverlayGPU, WrongThreadOverlayCallsAreRejectedWithoutRegistryCorruption)
{
    ASSERT_TRUE(InitBackendOverlayRendering(m_Backend.BackendInterface()));
    const auto white = CreateOverlayTexture({255u, 255u, 255u, 255u});
    ASSERT_TRUE(white);

    RenderOverlayTextureHandle wrongThreadTexture{};
    std::thread worker(
        [this, &wrongThreadTexture]()
        {
            const auto pixels =
                SagaTests::Render::SolidTexturePixels({255u, 0u, 0u, 255u});
            RenderOverlayTextureDesc desc{};
            desc.width = 1u;
            desc.height = 1u;
            desc.rowPitchBytes = 4u;
            wrongThreadTexture = CreateBackendOverlayTexture(
                m_Backend.BackendInterface(), desc, pixels.data());
            DestroyBackendOverlayTexture(m_Backend.BackendInterface(), {1u, 1u});
        });
    worker.join();

    EXPECT_FALSE(wrongThreadTexture);

    const auto frame = MakeQuadFrame(
        white, 96.0f, 72.0f, 224.0f, 168.0f, 0xFF0000FFu);
    const auto capture = RenderOverlayAndCapture(frame);
    EXPECT_TRUE(SagaTests::Render::IsDominantRed(
        SagaTests::Render::PixelAt(
            capture, capture.width / 2u, capture.height / 2u)));

    DestroyBackendOverlayTexture(m_Backend.BackendInterface(), white);
}

TEST_F(OverlayGPU, EmptyFrameIsNoOp)
{
    ASSERT_TRUE(InitBackendOverlayRendering(m_Backend.BackendInterface()));
    const auto capture = RenderEmptyOverlayAndCapture();

    EXPECT_EQ(SagaTests::Render::CountPixelsNotNear(
                  capture, {0u, 0u, 0u, 255u}, 8),
              0u);
}

