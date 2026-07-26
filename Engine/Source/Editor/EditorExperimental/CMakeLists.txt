# SPDX-License-Identifier: MPL-2.0
saga_add_editor_module(EditorExperimental
    MODULE_DIR Engine/Source/Editor/EditorExperimental
    TARGET SagaEditorLib
    SOURCES
        Private/SagaEditor/Extensions/ExtensionHost.cpp
        Private/SagaEditor/Extensions/ExtensionRegistry.cpp
        Public/SagaEditor/Extensions/ExtensionHost.h
        Public/SagaEditor/Extensions/ExtensionRegistry.h
        Public/SagaEditor/Extensions/IEditorExtension.h
        Public/SagaEditor/Extensions/IExtensionContext.h
)

saga_register_module_tests(EditorExperimental
    MODULE_DIR Engine/Source/Editor/EditorExperimental
    LINK_TARGET SagaEditorLib
    SOURCES
        Tests/ExtensionRegistryTests.cpp
    LABELS unit editor extensions
)
