/// @file QtUIFactory.cpp
/// @brief Qt implementation of IUIFactory.

#include "SagaEditorQt/QtUIFactory.h"
#include "SagaEditorQt/QtUIApplication.h"
#include "SagaEditorQt/QtSettingsStore.h"
#include "SagaEditorQt/QtUIMainWindow.h"

namespace SagaEditor
{

std::unique_ptr<IUIApplication>
QtUIFactory::CreateApplication(int& argc, char** argv)
{
    return std::make_unique<QtUIApplication>(argc, argv);
}

std::unique_ptr<IUIMainWindow>
QtUIFactory::CreateMainWindow(const std::string& title, int width, int height)
{
    return std::make_unique<QtUIMainWindow>(title, width, height);
}

std::unique_ptr<IEditorSettingsStore>
QtUIFactory::CreateSettingsStore()
{
    return std::make_unique<QtSettingsStore>();
}

} // namespace SagaEditor
