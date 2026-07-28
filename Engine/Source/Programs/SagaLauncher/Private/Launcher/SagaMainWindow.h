// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "SagaEditor/Project/SagaProjectKitClient.h"

#include <QMainWindow>
#include <filesystem>
#include <memory>
#include <string>

namespace SagaEditor
{
class AuthoringModeService;
class EditorWorkspaceSession;
class QtSettingsStore;
}
class QStackedWidget;
class QListWidget;
class QComboBox;
class QCloseEvent;

namespace SagaProduct
{
enum class SagaMainWindowState
{
    Home, OpeningProject, Editor, ReturningHome, FatalStartupError
};

class SagaMainWindow final : public QMainWindow
{
public:
    SagaMainWindow(std::filesystem::path productExecutable,
                   std::filesystem::path initialProject = {},
                   std::string initialAuthoringMode = {},
                   QWidget* parent = nullptr);
    ~SagaMainWindow() override;

    [[nodiscard]] SagaMainWindowState State() const noexcept;
    [[nodiscard]] bool OpenProject(const std::filesystem::path& project);
    [[nodiscard]] SagaEditor::ProjectToolResult CreateProject(
        const std::filesystem::path& destination,
        const std::string& displayName);
    void ReturnHome();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void BuildHome();
    void RefreshRecents();
    void RefreshEditorProjectPresentation();

    std::filesystem::path executable_;
    std::filesystem::path initialProject_;
    std::string authoringMode_;
    SagaMainWindowState state_ = SagaMainWindowState::Home;
    QStackedWidget* stack_ = nullptr;
    QWidget* home_ = nullptr;
    QListWidget* recents_ = nullptr;
    QComboBox* mode_ = nullptr;
    std::unique_ptr<SagaEditor::QtSettingsStore> settings_;
    std::unique_ptr<SagaEditor::AuthoringModeService> authoringModes_;
    std::unique_ptr<SagaEditor::EditorWorkspaceSession> editor_;
};
} // namespace SagaProduct
