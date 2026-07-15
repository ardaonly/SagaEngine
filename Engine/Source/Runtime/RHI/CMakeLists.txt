saga_add_runtime_module(RHI
    MODULE_DIR Engine/Source/Runtime/RHI
    TARGET SagaEngine
    SOURCES
        Private/SagaEngine/Graphics/Backend/GraphicsRenderBackendMapping.h
        Private/SagaEngine/Graphics/Frame/FrameResourceAllocator.h
        Private/SagaEngine/RHI/RHI.cpp
        Public/SagaEngine/Graphics/Backend/GraphicsBackend.h
        Public/SagaEngine/Graphics/Bindings/GraphicsBindingTypes.h
        Public/SagaEngine/Graphics/Bindings/GraphicsBindingValidation.h
        Public/SagaEngine/Graphics/Core/GraphicsTypes.h
        Public/SagaEngine/Graphics/Descs/ResourceDescs.h
        Public/SagaEngine/Graphics/Frame/FrameResources.h
        Public/SagaEngine/Graphics/Graphics.h
        Public/SagaEngine/Graphics/Handles/GraphicsHandle.h
        Public/SagaEngine/RHI/DescriptorCache.h
        Public/SagaEngine/RHI/IRHI.h
        Public/SagaEngine/RHI/RHIResources.h
        Public/SagaEngine/RHI/Types/BufferTypes.h
        Public/SagaEngine/RHI/Types/PipelineTypes.h
        Public/SagaEngine/RHI/Types/ShaderTypes.h
        Public/SagaEngine/RHI/Types/TextureTypes.h
)

saga_add_runtime_module(RHI
    MODULE_DIR Engine/Source/Runtime/RHI
    TARGET SagaGraphicsCore
    SOURCES
        Private/SagaEngine/Graphics/Backend/NullGraphicsBackend.cpp
        Private/SagaEngine/Graphics/Bindings/GraphicsBindingValidation.cpp
)

saga_add_runtime_module(RHI
    MODULE_DIR Engine/Source/Runtime/RHI
    TARGET SagaDiligentRuntime
    SOURCES
        Private/SagaEngine/Graphics/Backends/Diligent/DiligentBindingCompiler.cpp
        Private/SagaEngine/Graphics/Backends/Diligent/DiligentBindingCompiler.h
        Private/SagaEngine/Graphics/Backends/Diligent/DiligentBindingRecords.h
        Private/SagaEngine/Graphics/Backends/Diligent/DiligentBindingResolver.cpp
        Private/SagaEngine/Graphics/Backends/Diligent/DiligentBindingResolver.h
        Private/SagaEngine/Graphics/Backends/Diligent/DiligentFallbackResources.cpp
        Private/SagaEngine/Graphics/Backends/Diligent/DiligentFallbackResources.h
        Private/SagaEngine/Graphics/Backends/Diligent/DiligentGraphicsBackend.cpp
        Private/SagaEngine/Graphics/Backends/Diligent/DiligentGraphicsBackend.h
        Private/SagaEngine/Graphics/Backends/Diligent/DiligentGraphicsBackendBindings.cpp
        Private/SagaEngine/Graphics/Backends/Diligent/DiligentGraphicsBackendDiagnostics.cpp
        Private/SagaEngine/Graphics/Backends/Diligent/DiligentGraphicsBackendFrame.cpp
        Private/SagaEngine/Graphics/Backends/Diligent/DiligentGraphicsBackendResources.cpp
        Private/SagaEngine/Graphics/Backends/Diligent/DiligentGraphicsBackendValidation.h
        Private/SagaEngine/Graphics/Backends/Diligent/DiligentGraphicsResourceRegistry.h
        Private/SagaEngine/Graphics/Backends/Diligent/Runtime/DiligentBindingCache.cpp
        Private/SagaEngine/Graphics/Backends/Diligent/Runtime/DiligentBindingCache.h
        Private/SagaEngine/Graphics/Backends/Diligent/Runtime/DiligentDeviceFactory.cpp
        Private/SagaEngine/Graphics/Backends/Diligent/Runtime/DiligentDeviceFactory.h
        Private/SagaEngine/Graphics/Backends/Diligent/Runtime/DiligentGraphicsRuntime.cpp
        Private/SagaEngine/Graphics/Backends/Diligent/Runtime/DiligentGraphicsRuntime.h
)

saga_add_runtime_module(RHI
    MODULE_DIR Engine/Source/Runtime/RHI
    TARGET SagaGraphicsPrivate
    SOURCES
        Private/SagaEngine/Graphics/GraphicsPrivateAnchor.cpp
)
