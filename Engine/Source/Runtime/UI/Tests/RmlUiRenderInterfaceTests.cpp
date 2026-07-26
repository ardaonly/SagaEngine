// SPDX-License-Identifier: MPL-2.0

#include "Backends/RmlUi/RmlUiRenderInterface.h"

#include <gtest/gtest.h>

namespace
{
namespace Render = SagaEngine::Render::Backend;
using SagaEngine::UI::Backends::RmlUiRenderInterface;

class RecordingOverlay final : public Render::IRenderOverlaySubmission
{
public:
    Render::RenderOverlaySubmissionResult InitializeOverlay() override
    { ++initializeCalls; return {initializeStatus, {}}; }
    Render::RenderOverlayTextureHandle CreateOverlayTextureResource(
        const Render::RenderOverlayTextureDesc& desc, const std::uint8_t* pixels) override
    {
        lastTexture = desc;
        return pixels && textureSucceeds ? Render::RenderOverlayTextureHandle{nextTexture++, 3} :
            Render::RenderOverlayTextureHandle{};
    }
    void DestroyOverlayTextureResource(Render::RenderOverlayTextureHandle texture) override
    { destroyed.push_back(texture); }
    Render::RenderOverlaySubmissionResult SubmitOverlayFrame(
        const Render::RenderOverlayFrame& frame) override
    { ++submitCalls; lastFrame = frame; return {submitStatus, {}}; }
    void ShutdownOverlay() override { ++shutdownCalls; }

    Render::RenderOverlaySubmissionStatus initializeStatus =
        Render::RenderOverlaySubmissionStatus::Success;
    Render::RenderOverlaySubmissionStatus submitStatus =
        Render::RenderOverlaySubmissionStatus::Success;
    bool textureSucceeds = true;
    int initializeCalls = 0;
    int submitCalls = 0;
    int shutdownCalls = 0;
    Render::RenderOverlayTextureDesc lastTexture;
    Render::RenderOverlayFrame lastFrame;
    std::vector<Render::RenderOverlayTextureHandle> destroyed;
    std::uint32_t nextTexture = 7;
};

TEST(RmlUiRenderInterfaceTests, ConvertsGeometryTextureTranslationAndScissor)
{
    RecordingOverlay overlay;
    RmlUiRenderInterface adapter;
    adapter.Configure(&overlay, {800, 600, 2.0F});
    ASSERT_TRUE(adapter.Initialize());
    std::array<Rml::byte, 16> pixels{};
    Rml::TextureHandle texture = 0;
    ASSERT_TRUE(adapter.GenerateTexture(texture, pixels.data(), {2, 2}));
    EXPECT_NE(texture, 0u);

    Rml::Vertex vertices[3]{};
    vertices[0].position = {1.0F, 2.0F};
    vertices[0].tex_coord = {0.25F, 0.5F};
    vertices[0].colour = {1, 2, 3, 4};
    vertices[1].position = {4.0F, 5.0F};
    vertices[2].position = {7.0F, 8.0F};
    int indices[3] = {0, 1, 2};
    adapter.BeginFrame();
    adapter.EnableScissorRegion(true);
    adapter.SetScissorRegion(10, 20, 30, 40);
    adapter.RenderGeometry(vertices, 3, indices, 3, texture, {5.0F, 7.0F});
    SagaEngine::UI::UiRenderStats stats;
    ASSERT_TRUE(adapter.EndFrame(stats));

    ASSERT_EQ(overlay.submitCalls, 1);
    ASSERT_EQ(overlay.lastFrame.drawLists.size(), 1u);
    const auto& list = overlay.lastFrame.drawLists.front();
    EXPECT_FLOAT_EQ(list.vertices[0].pos[0], 6.0F);
    EXPECT_FLOAT_EQ(list.vertices[0].pos[1], 9.0F);
    EXPECT_EQ(list.vertices[0].colorRgba8, 0x04030201u);
    EXPECT_EQ(list.indices, (std::vector<std::uint32_t>{0, 1, 2}));
    EXPECT_EQ(list.commands.front().texture, (Render::RenderOverlayTextureHandle{8, 3}));
    EXPECT_FLOAT_EQ(list.commands.front().clipRect.left, 10.0F);
    EXPECT_FLOAT_EQ(list.commands.front().clipRect.bottom, 60.0F);
    EXPECT_FLOAT_EQ(overlay.lastFrame.framebufferScale[0], 2.0F);
    EXPECT_EQ(stats.drawCallCount, 1u);
    EXPECT_EQ(stats.textureCount, 1u);

    adapter.Shutdown();
    adapter.Shutdown();
    EXPECT_EQ(overlay.destroyed.size(), 2u);
    EXPECT_EQ(overlay.shutdownCalls, 1);
}

TEST(RmlUiRenderInterfaceTests, HandlesCompiledGeometryMinimizeAndInjectedFailures)
{
    RecordingOverlay overlay;
    RmlUiRenderInterface adapter;
    overlay.initializeStatus = Render::RenderOverlaySubmissionStatus::NotReady;
    adapter.Configure(&overlay, {640, 480, 1.0F});
    EXPECT_FALSE(adapter.Initialize());
    overlay.initializeStatus = Render::RenderOverlaySubmissionStatus::Success;
    ASSERT_TRUE(adapter.Initialize());

    Rml::Vertex vertices[3]{};
    int indices[3] = {0, 1, 2};
    const auto geometry = adapter.CompileGeometry(vertices, 3, indices, 3, 0);
    ASSERT_NE(geometry, 0u);
    adapter.BeginFrame();
    adapter.RenderCompiledGeometry(geometry, {});
    overlay.submitStatus = Render::RenderOverlaySubmissionStatus::SubmissionFailure;
    SagaEngine::UI::UiRenderStats stats;
    EXPECT_FALSE(adapter.EndFrame(stats));
    ASSERT_EQ(overlay.lastFrame.drawLists.size(), 1U);
    EXPECT_EQ(overlay.lastFrame.drawLists.front().commands.front().texture,
              (Render::RenderOverlayTextureHandle{7, 3}));
    adapter.SetViewport({0, 0, 1.0F});
    adapter.BeginFrame();
    adapter.RenderCompiledGeometry(geometry, {});
    EXPECT_TRUE(adapter.EndFrame(stats));
    EXPECT_EQ(overlay.submitCalls, 1);
    adapter.ReleaseCompiledGeometry(geometry);
    adapter.Shutdown();
}

} // namespace
