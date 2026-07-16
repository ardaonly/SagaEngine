saga_add_runtime_module(ServerAuthority
    MODULE_DIR Engine/Source/Runtime/ServerAuthority
    TARGET SagaServerLib
    PRIVATE_DEPS asio::asio
    SOURCES
        Private/SagaEngine/ServerAuthority/ConnectionManager.cpp
        Private/SagaEngine/ServerAuthority/ConnectionManager.h
        Private/SagaEngine/ServerAuthority/Gameplay/GameplayCommandDispatcher.cpp
        Private/SagaEngine/ServerAuthority/Security/ReplayGuard.cpp
        Private/SagaEngine/ServerAuthority/ServerConnection.cpp
        Private/SagaEngine/ServerAuthority/ServerConnection.h
        Private/SagaEngine/ServerAuthority/ShardManager.cpp
        Private/SagaEngine/ServerAuthority/Simulation/ActorOwnershipRegistry.cpp
        Private/SagaEngine/ServerAuthority/Simulation/AuthoritativeMovementCommandIntake.cpp
        Private/SagaEngine/ServerAuthority/Simulation/AuthoritativeMovementCore.cpp
        Private/SagaEngine/ServerAuthority/Simulation/AuthoritativeMovementInputAdapter.cpp
        Private/SagaEngine/ServerAuthority/Simulation/InputCommandQueue.cpp
        Private/SagaEngine/ServerAuthority/Simulation/MovementValidator.cpp
        Private/SagaEngine/ServerAuthority/ZoneServer.cpp
        Public/SagaEngine/ServerAuthority/ConnectionMigration.h
        Public/SagaEngine/ServerAuthority/CrossZoneVisibility.h
        Public/SagaEngine/ServerAuthority/DynamicZoneSpawner.h
        Public/SagaEngine/ServerAuthority/Gameplay/GameplayCommandDispatcher.h
        Public/SagaEngine/ServerAuthority/Security/AuthToken.h
        Public/SagaEngine/ServerAuthority/Security/ReplayGuard.h
        Public/SagaEngine/ServerAuthority/ShardManager.h
        Public/SagaEngine/ServerAuthority/ShardMesh.h
        Public/SagaEngine/ServerAuthority/Simulation/ActorOwnershipRegistry.h
        Public/SagaEngine/ServerAuthority/Simulation/AuthoritativeMovementCommandIntake.h
        Public/SagaEngine/ServerAuthority/Simulation/AuthoritativeMovementCore.h
        Public/SagaEngine/ServerAuthority/Simulation/AuthoritativeMovementInputAdapter.h
        Public/SagaEngine/ServerAuthority/Simulation/InputCommandQueue.h
        Public/SagaEngine/ServerAuthority/Simulation/MovementValidator.h
        Public/SagaEngine/ServerAuthority/ZoneServer.h
        Public/SagaEngine/ServerAuthority/ZoneTransferProtocol.h
)

saga_register_module_tests(ServerAuthority
    MODULE_DIR Engine/Source/Runtime/ServerAuthority
    LINK_TARGET SagaServerLib
    PRIVATE_DEPS SagaDiagnostics asio::asio nlohmann_json::nlohmann_json
    LABELS "unit;runtime;server"
    SOURCES
        Tests/ActorOwnershipRegistryTests.cpp
        Tests/AuthoritativeMovementCommandIntakeTests.cpp
        Tests/AuthoritativeMovementCoreTests.cpp
        Tests/AuthoritativeMovementInputAdapterTests.cpp
        Tests/ConnectionManagerTests.cpp
        Tests/GameplayCommandDispatcherTests.cpp
        Tests/MovementDirtyReplicationBridgeTests.cpp
        Tests/ServerLifecycleDiagnosticsTests.cpp
        Tests/ZoneServerSecurityBoundaryTests.cpp
        Tests/ZoneServerDiagnosticsTests.cpp
        Tests/ZoneServerMovementAuthorityTests.cpp
        Tests/ZoneServerPacketDrainTests.cpp
)
