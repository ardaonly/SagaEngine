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
        capture.width = 2;
        capture.height = 1;
        capture.rowPitch = 8;
        capture.pixels = {0, 0, 0, 255, 255, 255, 255, 255};
        return true;
    }
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
    const auto path = files.Root() / "frame.ppm";
    CapturingBackend backend;
    RuntimeEvidenceSession evidence(path);
    ASSERT_TRUE(evidence.CaptureMeaningfulFrame(backend));
    EXPECT_TRUE(evidence.HasMeaningfulPixels());
    EXPECT_EQ(evidence.CapturePath(), path);
    EXPECT_GT(std::filesystem::file_size(path), 8U);
}

TEST(RuntimeFrameLoopTests, RejectsMissingEnvelopeBeforeStartingSubsystems)
{
    const auto result = SagaRuntimeApp::RunRuntimeFrameLoop({}, false, false, false);
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.diagnosticId, "Runtime.Host.PlayEnvelopeRequired");
}

} // namespace
