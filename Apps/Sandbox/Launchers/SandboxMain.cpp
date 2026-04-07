/// @file SandboxMain.cpp
/// @brief Main of Sandbox.

#define SDL_MAIN_HANDLED

#include <SagaEngine/Platform/PlatformFactory.h>
#include <SagaEngine/Platform/SDL/SDLPlatformFactory.h>
#include <SagaSandbox/Core/SandboxHost.h>
#include <SagaSandbox/Core/SandboxConfig.h>

int main(int /*argc*/, char* /*argv*/[])
{
    static Saga::SDLPlatformFactory sdlFactory;
    Saga::PlatformFactory::Set(&sdlFactory);

    SagaSandbox::SandboxConfig config;
    config.windowTitle       = "SagaEngine Sandbox";
    config.initialScenarioId = "";

    SagaSandbox::SandboxHost host(std::move(config));
    host.Run();

    return 0;
}