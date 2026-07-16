saga_add_runtime_module(Audio
    MODULE_DIR Engine/Source/Runtime/Audio
    TARGET SagaEngine
    SOURCES
        Private/SagaEngine/Audio/AudioEngine.cpp
        Private/SagaEngine/Audio/AudioEventMapper.cpp
        Private/SagaEngine/Audio/AudioFilter.cpp
        Private/SagaEngine/Audio/AudioQueue.cpp
        Public/SagaEngine/Audio/AudioEngine.h
        Public/SagaEngine/Audio/AudioEvent.h
        Public/SagaEngine/Audio/AudioEventMapper.h
        Public/SagaEngine/Audio/AudioFilter.h
        Public/SagaEngine/Audio/AudioQueue.h
        Public/SagaEngine/Audio/AudioTypes.h
        Public/SagaEngine/Audio/IAudioBackend.h
)

saga_register_module_tests(Audio
    MODULE_DIR Engine/Source/Runtime/Audio
    LINK_TARGET SagaEngine
    SOURCES
        Tests/AudioPipelineTests.cpp
    LABELS unit runtime audio
)
