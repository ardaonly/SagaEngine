saga_add_runtime_module(Math
    MODULE_DIR Engine/Source/Runtime/Math
    TARGET SagaEngine
    PUBLIC_DEPS glm::glm
    SOURCES
        Private/SagaEngine/Math/DeterministicMath.cpp
        Private/SagaEngine/Math/Mat4.cpp
        Private/SagaEngine/Math/MathGLMBridge.cpp
        Private/SagaEngine/Math/Quat.cpp
        Private/SagaEngine/Math/Transform.cpp
        Private/SagaEngine/Math/Vec3.cpp
        Public/SagaEngine/Math/DeterministicMath.h
        Public/SagaEngine/Math/DeterministicVectors.h
        Public/SagaEngine/Math/Mat4.h
        Public/SagaEngine/Math/Math.h
        Public/SagaEngine/Math/MathConstants.h
        Public/SagaEngine/Math/MathGLMBridge.h
        Public/SagaEngine/Math/MathScalar.h
        Public/SagaEngine/Math/MathSimd.h
        Public/SagaEngine/Math/Quat.h
        Public/SagaEngine/Math/Transform.h
        Public/SagaEngine/Math/Vec3.h
)
