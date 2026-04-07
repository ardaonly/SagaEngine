/// @file Application.cpp
/// @brief Core application loop.

#include <SagaEngine/Core/Application/Application.h>
#include <SagaEngine/Core/Time/Time.h>
#include <SagaEngine/Core/Log/Log.h>
#include <cassert>

namespace SagaEngine::Core {

Application* Application::s_Instance = nullptr;

Application::Application(const std::string& name)
    : m_Name(name)
{
    s_Instance = this;
    LOG_INFO("Application", "Created: %s", m_Name.c_str());
}

Application::~Application()
{
    LOG_INFO("Application", "Destroyed: %s", m_Name.c_str());
    s_Instance = nullptr;
}

Application& Application::Get()
{
    assert(s_Instance && "Application instance not found");
    return *s_Instance;
}

void Application::Run()
{
    LOG_INFO("Application", "Starting engine loop: %s", m_Name.c_str());

    Time::Init();
    OnInit();

    if (!m_Running)
    {
        LOG_WARN("Application", "Aborted during OnInit");
        return;
    }

    LOG_INFO("Application", "Entering main loop");

    while (m_Running)
    {
        Time::Tick();
        OnUpdate(Time::GetDeltaTime());
    }

    LOG_INFO("Application", "Main loop exited");

    OnShutdown();

    LOG_INFO("Application", "Engine loop terminated gracefully");
}

void Application::Close()
{
    LOG_INFO("Application", "Close requested");
    m_Running = false;
}

} // namespace SagaEngine::Core