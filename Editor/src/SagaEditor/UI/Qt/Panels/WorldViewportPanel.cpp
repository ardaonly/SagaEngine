/// @file WorldViewportPanel.cpp
/// @brief Qt backend implementation. The public header lives outside
///        UI/Qt/ (Panels/WorldViewportPanel.h);
///        Qt headers are permitted only inside this folder. If you
///        need to use Qt for something new, add a sibling here.

#include "SagaEditor/Panels/WorldViewportPanel.h"
#include <QColor>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <QWidget>

namespace SagaEditor
{

// ─── Viewport surface (inner Qt widget) ──────────────────────────────────────

class ViewportSurface : public QWidget
{
public:
    explicit ViewportSurface(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setFocusPolicy(Qt::StrongFocus);
        setMouseTracking(true);
    }

    ViewportMode mode     = ViewportMode::Perspective;
    bool         orbiting = false;
    int          lastX    = 0;
    int          lastY    = 0;

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.fillRect(rect(), QColor(30, 30, 30));

        p.setPen(QColor(60, 60, 60));
        constexpr int kStep = 32;
        for (int x = 0; x < width();  x += kStep) p.drawLine(x, 0, x, height());
        for (int y = 0; y < height(); y += kStep) p.drawLine(0, y, width(), y);

        p.setPen(QColor(180, 180, 180));
        p.drawText(rect(), Qt::AlignCenter, "World Viewport — Renderer not yet attached");
    }

    void mousePressEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::RightButton)
        {
            orbiting = true;
            lastX = e->pos().x();
            lastY = e->pos().y();
        }
    }

    void mouseMoveEvent(QMouseEvent* e) override
    {
        if (orbiting)
        {
            // Placeholder — orbit wired to renderer camera later.
            lastX = e->pos().x();
            lastY = e->pos().y();
        }
    }

    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::RightButton) orbiting = false;
    }

    void wheelEvent(QWheelEvent*) override { /* TODO: camera zoom */ }
    void keyPressEvent(QKeyEvent*) override { /* TODO: WASD fly-cam */ }
};

// ─── Impl ─────────────────────────────────────────────────────────────────────

struct WorldViewportPanel::Impl
{
    ViewportSurface* surface = nullptr;

    Impl() : surface(new ViewportSurface()) {}
};

// ─── WorldViewportPanel ───────────────────────────────────────────────────────

WorldViewportPanel::WorldViewportPanel()
    : m_impl(std::make_unique<Impl>())
{}

WorldViewportPanel::~WorldViewportPanel() = default;

PanelId     WorldViewportPanel::GetPanelId()      const { return "saga.panel.worldviewport"; }
std::string WorldViewportPanel::GetTitle()        const { return "World Viewport"; }
void*       WorldViewportPanel::GetNativeWidget() const noexcept { return m_impl->surface; }

void WorldViewportPanel::OnInit()     {}
void WorldViewportPanel::OnShutdown() {}

void WorldViewportPanel::SetViewportMode(ViewportMode mode)
{
    m_impl->surface->mode = mode;
    m_impl->surface->update();
}

ViewportMode WorldViewportPanel::GetViewportMode() const noexcept
{
    return m_impl->surface->mode;
}

} // namespace SagaEditor
