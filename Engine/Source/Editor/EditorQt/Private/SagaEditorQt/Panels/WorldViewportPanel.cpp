// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Panels/WorldViewportPanel.h"

#include "SagaEditor/Processes/ToolProcessRunner.h"
#include "SagaEditor/Project/SagaProjectKitClient.h"
#include "SagaEditor/SceneEditing/SceneSelectionIdentity.h"
#include "SagaEditor/Selection/SelectionManager.h"
#include "SagaEditor/Viewport/CameraController.h"
#include "SagaEditor/Viewport/EditorRenderSceneSnapshot.h"
#include "SagaEditor/Viewport/ViewportContentBuildCoordinator.h"
#include "SagaEditor/Workspace/EditorDocumentWorkspace.h"
#include "SagaEditorQtRender/QtRenderHost.h"

#include <QtConcurrent/QtConcurrentRun>
#include <QApplication>
#include <QFutureWatcher>
#include <QDeadlineTimer>
#include <QEventLoop>
#include <QGuiApplication>
#include <QHideEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPaintEngine>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>
#include <QThread>
#include <QWheelEvent>
#include <QWidget>

#if defined(Q_OS_LINUX)
#include <QtGui/qguiapplication_platform.h>
#endif

#include <atomic>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <optional>
#include <string>

namespace SagaEditor
{
namespace
{
ViewportContentBuildResult CompileViewportContent(
    ViewportContentBuildRequest build,
    const std::shared_ptr<std::atomic_bool>& cancellation)
{
    ViewportContentBuildResult result;
    result.request = std::move(build);
    result.contentRoot = result.request.projectRoot / "Build/Editor/ViewportContent/generations" /
        result.request.operationId;
    const auto toolchain = ResolveEditorToolchain();
    ToolProcessRequest request;
    request.timeout = std::chrono::minutes(2);
    request.cancellationRequested = [cancellation]
    { return cancellation->load(std::memory_order_acquire); };
    const auto process = ManagedToolRuntime(toolchain.managedRuntime).Run(
        ManagedToolId::Packager,
        {"compile-assets", "--project",
         result.request.projectManifestPath.string(),
         "--out", result.contentRoot.string()},
        std::move(request)).process;
    result.passed = process.Succeeded();
    result.cancelled = process.classification == ToolProcessExitClassification::Cancelled;
    if (!result.passed)
    {
        result.diagnosticCode = process.diagnosticCode.empty() ?
            "EditorRender.ContentCompileFailed" : process.diagnosticCode;
        result.diagnosticMessage = process.standardError.empty() ?
            process.diagnosticMessage : process.standardError;
    }
    return result;
}

MouseButton Buttons(Qt::MouseButtons value)
{
    MouseButton result = MouseButton::None;
    if (value.testFlag(Qt::LeftButton)) result |= MouseButton::Left;
    if (value.testFlag(Qt::RightButton)) result |= MouseButton::Right;
    if (value.testFlag(Qt::MiddleButton)) result |= MouseButton::Middle;
    return result;
}

KeyModifiers Modifiers(Qt::KeyboardModifiers value)
{
    KeyModifiers result = KeyModifiers::None;
    if (value.testFlag(Qt::ShiftModifier)) result |= KeyModifiers::Shift;
    if (value.testFlag(Qt::ControlModifier) || value.testFlag(Qt::MetaModifier)) result |= KeyModifiers::Ctrl;
    if (value.testFlag(Qt::AltModifier)) result |= KeyModifiers::Alt;
    return result;
}
}

class ViewportSurface final : public QWidget
{
public:
    ViewportSurface()
        : host(), controller(&camera), contentWatcher(new QFutureWatcher<ViewportContentBuildResult>(this)),
          diagnosticLabel(new QLabel(this))
    {
        setFocusPolicy(Qt::StrongFocus);
        setMouseTracking(true);
        setAttribute(Qt::WA_NativeWindow);
        setAttribute(Qt::WA_OpaquePaintEvent);
        setAttribute(Qt::WA_NoSystemBackground);
        camera.pivot = {0.0f, 1.0f, 0.0f};
        camera.orbitRadius = 10.0f;
        camera.yawRadians = 0.0;
        camera.pitchRadians = -0.25;
        diagnosticLabel->setWordWrap(true);
        diagnosticLabel->setAlignment(Qt::AlignCenter);
        diagnosticLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: #f0c0c0; background: rgba(18, 20, 24, 210); padding: 12px; }"));
        diagnosticLabel->hide();
        resizeTimer.setSingleShot(true);
        resizeTimer.setInterval(16);
        QObject::connect(&resizeTimer, &QTimer::timeout, this, [this]
        {
            host.Resize(PhysicalWidth(), PhysicalHeight(), static_cast<float>(devicePixelRatioF()));
            RequestFrame();
        });
        QObject::connect(contentWatcher, &QFutureWatcher<ViewportContentBuildResult>::finished,
            this, [this]
        {
            const auto result = contentWatcher->result();
            auto current = workspace
                ? CaptureViewportContentBuildRequest(*workspace, result.request.operationId)
                : std::nullopt;
            auto completed = contentBuilds.Complete(result, current);
            if (completed.publish)
            {
                contentDiagnostic.clear();
                diagnosticLabel->hide();
                contentRoot = completed.contentRoot;
                RebuildScene();
            }
            else if (!completed.diagnosticCode.empty())
            {
                contentDiagnostic = completed.diagnosticCode + ": " + completed.diagnosticMessage;
                if (contentRoot.empty())
                {
                    diagnosticLabel->setText(QString::fromStdString(contentDiagnostic));
                    diagnosticLabel->setGeometry(rect());
                    diagnosticLabel->show();
                }
            }
            if (completed.nextRequest) LaunchContentBuild(*completed.nextRequest);
            else if (completed.stale && !shuttingDown) StartContentBuild();
        });
    }

    ~ViewportSurface() override
    {
        DetachWorkspace(); DetachSelection();
        StopContentBuild();
        host.Destroy();
    }

    ViewportMode mode = ViewportMode::Perspective;
    EditorDocumentWorkspace* workspace = nullptr;
    SelectionManager* selection = nullptr;

    void SetMode(ViewportMode value)
    {
        mode = value;
        switch (mode)
        {
            case ViewportMode::Perspective:
                camera.mode = ProjectionMode::Perspective;
                break;
            case ViewportMode::Top:
                camera.SnapToAxis(AxisSnap::Top);
                break;
            case ViewportMode::Front:
                camera.SnapToAxis(AxisSnap::Front);
                break;
            case ViewportMode::Right:
                camera.SnapToAxis(AxisSnap::Right);
                break;
        }
        RequestFrame();
    }

    void BindWorkspace(EditorDocumentWorkspace* value)
    {
        DetachWorkspace(); workspace = value;
        if (workspace)
        {
            workspaceSubscription = workspace->SubscribeChanges([this]
            { BindScene(); StartContentBuild(); });
        }
        BindScene(); StartContentBuild();
    }

    void BindSelection(SelectionManager* value)
    {
        DetachSelection(); selection = value;
        if (selection)
            selectionSubscription = selection->Subscribe([this](const auto& values)
            {
                highlightedSelection = values.empty() ? kInvalidSelectableId : values.front();
                UpdateHighlight(); RequestFrame();
            });
    }

    void Shutdown()
    {
        DetachWorkspace(); DetachSelection(); StopContentBuild(); host.Detach();
    }

protected:
    QPaintEngine* paintEngine() const override { return nullptr; }
    void paintEvent(QPaintEvent*) override { (void)host.RequestFrame(camera); }
    void showEvent(QShowEvent* event) override
    {
        QWidget::showEvent(event);
        if (host.Status().state == QtRenderHostState::Hidden) host.Show();
        else if (!host.Status().backendReady)
        {
            const auto surface = NativeSurface();
            if (surface) (void)host.Attach(*surface);
            else
            {
                diagnosticLabel->setText(QStringLiteral(
                    "EditorRender.SurfaceUnavailable: this Qt platform does not expose a supported native render surface."));
                diagnosticLabel->setGeometry(rect());
                diagnosticLabel->show();
            }
        }
        RebuildScene(); RequestFrame();
    }
    void hideEvent(QHideEvent* event) override
    { host.Hide(); QWidget::hideEvent(event); }
    void resizeEvent(QResizeEvent* event) override
    {
        diagnosticLabel->setGeometry(rect());
        resizeTimer.start();
        QWidget::resizeEvent(event);
    }
    void mousePressEvent(QMouseEvent* event) override
    {
        lastPosition = event->position(); pressPosition = lastPosition;
        input.buttons = Buttons(event->buttons()); input.modifiers = Modifiers(event->modifiers());
        setFocus();
    }
    void mouseMoveEvent(QMouseEvent* event) override
    {
        const auto position = event->position();
        input.mouseDeltaX = static_cast<float>(position.x() - lastPosition.x());
        input.mouseDeltaY = static_cast<float>(position.y() - lastPosition.y());
        input.cursorX = static_cast<float>(position.x() * devicePixelRatioF());
        input.cursorY = static_cast<float>(position.y() * devicePixelRatioF());
        input.viewportW = static_cast<float>(PhysicalWidth()); input.viewportH = static_cast<float>(PhysicalHeight());
        input.buttons = Buttons(event->buttons()); input.modifiers = Modifiers(event->modifiers());
        input.deltaSeconds = 1.0f / 60.0f;
        (void)controller.Tick(input); input.mouseDeltaX = input.mouseDeltaY = 0.0f;
        lastPosition = position; RequestFrame();
    }
    void mouseReleaseEvent(QMouseEvent* event) override
    {
        input.buttons = Buttons(event->buttons());
        const auto delta = event->position() - pressPosition;
        if (event->button() == Qt::LeftButton &&
            std::abs(delta.x()) + std::abs(delta.y()) < 4.0 && selection &&
            workspace && workspace->ActiveScene())
        {
            const auto picked = host.Pick(static_cast<float>(event->position().x() * devicePixelRatioF()),
                static_cast<float>(event->position().y() * devicePixelRatioF()), camera);
            if (picked) selection->SetSelection({SceneSelectionId(workspace->ActiveScene()->SceneId(), *picked)});
            else selection->ClearSelection();
        }
        RequestFrame();
    }
    void wheelEvent(QWheelEvent* event) override
    {
        input.scrollDelta = static_cast<float>(event->angleDelta().y()) / 120.0f;
        input.viewportW = static_cast<float>(PhysicalWidth()); input.viewportH = static_cast<float>(PhysicalHeight());
        (void)controller.Tick(input); input.scrollDelta = 0.0f; RequestFrame();
    }
    void keyPressEvent(QKeyEvent* event) override
    {
        if (event->key() == Qt::Key_F) FocusSelection();
        QWidget::keyPressEvent(event);
    }

private:
    std::uint32_t PhysicalWidth() const
    { return static_cast<std::uint32_t>(std::max(1.0, width() * devicePixelRatioF())); }
    std::uint32_t PhysicalHeight() const
    { return static_cast<std::uint32_t>(std::max(1.0, height() * devicePixelRatioF())); }

    std::optional<QtRenderSurface> NativeSurface()
    {
        QtRenderSurface surface;
        surface.width = PhysicalWidth(); surface.height = PhysicalHeight();
        surface.devicePixelRatio = static_cast<float>(devicePixelRatioF());
#if defined(Q_OS_WIN)
        surface.native.backend = Saga::NativeWindowBackend::Win32;
        surface.native.window = reinterpret_cast<void*>(winId());
#elif defined(Q_OS_MACOS)
        surface.native.backend = Saga::NativeWindowBackend::Cocoa;
        surface.native.window = reinterpret_cast<void*>(winId());
#elif defined(Q_OS_LINUX)
        if (QGuiApplication::platformName() == QStringLiteral("xcb"))
        {
            auto* native = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
            if (!native) return std::nullopt;
            surface.native.backend = Saga::NativeWindowBackend::X11;
            surface.native.display = native->display();
            surface.native.xcbConnection = native->connection();
            surface.native.windowId = static_cast<uintptr_t>(winId());
        }
        else return std::nullopt;
#else
        return std::nullopt;
#endif
        return surface;
    }

    void RequestFrame()
    {
        if (!isVisible() || framePending) return;
        framePending = true;
        QTimer::singleShot(0, this, [this] { framePending = false; update(); });
    }

    void BindScene()
    {
        if (observedScene && sceneSubscription != SceneDocument::InvalidChangeSubscription)
            observedScene->UnsubscribeChanges(sceneSubscription);
        observedScene = workspace ? workspace->ActiveScene() : nullptr;
        sceneSubscription = SceneDocument::InvalidChangeSubscription;
        if (observedScene)
            sceneSubscription = observedScene->SubscribeChanges([this]
            { RebuildScene(); RequestFrame(); });
    }

    void RebuildScene()
    {
        if (!observedScene || contentRoot.empty()) return;
        const auto built = BuildEditorRenderSceneSnapshot(*observedScene);
        if (built.Succeeded())
        { (void)host.LoadScene(built.snapshot, contentRoot); UpdateHighlight(); }
        RequestFrame();
    }

    void StartContentBuild()
    {
        if (shuttingDown || !workspace || !contentWatcher) return;
        auto request = CaptureViewportContentBuildRequest(
            *workspace, "viewport-" + std::to_string(++nextContentOperation));
        if (!request) return;
        if (auto start = contentBuilds.Request(std::move(*request))) LaunchContentBuild(*start);
    }

    void LaunchContentBuild(ViewportContentBuildRequest request)
    {
        if (shuttingDown || !contentWatcher) return;
        contentCancellation = std::make_shared<std::atomic_bool>(false);
        const auto cancellation = contentCancellation;
        contentWatcher->setFuture(QtConcurrent::run(
            [request = std::move(request), cancellation]() mutable
            { return CompileViewportContent(std::move(request), cancellation); }));
    }

    void StopContentBuild()
    {
        // Shutdown is bounded and lifetime-safe: the watcher gets up to 4500 ms
        // to observe cancellation. If it is still running, every callback into
        // this panel is disconnected and the watcher owns its deferred deletion.
        constexpr qint64 kShutdownDeadlineMilliseconds = 4500;
        if (shuttingDown) return;
        shuttingDown = true;
        contentBuilds.Cancel();
        if (contentCancellation) contentCancellation->store(true, std::memory_order_release);
        if (!contentWatcher || !contentWatcher->isRunning()) return;

        QDeadlineTimer deadline(kShutdownDeadlineMilliseconds);
        while (contentWatcher->isRunning() && !deadline.hasExpired())
        {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            QThread::msleep(1);
        }
        if (!contentWatcher->isRunning()) return;

        QObject::disconnect(contentWatcher, nullptr, this, nullptr);
        contentWatcher->setParent(nullptr);
        QObject::connect(contentWatcher, &QFutureWatcher<ViewportContentBuildResult>::finished,
            contentWatcher, &QObject::deleteLater);
        contentWatcher = nullptr;
    }

    void UpdateHighlight()
    {
        if (!workspace || !workspace->ActiveScene() || highlightedSelection == kInvalidSelectableId)
        { host.SetHighlightedEntity({}); return; }
        for (const auto& entity : workspace->ActiveScene()->Entities())
            if (SceneSelectionId(workspace->ActiveScene()->SceneId(), entity.entityId) == highlightedSelection)
            { host.SetHighlightedEntity(entity.entityId); return; }
        host.SetHighlightedEntity({});
    }

    void FocusSelection()
    {
        if (!workspace || !workspace->ActiveScene()) return;
        for (const auto& entity : workspace->ActiveScene()->Entities())
            if (entity.transform && SceneSelectionId(workspace->ActiveScene()->SceneId(), entity.entityId) == highlightedSelection)
            {
                controller.FrameBounds({static_cast<float>(entity.transform->position[0]),
                    static_cast<float>(entity.transform->position[1]),
                    static_cast<float>(entity.transform->position[2])}, 1.5f,
                    height() ? static_cast<float>(width()) / height() : 1.0f);
                RequestFrame(); return;
            }
    }

    void DetachWorkspace()
    {
        if (observedScene && sceneSubscription != SceneDocument::InvalidChangeSubscription)
            observedScene->UnsubscribeChanges(sceneSubscription);
        observedScene = nullptr; sceneSubscription = SceneDocument::InvalidChangeSubscription;
        if (workspace && workspaceSubscription != EditorDocumentWorkspace::InvalidChangeSubscription)
            workspace->UnsubscribeChanges(workspaceSubscription);
        workspace = nullptr; workspaceSubscription = EditorDocumentWorkspace::InvalidChangeSubscription;
    }
    void DetachSelection()
    {
        if (selection && selectionSubscription != kInvalidSelectionSubscription)
            selection->Unsubscribe(selectionSubscription);
        selection = nullptr; selectionSubscription = kInvalidSelectionSubscription;
    }

    QtRenderHost host;
    ViewportCamera camera;
    CameraController controller;
    ViewportInputState input;
    QTimer resizeTimer;
    QFutureWatcher<ViewportContentBuildResult>* contentWatcher;
    ViewportContentBuildCoordinator contentBuilds;
    std::shared_ptr<std::atomic_bool> contentCancellation;
    QLabel* diagnosticLabel;
    std::filesystem::path contentRoot;
    std::string contentDiagnostic;
    QPointF lastPosition, pressPosition;
    bool framePending = false;
    bool shuttingDown = false;
    std::uint64_t nextContentOperation = 0;
    EditorDocumentWorkspace::ChangeSubscription workspaceSubscription =
        EditorDocumentWorkspace::InvalidChangeSubscription;
    SceneDocument* observedScene = nullptr;
    SceneDocument::ChangeSubscription sceneSubscription = SceneDocument::InvalidChangeSubscription;
    SelectionSubscription selectionSubscription = kInvalidSelectionSubscription;
    SelectableId highlightedSelection = kInvalidSelectableId;
};

struct WorldViewportPanel::Impl { ViewportSurface* surface = new ViewportSurface(); };
WorldViewportPanel::WorldViewportPanel() : m_impl(std::make_unique<Impl>()) {}
WorldViewportPanel::~WorldViewportPanel() = default;
PanelId WorldViewportPanel::GetPanelId() const { return "saga.panel.viewport"; }
std::string WorldViewportPanel::GetTitle() const { return "World Viewport"; }
QWidget* WorldViewportPanel::Widget() const noexcept { return m_impl->surface; }
void WorldViewportPanel::OnInit() {}
void WorldViewportPanel::OnShutdown() { m_impl->surface->Shutdown(); }
void WorldViewportPanel::SetViewportMode(ViewportMode mode)
{
    m_impl->surface->SetMode(mode);
}
ViewportMode WorldViewportPanel::GetViewportMode() const noexcept { return m_impl->surface->mode; }
void WorldViewportPanel::SetDocumentWorkspace(EditorDocumentWorkspace* workspace) noexcept
{ m_impl->surface->BindWorkspace(workspace); }
void WorldViewportPanel::SetSelectionManager(SelectionManager* selection) noexcept
{ m_impl->surface->BindSelection(selection); }
} // namespace SagaEditor
