// Main/main.cpp
#include "SagaEngine/Core/Application/Application.h"
#include "SagaEngine/Core/Log/Log.h"

class MyApp : public SagaEngine::Core::Application {
public:
    void OnInit() override {
        LOG_INFO("App", "Initializing...");
    }
    
    void OnUpdate(float dt) override {
        // Game loop
    }
    
    void OnShutdown() override {
        LOG_INFO("App", "Shutting down...");
    }
};

int main() {
    SagaEngine::Core::Log::Init("saga.log");
    MyApp app;
    app.Run();
    SagaEngine::Core::Log::Shutdown();
    return 0;
}