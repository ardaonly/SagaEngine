/// @file QtUIFactory.cpp
/// @brief Qt implementation of IUIFactory.

#include "SagaEditor/UI/Qt/QtUIFactory.h"
#include "SagaEditor/UI/Qt/QtUIApplication.h"
#include "SagaEditor/UI/Qt/QtSettingsStore.h"
#include "SagaEditor/UI/Qt/QtUIMainWindow.h"

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
