#ifndef SAGAENGINE_CORE_APPLICATION_H
#define SAGAENGINE_CORE_APPLICATION_H

#include <string>

namespace SagaEngine::Core {

    class Application {
    public:
        Application(const std::string& name = "Saga Application");
        virtual ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        void Run();
        void Close();

        virtual void OnInit() = 0;
        virtual void OnUpdate(float deltaTime) = 0;
        virtual void OnShutdown() = 0;

        static Application& Get();

    protected:
        bool m_Running = true;
        std::string m_Name;

    private:
        static Application* s_Instance;
    };

}

#endif
