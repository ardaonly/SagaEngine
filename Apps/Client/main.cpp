/// @file main.cpp
/// @brief Entry point for the SagaEngine MMO client application.
///
/// This is the real client: it connects to an authoritative server via UDP,
/// receives replication snapshots, applies them to a local ECS world, runs
/// client-side prediction + reconciliation for the local player, interpolates
/// remote entity positions, and renders a debug overlay.
///
/// MMO client loop (every frame):
///   1. Recv → Decode → Apply → ECS
///   2. Send Input → Server
///   3. Prediction → Reconciliation → Interpolation
///   4. Render Prep → Render

#ifndef SDL_MAIN_HANDLED
#   define SDL_MAIN_HANDLED
#endif

#include "ClientHost.h"
#include <SagaEngine/Core/Log/Log.h>
#include <SagaEngine/Platform/IWindow.h>
#include <SagaEngine/Platform/PlatformFactory.h>

#include <string>
#include <cstdint>

// ─── Command-line parsing ─────────────────────────────────────────────────────

static Saga::ClientConfig ParseArgs(int argc, char* argv[])
{
    Saga::ClientConfig config;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if ((arg == "--server" || arg == "-s") && i + 1 < argc)
        {
            config.serverHost = argv[++i];
        }
        else if (arg == "--port" && i + 1 < argc)
        {
            config.serverPort = static_cast<uint16_t>(std::stoi(argv[++i]));
        }
        else if (arg == "--headless" || arg == "-h")
        {
            config.headless = true;
        }
        else if (arg == "--help" || arg == "--?")
        {
            // Print usage and exit.
            printf("Usage: SagaClient [options]\n"
                   "  --server, -s <host>   Server address (default: 127.0.0.1)\n"
                   "  --port <port>         Server port (default: 7777)\n"
                   "  --headless, -h        Run without window / renderer\n"
                   "  --help, --?           Show this help\n");
            std::exit(0);
        }
    }

    return config;
}

// ─── Entry point ──────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    SagaEngine::Core::Log::Init();

    auto config = ParseArgs(argc, argv);

    auto platformFactory = Saga::CreateSDLPlatformFactory();
    Saga::PlatformFactory::Set(platformFactory.get());

    Saga::ClientHost host(std::move(config));
    host.Run();

    SagaEngine::Core::Log::Shutdown();
    return 0;
}
