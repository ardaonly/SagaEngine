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

    include("${SAGA_ROOT}/cmake/testing/SagaTestSuites.cmake")
    saga_register_local_test_suites()

    file(GENERATE
        OUTPUT "${CMAKE_BINARY_DIR}/generated/SagaFocusedTestOwnership-$<CONFIG>.txt"
        CONTENT
"SagaRenderUnitTests.SOURCES=$<TARGET_PROPERTY:SagaRenderUnitTests,SOURCES>
SagaRenderUnitTests.LINK_LIBRARIES=$<TARGET_PROPERTY:SagaRenderUnitTests,LINK_LIBRARIES>
SagaRenderUnitTests.INCLUDE_DIRECTORIES=$<TARGET_PROPERTY:SagaRenderUnitTests,INCLUDE_DIRECTORIES>
SagaSandboxUnitTests.SOURCES=$<TARGET_PROPERTY:SagaSandboxUnitTests,SOURCES>
SagaSandboxUnitTests.LINK_LIBRARIES=$<TARGET_PROPERTY:SagaSandboxUnitTests,LINK_LIBRARIES>
SagaSandboxUnitTests.INCLUDE_DIRECTORIES=$<TARGET_PROPERTY:SagaSandboxUnitTests,INCLUDE_DIRECTORIES>
SagaRuntimeHostUnitTests.SOURCES=$<TARGET_PROPERTY:SagaRuntimeHostUnitTests,SOURCES>
SagaRuntimeHostUnitTests.LINK_LIBRARIES=$<TARGET_PROPERTY:SagaRuntimeHostUnitTests,LINK_LIBRARIES>
SagaRuntimeHostUnitTests.INCLUDE_DIRECTORIES=$<TARGET_PROPERTY:SagaRuntimeHostUnitTests,INCLUDE_DIRECTORIES>
SagaAssetIdentityGeneratorTests.SOURCES=$<TARGET_PROPERTY:SagaAssetIdentityGeneratorTests,SOURCES>
SagaAssetIdentityGeneratorTests.LINK_LIBRARIES=$<TARGET_PROPERTY:SagaAssetIdentityGeneratorTests,LINK_LIBRARIES>
SagaAssetIdentityGeneratorTests.INCLUDE_DIRECTORIES=$<TARGET_PROPERTY:SagaAssetIdentityGeneratorTests,INCLUDE_DIRECTORIES>
SagaEditorQtModuleTests.SOURCES=$<TARGET_PROPERTY:SagaEditorQtModuleTests,SOURCES>
SagaEditorQtModuleTests.LINK_LIBRARIES=$<TARGET_PROPERTY:SagaEditorQtModuleTests,LINK_LIBRARIES>
SagaSagaEditorLabModuleTests.SOURCES=$<TARGET_PROPERTY:SagaSagaEditorLabModuleTests,SOURCES>
SagaSagaEditorLabModuleTests.LINK_LIBRARIES=$<TARGET_PROPERTY:SagaSagaEditorLabModuleTests,LINK_LIBRARIES>
")

endfunction()
