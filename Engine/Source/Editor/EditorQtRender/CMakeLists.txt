# SPDX-License-Identifier: MPL-2.0
saga_add_editor_module(EditorQtRender
    MODULE_DIR Engine/Source/Editor/EditorQtRender
    TARGET SagaEditorLib
    PRIVATE_DEPS
        nlohmann_json::nlohmann_json
    SOURCES
        Private/SagaEditorQtRender/QtRenderHost.cpp
        Public/SagaEditorQtRender/QtRenderHost.h
)

saga_register_module_tests(EditorQtRender
    MODULE_DIR Engine/Source/Editor/EditorQtRender
    LINK_TARGET SagaEditorLib
    LABELS "unit;editor;qt-render"
    SOURCES
        Tests/QtRenderHostTests.cpp
)
