saga_add_editor_module(EditorCore
    MODULE_DIR Engine/Source/Editor/EditorCore
    TARGET SagaEditorLib
    PRIVATE_DEPS nlohmann_json::nlohmann_json
    SOURCES
        Private/SagaEditor/Commands/CommandDispatcher.cpp
        Private/SagaEditor/Commands/CommandRegistry.cpp
        Private/SagaEditor/Commands/ShortcutManager.cpp
        Private/SagaEditor/Commands/UndoRedoStack.cpp
        Private/SagaEditor/Diagnostics/EditorDiagnosticsService.cpp
        Private/SagaEditor/Project/ProjectIndexer.cpp
        Private/SagaEditor/Project/ProjectManager.cpp
        Private/SagaEditor/Runtime/IEditorEngineBridge.cpp
        Private/SagaEditor/Runtime/LocalEditorEngineBridge.cpp
        Private/SagaEditor/Selection/SelectionFilter.cpp
        Private/SagaEditor/Selection/SelectionHistory.cpp
        Private/SagaEditor/Selection/SelectionManager.cpp
        Private/SagaEditor/Settings/MemoryEditorSettingsStore.cpp
        Public/SagaEditor/Commands/CommandDispatcher.h
        Public/SagaEditor/Commands/CommandPalette.h
        Public/SagaEditor/Commands/CommandRegistry.h
        Public/SagaEditor/Commands/ShortcutManager.h
        Public/SagaEditor/Commands/UndoRedoStack.h
        Public/SagaEditor/Diagnostics/EditorDiagnostic.h
        Public/SagaEditor/Diagnostics/EditorDiagnosticsService.h
        Public/SagaEditor/Diagnostics/IEditorDiagnosticsService.h
        Public/SagaEditor/Project/ProjectIndexer.h
        Public/SagaEditor/Project/ProjectManager.h
        Public/SagaEditor/Project/RecentProjects.h
        Public/SagaEditor/Runtime/IEditorEngineBridge.h
        Public/SagaEditor/Runtime/LocalEditorEngineBridge.h
        Public/SagaEditor/Selection/SelectionFilter.h
        Public/SagaEditor/Selection/SelectionHistory.h
        Public/SagaEditor/Selection/SelectionManager.h
        Public/SagaEditor/Serialization/EditorLayoutSerializer.h
        Public/SagaEditor/Serialization/GraphSerializer.h
        Public/SagaEditor/Serialization/ProjectStateSerializer.h
        Public/SagaEditor/Settings/IEditorSettingsStore.h
        Public/SagaEditor/Settings/MemoryEditorSettingsStore.h
        Public/SagaEditor/Validation/AssetValidation.h
        Public/SagaEditor/Validation/GraphValidation.h
        Public/SagaEditor/Validation/PrefabValidation.h
        Public/SagaEditor/Validation/ReferenceValidator.h
)

saga_register_module_tests(EditorCore
    MODULE_DIR Engine/Source/Editor/EditorCore
    LINK_TARGET SagaEditorLib
    LABELS "unit;editor;core"
    SOURCES
        Tests/EditorDiagnosticsServiceTests.cpp
        Tests/ProjectManagerTests.cpp
        Tests/SelectionStateTests.cpp
)
