# ─── SagaTests.cmake ─────────────────────────────────────────────────────────
#
# Wires every test sub-tree under /Tests into a dedicated CTest target.  The
# layout on disk is:
#
#     Tests/Unit/         → SagaUnitTests          (fast, deterministic)
#     Tests/Integration/  → SagaIntegrationTests   (ECS + subsystem wiring)
#     Tests/Replication/  → SagaReplicationTests   (round-trip + fuzz + soak)
#     Tests/Stress/       → SagaStressTests        (concurrency + network load)
#
# All four targets share the same include paths and link against the same
# stack (SagaEngine + SagaRuntimeLib + SagaServerLib + SagaBackend + GTest +
# rapidcheck) so you can move a
# test file between directories without touching any CMake.
#
# Each target is registered with `add_test` so a single `ctest` invocation
# at the top level runs the lot; the per-target names let CI carve them up
# when Replication or Stress are too slow to run on every commit.

function(saga_setup_tests)
    if(NOT BUILD_TESTING)
        return()
    endif()

    enable_testing()

    # ─── Source discovery ───────────────────────────────────────────────────

    file(GLOB_RECURSE UNIT_TEST_SOURCES CONFIGURE_DEPENDS
        "${SAGA_ROOT}/Tests/Unit/*.cpp"
    )

    if(NOT SAGA_WITH_SDE)
        list(FILTER UNIT_TEST_SOURCES EXCLUDE REGEX ".*/Tests/Unit/Saga/.*\\.cpp$")
    endif()

    file(GLOB_RECURSE EDITORLAB_SOURCES CONFIGURE_DEPENDS
        "${SAGA_ROOT}/Apps/EditorLab/src/*.cpp"
    )

    file(GLOB_RECURSE INTEGRATION_SOURCES CONFIGURE_DEPENDS
        "${SAGA_ROOT}/Tests/Integration/*.cpp"
    )

    file(GLOB_RECURSE REPLICATION_SOURCES CONFIGURE_DEPENDS
        "${SAGA_ROOT}/Tests/Replication/*.cpp"
    )

    file(GLOB_RECURSE STRESS_SOURCES CONFIGURE_DEPENDS
        "${SAGA_ROOT}/Tests/Stress/*.cpp"
    )

    file(GLOB_RECURSE ARCHITECTURE_TEST_SOURCES CONFIGURE_DEPENDS
        "${SAGA_ROOT}/Tests/Unit/Architecture/*.cpp"
        "${SAGA_ROOT}/Tests/Unit/Collaboration/*.cpp"
        "${SAGA_ROOT}/Tests/Unit/Shared/*.cpp"
    )

    if(ARCHITECTURE_TEST_SOURCES)
        list(REMOVE_ITEM UNIT_TEST_SOURCES ${ARCHITECTURE_TEST_SOURCES})
    endif()

    # ─── Common include directories ─────────────────────────────────────────
    #
    # Every test target wants exactly the same include paths, so we stage
    # them in a list and spread it into each `target_include_directories`
    # call below.  Keeping the list local to the function avoids leaking
    # into the parent scope.
    set(SAGA_TEST_INCLUDE_DIRS
        ${SAGA_ROOT}/Engine/Public
        ${SAGA_ROOT}/Runtime/include
        ${SAGA_ROOT}/Server/include
        ${SAGA_ROOT}/Shared/include
        ${SAGA_ROOT}/Collaboration/include
        ${SAGA_ROOT}/Backends/include
        ${SAGA_ROOT}/Editor/include
        ${SAGA_ROOT}/Apps/EditorLab/include
        ${SAGA_ROOT}/Apps/Saga
        $<TARGET_PROPERTY:GTest::gtest,INTERFACE_INCLUDE_DIRECTORIES>
    )

    # ─── Unit tests ─────────────────────────────────────────────────────────

    add_executable(SagaUnitTests ${UNIT_TEST_SOURCES} ${EDITORLAB_SOURCES})
    saga_link_thirdparty(SagaUnitTests)
    target_link_libraries(SagaUnitTests PRIVATE
        SagaEngine
        SagaDiagnostics
        SagaRuntimeLib
        SagaServerLib
        SagaShared
        SagaCollaboration
        SagaBackend
        SagaEditorLib    # needed by Tests/Unit/Editor/* (block authoring,
                         # InspectorEditing, persona, viewport, etc.)
        GTest::gtest
        GTest::gmock
        GTest::gtest_main
        rapidcheck::rapidcheck
    )
    if(SAGA_WITH_SDE)
        target_link_libraries(SagaUnitTests PRIVATE SagaProductLib)
    endif()
    target_include_directories(SagaUnitTests PRIVATE ${SAGA_TEST_INCLUDE_DIRS})
    target_compile_definitions(SagaUnitTests PRIVATE
        SAGA_SOURCE_ROOT="${SAGA_ROOT}"
    )
    add_test(NAME UnitTests COMMAND SagaUnitTests)

    # ─── Architecture boundary tests ────────────────────────────────────────
    #
    # Keep boundary and contract checks runnable without pulling in the Qt-heavy
    # editor unit-test link graph.
    if(ARCHITECTURE_TEST_SOURCES)
        add_executable(SagaArchitectureTests ${ARCHITECTURE_TEST_SOURCES})
        target_link_libraries(SagaArchitectureTests PRIVATE
            SagaShared
            SagaCollaboration
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(SagaArchitectureTests PRIVATE ${SAGA_TEST_INCLUDE_DIRS})
        target_compile_definitions(SagaArchitectureTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
        )
        add_test(NAME ArchitectureTests COMMAND SagaArchitectureTests)
    endif()

    # ─── Integration tests ──────────────────────────────────────────────────

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
        SDL2::SDL2          # DiligentBackend integration tests need a hidden window
    )
    target_include_directories(SagaIntegrationTests PRIVATE ${SAGA_TEST_INCLUDE_DIRS})
    add_test(NAME IntegrationTests COMMAND SagaIntegrationTests)

    # ─── Replication tests ──────────────────────────────────────────────────
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
    endif()

    # ─── Stress tests ───────────────────────────────────────────────────────
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
        set_tests_properties(StressTests PROPERTIES LABELS "stress")
    endif()

endfunction()
