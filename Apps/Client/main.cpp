/// @file main.cpp
/// @brief Entry point for the Saga Client application.
///
/// Intended to host rendering, input, and network replication runtime.
/// Currently serves as a structural placeholder. Future work:
///   - Window creation and RHI initialization
///   - Input backend (SDL)
///   - Network connection to authoritative server
///   - Client-side prediction + interpolation loop
///   - Viewport rendering

#include "SagaEngine/Core/Application/Application.h"
#include "SagaEngine/Core/Log/Log.h"

int main()
{
    SagaEngine::Core::Log::Init();

    LOG_INFO("Client", "Client placeholder started.");
    LOG_INFO("Client", "No rendering, no input, no networking yet.");

    SagaEngine::Core::Log::Shutdown();
    return 0;
}
