# --- SagaTests.cmake ---------------------------------------------------------
#
# Wires every test sub-tree under /Tests into a dedicated CTest target.  The
# layout on disk is:
#
#     Tests/Unit/         -> SagaUnitTests          (fast, deterministic)
#     Tests/Integration/  -> SagaIntegrationTests   (ECS + subsystem wiring)
#     Tests/Replication/  -> SagaReplicationTests   (round-trip + fuzz + soak)
#     Tests/Stress/       -> SagaStressTests        (concurrency + network load)
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

    # --- Source discovery ---------------------------------------------------

    file(GLOB_RECURSE UNIT_TEST_SOURCES CONFIGURE_DEPENDS
        "${SAGA_ROOT}/Tests/Unit/*.cpp"
    )

    file(GLOB_RECURSE SAGA_EDITOR_COMPOSITION_TEST_SOURCES CONFIGURE_DEPENDS
        "${SAGA_ROOT}/Tests/Tools/SagaEditorComposition/*.cpp"
    )

    file(GLOB_RECURSE SAGA_PIPELINE_TEST_SOURCES CONFIGURE_DEPENDS
        "${SAGA_ROOT}/Tests/Tools/SagaPipeline/*.cpp"
    )

    file(GLOB_RECURSE SAGA_PRODUCT_TEST_SOURCES CONFIGURE_DEPENDS
        "${SAGA_ROOT}/Tests/Unit/Saga/*.cpp"
    )

    set(SAGA_PACKAGE_STAGING_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Saga/SagaPackageStagingTests.cpp"
    )

    set(SAGA_SCRIPT_LIFECYCLE_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Runtime/ScriptLifecycleServiceTests.cpp"
    )
    set(SAGA_SCRIPT_BINDING_RUNTIME_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Runtime/ScriptBindingRuntimeTests.cpp"
    )
    set(SAGA_CSHARP_SCRIPT_HOST_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Runtime/CSharpScriptHostTests.cpp"
    )
    set(SAGA_CSHARP_GAMEPLAY_PROOF_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Runtime/CSharpGameplayProofTests.cpp"
    )

    if(SAGA_PRODUCT_TEST_SOURCES)
        list(REMOVE_ITEM UNIT_TEST_SOURCES
            ${SAGA_PRODUCT_TEST_SOURCES}
        )
        list(REMOVE_ITEM SAGA_PRODUCT_TEST_SOURCES
            ${SAGA_PACKAGE_STAGING_TEST_SOURCE}
        )
    endif()

    list(REMOVE_ITEM UNIT_TEST_SOURCES
        ${SAGA_SCRIPT_LIFECYCLE_TEST_SOURCE}
        ${SAGA_SCRIPT_BINDING_RUNTIME_TEST_SOURCE}
        ${SAGA_CSHARP_SCRIPT_HOST_TEST_SOURCE}
        ${SAGA_CSHARP_GAMEPLAY_PROOF_TEST_SOURCE}
    )

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

    # --- Common include directories -----------------------------------------
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
        ${SAGA_ROOT}/Tools/AssetPipeline/include
        ${SAGA_ROOT}/Backends/include
        ${SAGA_ROOT}/Editor/include
        ${SAGA_ROOT}/Apps/EditorLab/include
        ${SAGA_ROOT}/Apps/Saga
        $<TARGET_PROPERTY:GTest::gtest,INTERFACE_INCLUDE_DIRECTORIES>
    )

    # --- Managed SagaScript runtime bridge ---------------------------------
    set(SAGA_SCRIPT_RUNTIME_BRIDGE_PROJECT
        "${SAGA_ROOT}/Engine/Managed/SagaScript.RuntimeBridge/SagaScript.RuntimeBridge.csproj"
    )
    set(SAGA_SCRIPT_RUNTIME_BRIDGE_OUTPUT_DIR
        "${CMAKE_BINARY_DIR}/Managed/SagaScript.RuntimeBridge"
    )
    set(SAGA_SCRIPT_RUNTIME_BRIDGE_ASSEMBLY
        "${SAGA_SCRIPT_RUNTIME_BRIDGE_OUTPUT_DIR}/SagaScript.RuntimeBridge.dll"
    )
    if(EXISTS "${SAGA_SCRIPT_RUNTIME_BRIDGE_PROJECT}")
        add_custom_command(
            OUTPUT "${SAGA_SCRIPT_RUNTIME_BRIDGE_ASSEMBLY}"
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "${SAGA_SCRIPT_RUNTIME_BRIDGE_OUTPUT_DIR}"
            COMMAND ${CMAKE_COMMAND} -E env
                DOTNET_CLI_HOME=/tmp/sagascript-dotnet-home
                NUGET_PACKAGES=/tmp/sagascript-nuget
                DOTNET_CLI_TELEMETRY_OPTOUT=1
                "${SAGA_DOTNET_EXECUTABLE}" build
                "${SAGA_SCRIPT_RUNTIME_BRIDGE_PROJECT}"
                --configuration Release
                --output "${SAGA_SCRIPT_RUNTIME_BRIDGE_OUTPUT_DIR}"
                --nologo
                --verbosity minimal
            DEPENDS
                "${SAGA_SCRIPT_RUNTIME_BRIDGE_PROJECT}"
                "${SAGA_ROOT}/Engine/Managed/SagaScript.RuntimeBridge/SagaScript.cs"
                "${SAGA_ROOT}/Engine/Managed/SagaScript.RuntimeBridge/ScriptContext.cs"
                "${SAGA_ROOT}/Engine/Managed/SagaScript.RuntimeBridge/NativeBridge.cs"
            COMMENT "Building managed SagaScript runtime bridge"
            VERBATIM
        )
        add_custom_target(SagaScriptRuntimeBridge
            DEPENDS "${SAGA_SCRIPT_RUNTIME_BRIDGE_ASSEMBLY}"
        )
    endif()

    # --- Unit tests ---------------------------------------------------------

    add_executable(SagaUnitTests ${UNIT_TEST_SOURCES} ${EDITORLAB_SOURCES})
    saga_link_thirdparty(SagaUnitTests)
    target_link_libraries(SagaUnitTests PRIVATE
        SagaEngine
        SagaDiagnostics
        SagaRuntimeLib
        SagaServerLib
        SagaShared
        SagaCollaboration
        SagaAssetPipelineLib
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

    # --- Saga editor composition tool tests --------------------------------
    if(SAGA_EDITOR_COMPOSITION_TEST_SOURCES)
        if(SAGA_WITH_SDE AND TARGET SagaEditorCompositionLib)
            add_executable(SagaEditorCompositionTests
                ${SAGA_EDITOR_COMPOSITION_TEST_SOURCES}
            )
            target_link_libraries(SagaEditorCompositionTests PRIVATE
                SagaEditorCompositionLib
                SagaEditorLib
                GTest::gtest
                GTest::gmock
                GTest::gtest_main
            )
            target_include_directories(SagaEditorCompositionTests PRIVATE
                ${SAGA_ROOT}/Tools/SagaEditorComposition/include
                ${SAGA_ROOT}/Editor/include
                $<TARGET_PROPERTY:GTest::gtest,INTERFACE_INCLUDE_DIRECTORIES>
            )
            target_compile_definitions(SagaEditorCompositionTests PRIVATE
                SAGA_SOURCE_ROOT="${SAGA_ROOT}"
            )
            set_target_properties(SagaEditorCompositionTests PROPERTIES
                FOLDER "Tests/Tools"
            )
            add_test(NAME SagaEditorCompositionTests COMMAND SagaEditorCompositionTests)
            set_tests_properties(SagaEditorCompositionTests PROPERTIES
                LABELS "tools;sde;editor-composition"
            )
            if(TARGET saga-editor-composition-compiler)
                add_test(
                    NAME SagaEditorCompositionCompilerHelp
                    COMMAND saga-editor-composition-compiler --help
                )
                set_tests_properties(SagaEditorCompositionCompilerHelp PROPERTIES
                    LABELS "tools;sde;editor-composition"
                )
            endif()
        else()
            message(STATUS
                "SagaEditorCompositionTests skipped because SAGA_WITH_SDE is OFF")
        endif()
    endif()

    # --- Saga pipeline tool tests ------------------------------------------
    if(SAGA_PIPELINE_TEST_SOURCES AND TARGET SagaPipelineLib)
        add_executable(SagaPipelineTests
            ${SAGA_PIPELINE_TEST_SOURCES}
        )
        target_link_libraries(SagaPipelineTests PRIVATE
            SagaPipelineLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(SagaPipelineTests PRIVATE
            ${SAGA_ROOT}/Tools/SagaPipeline/include
            $<TARGET_PROPERTY:GTest::gtest,INTERFACE_INCLUDE_DIRECTORIES>
        )
        target_compile_definitions(SagaPipelineTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
        )
        if(TARGET saga-editor-composition-compiler)
            add_dependencies(SagaPipelineTests saga-editor-composition-compiler)
            target_compile_definitions(SagaPipelineTests PRIVATE
                SAGA_EDITOR_COMPOSITION_COMPILER="$<TARGET_FILE:saga-editor-composition-compiler>"
            )
        endif()
        set_target_properties(SagaPipelineTests PROPERTIES
            FOLDER "Tests/Tools"
        )
        add_test(NAME SagaPipelineTests COMMAND SagaPipelineTests)
        set_tests_properties(SagaPipelineTests PROPERTIES
            LABELS "tools;pipeline"
        )
    endif()

    # --- Package staging tests ----------------------------------------------
    #
    # Build the package-staging slice without SagaProductLib or SagaEngine so
    # script artifact placement can be verified without a full product rebuild.
    if(EXISTS "${SAGA_PACKAGE_STAGING_TEST_SOURCE}")
        add_executable(SagaPackageStagingTests
            ${SAGA_PACKAGE_STAGING_TEST_SOURCE}
            ${SAGA_ROOT}/Apps/Saga/SagaPackageStaging.cpp
            ${SAGA_ROOT}/Apps/Saga/SagaProjectSystem.cpp
            ${SAGA_ROOT}/Apps/Saga/SagaPublishReadiness.cpp
            ${SAGA_ROOT}/Apps/Saga/SagaSessionModel.cpp
            ${SAGA_ROOT}/Engine/Private/SagaEngine/Packages/PackageManifestLoader.cpp
        )
        target_link_libraries(SagaPackageStagingTests PRIVATE
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
            nlohmann_json::nlohmann_json
        )
        target_include_directories(SagaPackageStagingTests PRIVATE
            ${SAGA_ROOT}/Apps/Saga
            ${SAGA_ROOT}/Engine/Public
            ${SAGA_ROOT}/Shared/include
        )
        target_compile_definitions(SagaPackageStagingTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
        )
        add_test(NAME SagaPackageStagingTests COMMAND SagaPackageStagingTests)
        set_tests_properties(SagaPackageStagingTests PROPERTIES LABELS "product;package")
    endif()

    # --- Script lifecycle tests ---------------------------------------------
    #
    # Build the initial script-host lifecycle seam without the broad runtime,
    # server, editor, or product test graph.
    if(EXISTS "${SAGA_SCRIPT_LIFECYCLE_TEST_SOURCE}")
        add_executable(SagaScriptLifecycleTests
            ${SAGA_SCRIPT_LIFECYCLE_TEST_SOURCE}
            ${SAGA_ROOT}/Engine/Private/SagaEngine/Scripting/ScriptLifecycleService.cpp
            ${SAGA_ROOT}/Engine/Private/SagaEngine/Scripting/ScriptPackageValidator.cpp
        )
        target_link_libraries(SagaScriptLifecycleTests PRIVATE
            SagaShared
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
            nlohmann_json::nlohmann_json
            OpenSSL::Crypto
        )
        target_include_directories(SagaScriptLifecycleTests PRIVATE
            ${SAGA_ROOT}/Engine/Public
            ${SAGA_ROOT}/Shared/include
            ${SAGA_OPENSSL_INCLUDE_DIRS}
        )
        target_compile_definitions(SagaScriptLifecycleTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
        )
        add_test(NAME SagaScriptLifecycleTests COMMAND SagaScriptLifecycleTests)
        set_tests_properties(SagaScriptLifecycleTests PROPERTIES LABELS "runtime;scripting")
    endif()

    # --- Script binding runtime tests ---------------------------------------
    if(EXISTS "${SAGA_SCRIPT_BINDING_RUNTIME_TEST_SOURCE}")
        add_executable(ScriptBindingRuntimeTests
            ${SAGA_SCRIPT_BINDING_RUNTIME_TEST_SOURCE}
            ${SAGA_ROOT}/Engine/Private/SagaEngine/Scripting/ScriptBindingManifestLoader.cpp
            ${SAGA_ROOT}/Engine/Private/SagaEngine/Scripting/ScriptBindingRuntime.cpp
            ${SAGA_ROOT}/Engine/Private/SagaEngine/Scripting/ScriptLifecycleService.cpp
            ${SAGA_ROOT}/Engine/Private/SagaEngine/Scripting/ScriptPackageValidator.cpp
        )
        target_link_libraries(ScriptBindingRuntimeTests PRIVATE
            SagaShared
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
            nlohmann_json::nlohmann_json
            OpenSSL::Crypto
        )
        target_include_directories(ScriptBindingRuntimeTests PRIVATE
            ${SAGA_ROOT}/Engine/Public
            ${SAGA_ROOT}/Shared/include
            ${SAGA_OPENSSL_INCLUDE_DIRS}
        )
        target_compile_definitions(ScriptBindingRuntimeTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
        )
        add_test(NAME ScriptBindingRuntimeTests COMMAND ScriptBindingRuntimeTests)
        set_tests_properties(ScriptBindingRuntimeTests PROPERTIES LABELS "runtime;scripting")
    endif()

    # --- C# script host bridge tests ---------------------------------------
    if(EXISTS "${SAGA_CSHARP_SCRIPT_HOST_TEST_SOURCE}" AND
       TARGET SagaScriptRuntimeBridge)
        set(SAGA_CSHARP_SCRIPT_FIXTURE_PROJECT
            "${SAGA_ROOT}/Tests/Fixtures/CSharpScriptHost/CSharpScriptHostFixture.csproj"
        )
        set(SAGA_CSHARP_SCRIPT_FIXTURE_OUTPUT_DIR
            "${CMAKE_BINARY_DIR}/Managed/CSharpScriptHostFixture"
        )
        set(SAGA_CSHARP_SCRIPT_FIXTURE_ASSEMBLY
            "${SAGA_CSHARP_SCRIPT_FIXTURE_OUTPUT_DIR}/CSharpScriptHostFixture.scripts.dll"
        )
        add_custom_command(
            OUTPUT "${SAGA_CSHARP_SCRIPT_FIXTURE_ASSEMBLY}"
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "${SAGA_CSHARP_SCRIPT_FIXTURE_OUTPUT_DIR}"
            COMMAND ${CMAKE_COMMAND} -E env
                DOTNET_CLI_HOME=/tmp/sagascript-dotnet-home
                NUGET_PACKAGES=/tmp/sagascript-nuget
                DOTNET_CLI_TELEMETRY_OPTOUT=1
                "${SAGA_DOTNET_EXECUTABLE}" build
                "${SAGA_CSHARP_SCRIPT_FIXTURE_PROJECT}"
                --configuration Release
                --output "${SAGA_CSHARP_SCRIPT_FIXTURE_OUTPUT_DIR}"
                --nologo
                --verbosity minimal
            DEPENDS
                SagaScriptRuntimeBridge
                "${SAGA_CSHARP_SCRIPT_FIXTURE_PROJECT}"
                "${SAGA_ROOT}/Tests/Fixtures/CSharpScriptHost/TestScripts.cs"
            COMMENT "Building C# script host fixture assembly"
            VERBATIM
        )
        add_custom_target(CSharpScriptHostFixture
            DEPENDS "${SAGA_CSHARP_SCRIPT_FIXTURE_ASSEMBLY}"
        )

        add_executable(CSharpScriptHostTests
            ${SAGA_CSHARP_SCRIPT_HOST_TEST_SOURCE}
            ${SAGA_ROOT}/Engine/Private/SagaEngine/Scripting/CSharpScriptHost.cpp
            ${SAGA_ROOT}/Engine/Private/SagaEngine/Scripting/ScriptLifecycleService.cpp
            ${SAGA_ROOT}/Engine/Private/SagaEngine/Scripting/ScriptPackageValidator.cpp
        )
        add_dependencies(CSharpScriptHostTests
            SagaScriptRuntimeBridge
            CSharpScriptHostFixture
        )
        target_link_libraries(CSharpScriptHostTests PRIVATE
            SagaShared
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
            nlohmann_json::nlohmann_json
            OpenSSL::Crypto
            ${SAGA_DOTNET_NETHOST_LIBRARY}
            ${CMAKE_DL_LIBS}
        )
        target_include_directories(CSharpScriptHostTests PRIVATE
            ${SAGA_ROOT}/Engine/Public
            ${SAGA_ROOT}/Shared/include
            ${SAGA_OPENSSL_INCLUDE_DIRS}
            ${SAGA_DOTNET_HOST_INCLUDE_DIR}
        )
        target_compile_definitions(CSharpScriptHostTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
            SAGA_SCRIPT_RUNTIME_BRIDGE_ASSEMBLY="${SAGA_SCRIPT_RUNTIME_BRIDGE_ASSEMBLY}"
            SAGA_CSHARP_SCRIPT_FIXTURE_DIR="${SAGA_CSHARP_SCRIPT_FIXTURE_OUTPUT_DIR}"
            SAGA_DOTNET_ROOT="${SAGA_DOTNET_ROOT}"
        )
        add_test(NAME CSharpScriptHostTests COMMAND CSharpScriptHostTests)
        set_tests_properties(CSharpScriptHostTests PROPERTIES LABELS "runtime;scripting;csharp")
    endif()

    # --- Editorless C# gameplay proof tests --------------------------------
    if(EXISTS "${SAGA_CSHARP_GAMEPLAY_PROOF_TEST_SOURCE}" AND
       TARGET SagaScriptRuntimeBridge)
        add_executable(CSharpGameplayProofTests
            ${SAGA_CSHARP_GAMEPLAY_PROOF_TEST_SOURCE}
            ${SAGA_ROOT}/Engine/Private/SagaEngine/ECS/ComponentRegistration.cpp
            ${SAGA_ROOT}/Engine/Private/SagaEngine/ECS/ComponentRegistry.cpp
            ${SAGA_ROOT}/Engine/Private/SagaEngine/Simulation/WorldState.cpp
            ${SAGA_ROOT}/Engine/Private/SagaEngine/Scripting/CSharpScriptHost.cpp
            ${SAGA_ROOT}/Engine/Private/SagaEngine/Scripting/ScriptLifecycleService.cpp
            ${SAGA_ROOT}/Engine/Private/SagaEngine/Scripting/ScriptPackageValidator.cpp
            ${SAGA_ROOT}/Engine/Private/SagaEngine/Scripting/WorldStateScriptWorld.cpp
        )
        add_dependencies(CSharpGameplayProofTests
            SagaScriptRuntimeBridge
        )
        target_link_libraries(CSharpGameplayProofTests PRIVATE
            SagaCoreLog
            SagaShared
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
            nlohmann_json::nlohmann_json
            OpenSSL::Crypto
            ${SAGA_DOTNET_NETHOST_LIBRARY}
            ${CMAKE_DL_LIBS}
        )
        target_include_directories(CSharpGameplayProofTests PRIVATE
            ${SAGA_ROOT}/Engine/Public
            ${SAGA_ROOT}/Shared/include
            ${SAGA_OPENSSL_INCLUDE_DIRS}
            ${SAGA_DOTNET_HOST_INCLUDE_DIR}
        )
        target_compile_definitions(CSharpGameplayProofTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
            SAGA_SCRIPT_RUNTIME_BRIDGE_ASSEMBLY="${SAGA_SCRIPT_RUNTIME_BRIDGE_ASSEMBLY}"
            SAGA_DOTNET_EXECUTABLE="${SAGA_DOTNET_EXECUTABLE}"
            SAGA_DOTNET_ROOT="${SAGA_DOTNET_ROOT}"
        )
        add_test(NAME CSharpGameplayProofTests COMMAND CSharpGameplayProofTests)
        set_tests_properties(CSharpGameplayProofTests PROPERTIES LABELS "runtime;scripting;csharp")
    endif()

    # --- Script binding runtime thin runner --------------------------------
    if(TARGET SagaScriptRuntimeBridge AND
       EXISTS "${SAGA_ROOT}/Tests/Tools/ScriptBindingRuntimeRunner.cpp")
        add_executable(ScriptBindingRuntimeRunner
            ${SAGA_ROOT}/Tests/Tools/ScriptBindingRuntimeRunner.cpp
        )
        add_dependencies(ScriptBindingRuntimeRunner SagaScriptRuntimeBridge)
        target_link_libraries(ScriptBindingRuntimeRunner PRIVATE
            SagaEngine
            SagaShared
            nlohmann_json::nlohmann_json
            OpenSSL::Crypto
            ${SAGA_DOTNET_NETHOST_LIBRARY}
            ${CMAKE_DL_LIBS}
        )
        target_include_directories(ScriptBindingRuntimeRunner PRIVATE
            ${SAGA_ROOT}/Engine/Public
            ${SAGA_ROOT}/Shared/include
            ${SAGA_OPENSSL_INCLUDE_DIRS}
            ${SAGA_DOTNET_HOST_INCLUDE_DIR}
        )
        target_compile_definitions(ScriptBindingRuntimeRunner PRIVATE
            SAGA_SCRIPT_RUNTIME_BRIDGE_ASSEMBLY="${SAGA_SCRIPT_RUNTIME_BRIDGE_ASSEMBLY}"
            SAGA_DOTNET_ROOT="${SAGA_DOTNET_ROOT}"
        )
        set_target_properties(ScriptBindingRuntimeRunner PROPERTIES
            FOLDER "Tests/Tools"
        )
        if(TARGET ScriptBindingRuntimeTests)
            add_dependencies(ScriptBindingRuntimeTests ScriptBindingRuntimeRunner)
            target_compile_definitions(ScriptBindingRuntimeTests PRIVATE
                SAGA_SCRIPT_BINDING_RUNTIME_RUNNER="$<TARGET_FILE:ScriptBindingRuntimeRunner>"
                SAGA_SCRIPT_RUNTIME_BRIDGE_ASSEMBLY="${SAGA_SCRIPT_RUNTIME_BRIDGE_ASSEMBLY}"
                SAGA_DOTNET_EXECUTABLE="${SAGA_DOTNET_EXECUTABLE}"
                SAGA_DOTNET_ROOT="${SAGA_DOTNET_ROOT}"
            )
        endif()
    endif()

    # --- Saga product tests -------------------------------------------------
    #
    # Keep product orchestration tests in a separate target so package-staging
    # checks can run in SDE-enabled builds without rebuilding every unit test.
    if(SAGA_WITH_SDE AND SAGA_PRODUCT_TEST_SOURCES)
        add_executable(SagaProductTests ${SAGA_PRODUCT_TEST_SOURCES})
        saga_link_thirdparty(SagaProductTests)
        target_link_libraries(SagaProductTests PRIVATE
            SagaProductLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(SagaProductTests PRIVATE ${SAGA_TEST_INCLUDE_DIRS})
        target_compile_definitions(SagaProductTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
        )
        add_test(NAME SagaProductTests COMMAND SagaProductTests)
        set_tests_properties(SagaProductTests PROPERTIES LABELS "product")
    endif()

    # --- Architecture boundary tests ----------------------------------------
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
        SDL2::SDL2          # DiligentBackend integration tests need a hidden window
    )
    target_include_directories(SagaIntegrationTests PRIVATE ${SAGA_TEST_INCLUDE_DIRS})
    add_test(NAME IntegrationTests COMMAND SagaIntegrationTests)

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
        set_tests_properties(StressTests PROPERTIES LABELS "stress")
    endif()

endfunction()
