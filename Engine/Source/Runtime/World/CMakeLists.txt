saga_add_runtime_module(World
    MODULE_DIR Engine/Source/Runtime/World
    TARGET SagaEngine
    SOURCES
        Private/SagaEngine/World/DomainScheduler.cpp
        Private/SagaEngine/World/DomainScheduler.h
        Private/SagaEngine/World/EventStream.cpp
        Private/SagaEngine/World/EventStream.h
        Private/SagaEngine/World/Partition/Boundary.cpp
        Private/SagaEngine/World/Partition/Boundary.h
        Private/SagaEngine/World/Partition/Cell.cpp
        Private/SagaEngine/World/Partition/Cell.h
        Private/SagaEngine/World/Partition/Zone.cpp
        Private/SagaEngine/World/Partition/Zone.h
        Private/SagaEngine/World/RelevanceGraph.cpp
        Private/SagaEngine/World/RelevanceGraph.h
        Private/SagaEngine/World/SimCell.cpp
        Private/SagaEngine/World/SimCell.h
        Private/SagaEngine/World/SimCell_SpatialLogic.cpp
        Private/SagaEngine/World/Streaming/LODManager.cpp
        Private/SagaEngine/World/Streaming/LODManager.h
        Private/SagaEngine/World/Streaming/ResourceStream.cpp
        Private/SagaEngine/World/Streaming/ResourceStream.h
        Private/SagaEngine/World/WorldNode.cpp
        Private/SagaEngine/World/WorldNode.h
        Public/SagaEngine/World/WorldFacade.h
)
