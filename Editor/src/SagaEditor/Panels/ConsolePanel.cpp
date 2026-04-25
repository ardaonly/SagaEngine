/// @file ConsolePanel.cpp
/// @brief Console panel — Qt widget lives entirely inside Impl.

#include "SagaEditor/Panels/ConsolePanel.h"
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollBar>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

namespace SagaEditor
{

// ─── Qt widget implementation ─────────────────────────────────────────────────

struct ConsolePanel::Impl
{
    QWidget*     widget      = nullptr;
    QTextEdit*   output      = nullptr;
    QPushButton* clearButton = nullptr;

    Impl()
    {
        widget = new QWidget();
        auto* layout = new QVBoxLayout(widget);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(4);

        output = new QTextEdit(widget);
        output->setReadOnly(true);
        output->setLineWrapMode(QTextEdit::NoWrap);
        layout->addWidget(output);

        auto* bar = new QHBoxLayout();
        clearButton = new QPushButton("Clear", widget);
        bar->addStretch();
        bar->addWidget(clearButton);
        layout->addLayout(bar);

        QObject::connect(clearButton, &QPushButton::clicked,
            [this]() { output->clear(); });
    }

    void Log(LogSeverity severity, const std::string& message)
    {
        const char* color = nullptr;
        switch (severity)
        {
            case LogSeverity::Warning: color = "#e8c46a"; break;
            case LogSeverity::Error:   color = "#e06c75"; break;
            default:                   color = "#abb2bf"; break;
        }

        const bool atBottom =
            output->verticalScrollBar()->value() >=
            output->verticalScrollBar()->maximum() - 4;

        output->append(
            QString(R"(<span style="color:%1;">%2</span>)")
                .arg(QLatin1String(color),
                     QString::fromStdString(message)));

        if (atBottom)
            output->verticalScrollBar()->setValue(
                output->verticalScrollBar()->maximum());
    }
};

// ─── ConsolePanel ─────────────────────────────────────────────────────────────

ConsolePanel::ConsolePanel()
    : m_impl(std::make_unique<Impl>())
{}

ConsolePanel::~ConsolePanel() = default;

PanelId     ConsolePanel::GetPanelId()      const { return "saga.panel.console"; }
std::string ConsolePanel::GetTitle()        const { return "Console"; }
void*       ConsolePanel::GetNativeWidget() const noexcept { return m_impl->widget; }

void ConsolePanel::OnInit()     {}
void ConsolePanel::OnShutdown() {}

void ConsolePanel::Log(LogSeverity severity, const std::string& message)
{
    m_impl->Log(severity, message);
}

void ConsolePanel::Clear()
{
    m_impl->output->clear();
}

} // namespace SagaEditor
