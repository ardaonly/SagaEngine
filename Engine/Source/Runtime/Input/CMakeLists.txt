saga_add_runtime_module(Input
    MODULE_DIR Engine/Source/Runtime/Input
    TARGET SagaPlatformSDL
    SOURCES
        Private/SagaEngine/Input/Backends/SDL/SDLInputBackend.cpp
        Private/SagaEngine/Input/Backends/SDL/SDLInputBackend.h
        Private/SagaEngine/Platform/SDL/SDLDebugRenderer2D.cpp
        Private/SagaEngine/Platform/SDL/SDLDebugRenderer2D.h
        Private/SagaEngine/Platform/SDL/SDLPlatformFactory.cpp
        Private/SagaEngine/Platform/SDL/SDLPlatformFactory.h
        Private/SagaEngine/Platform/SDL/SDLWindow.cpp
        Private/SagaEngine/Platform/SDL/SDLWindow.h
)

saga_add_runtime_module(Input
    MODULE_DIR Engine/Source/Runtime/Input
    TARGET SagaEngine
    SOURCES
        Private/SagaEngine/Input/Commands/InputCommandBuffer.cpp
        Private/SagaEngine/Input/Commands/InputCommandSerializer.cpp
        Private/SagaEngine/Input/Core/DeviceRegistry.cpp
        Private/SagaEngine/Input/Core/InputManager.cpp
        Private/SagaEngine/Input/Devices/GamepadDevice.cpp
        Private/SagaEngine/Input/Devices/KeyboardDevice.cpp
        Private/SagaEngine/Input/Devices/MouseDevice.cpp
        Private/SagaEngine/Input/Mapping/InputActionMap.cpp
        Private/SagaEngine/Input/Networking/InputCommandInbox.cpp
        Private/SagaEngine/Input/Networking/InputPacketHandler.cpp
        Private/SagaEngine/Input/Networking/ServerInputProcessor.cpp
        Private/SagaEngine/Platform/PlatformFactory.cpp
        Public/SagaEngine/Input/Backends/IPlatformInputBackend.h
        Public/SagaEngine/Input/Commands/InputCommand.h
        Public/SagaEngine/Input/Commands/InputCommandBuffer.h
        Public/SagaEngine/Input/Commands/InputCommandSerializer.h
        Public/SagaEngine/Input/Core/DeviceRegistry.h
        Public/SagaEngine/Input/Core/InputManager.h
        Public/SagaEngine/Input/Core/InputState.h
        Public/SagaEngine/Input/Devices/GamepadDevice.h
        Public/SagaEngine/Input/Devices/InputDevice.h
        Public/SagaEngine/Input/Devices/KeyboardDevice.h
        Public/SagaEngine/Input/Devices/MouseDevice.h
        Public/SagaEngine/Input/Frames/RawInputFrame.h
        Public/SagaEngine/Input/Mapping/GameplayInputFrame.h
        Public/SagaEngine/Input/Mapping/InputAction.h
        Public/SagaEngine/Input/Mapping/InputActionMap.h
        Public/SagaEngine/Input/Networking/ClientPrediction.h
        Public/SagaEngine/Input/Networking/InputCommandInbox.h
        Public/SagaEngine/Input/Networking/InputPacketHandler.h
        Public/SagaEngine/Input/Networking/ServerInputProcessor.h
        Public/SagaEngine/Input/Types/InputTypes.h
        Public/SagaEngine/Platform/IDebugRenderer2D.h
        Public/SagaEngine/Platform/IFileSystem.h
        Public/SagaEngine/Platform/IHighPrecisionTimer.h
        Public/SagaEngine/Platform/IInputDevice.h
        Public/SagaEngine/Platform/IWindow.h
        Public/SagaEngine/Platform/PlatformFactory.h
)
