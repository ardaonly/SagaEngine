saga_add_program(SagaEditorLab
    MODULE_DIR Engine/Source/Programs/SagaEditorLab
    TARGET EditorLab
    SOURCES
        Private/EditorLabQtStaticPlugins.cpp
        Private/include/SagaEditorLab/Scenario/BuiltinScenarioDefinitions.h
        Private/include/SagaEditorLab/Scenario/CustomizationScenario.h
        Private/include/SagaEditorLab/Scenario/DeterministicScenarioRuntimeAdapter.h
        Private/include/SagaEditorLab/Scenario/EditorHostScenarioRuntimeAdapter.h
        Private/include/SagaEditorLab/Scenario/EditorShellScenarioRuntimeAdapter.h
        Private/include/SagaEditorLab/Scenario/ProfileSwitchScenario.h
        Private/include/SagaEditorLab/Scenario/ScenarioDefinition.h
        Private/include/SagaEditorLab/Scenario/ScenarioResult.h
        Private/include/SagaEditorLab/Scenario/ScenarioRunner.h
        Private/include/SagaEditorLab/Scenario/ScenarioRuntimeAdapter.h
        Private/include/SagaEditorLab/Scenario/ScenarioStep.h
        Private/include/SagaEditorLab/UI/ScenarioRunnerDockPanel.h
        Private/include/SagaEditorLab/UI/ScenarioRunnerPanelViewModel.h
        Private/main.cpp
)

saga_register_module_tests(SagaEditorLab
    MODULE_DIR Engine/Source/Programs/SagaEditorLab
    LINK_TARGET SagaEditorLabLib
    PRIVATE_INCLUDES
        ${SAGA_ROOT}/Engine/Source/Programs/SagaEditorLab/Private/include
    LABELS "unit;editor;program"
    SOURCES
        Tests/EditorLabBuiltinScenarioTests.cpp
)

saga_add_program(SagaEditorLab
    MODULE_DIR Engine/Source/Programs/SagaEditorLab
    TARGET SagaEditorLabLib
    SOURCES
        Private/src/SagaEditorLab/BuiltinScenarioDefinitions.cpp
        Private/src/SagaEditorLab/CustomizationScenario.cpp
        Private/src/SagaEditorLab/DeterministicScenarioRuntimeAdapter.cpp
        Private/src/SagaEditorLab/EditorHostScenarioRuntimeAdapter.cpp
        Private/src/SagaEditorLab/EditorShellScenarioRuntimeAdapter.cpp
        Private/src/SagaEditorLab/ProfileSwitchScenario.cpp
        Private/src/SagaEditorLab/ScenarioResult.cpp
        Private/src/SagaEditorLab/ScenarioRunner.cpp
        Private/src/SagaEditorLab/ScenarioRunnerDockPanel.cpp
        Private/src/SagaEditorLab/ScenarioRunnerPanel.cpp
        Private/src/SagaEditorLab/ScenarioRunnerPanel.h
        Private/src/SagaEditorLab/ScenarioRunnerPanelViewModel.cpp
        Private/src/SagaEditorLab/ScenarioRuntimeAdapter.cpp
        Private/src/SagaEditorLab/ScenarioStep.cpp
)
