# SPDX-License-Identifier: MPL-2.0
saga_add_editor_module(EditorQt
    MODULE_DIR Engine/Source/Editor/EditorQt
    TARGET SagaEditorLib
    PUBLIC_DEPS
        Qt6::Core
        Qt6::Widgets
    PRIVATE_DEPS
        Qt6::Concurrent
    SOURCES
        Private/SagaEditorQt/Commands/CommandPalette.cpp
        Private/SagaEditorQt/Host/EditorProjectInspection.cpp
        Private/SagaEditorQt/Panels/AssetBrowserPanel.cpp
        Private/SagaEditorQt/Panels/ConsolePanel.cpp
        Private/SagaEditorQt/Panels/CustomizeWorkspacePanel.cpp
        Private/SagaEditorQt/Panels/HierarchyPanel.cpp
        Private/SagaEditorQt/Panels/InspectorPanel.cpp
        Private/SagaEditorQt/Panels/ProblemsPanel.cpp
        Private/SagaEditorQt/Panels/PackagesPanel.cpp
        Private/SagaEditorQt/Panels/ReadOnlyBlocksWidget.cpp
        Private/SagaEditorQt/Panels/ReadOnlyBlocksWidget.h
        Private/SagaEditorQt/Panels/ProductionDashboardPanel.cpp
        Private/SagaEditorQt/Panels/ShortcutPreferencesPanel.cpp
        Private/SagaEditorQt/Panels/CodeBoxPanel.cpp
        Private/SagaEditorQt/Panels/WorldViewportPanel.cpp
        Private/SagaEditorQt/QtSettingsStore.cpp
        Private/SagaEditorQt/QtUIMainWindow.cpp
        Private/SagaEditorQt/Themes/ThemeRegistry.cpp
        Public/SagaEditorQt/QtSettingsStore.h
        Public/SagaEditorQt/QtUIMainWindow.h
)

saga_register_module_tests(EditorQt
    MODULE_DIR Engine/Source/Editor/EditorQt
    LINK_TARGET SagaEditorLib
    PRIVATE_DEPS
        nlohmann_json::nlohmann_json
    SOURCES
        Tests/EditorProjectInspectionTests.cpp
        Tests/QtAuthoringPanelsTests.cpp
        Tests/QtReadOnlyBlocksWorkflowTests.cpp
        Tests/QtSettingsStoreTests.cpp
    LABELS unit editor qt
)
