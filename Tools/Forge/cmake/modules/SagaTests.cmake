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
# stack (SagaEngine + SagaBackend + GTest + rapidcheck) so you can move a
# test file between directories without touching any CMake.
#
# Each target is registered with `add_test` so a single `ctest` invocation
# at the top level runs the lot; the per-target names let CI carve them up
# when Replication or Stress are too slow to run on every commit.

function(saga_setup_tests)
    enable_testing()

    # ─── Source discovery ───────────────────────────────────────────────────

    file(GLOB_RECURSE UNIT_TEST_SOURCES CONFIGURE_DEPENDS
        "${SAGA_ROOT}/Tests/Unit/*.cpp"
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

    # ─── Common include directories ─────────────────────────────────────────
    #
    # Every test target wants exactly the same include paths, so we stage
    # them in a list and spread it into each `target_include_directories`
    # call below.  Keeping the list local to the function avoids leaking
    # into the parent scope.
    set(SAGA_TEST_INCLUDE_DIRS
        ${SAGA_ROOT}/Engine/include
        ${SAGA_ROOT}/Runtime/include
        ${SAGA_ROOT}/Server/include
        ${SAGA_ROOT}/Backends/include
        ${SAGA_ROOT}/Editor/include
        $<TARGET_PROPERTY:GTest::gtest,INTERFACE_INCLUDE_DIRECTORIES>
    )

    # ─── Unit tests ─────────────────────────────────────────────────────────

    add_executable(SagaUnitTests ${UNIT_TEST_SOURCES})
    saga_link_thirdparty(SagaUnitTests)
    target_link_libraries(SagaUnitTests PRIVATE
        SagaEngine
        SagaBackend
        SagaEditorLib    # needed by Tests/Unit/Editor/* (block authoring,
                         # InspectorEditing, persona, viewport, etc.)
        GTest::gtest
        GTest::gmock
        GTest::gtest_main
        rapidcheck::rapidcheck
    )
    target_include_directories(SagaUnitTests PRIVATE ${SAGA_TEST_INCLUDE_DIRS})
    add_test(NAME UnitTests COMMAND SagaUnitTests)

    # ─── Integration tests ──────────────────────────────────────────────────

    add_executable(SagaIntegrationTests ${INTEGRATION_SOURCES})
    saga_link_thirdparty(SagaIntegrationTests)
    target_link_libraries(SagaIntegrationTests PRIVATE
        SagaBackend
        SagaEngine
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
