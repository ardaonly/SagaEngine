saga_add_runtime_module(Simulation
    MODULE_DIR Engine/Source/Runtime/Simulation
    TARGET SagaEngine
    SOURCES
        Private/SagaEngine/Animation/AnimationClip.cpp
        Private/SagaEngine/Animation/PoseEvaluator.cpp
        Private/SagaEngine/Animation/Skeleton.cpp
        Private/SagaEngine/Physics/PhysicsEngine.cpp
        Private/SagaEngine/Physics/PhysicsWorld.cpp
        Private/SagaEngine/Simulation/Authority.cpp
        Private/SagaEngine/Simulation/Deterministic.cpp
        Private/SagaEngine/Simulation/SimulationTick.cpp
        Private/SagaEngine/Simulation/WorldPartitionSystem.cpp
        Private/SagaEngine/Simulation/WorldState.cpp
        Public/SagaEngine/Animation/AnimationClip.h
        Public/SagaEngine/Animation/PoseEvaluator.h
        Public/SagaEngine/Animation/Skeleton.h
        Public/SagaEngine/Physics/PhysicsEngine.h
        Public/SagaEngine/Physics/PhysicsTypes.h
        Public/SagaEngine/Physics/PhysicsWorld.h
        Public/SagaEngine/Simulation/Authority.h
        Public/SagaEngine/Simulation/Deterministic.h
        Public/SagaEngine/Simulation/SimulationTick.h
        Public/SagaEngine/Simulation/WorldPartitionSystem.h
        Public/SagaEngine/Simulation/WorldState.h
)
