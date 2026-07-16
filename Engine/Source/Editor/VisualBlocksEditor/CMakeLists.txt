saga_add_editor_module(VisualBlocksEditor
    MODULE_DIR Engine/Source/Editor/VisualBlocksEditor
    TARGET SagaEditorLib
    SOURCES
        Private/SagaEditor/VisualBlocks/Blocks/BlockCategory.cpp
        Private/SagaEditor/VisualBlocks/Blocks/BlockDefinition.cpp
        Private/SagaEditor/VisualBlocks/Blocks/BlockIR.cpp
        Private/SagaEditor/VisualBlocks/Blocks/BlockInstance.cpp
        Private/SagaEditor/VisualBlocks/Blocks/BlockKind.cpp
        Private/SagaEditor/VisualBlocks/Blocks/BlockLibrary.cpp
        Private/SagaEditor/VisualBlocks/Blocks/BlockScript.cpp
        Private/SagaEditor/VisualBlocks/Blocks/BlockSlot.cpp
        Private/SagaEditor/VisualBlocks/Blocks/BlockSnapRules.cpp
        Private/SagaEditor/VisualBlocks/Blocks/BlockToIRLowerer.cpp
        Public/SagaEditor/VisualBlocks/Blocks/BlockCategory.h
        Public/SagaEditor/VisualBlocks/Blocks/BlockDefinition.h
        Public/SagaEditor/VisualBlocks/Blocks/BlockIR.h
        Public/SagaEditor/VisualBlocks/Blocks/BlockInstance.h
        Public/SagaEditor/VisualBlocks/Blocks/BlockKind.h
        Public/SagaEditor/VisualBlocks/Blocks/BlockLibrary.h
        Public/SagaEditor/VisualBlocks/Blocks/BlockScript.h
        Public/SagaEditor/VisualBlocks/Blocks/BlockSlot.h
        Public/SagaEditor/VisualBlocks/Blocks/BlockSnapRules.h
        Public/SagaEditor/VisualBlocks/Blocks/BlockStack.h
        Public/SagaEditor/VisualBlocks/Blocks/BlockToIRLowerer.h
)

saga_add_program(VisualBlocksProductIntegration
    MODULE_DIR Engine/Source/Editor/VisualBlocksEditor
    TARGET SagaProductLib
    PRIVATE_DEPS
        nlohmann_json::nlohmann_json
        OpenSSL::Crypto
    PRIVATE_INCLUDES
        ${SAGA_ROOT}/Engine/Source/Programs/SagaLauncher/Private
    SOURCES
        Private/ProductIntegration/VisualBlocksDescriptor.cpp
        Private/ProductIntegration/VisualBlocksDescriptor.h
)

saga_register_module_tests(VisualBlocksEditor
    MODULE_DIR Engine/Source/Editor/VisualBlocksEditor
    LINK_TARGET SagaEditorLib
    LABELS "unit;editor;visual-blocks"
    SOURCES
        Tests/BlockAuthoringTests.cpp
        Tests/BlockSnapAndIRTests.cpp
)
