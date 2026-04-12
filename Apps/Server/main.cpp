/// @file main.cpp
/// @brief Entry point for the SagaEngine test snapshot server.
///
/// This server:
///   - Listens on UDP for client connections
///   - Sends WorldState snapshots to connected clients
///   - Receives InputCommand packets and sends InputAck
///   - Logs all activity for verification
///
/// Usage: SagaServer [--port 7777] [--entities 5] [--snap-interval 0.5]

#include "TestSnapshotServer.h"
#include <SagaEngine/Core/Log/Log.h>

#include <string>
#include <cstdint>
#include <cstdlib>
#include <cstdio>

static Saga::TestServerConfig ParseArgs(int argc, char* argv[])
{
    Saga::TestServerConfig config;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if ((arg == "--port" || arg == "-p") && i + 1 < argc)
        {
            config.bindPort = static_cast<uint16_t>(std::atoi(argv[++i]));
        }
        else if ((arg == "--entities" || arg == "-e") && i + 1 < argc)
        {
            config.testEntityCount = static_cast<uint32_t>(std::atoi(argv[++i]));
        }
        else if ((arg == "--snap-interval" || arg == "-s") && i + 1 < argc)
        {
            config.snapshotIntervalSec = std::atof(argv[++i]);
        }
        else if (arg == "--help" || arg == "--?")
        {
            std::printf("Usage: SagaServer [options]\n"
                        "  --port, -p <port>         UDP port (default: 7777)\n"
                        "  --entities, -e <count>    Test entity count (default: 5)\n"
                        "  --snap-interval, -s <sec> Snapshot interval in seconds (default: 0.5)\n"
                        "  --help, --?               Show this help\n");
            std::exit(0);
        }
    }

    return config;
}

int main(int argc, char* argv[])
{
    SagaEngine::Core::Log::Init();

    auto config = ParseArgs(argc, argv);

    LOG_INFO("Server", "Starting Test Snapshot Server...");
    LOG_INFO("Server", "  Port: %u", config.bindPort);
    LOG_INFO("Server", "  Entities: %u", config.testEntityCount);
    LOG_INFO("Server", "  Snapshot interval: %.2fs", config.snapshotIntervalSec);

    Saga::TestSnapshotServer server;

    if (!server.Init(config))
    {
        LOG_ERROR("Server", "Failed to initialize server.");
        return 1;
    }

    LOG_INFO("Server", "Server started. Waiting for client connections...");
    LOG_INFO("Server", "Press Ctrl+C to stop.");

    server.Start();

    // Block until interrupted.
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    server.Stop();
    SagaEngine::Core::Log::Shutdown();
    return 0;
}
