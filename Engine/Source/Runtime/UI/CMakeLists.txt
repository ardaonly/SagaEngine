saga_add_runtime_module(UI
    MODULE_DIR Engine/Source/Runtime/UI
    TARGET SagaBackend
    SOURCES
        Private/Backends/RmlUi/RmlUiUiBackend.cpp
        Private/Backends/RmlUi/RmlUiUiBackend.h
)

saga_register_module_tests(UI
    MODULE_DIR Engine/Source/Runtime/UI
    LINK_TARGET SagaEngine
    PRIVATE_DEPS SagaBackend
    LABELS "unit;runtime;ui"
    SOURCES
        Tests/RuntimeUiBootstrapperTests.cpp
        Tests/UiActionBindingTests.cpp
        Tests/UiBackendTests.cpp
        Tests/UiEventBridgeTests.cpp
        Tests/UiInputRouterTests.cpp
        Tests/UiInteractionPipelineTests.cpp
        Tests/UiNamedActionHandlerTests.cpp
        Tests/UiResourceProviderTests.cpp
        Tests/UiScreenManagerTests.cpp
)

saga_add_runtime_module(UI
    MODULE_DIR Engine/Source/Runtime/UI
    TARGET SagaEngine
    SOURCES
        Private/SagaEngine/UI/DefaultUiResourceProvider.cpp
        Private/SagaEngine/UI/RuntimeUiBootstrapper.cpp
        Private/SagaEngine/UI/UiActionDispatcher.cpp
        Private/SagaEngine/UI/UiEventQueue.cpp
        Private/SagaEngine/UI/UiInputRouter.cpp
        Private/SagaEngine/UI/UiNamedActionHandlerRegistry.cpp
        Private/SagaEngine/UI/UiScreenManager.cpp
        Public/SagaEngine/UI/CSharpUiNamedActionHandler.h
        Public/SagaEngine/UI/IUiActionDispatcher.h
        Public/SagaEngine/UI/IUiBackend.h
        Public/SagaEngine/UI/IUiEventQueue.h
        Public/SagaEngine/UI/IUiInputRouter.h
        Public/SagaEngine/UI/IUiNamedActionHandler.h
        Public/SagaEngine/UI/IUiResourceProvider.h
        Public/SagaEngine/UI/IUiScreenManager.h
        Public/SagaEngine/UI/RuntimeUiBootstrapper.h
        Public/SagaEngine/UI/UiResourceIds.h
        Public/SagaEngine/UI/UiTypes.h
)
