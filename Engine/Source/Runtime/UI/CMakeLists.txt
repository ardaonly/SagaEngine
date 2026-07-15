saga_add_runtime_module(UI
    MODULE_DIR Engine/Source/Runtime/UI
    TARGET SagaBackend
    SOURCES
        Private/Backends/RmlUi/RmlUiUiBackend.cpp
        Private/Backends/RmlUi/RmlUiUiBackend.h
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
