/// @file main.cpp
/// @brief Headless smoke test for the RenderClientApp facade.
///
/// Layer  : Apps / RenderClientSmokeTest
/// Purpose: Verifies the full render client contract end-to-end without
///          any GPU, window, or display. This is NOT a render demo — no
///          pixels are produced, no window opens, no visual output exists.
///
///          What it tests:
///            - RenderClientApp facade initialises with NullRenderBackend
///            - NetEntityBinding bridge: bind, update, release lifecycle
///            - RenderSubsystem deterministic 120-frame pump
///            - Multi-camera slot registration (main + shadow)
///            - Aggregate stats accounting (drawn / visited / cameras)
///
///          This binary is CI-safe, headless, and deterministic. It runs
///          identically on every platform because NullRenderBackend has no
///          hardware dependency. Visual / GPU validation belongs in Sandbox.
///
/// Not this file's concern:
///   - DiligentRenderBackend — see integration tests
///   - SDL / window / input — see Sandbox
///   - Rendering correctness — Phase 2+

#include "SagaRuntime/Client/RenderClientApp.h"
#include "SagaEngine/Render/Backend/IRenderBackend.h"

#include <cstdio>
#include <cstdint>

namespace
{

namespace R = SagaEngine::Render;
namespace C = SagaEngine::Runtime::Client;
using SagaEngine::Math::Mat4;
using SagaEngine::Math::Vec3;
using SagaEngine::Math::Transform;
using SagaEngine::Math::Quat;

R::World::RenderEntity MakeEntity(std::uint32_t netId, const Vec3& pos)
{
    R::World::RenderEntity e{};
    e.transform.position = pos;
    e.boundsRadius       = 0.05f;
    e.visible            = true;
    e.networkId          = netId;
    return e;
}

/// Build a scripted sequence that, across 120 frames, exercises every
/// hook the facade exposes: bind, update, release.
void DriveOneFrame(std::uint64_t frameIndex, C::RenderClientApp& app)
{
    auto& bridge = app.Bridge();

    // Frame 0: seed 4 replicated entities.
    if (frameIndex == 0)
    {
        bridge.BindOrCreate(1001, MakeEntity(1001, Vec3{ 0.00f, 0, 0}));
        bridge.BindOrCreate(1002, MakeEntity(1002, Vec3{ 0.25f, 0, 0}));
        bridge.BindOrCreate(1003, MakeEntity(1003, Vec3{-0.25f, 0, 0}));
        bridge.BindOrCreate(1004, MakeEntity(1004, Vec3{ 0.00f, 0.25f, 0}));
    }

    // Every frame: jitter entity 1002 to simulate replicated motion.
    {
        const float t = static_cast<float>(frameIndex) * 0.01f;
        Transform tr{Vec3{0.25f, t * 0.01f, 0}, Quat::Identity(), Vec3::One()};
        bridge.UpdateTransform(1002, tr);
    }

    // Frame 60: mid-session despawn of entity 1004.
    if (frameIndex == 60)
        bridge.Release(1004);

    // Frame 90: respawn with a different network id.
    if (frameIndex == 90)
        bridge.BindOrCreate(1005, MakeEntity(1005, Vec3{0, -0.25f, 0}));
}

} // namespace

int main()
{
    // 1. Backend. NullRenderBackend records draw counts without touching
    //    a real GPU. This is intentional — smoke test validates the facade
    //    contract, not GPU rendering. GPU tests live in Integration/.
    R::Backend::NullRenderBackend backend;

    // 2. Facade. Owns world + bridge + subsystem; injects backend.
    C::RenderClientApp app(backend);

    // 3. Config. Default cameras: main + shadow (for demo parity with the
    //    earlier discussion of multi-camera flow).
    C::RenderClientConfig cfg;
    cfg.swapchain.width  = 1280;
    cfg.swapchain.height = 720;
    cfg.quality          = R::LOD::QualityPreset::High;
    cfg.defaultCameras.main    = true;
    cfg.defaultCameras.shadow  = true;
    cfg.defaultCameras.minimap = false;
    cfg.defaultCameras.debug   = false;

    if (!app.Initialize(cfg))
    {
        std::fprintf(stderr, "RenderClientApp::Initialize failed\n");
        return 1;
    }

    // Both canonical cameras need a usable view+projection. The
    // subsystem calls RecomputeFrustum each frame; we only need to set
    // the matrices once here.
    for (auto id : {app.MainCameraId(), app.ShadowCameraId()})
    {
        if (id == R::Scene::CameraId::kInvalid) continue;
        if (auto* cam = app.Subsystem().GetCamera(id))
        {
            cam->view       = Mat4::Identity();
            cam->projection = Mat4::Identity();
            cam->position   = Vec3{0, 0, 0};
        }
    }

    // 4. Deterministic 120-frame loop.
    constexpr std::uint64_t kTotalFrames = 120;
    constexpr float         kDtSec       = 1.0f / 60.0f;

    for (std::uint64_t f = 0; f < kTotalFrames; ++f)
    {
        DriveOneFrame(f, app);

        R::Scene::FrameContext ctx;
        ctx.frameIndex      = f;
        ctx.deltaTimeSec    = kDtSec;
        ctx.absoluteTimeSec = static_cast<double>(f) * kDtSec;

        app.Tick(ctx);
    }

    const auto& stats = app.Subsystem().LastFrameStats();
    std::printf(
        "RenderClientSmokeTest: frames=%llu backendFrameIdx=%llu "
        "cameras=%u drawn=%u visited=%u\n",
        static_cast<unsigned long long>(kTotalFrames),
        static_cast<unsigned long long>(backend.FrameIndex()),
        stats.cameraCount, stats.totalDrawn, stats.totalVisited);

    // 5. Shutdown. Facade clears the bridge, removes default cameras,
    //    and tells the backend to release swapchain / devices.
    app.Shutdown();
    return 0;
}
