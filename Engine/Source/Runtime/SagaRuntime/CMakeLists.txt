saga_add_runtime_module(SagaRuntime
    MODULE_DIR Engine/Source/Runtime/SagaRuntime
    TARGET SagaRuntimeLib
    SOURCES
        Private/SagaEngine/Startup/RuntimeStartupGate.cpp
        Private/SagaRuntime/RuntimeAssetBootstrap.cpp
        Private/SagaRuntime/RuntimeAssetBootstrapDiagnostics.cpp
        Private/SagaRuntime/RuntimeAssetCatalog.cpp
        Private/SagaRuntime/RuntimeModule.cpp
        Private/SagaRuntime/RuntimeServiceLifecycle.cpp
        Private/SagaRuntime/RuntimeServiceRegistry.cpp
        Private/SagaRuntime/RuntimeServiceRegistryDiagnostics.cpp
        Private/SagaRuntime/RuntimeStartupDiagnostics.cpp
        Private/SagaRuntime/RuntimeStartupPreflight.cpp
        Private/SagaRuntime/RuntimeStartupSession.cpp
        Public/SagaEngine/Startup/RuntimeStartupGate.hpp
        Public/SagaRuntime/RuntimeAssetBootstrap.hpp
        Public/SagaRuntime/RuntimeAssetBootstrapDiagnostics.hpp
        Public/SagaRuntime/RuntimeAssetCatalog.hpp
        Public/SagaRuntime/RuntimeModule.hpp
        Public/SagaRuntime/RuntimeServiceLifecycle.hpp
        Public/SagaRuntime/RuntimeServiceRegistry.hpp
        Public/SagaRuntime/RuntimeServiceRegistryDiagnostics.hpp
        Public/SagaRuntime/RuntimeStartupDiagnostics.hpp
        Public/SagaRuntime/RuntimeStartupPreflight.hpp
        Public/SagaRuntime/RuntimeStartupSession.hpp
)
