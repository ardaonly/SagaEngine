saga_add_editor_module(EditorExperimental
    MODULE_DIR Engine/Source/Editor/EditorExperimental
    TARGET SagaEditorLib
    SOURCES
        Private/SagaEditor/Extensions/ExtensionCommandBridge.cpp
        Private/SagaEditor/Extensions/ExtensionHost.cpp
        Private/SagaEditor/Extensions/ExtensionPanelBridge.cpp
        Private/SagaEditor/Extensions/ExtensionRegistry.cpp
        Public/SagaEditor/Extensions/ExtensionCommandBridge.h
        Public/SagaEditor/Extensions/ExtensionHost.h
        Public/SagaEditor/Extensions/ExtensionPanelBridge.h
        Public/SagaEditor/Extensions/ExtensionPoint.h
        Public/SagaEditor/Extensions/ExtensionRegistry.h
        Public/SagaEditor/Extensions/IEditorExtension.h
        Public/SagaEditor/Extensions/IExtensionCommand.h
        Public/SagaEditor/Extensions/IExtensionContext.h
        Public/SagaEditor/Extensions/IExtensionPanel.h
        Public/SagaEditor/Extensions/IExtensionSerializer.h
)

saga_register_module_tests(EditorExperimental
    MODULE_DIR Engine/Source/Editor/EditorExperimental
    LINK_TARGET SagaEditorLib
    SOURCES
        Tests/ExtensionRegistryTests.cpp
    LABELS unit editor extensions
)
