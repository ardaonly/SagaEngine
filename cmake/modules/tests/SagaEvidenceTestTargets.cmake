# SPDX-License-Identifier: MPL-2.0
    # --- Architecture boundary tests ----------------------------------------
    #
    # Keep boundary and contract checks runnable without pulling in the Qt-heavy
    # editor unit-test link graph.
    find_program(_saga_python3 NAMES python3)
    if(_saga_python3 AND
       EXISTS "${SAGA_ROOT}/Tools/Developer/RepoAudit/tests/test_saga_repo_xray.py")
        add_test(NAME RepoAuditTests
            COMMAND "${_saga_python3}" -m unittest discover
                -s "${SAGA_ROOT}/Tools/Developer/RepoAudit/tests"
                -p "test_*.py")
        set_tests_properties(RepoAuditTests PROPERTIES
            LABELS "repository;architecture;python")
    endif()

    if(_saga_python3 AND
       EXISTS "${SAGA_ROOT}/Tools/Developer/DistributionCheck/tests/test_linux_distribution_whitelist.py")
        add_test(NAME SagaLinuxDistributionWhitelistTests
            COMMAND "${_saga_python3}"
                "${SAGA_ROOT}/Tools/Developer/DistributionCheck/tests/test_linux_distribution_whitelist.py")
        set_tests_properties(SagaLinuxDistributionWhitelistTests PROPERTIES
            LABELS "package;architecture")
    endif()

    if(_saga_python3 AND
       EXISTS "${SAGA_ROOT}/Tools/Developer/VisibleSliceEvidence/tests/test_visible_slice_evidence.py")
        add_test(NAME VisibleSliceEvidenceRunnerTests
            COMMAND "${_saga_python3}" -m unittest discover
                -s "${SAGA_ROOT}/Tools/Developer/VisibleSliceEvidence/tests"
                -p "test_*.py")
        set_tests_properties(VisibleSliceEvidenceRunnerTests PROPERTIES
            LABELS "visible-slice;runtime-ui;rmlui;visual-blocks;projection;parity;recovery;failure-injection;starter-arena;invariant-closure;graphical-evidence;input-variant")
    endif()

    if(ARCHITECTURE_TEST_SOURCES)
        add_executable(SagaArchitectureTests ${ARCHITECTURE_TEST_SOURCES})
        target_link_libraries(SagaArchitectureTests PRIVATE
            SagaEngine
            SagaShared
            SagaCollaboration
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(SagaArchitectureTests PRIVATE ${SAGA_TEST_INCLUDE_DIRS})
        target_compile_definitions(SagaArchitectureTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
            SAGA_BUILD_ROOT="${CMAKE_BINARY_DIR}"
        )
        add_test(NAME ArchitectureTests COMMAND SagaArchitectureTests)
        set_tests_properties(ArchitectureTests PROPERTIES LABELS "architecture;unit")
        add_test(NAME EditorQtPublicAbiBoundaryTests
            COMMAND SagaArchitectureTests
                --gtest_filter=EditorQtPublicAbiBoundaryTests.*)
        set_tests_properties(EditorQtPublicAbiBoundaryTests PROPERTIES
            LABELS "unit;architecture;editor")
    endif()

    # --- Integration tests --------------------------------------------------

    add_executable(SagaIntegrationTests ${INTEGRATION_SOURCES})
    saga_link_thirdparty(SagaIntegrationTests)
    target_link_libraries(SagaIntegrationTests PRIVATE
        SagaBackend
        SagaEngine
        SagaRuntimeLib
        SagaServerLib
        SagaShared
        SagaCollaboration
        GTest::gtest
        GTest::gmock
        GTest::gtest_main
        rapidcheck::rapidcheck
    )
    target_include_directories(SagaIntegrationTests PRIVATE
        ${SAGA_TEST_INCLUDE_DIRS}
        ${SAGA_ROOT}/Tests/Support
        ${SAGA_MODULE_PRIVATE_INCLUDE_DIRS}
        ${SAGA_ROOT}/Engine/Source/Programs/SagaRuntime/Private
    )
    target_compile_definitions(SagaIntegrationTests PRIVATE
        SAGA_SOURCE_ROOT="${SAGA_ROOT}"
    )
    add_test(NAME IntegrationTests COMMAND SagaIntegrationTests)
    set_tests_properties(IntegrationTests PROPERTIES
        LABELS "integration;runtime;server;networking;replication;timing-sensitive"
    )

    # --- Diligent GPU integration tests ------------------------------------
    # The target is always buildable.  CTest registration is opt-in so an
    # ordinary local acceptance run never creates a graphics device by
    # surprise on a headless or resource-constrained machine.
    option(SAGA_ENABLE_GPU_TEST_EXECUTION
        "Register Diligent GPU integration tests with CTest" OFF)
    if(DILIGENT_GPU_INTEGRATION_SOURCES)
        add_executable(SagaDiligentGpuIntegrationTests
            ${DILIGENT_GPU_INTEGRATION_SOURCES}
        )
        saga_apply_compiler_flags(SagaDiligentGpuIntegrationTests)
        target_link_libraries(SagaDiligentGpuIntegrationTests PRIVATE
            SagaBackend
            SagaEngine
            SagaGraphicsPrivate
            SagaDiligentBackend
            SagaDiligentRuntime
            SagaRuntimeLib
            SagaServerLib
            SagaShared
            SagaCollaboration
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
            SDL2::SDL2
            nlohmann_json::nlohmann_json
        )
        target_include_directories(SagaDiligentGpuIntegrationTests PRIVATE
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_ROOT}/Engine/Source/Runtime/RHI/Private
            ${SAGA_ROOT}/Engine/Source/Runtime/Render/Private
            ${SAGA_ROOT}/Engine/Source/Programs/SagaRuntime/Private
            ${SAGA_ROOT}/Tests/GPU/Diligent
            ${SAGA_ROOT}/Tests/Support
        )
        target_compile_definitions(SagaDiligentGpuIntegrationTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
        )
        saga_link_diligent_backend(SagaDiligentGpuIntegrationTests)
        set_target_properties(SagaDiligentGpuIntegrationTests PROPERTIES
            FOLDER "Tests/GPU"
        )

        if(SAGA_ENABLE_GPU_TEST_EXECUTION)
            add_test(NAME DiligentGpuIntegrationTests
                COMMAND SagaDiligentGpuIntegrationTests)
            set_tests_properties(DiligentGpuIntegrationTests PROPERTIES
                LABELS "integration;runtime;render;gpu;whitebox-private")
        endif()
    endif()

    # --- Replication tests --------------------------------------------------
    #
    # The replication suite is the heaviest: fuzzers, soak loops, determinism
    # guards, chaos tests.  We still add it to the default CTest set because
    # a missing replication regression is much more expensive to fix than an
    # extra minute of local testing.  CI profiles that cannot afford the
    # extra minutes can filter by the `ReplicationTests` name.
    if(REPLICATION_SOURCES)
        add_executable(SagaReplicationTests ${REPLICATION_SOURCES})
        saga_link_thirdparty(SagaReplicationTests)
        target_link_libraries(SagaReplicationTests PRIVATE
            SagaEngine
            SagaRuntimeLib
            SagaServerLib
            SagaShared
            SagaCollaboration
            SagaBackend
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
            rapidcheck::rapidcheck
        )
        target_include_directories(SagaReplicationTests PRIVATE ${SAGA_TEST_INCLUDE_DIRS})
        add_test(NAME ReplicationTests COMMAND SagaReplicationTests)
        set_tests_properties(ReplicationTests PROPERTIES
            LABELS "replication;integration;slow;long-running"
        )
    endif()

    # --- Stress tests -------------------------------------------------------
    #
    # Concurrency and network-load tests.  Run under the default CTest set so
    # a broken lock or a missing atomic is caught locally, but they are
    # intentionally tagged with the `Stress` label so CI can exclude them
    # from the blocking pre-merge job.
    if(STRESS_SOURCES)
        add_executable(SagaStressTests ${STRESS_SOURCES})
        saga_link_thirdparty(SagaStressTests)
        target_link_libraries(SagaStressTests PRIVATE
            SagaEngine
            SagaRuntimeLib
            SagaServerLib
            SagaShared
            SagaCollaboration
            SagaBackend
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
            rapidcheck::rapidcheck
        )
        target_include_directories(SagaStressTests PRIVATE ${SAGA_TEST_INCLUDE_DIRS})
        add_test(NAME StressTests COMMAND SagaStressTests)
        set_tests_properties(StressTests PROPERTIES LABELS "stress;load;slow")
    endif()
