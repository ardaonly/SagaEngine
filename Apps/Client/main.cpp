/*
    Apps/Client/main.cpp

    Entry point for the Saga Client application.
    Intended to host rendering, input, and gameplay runtime logic.
    Currently serves as a structural placeholder.

*/

#include "SagaEngine/Core/Application/Application.h"
#include "SagaEngine/Core/Log/Log.h"

int main() {
    SagaEngine::Core::Log::Init();

    LOG_INFO("Client", "Client placeholder started.");
    LOG_INFO("Client", "No gameplay, no rendering, no networking yet.");

    SagaEngine::Core::Log::Shutdown();
    return 0;
}