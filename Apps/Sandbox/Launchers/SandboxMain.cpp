/// @file SandboxMain.cpp
/// @brief Main entry point for the windowed Sandbox application.

#define SDL_MAIN_HANDLED

#include <SagaEngine/Core/Log/Log.h>
#include <SagaEngine/Platform/PlatformFactory.h>
#include <SagaEngine/Platform/SDL/SDLPlatformFactory.h>
#include <SagaSandbox/Core/SandboxHost.h>
#include <SagaSandbox/Core/SandboxConfig.h>

#include <cstring>
#include <string>

int main(int argc, char* argv[])
{
    SagaEngine::Core::Log::Init();

    static Saga::SDLPlatformFactory sdlFactory;
    Saga::PlatformFactory::Set(&sdlFactory);

    SagaSandbox::SandboxConfig config;
    config.windowTitle       = "SagaEngine Sandbox";

    // Parse command line: [scenario_id] [--debug]
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--debug") == 0)
        {
            config.renderBackend.enableValidation = true;
        }
        else if (argv[i][0] != '-')
        {
            config.initialScenarioId = argv[i];
        }
    }

    SagaSandbox::SandboxHost host(std::move(config));
    host.Run();

    return 0;
}