/// @file ProductionDashboardPanel.cpp
/// @brief Qt backend implementation for the production dashboard.

#include "SagaEditor/Panels/ProductionDashboardPanel.h"
#include "SagaEditor/Customization/EditorCustomizationCatalog.h"
#include "SagaEditor/Host/EditorHost.h"
#include "SagaEditor/Persona/PersonaRegistry.h"
#include "SagaEditor/Persona/UIPersona.h"
#include "SagaEditor/Profile/EditorProfile.h"
#include "SagaEditor/Profile/EditorProfileRegistry.h"
#include "SagaEditor/Runtime/IEditorEngineBridge.h"

#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

namespace SagaEditor
{

namespace
{

[[nodiscard]] QString ValueText(const std::string& value)
{
    return QString::fromStdString(value.empty() ? "-" : value);
}

[[nodiscard]] QString BoolText(bool value)
{
    return value ? QStringLiteral("Available") : QStringLiteral("Unavailable");
}

} // namespace

struct ProductionDashboardPanel::Impl
{
    explicit Impl(EditorHost& hostRef)
        : host(hostRef)
    {
        widget = new QWidget();
        auto* root = new QVBoxLayout(widget);
        root->setContentsMargins(12, 12, 12, 12);
        root->setSpacing(10);

        auto* title = new QLabel(QStringLiteral("SAGA Production Dashboard"), widget);
        title->setObjectName(QStringLiteral("SagaProductionDashboardTitle"));
        root->addWidget(title);

        auto* line = new QFrame(widget);
        line->setFrameShape(QFrame::HLine);
        root->addWidget(line);

        auto* form = new QFormLayout();
        form->setLabelAlignment(Qt::AlignLeft);
        form->setFormAlignment(Qt::AlignTop);

        engineState = AddRow(form, "Engine Bridge");
        engineRole = AddRow(form, "Runtime Role");
        engineVersion = AddRow(form, "Engine Version");
        engineCommit = AddRow(form, "Commit");
        profile = AddRow(form, "Workspace Profile");
        persona = AddRow(form, "Persona");
        customization = AddRow(form, "SDE Customization");
        customizationSource = AddRow(form, "Customization Source");

        root->addLayout(form);
        root->addStretch();
    }

    static QLabel* AddRow(QFormLayout* form, const char* label)
    {
        auto* value = new QLabel();
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        value->setWordWrap(true);
        form->addRow(QString::fromLatin1(label), value);
        return value;
    }

    void Refresh()
    {
        const EditorEngineBridgeSnapshot bridge = host.GetEngineBridge().Snapshot();
        engineState->setText(QStringLiteral("%1 - %2")
            .arg(QString::fromLatin1(ToString(bridge.state)),
                 ValueText(bridge.message)));
        engineRole->setText(ValueText(bridge.runtimeRole));
        engineVersion->setText(ValueText(bridge.engineVersion));
        engineCommit->setText(ValueText(bridge.gitCommit));

        const EditorProfile* activeProfile = host.GetEditorProfileRegistry().GetActive();
        profile->setText(activeProfile == nullptr
            ? QStringLiteral("-")
            : QStringLiteral("%1 (%2)")
                .arg(ValueText(activeProfile->displayName),
                     ValueText(activeProfile->id)));

        const UIPersona* activePersona = host.GetPersonaRegistry().GetActive();
        persona->setText(activePersona == nullptr
            ? QStringLiteral("-")
            : QStringLiteral("%1 (%2)")
                .arg(ValueText(activePersona->displayName),
                     ValueText(activePersona->id)));

        const EditorCustomizationStatus& status =
            host.GetCustomizationCatalog().Status();
        customization->setText(QStringLiteral("%1 - %2")
            .arg(BoolText(status.sdeAvailable),
                 ValueText(status.message)));
        customizationSource->setText(
            QString::fromStdString(status.sourceRoot.generic_string()));
    }

    EditorHost& host;
    QWidget* widget = nullptr;
    QLabel* engineState = nullptr;
    QLabel* engineRole = nullptr;
    QLabel* engineVersion = nullptr;
    QLabel* engineCommit = nullptr;
    QLabel* profile = nullptr;
    QLabel* persona = nullptr;
    QLabel* customization = nullptr;
    QLabel* customizationSource = nullptr;
};

ProductionDashboardPanel::ProductionDashboardPanel(EditorHost& host)
    : m_impl(std::make_unique<Impl>(host))
{}

ProductionDashboardPanel::~ProductionDashboardPanel() = default;

PanelId ProductionDashboardPanel::GetPanelId() const
{
    return "saga.panel.production_dashboard";
}

std::string ProductionDashboardPanel::GetTitle() const
{
    return "Production";
}

void* ProductionDashboardPanel::GetNativeWidget() const noexcept
{
    return m_impl->widget;
}

void ProductionDashboardPanel::OnInit()
{
    Refresh();
}

void ProductionDashboardPanel::OnShutdown() {}

void ProductionDashboardPanel::Refresh()
{
    m_impl->Refresh();
}

} // namespace SagaEditor
