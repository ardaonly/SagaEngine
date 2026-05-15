/// @file ProblemsPanel.cpp
/// @brief Qt backend implementation for the editor Problems panel.

#include "SagaEditor/Panels/ProblemsPanel.h"
#include "SagaEditor/Diagnostics/IEditorDiagnosticsService.h"

#include <QBrush>
#include <QColor>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

#include <utility>

namespace SagaEditor
{

namespace
{

[[nodiscard]] QString SeverityText(EditorDiagnosticSeverity severity)
{
    switch (severity)
    {
        case EditorDiagnosticSeverity::Error: return QStringLiteral("Error");
        case EditorDiagnosticSeverity::Warning: return QStringLiteral("Warning");
        case EditorDiagnosticSeverity::Info: return QStringLiteral("Info");
    }

    return QStringLiteral("Info");
}

[[nodiscard]] QColor SeverityColor(EditorDiagnosticSeverity severity)
{
    switch (severity)
    {
        case EditorDiagnosticSeverity::Error: return QColor(198, 72, 77);
        case EditorDiagnosticSeverity::Warning: return QColor(196, 143, 50);
        case EditorDiagnosticSeverity::Info: return QColor(83, 139, 214);
    }

    return QColor(83, 139, 214);
}

[[nodiscard]] QString LocationText(const EditorDiagnosticLocation& location)
{
    if (location.resource.empty())
        return {};

    QString text = QString::fromStdString(location.resource);
    if (location.line > 0)
    {
        text += QStringLiteral(":") + QString::number(location.line);
        if (location.column > 0)
            text += QStringLiteral(":") + QString::number(location.column);
    }

    return text;
}

} // namespace

// ─── Qt Widget Implementation ────────────────────────────────────────────────

struct ProblemsPanel::Impl
{
    QWidget* widget = nullptr;
    QLabel* summaryLabel = nullptr;
    QTreeWidget* tree = nullptr;
    QPushButton* clearButton = nullptr;
    IEditorDiagnosticsService* service = nullptr;
    EditorDiagnosticsSubscription subscription =
        kInvalidEditorDiagnosticsSubscription;
    std::vector<EditorDiagnostic> diagnostics;

    Impl()
    {
        widget = new QWidget();
        auto* layout = new QVBoxLayout(widget);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(4);

        auto* header = new QHBoxLayout();
        summaryLabel = new QLabel("0 problems", widget);
        clearButton = new QPushButton("Clear", widget);
        header->addWidget(summaryLabel);
        header->addStretch();
        header->addWidget(clearButton);
        layout->addLayout(header);

        tree = new QTreeWidget(widget);
        tree->setColumnCount(5);
        tree->setHeaderLabels({
            QStringLiteral("Severity"),
            QStringLiteral("Source"),
            QStringLiteral("Code"),
            QStringLiteral("Message"),
            QStringLiteral("Location"),
        });
        tree->setRootIsDecorated(false);
        tree->setUniformRowHeights(true);
        tree->setAlternatingRowColors(true);
        tree->header()->setStretchLastSection(true);
        tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        tree->header()->setSectionResizeMode(3, QHeaderView::Stretch);
        layout->addWidget(tree);

        QObject::connect(clearButton, &QPushButton::clicked,
            [this]()
            {
                if (service)
                    (void)service->Clear();
                else
                    SetDiagnostics({});
            });
    }

    ~Impl()
    {
        Disconnect();
    }

    void Connect(IEditorDiagnosticsService* nextService)
    {
        Disconnect();
        service = nextService;
        if (!service)
        {
            SetDiagnostics({});
            return;
        }

        subscription = service->Subscribe(
            [this](const std::vector<EditorDiagnostic>& snapshot)
            {
                SetDiagnostics(snapshot);
            });
    }

    void Disconnect()
    {
        if (service && subscription != kInvalidEditorDiagnosticsSubscription)
        {
            (void)service->Unsubscribe(subscription);
        }

        subscription = kInvalidEditorDiagnosticsSubscription;
        service = nullptr;
    }

    void SetDiagnostics(std::vector<EditorDiagnostic> nextDiagnostics)
    {
        diagnostics = std::move(nextDiagnostics);
        RebuildRows();
    }

    void RebuildRows()
    {
        tree->clear();

        std::size_t errors = 0;
        std::size_t warnings = 0;
        for (const EditorDiagnostic& diagnostic : diagnostics)
        {
            if (diagnostic.severity == EditorDiagnosticSeverity::Error)
                ++errors;
            else if (diagnostic.severity == EditorDiagnosticSeverity::Warning)
                ++warnings;

            auto* item = new QTreeWidgetItem(tree);
            item->setText(0, SeverityText(diagnostic.severity));
            item->setText(1, QString::fromStdString(diagnostic.source));
            item->setText(2, QString::fromStdString(diagnostic.code));
            item->setText(3, QString::fromStdString(diagnostic.message));
            item->setText(4, LocationText(diagnostic.location));
            item->setForeground(0, QBrush(SeverityColor(diagnostic.severity)));
            if (diagnostic.publishBlocking)
            {
                item->setToolTip(0, QStringLiteral("Blocks publishing"));
                item->setToolTip(3, QStringLiteral("Blocks publishing"));
            }
        }

        summaryLabel->setText(
            QStringLiteral("%1 problems (%2 errors, %3 warnings)")
                .arg(static_cast<qulonglong>(diagnostics.size()))
                .arg(static_cast<qulonglong>(errors))
                .arg(static_cast<qulonglong>(warnings)));
    }
};

// ─── ProblemsPanel ───────────────────────────────────────────────────────────

ProblemsPanel::ProblemsPanel()
    : m_impl(std::make_unique<Impl>())
{}

ProblemsPanel::~ProblemsPanel() = default;

PanelId ProblemsPanel::GetPanelId() const
{
    return "saga.panel.problems";
}

std::string ProblemsPanel::GetTitle() const
{
    return "Problems";
}

void* ProblemsPanel::GetNativeWidget() const noexcept
{
    return m_impl->widget;
}

void ProblemsPanel::OnInit()
{
    if (m_impl->service)
        m_impl->SetDiagnostics(m_impl->service->GetAll());
}

void ProblemsPanel::OnShutdown()
{
    m_impl->Disconnect();
}

void ProblemsPanel::SetDiagnosticsService(IEditorDiagnosticsService* service)
{
    m_impl->Connect(service);
}

void ProblemsPanel::SetDiagnostics(std::vector<EditorDiagnostic> diagnostics)
{
    m_impl->SetDiagnostics(std::move(diagnostics));
}

std::size_t ProblemsPanel::GetDiagnosticCount() const noexcept
{
    return m_impl->diagnostics.size();
}

void ProblemsPanel::Clear()
{
    if (m_impl->service)
        (void)m_impl->service->Clear();
    else
        m_impl->SetDiagnostics({});
}

} // namespace SagaEditor
