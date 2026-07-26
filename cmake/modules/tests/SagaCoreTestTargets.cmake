# SPDX-License-Identifier: MPL-2.0
    # --- Unit tests ---------------------------------------------------------

    add_executable(SagaUnitTests ${UNIT_TEST_SOURCES})
    saga_link_thirdparty(SagaUnitTests)
    target_link_libraries(SagaUnitTests PRIVATE
        SagaEngine
        SagaDiagnostics
        SagaRuntimeAppLib
        SagaRuntimeLib
        SagaServerLib
        SagaShared
        SagaCollaboration
        SagaAssetPipelineLib
        SagaBackend
        SagaSandboxLib
        SagaEditorLib    # needed by Tests/Unit/Editor/* (block authoring,
                         # InspectorEditing, persona, viewport, etc.)
        SagaEditorLabLib # needed by dev-only EditorLab owner tests
        GTest::gtest
        GTest::gmock
        GTest::gtest_main
        rapidcheck::rapidcheck
    )
    target_link_libraries(SagaUnitTests PRIVATE SagaProductLib)
    target_include_directories(SagaUnitTests PRIVATE ${SAGA_TEST_INCLUDE_DIRS})
    target_include_directories(SagaUnitTests PRIVATE
        ${SAGA_MODULE_PRIVATE_INCLUDE_DIRS}
        ${SAGA_ROOT}/Engine/Source/Programs/SagaRuntime/Private
    )
    target_compile_definitions(SagaUnitTests PRIVATE
        SAGA_SOURCE_ROOT="${SAGA_ROOT}"
        SAGA_BUILD_ROOT="${CMAKE_BINARY_DIR}"
    )
    add_test(NAME UnitTests COMMAND SagaUnitTests)
    set_tests_properties(UnitTests PROPERTIES
        LABELS "unit;runtime;server;networking;replication;asset;editor"
    )

    # --- Diligent private white-box tests ----------------------------------
    if(SAGA_DILIGENT_WHITEBOX_TEST_SOURCES)
        add_executable(SagaDiligentWhiteboxTests
            ${SAGA_DILIGENT_WHITEBOX_TEST_SOURCES}
        )
        saga_apply_compiler_flags(SagaDiligentWhiteboxTests)
        target_link_libraries(SagaDiligentWhiteboxTests PRIVATE
            SagaDiligentBackend
            SagaDiligentRuntime
            SagaGraphicsPrivate
            SagaEngine
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(SagaDiligentWhiteboxTests PRIVATE
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_ROOT}/Engine/Source/Runtime/RHI/Private
            ${SAGA_ROOT}/Engine/Source/Runtime/Render/Private
            ${SAGA_ROOT}/Engine/Source/Runtime/RHI/Tests
        )
        saga_link_diligent_backend(SagaDiligentWhiteboxTests)
        set_target_properties(SagaDiligentWhiteboxTests PROPERTIES
            FOLDER "Tests/Whitebox"
        )
        add_test(NAME DiligentWhiteboxTests COMMAND SagaDiligentWhiteboxTests)
        set_tests_properties(DiligentWhiteboxTests PROPERTIES
            LABELS "unit;render;graphics;whitebox-private"
        )
    endif()

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
            FOLDER "Tests/Contract"
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
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
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
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
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
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
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
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
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
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
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
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
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
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
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
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
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
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
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
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
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
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
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
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
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
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
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
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
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
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
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
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
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
            ${SAGA_ROOT}/Tests/Support/Stress/SagaStressArena/include
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
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
            ${SAGA_ROOT}/Tools/Developer/DistributionCheck/ChaosLab/include
            ${SAGA_ROOT}/Tests/Support/Stress/SagaStressArena/include
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
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
            ${SAGA_ROOT}/Tools/Developer/DistributionCheck/StateCheck/include
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
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
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
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
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
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
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
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
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
            SagaRuntimeLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
            nlohmann_json::nlohmann_json
        )
        target_include_directories(PackageManifestWriterTests PRIVATE
            ${SAGA_ROOT}/Tools/AssetPipeline/include
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
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
            SagaRuntimeLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(GeneratedRuntimeSmokeManifestTests PRIVATE
            ${SAGA_ROOT}/Tools/AssetPipeline/include
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
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
            SagaRuntimeLib
            GTest::gtest
            GTest::gmock
            GTest::gtest_main
        )
        target_include_directories(GeneratedRuntimeSmokePackageTests PRIVATE
            ${SAGA_ROOT}/Tools/AssetPipeline/include
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
            ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
        )
        target_compile_definitions(GeneratedRuntimeSmokePackageTests PRIVATE
            SAGA_SOURCE_ROOT="${SAGA_ROOT}"
        )
        add_test(NAME GeneratedRuntimeSmokePackageTests
            COMMAND GeneratedRuntimeSmokePackageTests)
        set_tests_properties(GeneratedRuntimeSmokePackageTests PROPERTIES
            LABELS "unit;asset;tools;runtime;package")
    endif()
