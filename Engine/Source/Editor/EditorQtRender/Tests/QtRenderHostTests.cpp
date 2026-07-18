// SPDX-FileCopyrightText: 2026 Arda Koyuncu
// SPDX-License-Identifier: MPL-2.0

#include "SagaEditorQtRender/QtRenderHost.h"
#include "SagaEditor/Processes/ToolProcessRunner.h"
#include "SagaEditor/Project/SagaProjectKitClient.h"
#include "SagaEditor/SceneEditing/SceneDocument.h"
#include "SagaEditor/Viewport/EditorRenderSceneSnapshot.h"
#include "SagaEngine/Render/Materials/Material.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QGuiApplication>
#include <QWidget>
#if defined(Q_OS_LINUX)
#include <QtGui/qguiapplication_platform.h>
#endif

#include <chrono>
#include <cstdlib>
#include <filesystem>

namespace
{
class RecordingBackend final : public SagaEngine::Render::Backend::IRenderBackend
{
public:
    bool Initialize(const SagaEngine::Render::Backend::SwapchainDesc& desc) override
    { initialized = desc.nativeWindow && desc.width && desc.height; return initialized; }
    void Shutdown() override { initialized = false; ++shutdowns; }
    void OnResize(std::uint32_t width, std::uint32_t height) override
    { lastWidth = width; lastHeight = height; }
    SagaEngine::Render::World::MeshId CreateMesh(const SagaEngine::Render::MeshAsset&) override
    { return static_cast<SagaEngine::Render::World::MeshId>(++nextMesh); }
    SagaEngine::Render::World::MaterialId CreateMaterial(
        const SagaEngine::Render::MaterialRuntime&) override
    { return static_cast<SagaEngine::Render::World::MaterialId>(++nextMaterial); }
    void DestroyMesh(SagaEngine::Render::World::MeshId) override {}
    void DestroyMaterial(SagaEngine::Render::World::MaterialId) override {}
    SagaEngine::Render::TextureHandle CreateTexture(std::uint32_t, std::uint32_t,
        const std::uint8_t*) override { return static_cast<SagaEngine::Render::TextureHandle>(1); }
    void DestroyTexture(SagaEngine::Render::TextureHandle) override {}
    void BeginFrame() override { ++begins; }
    void Submit(const SagaEngine::Render::Scene::Camera&,
                const SagaEngine::Render::Scene::RenderView& view) override
    { draws = view.DrawCount(); }
    void EndFrame() override { ++ends; }
    bool initialized = false;
    int shutdowns = 0, begins = 0, ends = 0;
    std::uint32_t lastWidth = 0, lastHeight = 0;
    std::size_t draws = 0;
    std::uint32_t nextMesh = 0, nextMaterial = 0;
};

SagaEditor::QtRenderSurface Surface()
{
    SagaEditor::QtRenderSurface surface;
    surface.native.backend = Saga::NativeWindowBackend::X11;
    surface.native.windowId = 42;
    surface.width = 640; surface.height = 360;
    return surface;
}
}

TEST(QtRenderHostTests, OwnsOneBackendLifecycleAndDoesNotRenderWhileHidden)
{
    RecordingBackend* backend = nullptr;
    SagaEditor::QtRenderHost host([&]
    {
        auto instance = std::make_unique<RecordingBackend>();
        backend = instance.get(); return instance;
    });
    ASSERT_TRUE(host.Attach(Surface()));
    ASSERT_NE(backend, nullptr);
    EXPECT_TRUE(host.RequestFrame({}));
    EXPECT_EQ(backend->begins, 1);
    host.Hide();
    EXPECT_FALSE(host.RequestFrame({}));
    EXPECT_EQ(backend->begins, 1);
    host.Show(); host.Resize(800, 600, 2.0f);
    EXPECT_EQ(backend->lastWidth, 800U);
    EXPECT_TRUE(host.RequestFrame({}));
    EXPECT_EQ(host.Status().framesRendered, 2U);
    host.Destroy();
    EXPECT_EQ(host.Status().state, SagaEditor::QtRenderHostState::Destroyed);
}

TEST(QtRenderHostTests, RejectsUnknownNativeSurfaceAndSupportsDeviceLossState)
{
    SagaEditor::QtRenderHost host([] { return std::make_unique<RecordingBackend>(); });
    auto invalid = Surface(); invalid.native.backend = Saga::NativeWindowBackend::Unknown;
    EXPECT_FALSE(host.Attach(invalid));
    ASSERT_TRUE(host.Attach(Surface()));
    host.DeviceLost();
    EXPECT_EQ(host.Status().state, SagaEditor::QtRenderHostState::DeviceLost);
    EXPECT_TRUE(host.Recover(Surface()));
    EXPECT_TRUE(host.Status().backendReady);
}

TEST(QtRenderHostTests, FrameEvidenceRequiresNonUniformRealColorData)
{
    SagaEditor::QtRenderFrameEvidence clear{2, 1, {10, 20, 30, 255, 10, 20, 30, 255}};
    EXPECT_FALSE(clear.HasMeaningfulPixels());
    clear.rgba8[4] = 11;
    EXPECT_TRUE(clear.HasMeaningfulPixels());
}

TEST(QtRenderHostTests, ConsumesSagaPackagerCookedStarterArenaArtifacts)
{
    namespace fs = std::filesystem;
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto contentRoot = fs::temp_directory_path() /
        ("saga-editor-viewport-contract-" + std::to_string(unique));
    const auto sourceRoot = fs::path(SAGA_SOURCE_ROOT);
    const auto project = sourceRoot / "Samples/StarterArena/StarterArena.sagaproj";
    const auto toolchain = SagaEditor::ResolveEditorToolchain();

    SagaEditor::ToolProcessRequest request;
    request.executable = toolchain.sagaPackagerExecutable;
    request.arguments = {"compile-assets", "--project", project.string(),
                         "--out", contentRoot.string()};
    request.timeout = std::chrono::seconds(30);
    const auto process = SagaEditor::ToolProcessRunner{}.Run(request);
    ASSERT_TRUE(process.Succeeded()) << process.standardError;

    SagaEditor::SceneDocument scene(sourceRoot /
        "Samples/StarterArena/Scenes/StarterArena.sagascene");
    ASSERT_TRUE(scene.Load());
    const auto snapshot = SagaEditor::BuildEditorRenderSceneSnapshot(scene);
    ASSERT_TRUE(snapshot.Succeeded());

    RecordingBackend* backend = nullptr;
    SagaEditor::QtRenderHost host([&]
    {
        auto instance = std::make_unique<RecordingBackend>();
        backend = instance.get();
        return instance;
    });
    ASSERT_TRUE(host.Attach(Surface()));
    ASSERT_TRUE(host.LoadScene(snapshot.snapshot, contentRoot));
    EXPECT_EQ(backend->nextMesh, 3U);
    EXPECT_EQ(backend->nextMaterial, 6U);
    ASSERT_TRUE(host.RequestFrame({}));
    EXPECT_EQ(backend->draws, 3U);
    host.SetHighlightedEntity("door-mesh");
    EXPECT_TRUE(host.RequestFrame({}));
    host.Destroy();

    std::error_code ignored;
    fs::remove_all(contentRoot, ignored);
}

TEST(StarterArenaViewportRenderIntegrationTests, RealBackendProducesNonClearPixelsAndResizes)
{
    if (!std::getenv("SAGA_REQUIRE_REAL_VIEWPORT_RENDER"))
        GTEST_SKIP() << "Set SAGA_REQUIRE_REAL_VIEWPORT_RENDER=1 on a graphics-capable desktop.";

    int argc = 1;
    char applicationName[] = "SagaViewportIntegration";
    char* argv[] = {applicationName, nullptr};
    std::unique_ptr<QApplication> ownedApplication;
    if (!QApplication::instance()) ownedApplication = std::make_unique<QApplication>(argc, argv);

    QWidget window;
    window.resize(640, 360);
    window.setAttribute(Qt::WA_NativeWindow);
    window.show();
    QApplication::processEvents();

    SagaEditor::QtRenderSurface surface;
    surface.width = 640;
    surface.height = 360;
#if defined(Q_OS_WIN)
    surface.native.backend = Saga::NativeWindowBackend::Win32;
    surface.native.window = reinterpret_cast<void*>(window.winId());
#elif defined(Q_OS_MACOS)
    surface.native.backend = Saga::NativeWindowBackend::Cocoa;
    surface.native.window = reinterpret_cast<void*>(window.winId());
#elif defined(Q_OS_LINUX)
    if (QGuiApplication::platformName() != QStringLiteral("xcb"))
        GTEST_SKIP() << "The real Linux integration requires the Qt xcb platform.";
    auto* native = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
    ASSERT_NE(native, nullptr);
    surface.native.backend = Saga::NativeWindowBackend::X11;
    surface.native.display = native->display();
    surface.native.xcbConnection = native->connection();
    surface.native.windowId = static_cast<uintptr_t>(window.winId());
#else
    GTEST_SKIP() << "No native surface adapter is available on this platform.";
#endif

    namespace fs = std::filesystem;
    const auto sourceRoot = fs::path(SAGA_SOURCE_ROOT);
    const auto contentRoot = fs::temp_directory_path() /
        ("saga-real-viewport-content-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    SagaEditor::ToolProcessRequest request;
    request.executable = SagaEditor::ResolveEditorToolchain().sagaPackagerExecutable;
    request.arguments = {"compile-assets", "--project",
        (sourceRoot / "Samples/StarterArena/StarterArena.sagaproj").string(),
        "--out", contentRoot.string()};
    request.timeout = std::chrono::seconds(30);
    const auto process = SagaEditor::ToolProcessRunner{}.Run(request);
    ASSERT_TRUE(process.Succeeded()) << process.standardError;
    SagaEditor::SceneDocument scene(sourceRoot /
        "Samples/StarterArena/Scenes/StarterArena.sagascene");
    ASSERT_TRUE(scene.Load());
    const auto snapshot = SagaEditor::BuildEditorRenderSceneSnapshot(scene);
    ASSERT_TRUE(snapshot.Succeeded());

    SagaEditor::QtRenderHost host;
    ASSERT_TRUE(host.Attach(surface));
    ASSERT_TRUE(host.LoadScene(snapshot.snapshot, contentRoot));
    SagaEditor::ViewportCamera camera;
    camera.pivot = {0.0f, 1.0f, 0.0f};
    camera.orbitRadius = 10.0f;
    camera.pitchRadians = -0.25;
    const auto first = host.CaptureFrame(camera);
    ASSERT_TRUE(first.has_value());
    EXPECT_TRUE(first->HasMeaningfulPixels());
    window.resize(800, 450);
    QApplication::processEvents();
    host.Resize(800, 450, 1.0f);
    const auto resized = host.CaptureFrame(camera);
    ASSERT_TRUE(resized.has_value());
    EXPECT_EQ(resized->width, 800U);
    EXPECT_EQ(resized->height, 450U);
    EXPECT_TRUE(resized->HasMeaningfulPixels());
    host.Destroy();

    std::error_code ignored;
    fs::remove_all(contentRoot, ignored);
}
