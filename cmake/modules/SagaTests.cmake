function(saga_setup_tests)
    enable_testing()
    add_executable(SagaUnitTests)
    target_sources(SagaUnitTests PRIVATE
        Tests/Unit/ECS/ArchetypeTests.cpp
        Tests/Unit/ECS/ComponentTests.cpp
        Tests/Unit/ECS/EntityTests.cpp
        Tests/Unit/ECS/QueryTests.cpp
        Tests/Unit/ECS/SystemTests.cpp
        Tests/Unit/Input/InputBufferTests.cpp
        Tests/Unit/Input/InputSubsystemTests.cpp
        Tests/Unit/Input/NetworkInputLayerTests.cpp
        Tests/Unit/Mods/ModLoaderTests.cpp
        Tests/Unit/Mods/ModSandboxTests.cpp
        Tests/Unit/Networking/InterestTests.cpp
        Tests/Unit/Networking/ReliableChannelTests.cpp
        Tests/Unit/Networking/ReplicationTests.cpp
        Tests/Unit/Persistence/EventLogTests.cpp
        Tests/Unit/Profiling/ProfilerTests.cpp
        Tests/Unit/Services/AuthServiceTests.cpp
        Tests/Unit/Services/EconomyServiceTests.cpp
        Tests/Unit/Simulation/AuthorityTests.cpp
        Tests/Unit/Simulation/SimulationTickTests.cpp
        Tests/Unit/Simulation/WorldStateTests.cpp
    )
    target_link_libraries(SagaUnitTests PRIVATE SagaEngine GTest::gtest GTest::gmock GTest::Main)
    add_test(NAME UnitTests COMMAND SagaUnitTests)
    add_executable(SagaIntegrationTests)
    target_sources(SagaIntegrationTests PRIVATE
        Tests/Integration/Persistence/PostgreSQLTests.cpp
        Tests/Integration/ReplicationTests.cpp
        Tests/Integration/ClientServerTests.cpp
    )
    target_link_libraries(SagaIntegrationTests PRIVATE SagaBackend GTest::gtest GTest::gmock GTest::Main)
    add_test(NAME IntegrationTests COMMAND SagaIntegrationTests)
endfunction()