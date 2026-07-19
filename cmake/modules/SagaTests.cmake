# SPDX-License-Identifier: MPL-2.0
# --- SagaTests.cmake ---------------------------------------------------------
#
# Wires every test sub-tree under /Tests into a dedicated CTest target.  The
# layout on disk is:
#
#     Tests/Unit/          -> SagaUnitTests          (fast, deterministic)
#     Tests/Architecture/  -> SagaArchitectureTests  (repository boundaries)
#     Tests/Integration/   -> SagaIntegrationTests   (subsystem workflows)
#     Tests/GPU/           -> GPU-backed integration suites
#     Tests/Contract/      -> consumer and generated-artifact contracts
#     Tests/Support/       -> test-only infrastructure
#
# Public-consumer, private white-box, GPU, integration and stress tests use
# separate targets.  That ownership is deliberate: a private backend test must
# not turn its include paths into installed API evidence, and GPU execution is
# opt-in on developer machines.

function(saga_setup_tests)
    if(NOT BUILD_TESTING)
        return()
    endif()

    enable_testing()

    # Module-owned suites are registered by each module CMake file and created
    # only after all production targets exist.  This keeps private test access
    # scoped to the owning module instead of granting one global test target
    # every module's implementation include directory.
    saga_create_registered_module_tests(SAGA_MODULE_TEST_TARGETS)

    include("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tests/SagaTestSources.cmake")

    include("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tests/SagaCoreTestTargets.cmake")

    include("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tests/SagaRuntimeTestTargets.cmake")

    include("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tests/SagaProductEditorTestTargets.cmake")

    include("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tests/SagaEvidenceTestTargets.cmake")

endfunction()
