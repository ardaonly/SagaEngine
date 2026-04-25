/// @file QtUIFactory.h
/// @brief Qt backend implementation of IUIFactory.

#pragma once

#include "SagaEditor/UI/IUIFactory.h"

namespace SagaEditor
{

// ─── Qt UI Factory ────────────────────────────────────────────────────────────

/// Creates QtUIApplication and QtUIMainWindow.
/// EditorApp constructs one of these at startup; swapping it for a different
/// factory replaces the entire UI backend without touching any other code.
class QtUIFactory final : public IUIFactory
{
public:
    QtUIFactory()  = default;
    ~QtUIFactory() override = default;

    [[nodiscard]] std::unique_ptr<IUIApplication>
        CreateApplication(int& argc, char** argv) override;

    [[nodiscard]] std::unique_ptr<IUIMainWindow>
        CreateMainWindow(const std::string& title, int width, int height) override;
};

} // namespace SagaEditor
