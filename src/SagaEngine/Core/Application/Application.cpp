#include <SagaEngine/Core/Application/Application.h>
#include <SagaEngine/Core/Time/Time.h>
#include <cassert>

namespace SagaEngine::Core {

    Application* Application::s_Instance = nullptr;

    Application::Application(const std::string& name)
        : m_Name(name) {
        s_Instance = this;
    }

    Application::~Application() {
        s_Instance = nullptr;
    }

    Application& Application::Get() {
        assert(s_Instance);
        return *s_Instance;
    }

    void Application::Run() {
        OnInit();
        Time::Init();
        while (m_Running) {
            Time::Tick();
            float deltaTime = Time::GetDeltaTime();
            OnUpdate(deltaTime);
        }
        OnShutdown();
    }

    void Application::Close() {
        m_Running = false;
    }

}
