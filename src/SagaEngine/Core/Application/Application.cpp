#include <SagaEngine/Core/Application/Application.h>
#include <SagaEngine/Core/Time/Time.h>
#include <SagaEngine/Core/Log/Log.h>
#include <cassert>
#include <cstdio>

#if defined(_WIN32)
#include <Windows.h>
#endif

namespace SagaEngine::Core {

Application* Application::s_Instance = nullptr;

Application::Application(const std::string& name)
: m_Name(name) {
    #if defined(_WIN32)
    OutputDebugStringA("[Application] Constructor START\n");
    OutputDebugStringA("[Application] Setting s_Instance...\n");
    #endif
    
    std::printf("[Application] Constructor START\n");
    std::fflush(stdout);
    
    s_Instance = this;
    
    #if defined(_WIN32)
    OutputDebugStringA("[Application] s_Instance set\n");
    OutputDebugStringA("[Application] Constructor END\n");
    #endif
    
    std::printf("[Application] Constructor END\n");
    std::fflush(stdout);
}

Application::~Application() {
    #if defined(_WIN32)
    OutputDebugStringA("[Application] Destructor START\n");
    #endif
    
    std::printf("[Application] Destructor START\n");
    std::fflush(stdout);
    
    s_Instance = nullptr;
    
    #if defined(_WIN32)
    OutputDebugStringA("[Application] Destructor END\n");
    #endif
    
    std::printf("[Application] Destructor END\n");
    std::fflush(stdout);
}

Application& Application::Get() {
    assert(s_Instance && "Application instance not found");
    return *s_Instance;
}

void Application::Run() {
    #if defined(_WIN32)
    OutputDebugStringA("[Application] Run START\n");
    #endif
    
    std::printf("[Application] Run START\n");
    std::fflush(stdout);
    
    LOG_INFO("Application", "Starting engine loop: %s", m_Name.c_str());
    Time::Init();
    
    #if defined(_WIN32)
    OutputDebugStringA("[Application] Calling OnInit...\n");
    #endif
    
    std::printf("[Application] Calling OnInit...\n");
    std::fflush(stdout);
    
    OnInit();
    
    #if defined(_WIN32)
    OutputDebugStringA("[Application] OnInit returned\n");
    #endif
    
    std::printf("[Application] OnInit returned\n");
    std::fflush(stdout);

    if (!m_Running) {
        LOG_WARN("Application", "Aborted during OnInit");
        return;
    }

    #if defined(_WIN32)
    OutputDebugStringA("[Application] Entering main loop...\n");
    #endif
    
    std::printf("[Application] Entering main loop...\n");
    std::fflush(stdout);
    
    while (m_Running) {
        Time::Tick();
        float deltaTime = Time::GetDeltaTime();
        OnUpdate(deltaTime);
    }

    #if defined(_WIN32)
    OutputDebugStringA("[Application] Main loop exited\n");
    #endif
    
    std::printf("[Application] Main loop exited\n");
    std::fflush(stdout);
    
    OnShutdown();
    
    LOG_INFO("Application", "Engine loop terminated gracefully");
    
    #if defined(_WIN32)
    OutputDebugStringA("[Application] Run END\n");
    #endif
    
    std::printf("[Application] Run END\n");
    std::fflush(stdout);
}

void Application::Close() {
    LOG_INFO("Application", "Close requested");
    m_Running = false;
}

}