saga_add_runtime_module(Networking
    MODULE_DIR Engine/Source/Runtime/Networking
    TARGET SagaEngine
    PRIVATE_DEPS asio::asio SagaDiagnostics
    SOURCES
        Private/SagaEngine/Networking/BackpressureSignal.cpp
        Private/SagaEngine/Networking/BandwidthThrottle.cpp
        Private/SagaEngine/Networking/FragmentAssembler.cpp
        Private/SagaEngine/Networking/NetworkChaosLayer.cpp
        Private/SagaEngine/Networking/NetworkTransport.cpp
        Private/SagaEngine/Networking/NetworkTypes.cpp
        Private/SagaEngine/Networking/PacketBandwidthTracker.cpp
        Private/SagaEngine/Networking/PacketRegistry.cpp
        Private/SagaEngine/Networking/RateLimiter.cpp
        Private/SagaEngine/Networking/ReliableChannel.cpp
        Private/SagaEngine/Networking/ServerPacketNormalizer.cpp
        Public/SagaEngine/Networking/BackpressureSignal.h
        Public/SagaEngine/Networking/BandwidthThrottle.h
        Public/SagaEngine/Networking/FragmentAssembler.h
        Public/SagaEngine/Networking/NetworkChaosLayer.h
        Public/SagaEngine/Networking/NetworkTransport.h
        Public/SagaEngine/Networking/NetworkTypes.h
        Public/SagaEngine/Networking/Packet.h
        Public/SagaEngine/Networking/PacketBandwidthTracker.h
        Public/SagaEngine/Networking/PacketRegistry.h
        Public/SagaEngine/Networking/RateLimiter.h
        Public/SagaEngine/Networking/ReliableChannel.h
        Public/SagaEngine/Networking/ServerPacketNormalizer.h
)

saga_register_module_tests(Networking
    MODULE_DIR Engine/Source/Runtime/Networking
    LINK_TARGET SagaEngine
    PRIVATE_DEPS SagaDiagnostics
    LABELS "unit;runtime;networking"
    SOURCES
        Tests/NetworkChaosLayerTests.cpp
        Tests/ReliableChannelTests.cpp
        Tests/ServerPacketNormalizationTests.cpp
        Tests/TransportFactoryTests.cpp
)
