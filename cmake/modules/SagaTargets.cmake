function(saga_create_engine_targets)
    add_library(SagaEngine STATIC)
    saga_apply_compiler_flags(SagaEngine)
    saga_link_thirdparty(SagaEngine)
    target_sources(SagaEngine PRIVATE
        Engine/src/SagaEngine/Core/Application/Application.cpp
        Engine/src/SagaEngine/Core/Log/Log.cpp
        Engine/src/SagaEngine/Core/Memory/ArenaAllocator.cpp
        Engine/src/SagaEngine/Core/Memory/MemoryTracker.cpp
        Engine/src/SagaEngine/Core/Profiling/Profiler.cpp
        Engine/src/SagaEngine/Core/Subsystem/SubsystemManager.cpp
        Engine/src/SagaEngine/Core/Threading/JobSystem.cpp
        Engine/src/SagaEngine/Core/Time/Time.cpp
        Engine/src/SagaEngine/ECS/Archetype.cpp
        Engine/src/SagaEngine/ECS/Component.cpp
        Engine/src/SagaEngine/ECS/Entity.cpp
        Engine/src/SagaEngine/ECS/Query.cpp
        Engine/src/SagaEngine/ECS/Serialization.cpp
        Engine/src/SagaEngine/ECS/System.cpp
        Engine/src/SagaEngine/Input/GamepadDevice.cpp
        Engine/src/SagaEngine/Input/InputAction.cpp
        Engine/src/SagaEngine/Input/InputEvent.cpp
        Engine/src/SagaEngine/Input/InputManager.cpp
        Engine/src/SagaEngine/Input/InputMapping.cpp
        Engine/src/SagaEngine/Input/InputSubsystem.cpp
        Engine/src/SagaEngine/Input/KeyboardDevice.cpp
        Engine/src/SagaEngine/Input/MouseDevice.cpp
        Engine/src/SagaEngine/Input/Network/InputBuffer.cpp
        Engine/src/SagaEngine/Input/Network/NetworkInputLayer.cpp
        Engine/src/SagaEngine/Networking/Core/NetworkTransport.cpp
        Engine/src/SagaEngine/Networking/Core/ReliableChannel.cpp
        Engine/src/SagaEngine/Networking/Interest/InterestArea.cpp
        Engine/src/SagaEngine/Networking/Replication/ReplicationManager.cpp
        Engine/src/SagaEngine/Platform/PlatformFactory.cpp
        Engine/src/SagaEngine/Platform/Windows/HighPrecisionTimerWin32.cpp
        Engine/src/SagaEngine/Platform/Windows/InputWin32.cpp
        Engine/src/SagaEngine/Platform/Windows/WindowWin32.cpp
        Engine/src/SagaEngine/RHI/RHI.cpp
        Engine/src/SagaEngine/Render/CommandRecording/CommandBuffer.cpp
        Engine/src/SagaEngine/Render/FrameGraphExecutor.cpp
        Engine/src/SagaEngine/Render/RenderCommand.cpp
        Engine/src/SagaEngine/Render/RenderGraph/RGCompilation.cpp
        Engine/src/SagaEngine/Render/RenderGraph/RenderGraph.cpp
        Engine/src/SagaEngine/Render/RenderPass.cpp
        Engine/src/SagaEngine/Render/Renderer.cpp
        Engine/src/SagaEngine/Simulation/Authority.cpp
        Engine/src/SagaEngine/Simulation/Deterministic.cpp
        Engine/src/SagaEngine/Simulation/SimulationTick.cpp
        Engine/src/SagaEngine/Simulation/WorldState.cpp
        Main/main.cpp
    )
    add_library(SagaBackend STATIC)
    saga_apply_compiler_flags(SagaBackend)
    saga_link_thirdparty(SagaBackend)
    target_link_libraries(SagaBackend PRIVATE SagaEngine)
    target_sources(SagaBackend PRIVATE
        Backends/src/Persistence/Database/PostgreSQLImpl.cpp
        Backends/src/Persistence/Database/RedisImpl.cpp
        Backends/src/Persistence/EventSourcing/EventLog.cpp
        Backends/src/Services/AnalyticsService.cpp
        Backends/src/Services/AuthService.cpp
        Backends/src/Services/ChatService.cpp
        Backends/src/Services/EconomyService.cpp
        Backends/src/Services/MatchmakingService.cpp
    )
    add_executable(SagaApp)
    saga_apply_compiler_flags(SagaApp)
    saga_link_thirdparty(SagaApp)
    target_link_libraries(SagaApp PRIVATE SagaEngine SagaBackend)
    target_sources(SagaApp PRIVATE
        Main/main.cpp
    )
endfunction()