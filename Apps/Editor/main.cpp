/// @file main.cpp
/// @brief Entry point for the Saga Editor application.
///
/// Intended to provide development tools, scene editing, and asset workflows.
/// Currently serves as a structural placeholder. Future work:
///   - Qt-based UI shell (docking, menus, toolbars)
///   - Scene hierarchy panel
///   - Entity inspector with component editing
///   - 3D viewport with RHI integration
///   - Asset browser with streaming pipeline
///   - Editor state serialization

#include "SagaEngine/Core/Application/Application.h"
#include "SagaEngine/Core/Log/Log.h"

int main()
{
    SagaEngine::Core::Log::Init();

    LOG_INFO("Editor", "Editor placeholder started.");
    LOG_INFO("Editor", "No UI, no tools, no scene system yet.");

    SagaEngine::Core::Log::Shutdown();
    return 0;
}
