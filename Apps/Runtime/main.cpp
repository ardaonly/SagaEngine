/// @file main.cpp
/// @brief Temporary SagaRuntime adapter over the current client host.
///
/// v0.0.8 ships a role-based runtime binary while the runtime core is still
/// being separated from the older client entrypoint. Keep editor/tooling
/// dependencies out of this adapter; Phase 2 should split Runtime Core from
/// optional client UI semantics.

#include "ClientHost.h"

#include <SagaEngine/Core/Log/Log.h>
#include <SagaEngine/Platform/PlatformFactory.h>
#include <SagaEngine/Platform/SDL/SDLPlatformFactory.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>

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
            config.serverPort = static_cast<std::uint16_t>(std::stoi(argv[++i]));
        }
        else if (arg == "--headless" || arg == "-h")
        {
            config.headless = true;
        }
        else if (arg == "--help" || arg == "--?")
        {
            std::printf("Usage: SagaRuntime [options]\n"
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

    static Saga::SDLPlatformFactory sdlFactory;
    Saga::PlatformFactory::Set(&sdlFactory);

    Saga::ClientHost host(std::move(config));
    host.Run();

    SagaEngine::Core::Log::Shutdown();
    return 0;
}
