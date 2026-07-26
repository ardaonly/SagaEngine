# SPDX-License-Identifier: MPL-2.0
    # --- Source discovery ---------------------------------------------------

    # Unit ownership is explicit below. Do not restore a recursive catch-all:
    # it hides missing owner dependencies behind one giant executable.
    set(UNIT_TEST_SOURCES)

    set(SAGA_STRESS_ARENA_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Support/Stress/SagaStressArena/Tests.cpp"
    )
    set(SAGA_CHAOS_LAB_TEST_SOURCE
        "${SAGA_ROOT}/Tools/Developer/DistributionCheck/ChaosLab/Tests.cpp"
    )
    set(SAGA_STATE_CHECK_TEST_SOURCE
        "${SAGA_ROOT}/Tools/Developer/DistributionCheck/StateCheck/Tests.cpp"
    )
    set(EDITOR_AUTHORING_SPINE_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Editor/EditorAuthoringSpineTests.cpp"
    )
    set(COLLABORATION_MODEL_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Architecture/Collaboration/CollaborationModelTests.cpp"
    )
    set(MULTIPLAYER_SANDBOX_HEADLESS_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Samples/MultiplayerSandboxHeadlessTests.cpp"
    )

    file(GLOB_RECURSE SAGA_PRODUCT_TEST_SOURCES CONFIGURE_DEPENDS
        "${SAGA_ROOT}/Tests/Unit/Saga/*.cpp"
    )
    set(SAGA_ASSET_IDENTITY_RUNTIME_CONTRACT_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/AssetPipeline/AssetIdentityRuntimeContractTests.cpp"
    )
    set(SAGA_ASSET_IDENTITY_GENERATOR_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/AssetPipeline/AssetIdentityGeneratorTests.cpp"
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
        "${SAGA_ROOT}/Tests/Contract/InstalledConsumer/SagaGraphicsPublicConsumerLinkTest.cpp"
    )
    set(SAGA_GRAPHICS_INSTALLED_CONSUMER_TEST_SCRIPT
        "${SAGA_ROOT}/Tests/Contract/InstalledConsumer/SagaGraphicsInstalledConsumerTest.cmake"
    )
    set(SAGA_GRAPHICS_INSTALLED_CONSUMER_SOURCE_DIR
        "${SAGA_ROOT}/Tests/Contract/InstalledConsumer/SagaGraphicsInstalledConsumerProject"
    )
    set(SAGA_DILIGENT_WHITEBOX_TEST_SOURCES
        "${SAGA_ROOT}/Engine/Source/Runtime/Render/Tests/DiligentFrameSlotTrackerTests.cpp"
        "${SAGA_ROOT}/Engine/Source/Runtime/Render/Tests/DiligentGpuTimelineTests.cpp"
        "${SAGA_ROOT}/Engine/Source/Runtime/Render/Tests/DiligentOverlayRendererTests.cpp"
        "${SAGA_ROOT}/Engine/Source/Runtime/Render/Tests/DiligentRenderBackendTests.cpp"
        "${SAGA_ROOT}/Engine/Source/Runtime/RHI/Tests/GraphicsDiligentBackendBindingTests.cpp"
        "${SAGA_ROOT}/Engine/Source/Runtime/RHI/Tests/GraphicsDiligentBackendLifecycleTests.cpp"
        "${SAGA_ROOT}/Engine/Source/Runtime/RHI/Tests/GraphicsDiligentBackendNativeBufferTests.cpp"
        "${SAGA_ROOT}/Engine/Source/Runtime/RHI/Tests/GraphicsDiligentBackendNativeSamplerTests.cpp"
        "${SAGA_ROOT}/Engine/Source/Runtime/RHI/Tests/GraphicsDiligentBackendNativeShaderPipelineTests.cpp"
        "${SAGA_ROOT}/Engine/Source/Runtime/RHI/Tests/GraphicsDiligentBackendNativeTextureTests.cpp"
        "${SAGA_ROOT}/Engine/Source/Runtime/RHI/Tests/GraphicsDiligentBackendResourceRegistryTests.cpp"
        "${SAGA_ROOT}/Engine/Source/Runtime/RHI/Tests/SagaDiligentRuntimeTests.cpp"
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
        "${SAGA_ROOT}/Engine/Source/Runtime/Scripting/Tests/ScriptBindingRuntimeTests.cpp"
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
        "${SAGA_ROOT}/Engine/Source/Programs/SagaRuntime/Tests/RuntimeAssetStartupBootstrapTests.cpp"
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
        "${SAGA_ROOT}/Engine/Source/Runtime/Scripting/Tests/CSharpScriptHostTests.cpp"
    )
    set(SAGA_CSHARP_GAMEPLAY_PROOF_TEST_SOURCE
        "${SAGA_ROOT}/Engine/Source/Runtime/Scripting/Tests/CSharpGameplayProofTests.cpp"
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

    set(SAGA_RENDER_UNIT_TEST_SOURCES
        "${SAGA_ROOT}/Tests/Unit/Render/CookedRenderAssetLoaderTests.cpp"
        "${SAGA_ROOT}/Tests/Unit/Render/CullingPipelineTests.cpp"
        "${SAGA_ROOT}/Tests/Unit/Render/FrustumTests.cpp"
        "${SAGA_ROOT}/Tests/Unit/Render/GraphicsBindingValidationTests.cpp"
        "${SAGA_ROOT}/Tests/Unit/Render/GraphicsCoordinateConventionTests.cpp"
        "${SAGA_ROOT}/Tests/Unit/Render/GraphicsFrameResourceAllocatorTests.cpp"
        "${SAGA_ROOT}/Tests/Unit/Render/GraphicsNullBackendResourceTests.cpp"
        "${SAGA_ROOT}/Tests/Unit/Render/GraphicsRenderBackendMappingTests.cpp"
        "${SAGA_ROOT}/Tests/Unit/Render/LODSelectorTests.cpp"
        "${SAGA_ROOT}/Tests/Unit/Render/LightingContractTests.cpp"
        "${SAGA_ROOT}/Tests/Unit/Render/NetEntityBindingTests.cpp"
        "${SAGA_ROOT}/Tests/Unit/Render/RenderGraphValidationTests.cpp"
        "${SAGA_ROOT}/Tests/Unit/Render/RenderInterestWorldStreamingTests.cpp"
        "${SAGA_ROOT}/Tests/Unit/Render/RenderSceneExtractionTests.cpp"
        "${SAGA_ROOT}/Tests/Unit/Render/RenderStreamingResidencyTests.cpp"
        "${SAGA_ROOT}/Tests/Unit/Render/RenderSubsystemIntegrationTests.cpp"
        "${SAGA_ROOT}/Tests/Unit/Render/RenderSubsystemTests.cpp"
        "${SAGA_ROOT}/Tests/Unit/Render/RenderWorldTests.cpp"
        "${SAGA_ROOT}/Tests/Unit/Render/ShaderMaterialContractTests.cpp"
    )
    set(SAGA_SANDBOX_UNIT_TEST_SOURCES
        "${SAGA_ROOT}/Tests/Unit/Sandbox/SandboxDiagnosticMigrationTests.cpp"
        "${SAGA_ROOT}/Tests/Unit/Sandbox/SandboxImGuiOverlayTokenTests.cpp"
        "${SAGA_ROOT}/Tests/Unit/Render/PlayableRenderSliceTests.cpp"
    )
    set(SAGA_RUNTIME_HOST_UNIT_TEST_SOURCE
        "${SAGA_ROOT}/Tests/Unit/Runtime/RuntimeHostTests.cpp"
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
        ${SAGA_ASSET_IDENTITY_GENERATOR_TEST_SOURCE}
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
        ${SAGA_DILIGENT_WHITEBOX_TEST_SOURCES}
        ${SAGA_RENDER_UNIT_TEST_SOURCES}
        ${SAGA_SANDBOX_UNIT_TEST_SOURCES}
        ${SAGA_RUNTIME_HOST_UNIT_TEST_SOURCE}
    )

    file(GLOB_RECURSE EDITORLAB_SOURCES CONFIGURE_DEPENDS
        "${SAGA_ROOT}/Engine/Source/Programs/SagaEditorLab/Private/*.cpp"
    )

    file(GLOB_RECURSE INTEGRATION_SOURCES CONFIGURE_DEPENDS
        "${SAGA_ROOT}/Tests/Integration/*.cpp"
    )
    file(GLOB_RECURSE DILIGENT_GPU_INTEGRATION_SOURCES CONFIGURE_DEPENDS
        "${SAGA_ROOT}/Tests/GPU/Diligent/*.cpp"
    )
    list(REMOVE_ITEM INTEGRATION_SOURCES
        "${SAGA_ROOT}/Tests/Integration/EditorWorkflow/SagaLauncherUiTests.cpp")

    file(GLOB_RECURSE REPLICATION_SOURCES CONFIGURE_DEPENDS
        "${SAGA_ROOT}/Tests/Integration/ClientServer/*.cpp"
    )
    list(REMOVE_ITEM INTEGRATION_SOURCES ${REPLICATION_SOURCES})

    file(GLOB_RECURSE STRESS_SOURCES CONFIGURE_DEPENDS
        "${SAGA_ROOT}/Tests/Support/Stress/Infrastructure/*.cpp"
    )

    file(GLOB_RECURSE ARCHITECTURE_TEST_SOURCES CONFIGURE_DEPENDS
        "${SAGA_ROOT}/Tests/Architecture/*.cpp"
    )

    if(ARCHITECTURE_TEST_SOURCES)
        list(REMOVE_ITEM UNIT_TEST_SOURCES ${ARCHITECTURE_TEST_SOURCES})
    endif()

    # --- Common include directories -----------------------------------------
    #
    if(UNIT_TEST_SOURCES)
        message(FATAL_ERROR
            "Unowned Tests/Unit sources remain after focused assignment: "
            "${UNIT_TEST_SOURCES}")
    endif()

    # Shared test visibility is public-only. White-box implementation paths
    # are granted explicitly by the focused owner target that needs them.
    set(SAGA_TEST_INCLUDE_DIRS
        ${SAGA_MODULE_PUBLIC_INCLUDE_DIRS}
        ${SAGA_ROOT}/Tools/AssetPipeline/include
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
