/// @file QtUIApplication.cpp
/// @brief Qt implementation of IUIApplication.

#include "SagaEditor/UI/Qt/QtUIApplication.h"
#include <QApplication>
#include <QTimer>

namespace SagaEditor
{

// ─── Pimpl ────────────────────────────────────────────────────────────────────

struct QtUIApplication::Impl
{
    QApplication app; ///< Owns the Qt event loop.

    Impl(int& argc, char** argv)
        : app(argc, argv)
    {
        QApplication::setApplicationName("SagaEditor");
        QApplication::setOrganizationName("Bytegeist");
    }
};

// ─── Construction ─────────────────────────────────────────────────────────────

QtUIApplication::QtUIApplication(int& argc, char** argv)
    : m_impl(std::make_unique<Impl>(argc, argv))
{}

QtUIApplication::~QtUIApplication() = default;

// ─── IUIApplication ───────────────────────────────────────────────────────────

int QtUIApplication::Run()
{
    return QApplication::exec();
}

int QtUIApplication::RunForSmoke(int timeoutMs)
{
    const int boundedTimeoutMs = timeoutMs > 0 ? timeoutMs : 1;
    QTimer::singleShot(boundedTimeoutMs, &m_impl->app, &QApplication::quit);
    return QApplication::exec();
}

void QtUIApplication::Quit()
{
    QApplication::quit();
}

} // namespace SagaEditor
