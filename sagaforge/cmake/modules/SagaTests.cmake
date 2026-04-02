function(saga_setup_tests)
    enable_testing()

    file(GLOB_RECURSE UNIT_TEST_SOURCES CONFIGURE_DEPENDS
        "${SAGA_ROOT}/Tests/Unit/*.cpp"
    )

    file(GLOB_RECURSE INTEGRATION_SOURCES CONFIGURE_DEPENDS
        "${SAGA_ROOT}/Tests/Integration/*.cpp"
    )

    add_executable(SagaUnitTests ${UNIT_TEST_SOURCES})
    target_link_libraries(SagaUnitTests PRIVATE
        SagaEngine
        SagaBackend
        GTest::gtest
        GTest::gmock
        GTest::gtest_main
    )
    target_include_directories(SagaUnitTests PRIVATE
        ${SAGA_ROOT}/Engine/include
        ${SAGA_ROOT}/Runtime/include
        ${SAGA_ROOT}/Server/include
        ${SAGA_ROOT}/Backends/include
        $<TARGET_PROPERTY:GTest::gtest,INTERFACE_INCLUDE_DIRECTORIES>
    )
    add_test(NAME UnitTests COMMAND SagaUnitTests)

    add_executable(SagaIntegrationTests ${INTEGRATION_SOURCES})
    target_link_libraries(SagaIntegrationTests PRIVATE
        SagaBackend
        SagaEngine
        GTest::gtest
        GTest::gmock
        GTest::gtest_main
    )
    target_include_directories(SagaIntegrationTests PRIVATE
        ${SAGA_ROOT}/Backends/include
        ${SAGA_ROOT}/Engine/include
        ${SAGA_ROOT}/Runtime/include
        ${SAGA_ROOT}/Server/include
        $<TARGET_PROPERTY:GTest::gtest,INTERFACE_INCLUDE_DIRECTORIES>
    )
    add_test(NAME IntegrationTests COMMAND SagaIntegrationTests)

endfunction()