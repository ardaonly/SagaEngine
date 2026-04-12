/// @file main.cpp
/// @brief Entry point for the Saga Server application.
///
/// Runs headless simulation, networking, and replication systems.
/// Currently serves as a structural placeholder.

#include "SagaEngine/Core/Application/Application.h"
#include "SagaEngine/Core/Log/Log.h"

int main()
{
    SagaEngine::Core::Log::Init();

    LOG_INFO("Server", "Server placeholder started.");
    LOG_INFO("Server", "No networking loop, no replication, no world tick yet.");

    SagaEngine::Core::Log::Shutdown();
    return 0;
}
