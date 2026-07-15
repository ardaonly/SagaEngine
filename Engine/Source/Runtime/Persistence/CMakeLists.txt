saga_add_runtime_module(Persistence
    MODULE_DIR Engine/Source/Runtime/Persistence
    TARGET SagaBackend
    SOURCES
        Private/Persistence/Database/PostgreSQLImpl.cpp
        Private/Persistence/Database/RedisImpl.cpp
        Private/Persistence/EventSourcing/EventLog.cpp
        Public/SagaEngine/Persistence/Database/IAsyncQueryExecutor.h
        Public/SagaEngine/Persistence/Database/IDatabaseConnectionPool.h
        Public/SagaEngine/Persistence/Database/IPresenceTracker.h
        Public/SagaEngine/Persistence/Database/IPubSubBus.h
        Public/SagaEngine/Persistence/Database/ISchemaMigrator.h
        Public/SagaEngine/Persistence/Database/ISessionCache.h
        Public/SagaEngine/Persistence/Database/PostgreSQLImpl.h
        Public/SagaEngine/Persistence/Database/RedisImpl.h
        Public/SagaEngine/Persistence/EventSourcing/EventLog.h
        Public/SagaEngine/Persistence/IDatabase.h
        Public/SagaEngine/Persistence/Types.h
)
