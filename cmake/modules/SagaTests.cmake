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

    set(SAGA_STRESS_ARENA_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Tools/SagaStressArenaTests.cpp"
    )
    set(SAGA_CHAOS_LAB_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Tools/SagaChaosLabTests.cpp"
    )
    set(SAGA_STATE_CHECK_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Tools/SagaStateCheckTests.cpp"
    )
    set(EDITOR_AUTHORING_SPINE_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Editor/EditorAuthoringSpineTests.cpp"
    )
    set(COLLABORATION_MODEL_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Collaboration/CollaborationModelTests.cpp"
    )
    set(MULTIPLAYER_SANDBOX_HEADLESS_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Samples/MultiplayerSandboxHeadlessTests.cpp"
    )

    file(GLOB_RECURSE SAGA_PRODUCT_TEST_SOURCES CONFIGURE_DEPENDS
        "${SAGA_ROOT}/Tests/Unit/Saga/*.cpp"
    )

    set(SAGA_PUBLISH_READINESS_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Saga/SagaPublishReadinessTests.cpp"
    )
    set(SAGA_PACKAGE_STAGING_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Saga/SagaPackageStagingTests.cpp"
    )
    set(SAGA_ASSET_IDENTITY_RUNTIME_CONTRACT_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/AssetPipeline/AssetIdentityRuntimeContractTests.cpp"
    )
    set(SAGA_ASSET_IDENTITY_MANIFEST_WRITER_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/AssetPipeline/AssetIdentityManifestWriterTests.cpp"
    )
    set(SAGA_ASSET_MANIFEST_WRITER_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/AssetPipeline/AssetManifestWriterTests.cpp"
    )
    set(SAGA_PACKAGE_MANIFEST_WRITER_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/AssetPipeline/PackageManifestWriterTests.cpp"
    )
    set(SAGA_GENERATED_RUNTIME_SMOKE_MANIFEST_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/AssetPipeline/GeneratedRuntimeSmokeManifestTests.cpp"
    )
    set(SAGA_GENERATED_RUNTIME_SMOKE_PACKAGE_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/AssetPipeline/GeneratedRuntimeSmokePackageTests.cpp"
    )
    set(SAGA_DIAGNOSTIC_FOUNDATION_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Diagnostics/DiagnosticFoundationTests.cpp"
    )
    set(SAGA_GRAPHICS_PUBLIC_CONSUMER_LINK_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Link/SagaGraphicsPublicConsumerLinkTest.cpp"
    )
    set(SAGA_GRAPHICS_INSTALLED_CONSUMER_TEST_SCRIPT
        "${SAGA_ROOT}/Tests/Link/SagaGraphicsInstalledConsumerTest.cmake"
    )
    set(SAGA_GRAPHICS_INSTALLED_CONSUMER_SOURCE_DIR
        "${SAGA_ROOT}/Tests/Link/SagaGraphicsInstalledConsumerProject"
    )
    set(SAGA_DIAGNOSTIC_REPORT_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Diagnostics/DiagnosticReportTests.cpp"
    )
    set(SAGA_DIAGNOSTIC_RELIABILITY_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Diagnostics/DiagnosticReliabilityTests.cpp"
    )
    set(SAGA_DIAGNOSTIC_MEMORY_RESOURCE_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Diagnostics/DiagnosticMemoryResourceTests.cpp"
    )
    set(SAGA_FAULT_BOUNDARY_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Diagnostics/FaultBoundaryTests.cpp"
    )

    set(SAGA_SCRIPT_LIFECYCLE_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Runtime/ScriptLifecycleServiceTests.cpp"
    )
    set(SAGA_SCRIPT_BINDING_RUNTIME_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Runtime/ScriptBindingRuntimeTests.cpp"
    )
    set(SAGA_RUNTIME_STARTUP_PREFLIGHT_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Runtime/RuntimeStartupPreflightTests.cpp"
    )
    set(SAGA_RUNTIME_STARTUP_SESSION_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Runtime/RuntimeStartupSessionTests.cpp"
    )
    set(SAGA_RUNTIME_STARTUP_DIAGNOSTICS_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Runtime/RuntimeStartupDiagnosticsTests.cpp"
    )
    set(SAGA_RUNTIME_ASSET_CATALOG_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Runtime/RuntimeAssetCatalogTests.cpp"
    )
    set(SAGA_RUNTIME_ASSET_BOOTSTRAP_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Runtime/RuntimeAssetBootstrapTests.cpp"
    )
    set(SAGA_RUNTIME_ASSET_STARTUP_BOOTSTRAP_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Runtime/RuntimeAssetStartupBootstrapTests.cpp"
    )
    set(SAGA_RUNTIME_ASSET_BOOTSTRAP_DIAGNOSTICS_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Runtime/RuntimeAssetBootstrapDiagnosticsTests.cpp"
    )
    set(SAGA_RUNTIME_PACKAGE_SMOKE_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Runtime/RuntimePackageSmokeTests.cpp"
    )
    set(SAGA_RUNTIME_SERVICE_LIFECYCLE_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Runtime/RuntimeServiceLifecycleTests.cpp"
    )
    set(SAGA_RUNTIME_SERVICE_REGISTRY_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Runtime/RuntimeServiceRegistryTests.cpp"
    )
    set(SAGA_RUNTIME_SERVICE_REGISTRY_DIAGNOSTICS_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Runtime/RuntimeServiceRegistryDiagnosticsTests.cpp"
    )
    set(SAGA_CSHARP_SCRIPT_HOST_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Runtime/CSharpScriptHostTests.cpp"
    )
    set(SAGA_CSHARP_GAMEPLAY_PROOF_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Runtime/CSharpGameplayProofTests.cpp"
    )
    set(STARTER_ARENA_RUNTIME_SMOKE_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Runtime/StarterArenaRuntimeSmokeTests.cpp"
    )
    set(STARTER_ARENA_PLAYABLE_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Runtime/StarterArenaPlayableTests.cpp"
    )
    set(SAGA_ACTOR_OWNERSHIP_REGISTRY_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Server/ActorOwnershipRegistryTests.cpp"
    )
    set(SAGA_AUTHORITATIVE_MOVEMENT_CORE_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Server/AuthoritativeMovementCoreTests.cpp"
    )
    set(SAGA_AUTHORITATIVE_MOVEMENT_INPUT_ADAPTER_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Server/AuthoritativeMovementInputAdapterTests.cpp"
    )
    set(SAGA_AUTHORITATIVE_MOVEMENT_COMMAND_INTAKE_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Server/AuthoritativeMovementCommandIntakeTests.cpp"
    )
    set(SAGA_SERVER_PACKET_NORMALIZATION_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Networking/ServerPacketNormalizationTests.cpp"
    )
    set(SAGA_NETWORK_CHAOS_LAYER_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Networking/NetworkChaosLayerTests.cpp"
    )
    set(SAGA_ZONE_SERVER_PACKET_DRAIN_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Server/ZoneServerPacketDrainTests.cpp"
    )
    set(SAGA_ZONE_SERVER_MOVEMENT_AUTHORITY_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Server/ZoneServerMovementAuthorityTests.cpp"
    )
    set(SAGA_ZONE_SERVER_DIAGNOSTICS_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Server/ZoneServerDiagnosticsTests.cpp"
    )
    set(SAGA_SERVER_LIFECYCLE_DIAGNOSTICS_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Server/ServerLifecycleDiagnosticsTests.cpp"
    )
    set(SAGA_MOVEMENT_DIRTY_REPLICATION_BRIDGE_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Server/MovementDirtyReplicationBridgeTests.cpp"
    )

    if(SAGA_PRODUCT_TEST_SOURCES)
        list(REMOVE_ITEM UNIT_TEST_SOURCES
            ${SAGA_PRODUCT_TEST_SOURCES}
        )
        list(REMOVE_ITEM SAGA_PRODUCT_TEST_SOURCES
            ${SAGA_PUBLISH_READINESS_TEST_SOURCE}
            ${SAGA_PACKAGE_STAGING_TEST_SOURCE}
        )
    endif()

    list(REMOVE_ITEM UNIT_TEST_SOURCES
        ${SAGA_ASSET_IDENTITY_RUNTIME_CONTRACT_TEST_SOURCE}
        ${SAGA_ASSET_IDENTITY_MANIFEST_WRITER_TEST_SOURCE}
        ${SAGA_ASSET_MANIFEST_WRITER_TEST_SOURCE}
        ${SAGA_PACKAGE_MANIFEST_WRITER_TEST_SOURCE}
        ${SAGA_GENERATED_RUNTIME_SMOKE_MANIFEST_TEST_SOURCE}
        ${SAGA_GENERATED_RUNTIME_SMOKE_PACKAGE_TEST_SOURCE}
        ${SAGA_DIAGNOSTIC_FOUNDATION_TEST_SOURCE}
        ${SAGA_DIAGNOSTIC_REPORT_TEST_SOURCE}
        ${SAGA_DIAGNOSTIC_RELIABILITY_TEST_SOURCE}
        ${SAGA_DIAGNOSTIC_MEMORY_RESOURCE_TEST_SOURCE}
        ${SAGA_FAULT_BOUNDARY_TEST_SOURCE}
        ${SAGA_STRESS_ARENA_TEST_SOURCE}
        ${SAGA_CHAOS_LAB_TEST_SOURCE}
        ${SAGA_STATE_CHECK_TEST_SOURCE}
        ${MULTIPLAYER_SANDBOX_HEADLESS_TEST_SOURCE}
        ${SAGA_SCRIPT_LIFECYCLE_TEST_SOURCE}
        ${SAGA_SCRIPT_BINDING_RUNTIME_TEST_SOURCE}
        ${SAGA_RUNTIME_STARTUP_PREFLIGHT_TEST_SOURCE}
        ${SAGA_RUNTIME_STARTUP_SESSION_TEST_SOURCE}
        ${SAGA_RUNTIME_STARTUP_DIAGNOSTICS_TEST_SOURCE}
        ${SAGA_RUNTIME_ASSET_CATALOG_TEST_SOURCE}
        ${SAGA_RUNTIME_ASSET_BOOTSTRAP_TEST_SOURCE}
        ${SAGA_RUNTIME_ASSET_STARTUP_BOOTSTRAP_TEST_SOURCE}
        ${SAGA_RUNTIME_ASSET_BOOTSTRAP_DIAGNOSTICS_TEST_SOURCE}
        ${SAGA_RUNTIME_PACKAGE_SMOKE_TEST_SOURCE}
        ${SAGA_RUNTIME_SERVICE_LIFECYCLE_TEST_SOURCE}
        ${SAGA_RUNTIME_SERVICE_REGISTRY_TEST_SOURCE}
        ${SAGA_RUNTIME_SERVICE_REGISTRY_DIAGNOSTICS_TEST_SOURCE}
        ${SAGA_CSHARP_SCRIPT_HOST_TEST_SOURCE}
        ${SAGA_CSHARP_GAMEPLAY_PROOF_TEST_SOURCE}
        ${STARTER_ARENA_RUNTIME_SMOKE_TEST_SOURCE}
        ${STARTER_ARENA_PLAYABLE_TEST_SOURCE}
        ${SAGA_ACTOR_OWNERSHIP_REGISTRY_TEST_SOURCE}
        ${SAGA_AUTHORITATIVE_MOVEMENT_CORE_TEST_SOURCE}
        ${SAGA_AUTHORITATIVE_MOVEMENT_INPUT_ADAPTER_TEST_SOURCE}
        ${SAGA_AUTHORITATIVE_MOVEMENT_COMMAND_INTAKE_TEST_SOURCE}
        ${SAGA_SERVER_PACKET_NORMALIZATION_TEST_SOURCE}
        ${SAGA_NETWORK_CHAOS_LAYER_TEST_SOURCE}
        ${SAGA_ZONE_SERVER_PACKET_DRAIN_TEST_SOURCE}
        ${SAGA_ZONE_SERVER_MOVEMENT_AUTHORITY_TEST_SOURCE}
        ${SAGA_ZONE_SERVER_DIAGNOSTICS_TEST_SOURCE}
        ${SAGA_SERVER_LIFECYCLE_DIAGNOSTICS_TEST_SOURCE}
        ${SAGA_MOVEMENT_DIRTY_REPLICATION_BRIDGE_TEST_SOURCE}
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
        ${SAGA_ROOT}/Apps/Sandbox/include
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
        SagaDiligentBackend
        SagaGraphicsPrivate
        SagaDiagnostics
        SagaRuntimeLib
        SagaServerLib
        SagaShared
        SagaCollaboration
        SagaAssetPipelineLib
        SagaBackend
        SagaSandboxLib
        SagaEditorLib    # needed by Tests/Unit/Editor/* (block authoring,
                         # InspectorEditing, persona, viewport, etc.)
        GTest::gtest
        GTest::gmock
        GTest::gtest_main
        rapidcheck::rapidcheck
    )
    target_link_libraries(SagaUnitTests PRIVATE SagaProductLib)
    target_include_directories(SagaUnitTests PRIVATE ${SAGA_TEST_INCLUDE_DIRS})
    target_include_directories(SagaUnitTests PRIVATE
        ${SAGA_ROOT}/Engine/Private
    )
    target_compile_definitions(SagaUnitTests PRIVATE
        SAGA_SOURCE_ROOT="${SAGA_ROOT}"
        SAGA_BUILD_ROOT="${CMAKE_BINARY_DIR}"
    )
    add_test(NAME UnitTests COMMAND SagaUnitTests)
    set_tests_properties(UnitTests PROPERTIES
        LABELS "unit;runtime;server;networking;replication;asset;editor"
    )

    # --- Public graphics consumer link test --------------------------------
    if(EXISTS "${SAGA_GRAPHICS_PUBLIC_CONSUMER_LINK_TEST_SOURCE}")
        add_executable(SagaGraphicsPublicConsumerLinkTest
            "${SAGA_GRAPHICS_PUBLIC_CONSUMER_LINK_TEST_SOURCE}"
        )
        saga_apply_compiler_flags(SagaGraphicsPublicConsumerLinkTest)
        target_link_libraries(SagaGraphicsPublicConsumerLinkTest PRIVATE
            SagaGraphics
        )
        set_target_properties(SagaGraphicsPublicConsumerLinkTest PROPERTIES
            FOLDER "Tests/Link"
        )
        add_test(
            NAME SagaGraphicsPublicConsumerLinkTest
            COMMAND SagaGraphicsPublicConsumerLinkTest)
        set_tests_properties(SagaGraphicsPublicConsumerLinkTest PROPERTIES
            LABELS "link;graphics;public")
    endif()

    if(EXISTS "${SAGA_GRAPHICS_INSTALLED_CONSUMER_TEST_SCRIPT}")
        add_test(
            NAME SagaGraphicsInstalledConsumerTest
            COMMAND ${CMAKE_COMMAND}
                -DSAGA_BUILD_DIR=${CMAKE_BINARY_DIR}
                -DSAGA_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/installed/SagaGraphicsConsumer
                -DSAGA_DOWNSTREAM_SOURCE_DIR=${SAGA_GRAPHICS_INSTALLED_CONSUMER_SOURCE_DIR}
                -DSAGA_DOWNSTREAM_BINARY_DIR=${CMAKE_BINARY_DIR}/installed-consumer-build/SagaGraphics
                -DSAGA_SOURCE_ROOT=${SAGA_ROOT}
                -P ${SAGA_GRAPHICS_INSTALLED_CONSUMER_TEST_SCRIPT})
        set_tests_properties(SagaGraphicsInstalledConsumerTest PROPERTIES
            LABELS "link;graphics;public;install")
    endif()

    # --- Diagnostics foundation tests ---------------------------------------
    if(EXISTS "${SAGA_DIAGNOSTIC_FOUNDATION_TEST_SOURCE}")
        add_executable(DiagnosticFoundationTests
            ${SAGA_DIAGNOSTIC_FOUNDATION_TEST_SOURCE}
        )
        target_link_libraries(DiagnosticFoundationTests PRIVATE
            SagaDiagnostics
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(DiagnosticFoundationTests PRIVATE
            ${SAGA_ROOT}/Engine/Public
        )
        add_test(NAME DiagnosticFoundationTests COMMAND DiagnosticFoundationTests)
        set_tests_properties(DiagnosticFoundationTests PROPERTIES
            LABELS "unit;diagnostics")
    endif()

    # --- Diagnostics report tests ------------------------------------------
    if(EXISTS "${SAGA_DIAGNOSTIC_REPORT_TEST_SOURCE}")
        add_executable(DiagnosticReportTests
            ${SAGA_DIAGNOSTIC_REPORT_TEST_SOURCE}
        )
        target_link_libraries(DiagnosticReportTests PRIVATE
            SagaDiagnostics
            nlohmann_json::nlohmann_json
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(DiagnosticReportTests PRIVATE
            ${SAGA_ROOT}/Engine/Public
        )
        add_test(NAME DiagnosticReportTests COMMAND DiagnosticReportTests)
        set_tests_properties(DiagnosticReportTests PROPERTIES
            LABELS "unit;diagnostics")
    endif()

    # --- Diagnostics reliability tests -------------------------------------
    if(EXISTS "${SAGA_DIAGNOSTIC_RELIABILITY_TEST_SOURCE}")
        add_executable(DiagnosticReliabilityTests
            ${SAGA_DIAGNOSTIC_RELIABILITY_TEST_SOURCE}
        )
        target_link_libraries(DiagnosticReliabilityTests PRIVATE
            SagaDiagnostics
            nlohmann_json::nlohmann_json
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(DiagnosticReliabilityTests PRIVATE
            ${SAGA_ROOT}/Engine/Public
        )
        add_test(NAME DiagnosticReliabilityTests
            COMMAND DiagnosticReliabilityTests)
        set_tests_properties(DiagnosticReliabilityTests PROPERTIES
            LABELS "unit;diagnostics")
    endif()

    # --- Diagnostics memory/resource tests ---------------------------------
    if(EXISTS "${SAGA_DIAGNOSTIC_MEMORY_RESOURCE_TEST_SOURCE}")
        add_executable(DiagnosticMemoryResourceTests
            ${SAGA_DIAGNOSTIC_MEMORY_RESOURCE_TEST_SOURCE}
        )
        target_link_libraries(DiagnosticMemoryResourceTests PRIVATE
            SagaDiagnostics
            nlohmann_json::nlohmann_json
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(DiagnosticMemoryResourceTests PRIVATE
            ${SAGA_ROOT}/Engine/Public
        )
        add_test(NAME DiagnosticMemoryResourceTests
            COMMAND DiagnosticMemoryResourceTests)
        set_tests_properties(DiagnosticMemoryResourceTests PROPERTIES
            LABELS "unit;diagnostics")
    endif()

    # --- Fault boundary diagnostics tests ----------------------------------
    if(EXISTS "${SAGA_FAULT_BOUNDARY_TEST_SOURCE}")
        add_executable(FaultBoundaryTests
            ${SAGA_FAULT_BOUNDARY_TEST_SOURCE}
        )
        target_link_libraries(FaultBoundaryTests PRIVATE
            SagaDiagnostics
            nlohmann_json::nlohmann_json
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(FaultBoundaryTests PRIVATE
            ${SAGA_ROOT}/Engine/Public
        )
        add_test(NAME FaultBoundaryTests COMMAND FaultBoundaryTests)
        set_tests_properties(FaultBoundaryTests PROPERTIES
            LABELS "unit;diagnostics")
    endif()

    # --- Actor ownership registry tests -------------------------------------
    if(EXISTS "${SAGA_ACTOR_OWNERSHIP_REGISTRY_TEST_SOURCE}")
        add_executable(ActorOwnershipRegistryTests
            ${SAGA_ACTOR_OWNERSHIP_REGISTRY_TEST_SOURCE}
        )
        target_link_libraries(ActorOwnershipRegistryTests PRIVATE
            SagaServerLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(ActorOwnershipRegistryTests PRIVATE
            ${SAGA_ROOT}/Server/include
            ${SAGA_ROOT}/Engine/Public
            $<TARGET_PROPERTY:GTest::gtest,INTERFACE_INCLUDE_DIRECTORIES>
        )
        add_test(NAME ActorOwnershipRegistryTests
            COMMAND ActorOwnershipRegistryTests)
        set_tests_properties(ActorOwnershipRegistryTests PROPERTIES
            LABELS "unit;server"
        )
    endif()

    # --- Authoritative movement core tests ---------------------------------
    if(EXISTS "${SAGA_AUTHORITATIVE_MOVEMENT_CORE_TEST_SOURCE}")
        add_executable(AuthoritativeMovementCoreTests
            ${SAGA_AUTHORITATIVE_MOVEMENT_CORE_TEST_SOURCE}
        )
        target_link_libraries(AuthoritativeMovementCoreTests PRIVATE
            SagaServerLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(AuthoritativeMovementCoreTests PRIVATE
            ${SAGA_ROOT}/Server/include
            $<TARGET_PROPERTY:GTest::gtest,INTERFACE_INCLUDE_DIRECTORIES>
        )
        add_test(NAME AuthoritativeMovementCoreTests
            COMMAND AuthoritativeMovementCoreTests)
        set_tests_properties(AuthoritativeMovementCoreTests PROPERTIES
            LABELS "unit;server"
        )
    endif()

    # --- Authoritative movement input adapter tests -------------------------
    if(EXISTS "${SAGA_AUTHORITATIVE_MOVEMENT_INPUT_ADAPTER_TEST_SOURCE}")
        add_executable(AuthoritativeMovementInputAdapterTests
            ${SAGA_AUTHORITATIVE_MOVEMENT_INPUT_ADAPTER_TEST_SOURCE}
        )
        target_link_libraries(AuthoritativeMovementInputAdapterTests PRIVATE
            SagaServerLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(AuthoritativeMovementInputAdapterTests PRIVATE
            ${SAGA_ROOT}/Server/include
            ${SAGA_ROOT}/Engine/Public
            $<TARGET_PROPERTY:GTest::gtest,INTERFACE_INCLUDE_DIRECTORIES>
        )
        add_test(NAME AuthoritativeMovementInputAdapterTests
            COMMAND AuthoritativeMovementInputAdapterTests)
        set_tests_properties(AuthoritativeMovementInputAdapterTests PROPERTIES
            LABELS "unit;server"
        )
    endif()

    # --- Authoritative movement command intake tests ------------------------
    if(EXISTS "${SAGA_AUTHORITATIVE_MOVEMENT_COMMAND_INTAKE_TEST_SOURCE}")
        add_executable(AuthoritativeMovementCommandIntakeTests
            ${SAGA_AUTHORITATIVE_MOVEMENT_COMMAND_INTAKE_TEST_SOURCE}
        )
        target_link_libraries(AuthoritativeMovementCommandIntakeTests PRIVATE
            SagaServerLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(AuthoritativeMovementCommandIntakeTests PRIVATE
            ${SAGA_ROOT}/Server/include
            ${SAGA_ROOT}/Engine/Public
            $<TARGET_PROPERTY:GTest::gtest,INTERFACE_INCLUDE_DIRECTORIES>
        )
        add_test(NAME AuthoritativeMovementCommandIntakeTests
            COMMAND AuthoritativeMovementCommandIntakeTests)
        set_tests_properties(AuthoritativeMovementCommandIntakeTests PROPERTIES
            LABELS "unit;server"
        )
    endif()

    # --- Server packet normalization tests ---------------------------------
    if(EXISTS "${SAGA_SERVER_PACKET_NORMALIZATION_TEST_SOURCE}")
        add_executable(ServerPacketNormalizationTests
            ${SAGA_SERVER_PACKET_NORMALIZATION_TEST_SOURCE}
        )
        target_link_libraries(ServerPacketNormalizationTests PRIVATE
            SagaServerLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(ServerPacketNormalizationTests PRIVATE
            ${SAGA_ROOT}/Server/include
            ${SAGA_ROOT}/Engine/Public
            $<TARGET_PROPERTY:GTest::gtest,INTERFACE_INCLUDE_DIRECTORIES>
        )
        add_test(NAME ServerPacketNormalizationTests
            COMMAND ServerPacketNormalizationTests)
        set_tests_properties(ServerPacketNormalizationTests PROPERTIES
            LABELS "unit;networking;server"
        )
    endif()

    # --- Network chaos policy tests ----------------------------------------
    if(EXISTS "${SAGA_NETWORK_CHAOS_LAYER_TEST_SOURCE}")
        add_executable(NetworkChaosLayerTests
            ${SAGA_NETWORK_CHAOS_LAYER_TEST_SOURCE}
        )
        saga_link_thirdparty(NetworkChaosLayerTests)
        target_link_libraries(NetworkChaosLayerTests PRIVATE
            SagaServerLib
            SagaDiagnostics
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(NetworkChaosLayerTests PRIVATE
            ${SAGA_ROOT}/Server/include
            ${SAGA_ROOT}/Engine/Public
            $<TARGET_PROPERTY:GTest::gtest,INTERFACE_INCLUDE_DIRECTORIES>
        )
        add_test(NAME NetworkChaosLayerTests
            COMMAND NetworkChaosLayerTests)
        set_tests_properties(NetworkChaosLayerTests PROPERTIES
            LABELS "unit;networking;diagnostics"
        )
    endif()

    # --- Zone server packet drain tests ------------------------------------
    if(EXISTS "${SAGA_ZONE_SERVER_PACKET_DRAIN_TEST_SOURCE}")
        add_executable(ZoneServerPacketDrainTests
            ${SAGA_ZONE_SERVER_PACKET_DRAIN_TEST_SOURCE}
        )
        saga_link_thirdparty(ZoneServerPacketDrainTests)
        target_link_libraries(ZoneServerPacketDrainTests PRIVATE
            SagaServerLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(ZoneServerPacketDrainTests PRIVATE
            ${SAGA_ROOT}/Server/include
            ${SAGA_ROOT}/Engine/Public
            $<TARGET_PROPERTY:GTest::gtest,INTERFACE_INCLUDE_DIRECTORIES>
        )
        add_test(NAME ZoneServerPacketDrainTests
            COMMAND ZoneServerPacketDrainTests)
        set_tests_properties(ZoneServerPacketDrainTests PROPERTIES
            LABELS "unit;server;networking"
        )
    endif()

    # --- Zone server movement authority tests -------------------------------
    if(EXISTS "${SAGA_ZONE_SERVER_MOVEMENT_AUTHORITY_TEST_SOURCE}")
        add_executable(ZoneServerMovementAuthorityTests
            ${SAGA_ZONE_SERVER_MOVEMENT_AUTHORITY_TEST_SOURCE}
        )
        saga_link_thirdparty(ZoneServerMovementAuthorityTests)
        target_link_libraries(ZoneServerMovementAuthorityTests PRIVATE
            SagaServerLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(ZoneServerMovementAuthorityTests PRIVATE
            ${SAGA_ROOT}/Server/include
            ${SAGA_ROOT}/Engine/Public
            $<TARGET_PROPERTY:GTest::gtest,INTERFACE_INCLUDE_DIRECTORIES>
        )
        add_test(NAME ZoneServerMovementAuthorityTests
            COMMAND ZoneServerMovementAuthorityTests)
        set_tests_properties(ZoneServerMovementAuthorityTests PROPERTIES
            LABELS "unit;server;networking"
        )
    endif()

    # --- Zone server diagnostics tests -------------------------------------
    if(EXISTS "${SAGA_ZONE_SERVER_DIAGNOSTICS_TEST_SOURCE}")
        add_executable(ZoneServerDiagnosticsTests
            ${SAGA_ZONE_SERVER_DIAGNOSTICS_TEST_SOURCE}
        )
        saga_link_thirdparty(ZoneServerDiagnosticsTests)
        target_link_libraries(ZoneServerDiagnosticsTests PRIVATE
            SagaServerLib
            SagaDiagnostics
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(ZoneServerDiagnosticsTests PRIVATE
            ${SAGA_ROOT}/Server/include
            ${SAGA_ROOT}/Engine/Public
            $<TARGET_PROPERTY:GTest::gtest,INTERFACE_INCLUDE_DIRECTORIES>
        )
        add_test(NAME ZoneServerDiagnosticsTests
            COMMAND ZoneServerDiagnosticsTests)
        set_tests_properties(ZoneServerDiagnosticsTests PROPERTIES
            LABELS "unit;server;diagnostics"
        )
    endif()

    # --- Server lifecycle diagnostics tests --------------------------------
    if(EXISTS "${SAGA_SERVER_LIFECYCLE_DIAGNOSTICS_TEST_SOURCE}")
        add_executable(ServerLifecycleDiagnosticsTests
            ${SAGA_SERVER_LIFECYCLE_DIAGNOSTICS_TEST_SOURCE}
        )
        saga_link_thirdparty(ServerLifecycleDiagnosticsTests)
        target_link_libraries(ServerLifecycleDiagnosticsTests PRIVATE
            SagaServerLib
            SagaDiagnostics
            nlohmann_json::nlohmann_json
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(ServerLifecycleDiagnosticsTests PRIVATE
            ${SAGA_ROOT}/Server/include
            ${SAGA_ROOT}/Engine/Public
            $<TARGET_PROPERTY:GTest::gtest,INTERFACE_INCLUDE_DIRECTORIES>
        )
        add_test(NAME ServerLifecycleDiagnosticsTests
            COMMAND ServerLifecycleDiagnosticsTests)
        set_tests_properties(ServerLifecycleDiagnosticsTests PROPERTIES
            LABELS "unit;server;diagnostics"
        )
    endif()

    # --- Movement dirty replication bridge tests ---------------------------
    if(EXISTS "${SAGA_MOVEMENT_DIRTY_REPLICATION_BRIDGE_TEST_SOURCE}")
        add_executable(MovementDirtyReplicationBridgeTests
            ${SAGA_MOVEMENT_DIRTY_REPLICATION_BRIDGE_TEST_SOURCE}
        )
        saga_link_thirdparty(MovementDirtyReplicationBridgeTests)
        target_link_libraries(MovementDirtyReplicationBridgeTests PRIVATE
            SagaServerLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(MovementDirtyReplicationBridgeTests PRIVATE
            ${SAGA_ROOT}/Server/include
            ${SAGA_ROOT}/Engine/Public
            $<TARGET_PROPERTY:GTest::gtest,INTERFACE_INCLUDE_DIRECTORIES>
        )
        add_test(NAME MovementDirtyReplicationBridgeTests
            COMMAND MovementDirtyReplicationBridgeTests)
        set_tests_properties(MovementDirtyReplicationBridgeTests PROPERTIES
            LABELS "unit;server;replication"
        )
    endif()

    # --- Saga Stress Arena diagnostics tests -------------------------------
    if(EXISTS "${SAGA_STRESS_ARENA_TEST_SOURCE}" AND TARGET SagaStressArenaLib)
        add_executable(SagaStressArenaTests
            ${SAGA_STRESS_ARENA_TEST_SOURCE}
        )
        saga_link_thirdparty(SagaStressArenaTests)
        target_link_libraries(SagaStressArenaTests PRIVATE
            SagaStressArenaLib
            SagaServerLib
            SagaDiagnostics
            nlohmann_json::nlohmann_json
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(SagaStressArenaTests PRIVATE
            ${SAGA_ROOT}/Tools/SagaStressArena/include
            ${SAGA_ROOT}/Server/include
            ${SAGA_ROOT}/Engine/Public
            $<TARGET_PROPERTY:GTest::gtest,INTERFACE_INCLUDE_DIRECTORIES>
        )
        set_target_properties(SagaStressArenaTests PROPERTIES
            FOLDER "Tests/Tools"
        )
        add_test(NAME SagaStressArenaTests COMMAND SagaStressArenaTests)
        set_tests_properties(SagaStressArenaTests PROPERTIES
            LABELS "unit;tools;diagnostics"
        )
        if(TARGET SagaStressArena)
            add_test(NAME SagaStressArenaHelp COMMAND SagaStressArena --help)
            set_tests_properties(SagaStressArenaHelp PROPERTIES
                LABELS "unit;tools;diagnostics"
            )
        endif()
    endif()

    # --- Saga Chaos Lab profile runner tests --------------------------------
    if(EXISTS "${SAGA_CHAOS_LAB_TEST_SOURCE}" AND TARGET SagaChaosLabLib)
        add_executable(SagaChaosLabTests
            ${SAGA_CHAOS_LAB_TEST_SOURCE}
        )
        saga_link_thirdparty(SagaChaosLabTests)
        target_link_libraries(SagaChaosLabTests PRIVATE
            SagaChaosLabLib
            SagaStressArenaLib
            SagaServerLib
            SagaDiagnostics
            nlohmann_json::nlohmann_json
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(SagaChaosLabTests PRIVATE
            ${SAGA_ROOT}/Tools/SagaChaosLab/include
            ${SAGA_ROOT}/Tools/SagaStressArena/include
            ${SAGA_ROOT}/Server/include
            ${SAGA_ROOT}/Engine/Public
            $<TARGET_PROPERTY:GTest::gtest,INTERFACE_INCLUDE_DIRECTORIES>
        )
        set_target_properties(SagaChaosLabTests PROPERTIES
            FOLDER "Tests/Tools"
        )
        add_test(NAME SagaChaosLabTests COMMAND SagaChaosLabTests)
        set_tests_properties(SagaChaosLabTests PROPERTIES
            LABELS "unit;tools;diagnostics"
        )
        if(TARGET SagaChaosLab)
            add_test(NAME SagaChaosLabHelp COMMAND SagaChaosLab --help)
            set_tests_properties(SagaChaosLabHelp PROPERTIES
                LABELS "unit;tools;diagnostics"
            )
        endif()
    endif()

    # --- Saga State Check tests --------------------------------------------
    if(EXISTS "${SAGA_STATE_CHECK_TEST_SOURCE}" AND TARGET SagaStateCheckLib)
        add_executable(SagaStateCheckTests
            ${SAGA_STATE_CHECK_TEST_SOURCE}
        )
        target_link_libraries(SagaStateCheckTests PRIVATE
            SagaStateCheckLib
            nlohmann_json::nlohmann_json
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(SagaStateCheckTests PRIVATE
            ${SAGA_ROOT}/Tools/SagaStateCheck/include
            $<TARGET_PROPERTY:GTest::gtest,INTERFACE_INCLUDE_DIRECTORIES>
        )
        set_target_properties(SagaStateCheckTests PROPERTIES
            FOLDER "Tests/Tools"
        )
        add_test(NAME SagaStateCheckTests COMMAND SagaStateCheckTests)
        set_tests_properties(SagaStateCheckTests PROPERTIES
            LABELS "unit;tools;diagnostics"
        )
    endif()

    # --- MultiplayerSandbox headless tests ---------------------------------
    if(EXISTS "${MULTIPLAYER_SANDBOX_HEADLESS_TEST_SOURCE}" AND
       TARGET MultiplayerSandboxHeadlessLib)
        add_executable(MultiplayerSandboxHeadlessTests
            ${MULTIPLAYER_SANDBOX_HEADLESS_TEST_SOURCE}
        )
        target_link_libraries(MultiplayerSandboxHeadlessTests PRIVATE
            MultiplayerSandboxHeadlessLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(MultiplayerSandboxHeadlessTests PRIVATE
            ${SAGA_ROOT}/Tools/MultiplayerSandboxHeadless/include
            ${SAGA_ROOT}/Server/include
            ${SAGA_ROOT}/Engine/Public
            $<TARGET_PROPERTY:GTest::gtest,INTERFACE_INCLUDE_DIRECTORIES>
        )
        target_compile_definitions(MultiplayerSandboxHeadlessTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
        )
        set_target_properties(MultiplayerSandboxHeadlessTests PROPERTIES
            FOLDER "Tests/Samples"
        )
        add_test(NAME MultiplayerSandboxHeadlessTests
            COMMAND MultiplayerSandboxHeadlessTests)
        set_tests_properties(MultiplayerSandboxHeadlessTests PROPERTIES
            LABELS "unit;tools;samples"
        )
    endif()

    # --- Package staging tests ----------------------------------------------
    #
    # Build the package-staging slice without SagaProductLib so script artifact
    # placement and generated AssetPipeline package contracts stay focused.
    if(EXISTS "${SAGA_PACKAGE_STAGING_TEST_SOURCE}")
        add_executable(SagaPackageStagingTests
            ${SAGA_PACKAGE_STAGING_TEST_SOURCE}
            ${SAGA_ROOT}/Apps/Saga/SagaPackageStaging.cpp
            ${SAGA_ROOT}/Apps/Saga/SagaProjectSystem.cpp
            ${SAGA_ROOT}/Apps/Saga/SagaPublishReadiness.cpp
            ${SAGA_ROOT}/Apps/Saga/SagaSessionModel.cpp
        )
        target_link_libraries(SagaPackageStagingTests PRIVATE
            SagaAssetPipelineLib
            SagaEngine
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
            nlohmann_json::nlohmann_json
        )
        target_include_directories(SagaPackageStagingTests PRIVATE
            ${SAGA_ROOT}/Apps/Saga
            ${SAGA_ROOT}/Tools/AssetPipeline/include
            ${SAGA_ROOT}/Engine/Public
            ${SAGA_ROOT}/Shared/include
        )
        target_compile_definitions(SagaPackageStagingTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
        )
        add_test(NAME SagaPackageStagingTests COMMAND SagaPackageStagingTests)
        set_tests_properties(SagaPackageStagingTests PROPERTIES LABELS "product;package;asset")
    endif()

    # --- Publish readiness package/asset identity report tests --------------
    if(EXISTS "${SAGA_PUBLISH_READINESS_TEST_SOURCE}")
        add_executable(SagaPublishReadinessTests
            ${SAGA_PUBLISH_READINESS_TEST_SOURCE}
            ${SAGA_ROOT}/Apps/Saga/SagaProjectSystem.cpp
            ${SAGA_ROOT}/Apps/Saga/SagaPublishReadiness.cpp
        )
        target_link_libraries(SagaPublishReadinessTests PRIVATE
            SagaEngine
            SagaShared
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
            nlohmann_json::nlohmann_json
        )
        target_include_directories(SagaPublishReadinessTests PRIVATE
            ${SAGA_ROOT}/Apps/Saga
            ${SAGA_ROOT}/Engine/Public
            ${SAGA_ROOT}/Shared/include
        )
        target_compile_definitions(SagaPublishReadinessTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
        )
        add_test(NAME SagaPublishReadinessTests COMMAND SagaPublishReadinessTests)
        set_tests_properties(SagaPublishReadinessTests PROPERTIES
            LABELS "unit;product;package;asset")
    endif()

    # --- AssetPipeline to Runtime identity contract tests -------------------
    if(EXISTS "${SAGA_ASSET_IDENTITY_RUNTIME_CONTRACT_TEST_SOURCE}")
        add_executable(AssetIdentityRuntimeContractTests
            ${SAGA_ASSET_IDENTITY_RUNTIME_CONTRACT_TEST_SOURCE}
        )
        target_link_libraries(AssetIdentityRuntimeContractTests PRIVATE
            SagaAssetPipelineLib
            SagaEngine
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
            nlohmann_json::nlohmann_json
        )
        target_include_directories(AssetIdentityRuntimeContractTests PRIVATE
            ${SAGA_ROOT}/Tools/AssetPipeline/include
            ${SAGA_ROOT}/Engine/Public
            ${SAGA_ROOT}/Shared/include
        )
        target_compile_definitions(AssetIdentityRuntimeContractTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
        )
        add_test(NAME AssetIdentityRuntimeContractTests
            COMMAND AssetIdentityRuntimeContractTests)
        set_tests_properties(AssetIdentityRuntimeContractTests PROPERTIES
            LABELS "unit;asset;tools;runtime;package")
    endif()

    # --- AssetPipeline identity manifest writer tests -----------------------
    if(EXISTS "${SAGA_ASSET_IDENTITY_MANIFEST_WRITER_TEST_SOURCE}")
        add_executable(AssetIdentityManifestWriterTests
            ${SAGA_ASSET_IDENTITY_MANIFEST_WRITER_TEST_SOURCE}
        )
        target_link_libraries(AssetIdentityManifestWriterTests PRIVATE
            SagaAssetPipelineLib
            SagaEngine
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
            nlohmann_json::nlohmann_json
        )
        target_include_directories(AssetIdentityManifestWriterTests PRIVATE
            ${SAGA_ROOT}/Tools/AssetPipeline/include
            ${SAGA_ROOT}/Engine/Public
            ${SAGA_ROOT}/Shared/include
        )
        add_test(NAME AssetIdentityManifestWriterTests
            COMMAND AssetIdentityManifestWriterTests)
        set_tests_properties(AssetIdentityManifestWriterTests PROPERTIES
            LABELS "unit;asset;tools;runtime")
    endif()

    # --- AssetPipeline asset manifest writer tests -------------------------
    if(EXISTS "${SAGA_ASSET_MANIFEST_WRITER_TEST_SOURCE}")
        add_executable(AssetManifestWriterTests
            ${SAGA_ASSET_MANIFEST_WRITER_TEST_SOURCE}
        )
        target_link_libraries(AssetManifestWriterTests PRIVATE
            SagaAssetPipelineLib
            SagaEngine
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
            nlohmann_json::nlohmann_json
        )
        target_include_directories(AssetManifestWriterTests PRIVATE
            ${SAGA_ROOT}/Tools/AssetPipeline/include
            ${SAGA_ROOT}/Engine/Public
            ${SAGA_ROOT}/Shared/include
        )
        add_test(NAME AssetManifestWriterTests
            COMMAND AssetManifestWriterTests)
        set_tests_properties(AssetManifestWriterTests PROPERTIES
            LABELS "unit;asset;tools;runtime;package")
    endif()

    # --- AssetPipeline package manifest writer tests -----------------------
    if(EXISTS "${SAGA_PACKAGE_MANIFEST_WRITER_TEST_SOURCE}")
        add_executable(PackageManifestWriterTests
            ${SAGA_PACKAGE_MANIFEST_WRITER_TEST_SOURCE}
        )
        target_link_libraries(PackageManifestWriterTests PRIVATE
            SagaAssetPipelineLib
            SagaEngine
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
            nlohmann_json::nlohmann_json
        )
        target_include_directories(PackageManifestWriterTests PRIVATE
            ${SAGA_ROOT}/Tools/AssetPipeline/include
            ${SAGA_ROOT}/Engine/Public
            ${SAGA_ROOT}/Shared/include
        )
        target_compile_definitions(PackageManifestWriterTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
        )
        add_test(NAME PackageManifestWriterTests
            COMMAND PackageManifestWriterTests)
        set_tests_properties(PackageManifestWriterTests PROPERTIES
            LABELS "unit;asset;tools;runtime;package")
    endif()

    # --- Generated RuntimeSmoke manifest replacement tests ------------------
    if(EXISTS "${SAGA_GENERATED_RUNTIME_SMOKE_MANIFEST_TEST_SOURCE}")
        add_executable(GeneratedRuntimeSmokeManifestTests
            ${SAGA_GENERATED_RUNTIME_SMOKE_MANIFEST_TEST_SOURCE}
        )
        target_link_libraries(GeneratedRuntimeSmokeManifestTests PRIVATE
            SagaAssetPipelineLib
            SagaEngine
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(GeneratedRuntimeSmokeManifestTests PRIVATE
            ${SAGA_ROOT}/Tools/AssetPipeline/include
            ${SAGA_ROOT}/Engine/Public
            ${SAGA_ROOT}/Shared/include
        )
        target_compile_definitions(GeneratedRuntimeSmokeManifestTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
        )
        add_test(NAME GeneratedRuntimeSmokeManifestTests
            COMMAND GeneratedRuntimeSmokeManifestTests)
        set_tests_properties(GeneratedRuntimeSmokeManifestTests PROPERTIES
            LABELS "unit;asset;tools;runtime;package")
    endif()

    # --- Generated RuntimeSmoke package tests -------------------------------
    if(EXISTS "${SAGA_GENERATED_RUNTIME_SMOKE_PACKAGE_TEST_SOURCE}")
        add_executable(GeneratedRuntimeSmokePackageTests
            ${SAGA_GENERATED_RUNTIME_SMOKE_PACKAGE_TEST_SOURCE}
        )
        target_link_libraries(GeneratedRuntimeSmokePackageTests PRIVATE
            SagaAssetPipelineLib
            SagaEngine
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(GeneratedRuntimeSmokePackageTests PRIVATE
            ${SAGA_ROOT}/Tools/AssetPipeline/include
            ${SAGA_ROOT}/Engine/Public
            ${SAGA_ROOT}/Shared/include
        )
        target_compile_definitions(GeneratedRuntimeSmokePackageTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
        )
        add_test(NAME GeneratedRuntimeSmokePackageTests
            COMMAND GeneratedRuntimeSmokePackageTests)
        set_tests_properties(GeneratedRuntimeSmokePackageTests PROPERTIES
            LABELS "unit;asset;tools;runtime;package")
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
        set_tests_properties(SagaScriptLifecycleTests PROPERTIES LABELS "unit;runtime;scripting")
    endif()

    # --- Runtime startup preflight facade tests ------------------------------
    if(EXISTS "${SAGA_RUNTIME_STARTUP_PREFLIGHT_TEST_SOURCE}")
        add_executable(RuntimeStartupPreflightTests
            ${SAGA_RUNTIME_STARTUP_PREFLIGHT_TEST_SOURCE}
        )
        target_link_libraries(RuntimeStartupPreflightTests PRIVATE
            SagaRuntimeLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(RuntimeStartupPreflightTests PRIVATE
            ${SAGA_ROOT}/Runtime/include
        )
        add_test(NAME RuntimeStartupPreflightTests COMMAND RuntimeStartupPreflightTests)
        set_tests_properties(RuntimeStartupPreflightTests PROPERTIES LABELS "unit;runtime")
    endif()

    # --- Runtime startup session facade tests --------------------------------
    if(EXISTS "${SAGA_RUNTIME_STARTUP_SESSION_TEST_SOURCE}")
        add_executable(RuntimeStartupSessionTests
            ${SAGA_RUNTIME_STARTUP_SESSION_TEST_SOURCE}
        )
        target_link_libraries(RuntimeStartupSessionTests PRIVATE
            SagaRuntimeLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(RuntimeStartupSessionTests PRIVATE
            ${SAGA_ROOT}/Runtime/include
        )
        add_test(NAME RuntimeStartupSessionTests COMMAND RuntimeStartupSessionTests)
        set_tests_properties(RuntimeStartupSessionTests PROPERTIES LABELS "unit;runtime")
    endif()

    # --- Runtime startup diagnostics facade tests ----------------------------
    if(EXISTS "${SAGA_RUNTIME_STARTUP_DIAGNOSTICS_TEST_SOURCE}")
        add_executable(RuntimeStartupDiagnosticsTests
            ${SAGA_RUNTIME_STARTUP_DIAGNOSTICS_TEST_SOURCE}
        )
        target_link_libraries(RuntimeStartupDiagnosticsTests PRIVATE
            SagaRuntimeLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(RuntimeStartupDiagnosticsTests PRIVATE
            ${SAGA_ROOT}/Runtime/include
        )
        add_test(NAME RuntimeStartupDiagnosticsTests COMMAND RuntimeStartupDiagnosticsTests)
        set_tests_properties(RuntimeStartupDiagnosticsTests PROPERTIES LABELS "unit;runtime")
    endif()

    # --- Runtime asset catalog facade tests --------------------------------
    if(EXISTS "${SAGA_RUNTIME_ASSET_CATALOG_TEST_SOURCE}")
        add_executable(RuntimeAssetCatalogTests
            ${SAGA_RUNTIME_ASSET_CATALOG_TEST_SOURCE}
        )
        target_link_libraries(RuntimeAssetCatalogTests PRIVATE
            SagaRuntimeLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(RuntimeAssetCatalogTests PRIVATE
            ${SAGA_ROOT}/Runtime/include
            ${SAGA_ROOT}/Engine/Public
            ${SAGA_ROOT}/Shared/include
        )
        target_compile_definitions(RuntimeAssetCatalogTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
        )
        add_test(NAME RuntimeAssetCatalogTests COMMAND RuntimeAssetCatalogTests)
        set_tests_properties(RuntimeAssetCatalogTests PROPERTIES
            LABELS "unit;runtime;asset;package")
    endif()

    # --- Runtime asset bootstrap facade tests -------------------------------
    if(EXISTS "${SAGA_RUNTIME_ASSET_BOOTSTRAP_TEST_SOURCE}")
        add_executable(RuntimeAssetBootstrapTests
            ${SAGA_RUNTIME_ASSET_BOOTSTRAP_TEST_SOURCE}
        )
        target_link_libraries(RuntimeAssetBootstrapTests PRIVATE
            SagaRuntimeLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(RuntimeAssetBootstrapTests PRIVATE
            ${SAGA_ROOT}/Runtime/include
            ${SAGA_ROOT}/Engine/Public
            ${SAGA_ROOT}/Shared/include
        )
        target_compile_definitions(RuntimeAssetBootstrapTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
        )
        add_test(NAME RuntimeAssetBootstrapTests COMMAND RuntimeAssetBootstrapTests)
        set_tests_properties(RuntimeAssetBootstrapTests PROPERTIES
            LABELS "unit;runtime;asset;package")
    endif()

    # --- Apps/Runtime asset bootstrap helper seam tests ---------------------
    if(EXISTS "${SAGA_RUNTIME_ASSET_STARTUP_BOOTSTRAP_TEST_SOURCE}")
        add_executable(RuntimeAssetStartupBootstrapTests
            ${SAGA_RUNTIME_ASSET_STARTUP_BOOTSTRAP_TEST_SOURCE}
            ${SAGA_ROOT}/Apps/Runtime/RuntimeAssetStartupBootstrap.cpp
        )
        target_link_libraries(RuntimeAssetStartupBootstrapTests PRIVATE
            SagaRuntimeLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(RuntimeAssetStartupBootstrapTests PRIVATE
            ${SAGA_ROOT}/Apps/Runtime
            ${SAGA_ROOT}/Runtime/include
            ${SAGA_ROOT}/Engine/Public
            ${SAGA_ROOT}/Shared/include
        )
        target_compile_definitions(RuntimeAssetStartupBootstrapTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
        )
        add_test(NAME RuntimeAssetStartupBootstrapTests
            COMMAND RuntimeAssetStartupBootstrapTests)
        set_tests_properties(RuntimeAssetStartupBootstrapTests PROPERTIES
            LABELS "unit;runtime;asset;package;app")
    endif()

    # --- Runtime asset bootstrap diagnostics tests --------------------------
    if(EXISTS "${SAGA_RUNTIME_ASSET_BOOTSTRAP_DIAGNOSTICS_TEST_SOURCE}")
        add_executable(RuntimeAssetBootstrapDiagnosticsTests
            ${SAGA_RUNTIME_ASSET_BOOTSTRAP_DIAGNOSTICS_TEST_SOURCE}
        )
        target_link_libraries(RuntimeAssetBootstrapDiagnosticsTests PRIVATE
            SagaRuntimeLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(RuntimeAssetBootstrapDiagnosticsTests PRIVATE
            ${SAGA_ROOT}/Runtime/include
            ${SAGA_ROOT}/Engine/Public
            ${SAGA_ROOT}/Shared/include
        )
        add_test(NAME RuntimeAssetBootstrapDiagnosticsTests
            COMMAND RuntimeAssetBootstrapDiagnosticsTests)
        set_tests_properties(RuntimeAssetBootstrapDiagnosticsTests PROPERTIES
            LABELS "unit;runtime;asset;package")
    endif()

    # --- Runtime package fixture smoke tests --------------------------------
    if(EXISTS "${SAGA_RUNTIME_PACKAGE_SMOKE_TEST_SOURCE}")
        add_executable(RuntimePackageSmokeTests
            ${SAGA_RUNTIME_PACKAGE_SMOKE_TEST_SOURCE}
        )
        target_link_libraries(RuntimePackageSmokeTests PRIVATE
            SagaEngine
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(RuntimePackageSmokeTests PRIVATE
            ${SAGA_ROOT}/Engine/Public
            ${SAGA_ROOT}/Shared/include
        )
        target_compile_definitions(RuntimePackageSmokeTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
        )
        add_test(NAME RuntimePackageSmokeTests COMMAND RuntimePackageSmokeTests)
        set_tests_properties(RuntimePackageSmokeTests PROPERTIES
            LABELS "unit;runtime;asset;package")
    endif()

    # --- Runtime service lifecycle value tests ------------------------------
    if(EXISTS "${SAGA_RUNTIME_SERVICE_LIFECYCLE_TEST_SOURCE}")
        add_executable(RuntimeServiceLifecycleTests
            ${SAGA_RUNTIME_SERVICE_LIFECYCLE_TEST_SOURCE}
        )
        target_link_libraries(RuntimeServiceLifecycleTests PRIVATE
            SagaRuntimeLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(RuntimeServiceLifecycleTests PRIVATE
            ${SAGA_ROOT}/Runtime/include
        )
        add_test(NAME RuntimeServiceLifecycleTests COMMAND RuntimeServiceLifecycleTests)
        set_tests_properties(RuntimeServiceLifecycleTests PROPERTIES LABELS "unit;runtime")
    endif()

    # --- Runtime service registry tests -------------------------------------
    if(EXISTS "${SAGA_RUNTIME_SERVICE_REGISTRY_TEST_SOURCE}")
        add_executable(RuntimeServiceRegistryTests
            ${SAGA_RUNTIME_SERVICE_REGISTRY_TEST_SOURCE}
        )
        target_link_libraries(RuntimeServiceRegistryTests PRIVATE
            SagaRuntimeLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(RuntimeServiceRegistryTests PRIVATE
            ${SAGA_ROOT}/Runtime/include
        )
        add_test(NAME RuntimeServiceRegistryTests COMMAND RuntimeServiceRegistryTests)
        set_tests_properties(RuntimeServiceRegistryTests PROPERTIES LABELS "unit;runtime")
    endif()

    # --- Runtime service registry diagnostic tests --------------------------
    if(EXISTS "${SAGA_RUNTIME_SERVICE_REGISTRY_DIAGNOSTICS_TEST_SOURCE}")
        add_executable(RuntimeServiceRegistryDiagnosticsTests
            ${SAGA_RUNTIME_SERVICE_REGISTRY_DIAGNOSTICS_TEST_SOURCE}
        )
        target_link_libraries(RuntimeServiceRegistryDiagnosticsTests PRIVATE
            SagaRuntimeLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(RuntimeServiceRegistryDiagnosticsTests PRIVATE
            ${SAGA_ROOT}/Runtime/include
        )
        target_compile_definitions(RuntimeServiceRegistryDiagnosticsTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
        )
        add_test(NAME RuntimeServiceRegistryDiagnosticsTests
            COMMAND RuntimeServiceRegistryDiagnosticsTests)
        set_tests_properties(RuntimeServiceRegistryDiagnosticsTests
            PROPERTIES LABELS "unit;runtime")
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
        set_tests_properties(ScriptBindingRuntimeTests PROPERTIES LABELS "unit;runtime;scripting")
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
        set_tests_properties(CSharpScriptHostTests PROPERTIES LABELS "unit;runtime;scripting;csharp")
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
        set_tests_properties(CSharpGameplayProofTests PROPERTIES LABELS "unit;runtime;scripting;csharp")
    endif()

    # --- StarterArena runtime smoke tests ----------------------------------
    if(EXISTS "${STARTER_ARENA_RUNTIME_SMOKE_TEST_SOURCE}" AND
       TARGET SagaRuntime AND
       TARGET SagaScriptRuntimeBridge)
        add_executable(StarterArenaRuntimeSmokeTests
            ${STARTER_ARENA_RUNTIME_SMOKE_TEST_SOURCE}
        )
        add_dependencies(StarterArenaRuntimeSmokeTests
            SagaRuntime
            SagaScriptRuntimeBridge
        )
        target_link_libraries(StarterArenaRuntimeSmokeTests PRIVATE
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
            nlohmann_json::nlohmann_json
        )
        target_compile_definitions(StarterArenaRuntimeSmokeTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
            SAGA_RUNTIME_EXECUTABLE="$<TARGET_FILE:SagaRuntime>"
            SAGA_SCRIPT_RUNTIME_BRIDGE_ASSEMBLY="${SAGA_SCRIPT_RUNTIME_BRIDGE_ASSEMBLY}"
            SAGA_DOTNET_EXECUTABLE="${SAGA_DOTNET_EXECUTABLE}"
            SAGA_DOTNET_ROOT="${SAGA_DOTNET_ROOT}"
        )
        add_test(NAME StarterArenaRuntimeSmokeTests
            COMMAND StarterArenaRuntimeSmokeTests)
        set_tests_properties(StarterArenaRuntimeSmokeTests PROPERTIES
            LABELS "unit;runtime;scripting;csharp;sample")
    endif()

    # --- StarterArena visible frame contract tests -------------------------
    if(EXISTS "${STARTER_ARENA_PLAYABLE_TEST_SOURCE}")
        add_executable(StarterArenaPlayableTests
            ${STARTER_ARENA_PLAYABLE_TEST_SOURCE}
            ${SAGA_ROOT}/Apps/Runtime/StarterArenaSimulation.cpp
            ${SAGA_ROOT}/Apps/Runtime/StarterArenaPlayableScene.cpp
        )
        target_link_libraries(StarterArenaPlayableTests PRIVATE
            SagaEngine
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(StarterArenaPlayableTests PRIVATE
            ${SAGA_ROOT}/Apps/Runtime
            ${SAGA_ROOT}/Engine/Public
        )
        add_test(NAME StarterArenaPlayableTests
            COMMAND StarterArenaPlayableTests)
        set_tests_properties(StarterArenaPlayableTests PROPERTIES
            LABELS "unit;runtime;render;sample")
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
    # checks can run without rebuilding every unit test.
    if(SAGA_PRODUCT_TEST_SOURCES)
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
        set_tests_properties(SagaProductTests PROPERTIES LABELS "product;unit")
    endif()

    # --- Editor authoring spine tests ---------------------------------------
    if(EXISTS "${EDITOR_AUTHORING_SPINE_TEST_SOURCE}")
        add_executable(EditorAuthoringSpineTests
            ${EDITOR_AUTHORING_SPINE_TEST_SOURCE}
        )
        saga_link_thirdparty(EditorAuthoringSpineTests)
        target_link_libraries(EditorAuthoringSpineTests PRIVATE
            SagaEditorLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(EditorAuthoringSpineTests PRIVATE
            ${SAGA_TEST_INCLUDE_DIRS}
        )
        target_compile_definitions(EditorAuthoringSpineTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
        )
        add_test(NAME EditorAuthoringSpineTests
            COMMAND EditorAuthoringSpineTests)
        set_tests_properties(EditorAuthoringSpineTests PROPERTIES
            LABELS "unit;editor;authoring")
    endif()

    # --- Collaboration model tests ------------------------------------------
    if(EXISTS "${COLLABORATION_MODEL_TEST_SOURCE}")
        add_executable(CollaborationModelTests
            ${COLLABORATION_MODEL_TEST_SOURCE}
        )
        target_link_libraries(CollaborationModelTests PRIVATE
            SagaCollaboration
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(CollaborationModelTests PRIVATE
            ${SAGA_TEST_INCLUDE_DIRS}
        )
        target_compile_definitions(CollaborationModelTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
        )
        add_test(NAME CollaborationModelTests
            COMMAND CollaborationModelTests)
        set_tests_properties(CollaborationModelTests PROPERTIES
            LABELS "unit;collaboration")
    endif()

    # --- Architecture boundary tests ----------------------------------------
    #
    # Keep boundary and contract checks runnable without pulling in the Qt-heavy
    # editor unit-test link graph.
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
    target_sources(SagaIntegrationTests PRIVATE
        ${SAGA_ROOT}/Apps/Runtime/StarterArenaPlayableScene.cpp
    )
    saga_link_thirdparty(SagaIntegrationTests)
    target_link_libraries(SagaIntegrationTests PRIVATE
        SagaBackend
        SagaEngine
        SagaGraphicsPrivate
        SagaDiligentBackend
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
    target_include_directories(SagaIntegrationTests PRIVATE
        ${SAGA_TEST_INCLUDE_DIRS}
        ${SAGA_ROOT}/Tests/Support
        ${SAGA_ROOT}/Engine/Private
        ${SAGA_ROOT}/Apps/Runtime
    )
    saga_link_diligent_backend(SagaIntegrationTests)
    add_test(NAME IntegrationTests COMMAND SagaIntegrationTests)
    set_tests_properties(IntegrationTests PROPERTIES
        LABELS "integration;runtime;server;networking;replication;timing-sensitive"
    )
    add_test(NAME StarterArenaPlayableGpuTests
        COMMAND SagaIntegrationTests
            --gtest_filter=DiligentGPU.StarterArenaFrameProducesArenaPlayerAndBoundaryPixels)
    set_tests_properties(StarterArenaPlayableGpuTests PROPERTIES
        LABELS "integration;runtime;render;gpu;sample")

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

endfunction()
