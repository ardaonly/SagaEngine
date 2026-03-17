function(saga_setup_tests)
    enable_testing()

    add_executable(SagaUnitTests)
    target_sources(SagaUnitTests PRIVATE
        ${CMAKE_SOURCE_DIR}/Tests/Unit/CoreTests.cpp
        ${CMAKE_SOURCE_DIR}/Tests/Unit/EngineTests.cpp
    )
    target_link_libraries(SagaUnitTests PRIVATE SagaEngine GTest::gtest GTest::gmock GTest::Main)
    add_test(NAME UnitTests COMMAND SagaUnitTests)

    add_executable(SagaIntegrationTests)
    target_sources(SagaIntegrationTests PRIVATE
        ${CMAKE_SOURCE_DIR}/Tests/Integration/NetworkTests.cpp
        ${CMAKE_SOURCE_DIR}/Tests/Integration/DatabaseTests.cpp
    )
    target_link_libraries(SagaIntegrationTests PRIVATE SagaRuntime GTest::gtest GTest::gmock GTest::Main)
    add_test(NAME IntegrationTests COMMAND SagaIntegrationTests)

    add_executable(SagaStressTests)
    target_sources(SagaStressTests PRIVATE
        ${CMAKE_SOURCE_DIR}/Tests/Stress/LoadTests.cpp
        ${CMAKE_SOURCE_DIR}/Tests/Stress/PerformanceTests.cpp
    )
    target_link_libraries(SagaStressTests PRIVATE SagaServer GTest::gtest GTest::gmock GTest::Main)
endfunction()