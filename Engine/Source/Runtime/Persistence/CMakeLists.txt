saga_add_runtime_module(Persistence
    MODULE_DIR Engine/Source/Runtime/Persistence
    TARGET SagaBackend
    SOURCES
        Private/SagaEngine/Persistence/Backends/PostgreSQL/PostgreSQLImpl.cpp
        Private/SagaEngine/Persistence/Backends/PostgreSQL/PostgreSQLImpl.h
        Private/SagaEngine/Persistence/Backends/Redis/RedisImpl.cpp
        Private/SagaEngine/Persistence/Backends/Redis/RedisImpl.h
        Private/Persistence/EventSourcing/EventLog.cpp
        Public/SagaEngine/Persistence/Database/IAsyncQueryExecutor.h
        Public/SagaEngine/Persistence/Database/IDatabaseConnectionPool.h
        Public/SagaEngine/Persistence/Database/IPresenceTracker.h
        Public/SagaEngine/Persistence/Database/IPubSubBus.h
        Public/SagaEngine/Persistence/Database/ISchemaMigrator.h
        Public/SagaEngine/Persistence/Database/ISessionCache.h
        Public/SagaEngine/Persistence/EventSourcing/EventLog.h
        Public/SagaEngine/Persistence/IDatabase.h
        Public/SagaEngine/Persistence/Types.h
)
