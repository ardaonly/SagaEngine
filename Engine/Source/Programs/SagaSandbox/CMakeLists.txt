set(SAGA_SANDBOX_AUXILIARY_SOURCES
    ${SAGA_ROOT}/Engine/Source/Programs/SagaSandbox/Private/Launchers/HeadlessLauncher.cpp
    ${SAGA_ROOT}/Engine/Source/Programs/SagaSandbox/Private/Launchers/InputProbeLauncher.cpp
    ${SAGA_ROOT}/Engine/Source/Programs/SagaSandbox/Private/Launchers/NetworkProbeLauncher.cpp
)

saga_add_program(SagaSandbox
    MODULE_DIR Engine/Source/Programs/SagaSandbox
    TARGET SagaSandbox
    SOURCES
        Private/Launchers/SandboxMain.cpp
)

saga_add_program(SagaSandbox
    MODULE_DIR Engine/Source/Programs/SagaSandbox
    TARGET SagaSandboxLib
    SOURCES
        Private/include/SagaSandbox/Core/IScenario.h
        Private/include/SagaSandbox/Core/SandboxConfig.h
        Private/include/SagaSandbox/Core/SandboxHost.h
        Private/include/SagaSandbox/Core/ScenarioManager.h
        Private/include/SagaSandbox/Core/ScenarioRegistry.h
        Private/include/SagaSandbox/Render/PlayableRenderSlice.h
        Private/include/SagaSandbox/UI/DebugHud.h
        Private/include/SagaSandbox/UI/Panels/EventLogPanel.h
        Private/include/SagaSandbox/UI/Panels/FrameStatsPanel.h
        Private/include/SagaSandbox/UI/Panels/IDebugPanel.h
        Private/include/SagaSandbox/UI/Panels/MemoryStatsPanel.h
        Private/include/SagaSandbox/UI/Panels/NetworkStatsPanel.h
        Private/include/SagaSandbox/UI/Panels/ProfilerPanel.h
        Private/include/SagaSandbox/UI/Panels/ScenarioPickerPanel.h
        Private/include/SagaSandbox/UI/SandboxImGuiOverlayAdapter.h
        Private/include/SagaSandbox/UI/SandboxImGuiOverlayToken.h
        Private/src/Core/SandboxHost.cpp
        Private/src/Core/ScenarioManager.cpp
        Private/src/Core/ScenarioRegistry.cpp
        Private/src/Scenarios/AudioTestScenario.cpp
        Private/src/Scenarios/AudioTestScenario.h
        Private/src/Scenarios/DirectionalLightShadowSliceScenario.cpp
        Private/src/Scenarios/DirectionalLightShadowSliceScenario.h
        Private/src/Scenarios/First3DVerticalSliceScenario.cpp
        Private/src/Scenarios/First3DVerticalSliceScenario.h
        Private/src/Scenarios/InputProbeScenario.cpp
        Private/src/Scenarios/InputProbeScenario.h
        Private/src/Scenarios/NetworkReplicationScenario.cpp
        Private/src/Scenarios/NetworkReplicationScenario.h
        Private/src/Scenarios/PredictionStressScenario.cpp
        Private/src/Scenarios/PredictionStressScenario.h
        Private/src/Scenarios/RenderPlaygroundScenario.cpp
        Private/src/Scenarios/RenderPlaygroundScenario.h
        Private/src/UI/DebugHud.cpp
        Private/src/UI/Panels/EventLogPanel.cpp
        Private/src/UI/Panels/FrameStatsPanel.cpp
        Private/src/UI/Panels/MemoryStatsPanel.cpp
        Private/src/UI/Panels/NetworkStatsPanel.cpp
        Private/src/UI/Panels/ProfilerPanel.cpp
        Private/src/UI/Panels/ScenarioPickerPanel.cpp
        Private/src/UI/SandboxImGuiOverlayAdapter.cpp
)
