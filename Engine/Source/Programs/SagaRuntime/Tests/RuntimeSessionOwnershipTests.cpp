// SPDX-License-Identifier: MPL-2.0

#include "RuntimeArtifactSession.h"
#include "RuntimeEvidenceSession.h"
#include "RuntimeFrameLoop.h"
#include "RuntimeLaunchSession.h"
#include "RuntimeRenderSession.h"
#include "RuntimeScriptSession.h"

#include <SagaEngine/Render/Backend/IRenderBackend.h>
#include <SagaRuntime/Play/RuntimePlayContracts.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace
{
namespace Play = SagaRuntime::Play;
using SagaRuntimeApp::RuntimeArtifactSession;
using SagaRuntimeApp::RuntimeEvidenceCaptureRequest;
using SagaRuntimeApp::RuntimeEvidenceSession;
using SagaRuntimeApp::RuntimeLaunchSession;
using SagaRuntimeApp::RuntimeRenderSession;
using SagaRuntimeApp::RuntimeScriptWorld;

class ScopedFiles final
{
public:
    ScopedFiles()
        : root_(std::filesystem::current_path() / "runtime-session-ownership-tests")
    {
        std::error_code error;
        std::filesystem::remove_all(root_, error);
        std::filesystem::create_directories(root_, error);
    }

    ~ScopedFiles()
    {
        std::error_code error;
        std::filesystem::remove_all(root_, error);
    }

    const std::filesystem::path& Root() const noexcept { return root_; }

    std::filesystem::path Write(std::string_view name, std::string_view bytes) const
    {
        const auto path = root_ / name;
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        return path;
    }

private:
    std::filesystem::path root_;
};

class CapturingBackend final : public SagaEngine::Render::Backend::IRenderBackend
{
public:
    void SetFrame(const std::uint32_t width, const std::uint32_t height,
                  const int variant)
    {
        capture_.width = width;
        capture_.height = height;
        capture_.rowPitch = width * 4U;
        capture_.pixels.assign(static_cast<std::size_t>(capture_.rowPitch) * height, 0U);
        for (std::size_t offset = 3; offset < capture_.pixels.size(); offset += 4)
            capture_.pixels[offset] = 255U;
        if (variant == 0) return;
        const auto startX = width * 20U / 100U;
        const auto startY = height * 65U / 100U;
        const auto regionWidth = std::max(1U, width * 60U / 100U);
        const auto regionHeight = std::max(1U, height * 30U / 100U);
        for (std::uint32_t y = 0; y < regionHeight; ++y)
            for (std::uint32_t x = 0; x < regionWidth; ++x)
            {
                const bool marked = variant == 1 ? ((x + y) % 2U == 0U) :
                    ((x * 3U + y) % 4U == 0U);
                if (!marked) continue;
                const auto offset = static_cast<std::size_t>(startY + y) * capture_.rowPitch +
                    static_cast<std::size_t>(startX + x) * 4U;
                capture_.pixels[offset] = 255U;
                capture_.pixels[offset + 1] = 255U;
                capture_.pixels[offset + 2] = 255U;
            }
    }

    bool Initialize(const SagaEngine::Render::Backend::SwapchainDesc&) override { return true; }
    void Shutdown() override {}
    void OnResize(std::uint32_t, std::uint32_t) override {}
    SagaEngine::Render::World::MeshId CreateMesh(
        const SagaEngine::Render::MeshAsset&) override
    { return static_cast<SagaEngine::Render::World::MeshId>(1); }
    SagaEngine::Render::World::MaterialId CreateMaterial(
        const SagaEngine::Render::MaterialRuntime&) override
    { return static_cast<SagaEngine::Render::World::MaterialId>(1); }
    void DestroyMesh(SagaEngine::Render::World::MeshId) override {}
    void DestroyMaterial(SagaEngine::Render::World::MaterialId) override {}
    SagaEngine::Render::TextureHandle CreateTexture(
        std::uint32_t, std::uint32_t, const std::uint8_t*) override
    { return static_cast<SagaEngine::Render::TextureHandle>(1); }
    void DestroyTexture(SagaEngine::Render::TextureHandle) override {}
    void BeginFrame() override {}
    void Submit(const SagaEngine::Render::Scene::Camera&,
                const SagaEngine::Render::Scene::RenderView&) override {}
    void EndFrame() override {}

    bool CaptureCurrentColorFrame(
        SagaEngine::Render::Backend::RenderFrameCapture& capture) override
    {
        capture = capture_;
        return true;
    }

private:
    SagaEngine::Render::Backend::RenderFrameCapture capture_;
};

TEST(RuntimeLaunchSessionTests, RejectsMalformedEnvelopeThroughItsParser)
{
    ScopedFiles files;
    const auto envelope = files.Write("launch.json", R"({"schemaVersion":2})");
    const auto result = RuntimeLaunchSession::ReadEnvelope(envelope);
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_EQ(result.diagnostics.front().code, "Runtime.Envelope.SchemaInvalid");
}

TEST(RuntimeArtifactSessionTests, VerifiesEveryBoundArtifactAndRejectsTampering)
{
    ScopedFiles files;
    const auto path = files.Write("artifact.bin", "verified bytes");
    const auto identity = Play::ArtifactIdentity{
        "scene", Play::Sha256Bytes("verified bytes")};
    std::vector<Play::RuntimeDiagnostic> diagnostics;
    EXPECT_TRUE(RuntimeArtifactSession::VerifyFiles(
        {{identity.logicalId, path}}, {identity}, diagnostics));
    EXPECT_TRUE(diagnostics.empty());

    files.Write("artifact.bin", "tampered bytes");
    EXPECT_FALSE(RuntimeArtifactSession::VerifyFiles(
        {{identity.logicalId, path}}, {identity}, diagnostics));
    ASSERT_FALSE(diagnostics.empty());
    EXPECT_EQ(diagnostics.back().code, "Runtime.Launch.ArtifactHashMismatch");
}

TEST(RuntimeScriptSessionTests, OwnsRuntimeEntityLookupAndTransforms)
{
    Play::RuntimeSceneArtifact scene;
    scene.entities.push_back({.entityId = "door", .name = "Door"});
    RuntimeScriptWorld world(std::move(scene));
    const auto door = world.Handle("door");
    ASSERT_TRUE(door.IsValid());
    EXPECT_TRUE(world.SetPosition(door, {1.0F, 2.0F, 3.0F}).Succeeded());
    const auto position = world.GetPosition(door);
    ASSERT_TRUE(position.Succeeded());
    EXPECT_FLOAT_EQ(position.position.x, 1.0F);
    EXPECT_FLOAT_EQ(position.position.y, 2.0F);
    EXPECT_FLOAT_EQ(position.position.z, 3.0F);
}

TEST(RuntimeRenderSessionTests, UninitializedShutdownIsIdempotent)
{
    RuntimeScriptWorld world({});
    const std::map<std::string, std::filesystem::path> artifacts;
    std::vector<Play::RuntimeDiagnostic> diagnostics;
    RuntimeRenderSession render(world, artifacts, diagnostics, {});
    EXPECT_TRUE(render.ShouldClose());
    EXPECT_EQ(render.Width(), 0U);
    EXPECT_EQ(render.Height(), 0U);
    render.Shutdown();
    render.Shutdown();
    EXPECT_TRUE(diagnostics.empty());
}

TEST(RuntimeEvidenceSessionTests, CapturesMeaningfulPixelsAsPpm)
{
    ScopedFiles files;
    CapturingBackend backend;
    RuntimeEvidenceSession evidence(files.Root());
    backend.SetFrame(20, 20, 0);
    ASSERT_TRUE(evidence.BeginNamedFrame(backend,
        RuntimeEvidenceCaptureRequest{"test-frame", 7, "E", "[E] Open Door", false, 0U, {}},
        {4.0F, 13.0F, 12.0F, 6.0F}, 1.0F));
    backend.SetFrame(20, 20, 1);
    ASSERT_TRUE(evidence.EndNamedFrame(backend, 2U));
    EXPECT_TRUE(evidence.HasMeaningfulPixels());
    ASSERT_EQ(evidence.Records().size(), 2U);
    EXPECT_EQ(evidence.Records()[0].name, "world-baseline");
    EXPECT_EQ(evidence.Records()[0].frameIndex, 7U);
    EXPECT_EQ(evidence.Records()[1].name, "test-frame");
    EXPECT_EQ(evidence.Records()[1].preOverlayFrameIndex, 7U);
    EXPECT_EQ(evidence.Records()[1].promptBindingLabel, "E");
    EXPECT_GE(evidence.Records()[1].baselineDifferencePixelCount, 32U);
    EXPECT_GE(evidence.Records()[1].baselineRgbAbsoluteDifference, 2048U);
    const auto path = files.Root() / "test-frame.ppm";
    EXPECT_GT(std::filesystem::file_size(path), 8U);
    EXPECT_GT(std::filesystem::file_size(
        files.Root() / "test-frame-pre-overlay.ppm"), 8U);
}

TEST(RuntimeEvidenceSessionTests, BindsNamedFramesToSemanticAndPixelMetrics)
{
    ScopedFiles files;
    CapturingBackend backend;
    RuntimeEvidenceSession evidence(files.Root());
    backend.SetFrame(20, 20, 0);
    ASSERT_TRUE(evidence.BeginNamedFrame(backend,
        RuntimeEvidenceCaptureRequest{"e-open", 0, "E", "[E] Open Door", false, 0U, {}},
        {4.0F, 13.0F, 12.0F, 6.0F}, 1.0F));
    backend.SetFrame(20, 20, 1);
    ASSERT_TRUE(evidence.EndNamedFrame(backend, 2U));
    backend.SetFrame(20, 20, 0);
    ASSERT_TRUE(evidence.BeginNamedFrame(backend,
        RuntimeEvidenceCaptureRequest{"e-close", 30, "E", "[E] Close Door", true,
                                      0U, "e-open"},
        {4.0F, 13.0F, 12.0F, 6.0F}, 1.0F));
    backend.SetFrame(20, 20, 2);
    ASSERT_TRUE(evidence.EndNamedFrame(backend, 2U));
    backend.SetFrame(30, 24, 0);
    ASSERT_TRUE(evidence.BeginNamedFrame(backend,
        RuntimeEvidenceCaptureRequest{"e-resized", 245, "E", "[E] Open Door", false,
                                      0U, "e-close"},
        {6.0F, 15.0F, 18.0F, 7.0F}, 1.0F));
    backend.SetFrame(30, 24, 1);
    ASSERT_TRUE(evidence.EndNamedFrame(backend, 2U));

    ASSERT_EQ(evidence.Records().size(), 4U);
    const auto& baseline = evidence.Records()[0];
    const auto& open = evidence.Records()[1];
    const auto& close = evidence.Records()[2];
    const auto& resized = evidence.Records()[3];
    EXPECT_EQ(baseline.baselineDifferencePixelCount, 0U);
    EXPECT_GT(open.nonBackgroundPixelCount, 0U);
    EXPECT_GT(open.baselineDifferencePixelCount, 0U);
    EXPECT_GT(open.baselineRgbAbsoluteDifference, 0U);
    EXPECT_GT(close.priorSemanticDifferencePixelCount, 0U);
    EXPECT_GT(resized.baselineDifferencePixelCount, 0U);
    EXPECT_EQ(resized.width, 30U);
    EXPECT_EQ(resized.height, 24U);
    EXPECT_FALSE(evidence.BeginNamedFrame(backend,
        RuntimeEvidenceCaptureRequest{"e-open", 999, "E", "stale", false, 0U, {}},
        {6.0F, 15.0F, 18.0F, 7.0F}, 1.0F));
    EXPECT_FALSE(evidence.BeginNamedFrame(backend,
        RuntimeEvidenceCaptureRequest{"../escape", 999, {}, {}, false, 0U, {}},
        {6.0F, 15.0F, 18.0F, 7.0F}, 1.0F));
}

TEST(RuntimeFrameLoopTests, RejectsMissingEnvelopeBeforeStartingSubsystems)
{
    const auto result = SagaRuntimeApp::RunRuntimeFrameLoop({}, false, false, false);
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.diagnosticId, "Runtime.Host.PlayEnvelopeRequired");
}

} // namespace
