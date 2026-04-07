#include "SagaEngine/Core/Application/Application.h"
#include "SagaEngine/Core/Log/Log.h"
#include "SagaEngine/Core/Time/Time.h"
#include "SagaEngine/Platform/IWindow.h"
#include "SagaEngine/Platform/PlatformFactory.h"

namespace Saga {

Application::Application(std::string name)
    : m_Name(std::move(name))
{
    LOG_INFO("Application", "Created: %s", m_Name.c_str());
}

void Application::RequestClose() noexcept
{
    m_ShouldClose = true;
}

void Application::Run()
{
    LOG_INFO("Application", "Starting engine loop: %s", m_Name.c_str());

    m_Window = PlatformFactory::Get()->CreateWindow();

    WindowDesc wDesc;
    wDesc.title     = m_Name;
    wDesc.width     = 1280;
    wDesc.height    = 720;
    wDesc.vsync     = true;
    wDesc.resizable = true;

    if (!m_Window->Init(wDesc))
    {
        LOG_ERROR("Application", "Window initialisation failed - aborting.");
        return;
    }

    OnInit();

    LOG_INFO("Application", "Entering main loop.");

    while (!m_ShouldClose && !m_Window->ShouldClose())
    {
        static uint64_t frame = 0;
        LOG_INFO("Application", "Frame: %llu", frame++);

        SagaEngine::Core::Time::Tick();
        m_Window->PollEvents();
        OnUpdate();
        m_Window->Present();
    }

    OnShutdown();
    m_Window->Shutdown();
    m_Window.reset();

    LOG_INFO("Application", "Shutdown complete.");
}
}