---
title: Persistence contracts
status: bounded
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Persistence contracts

The Persistence module contains storage-facing interfaces and a bounded event-log implementation. Its public contract covers database operations, asynchronous queries, connection pools, schema migration, session caching, presence, pub/sub, and persistence value types. These foundations do not establish a deployed database service or shipped persistent worlds.

## Contract layers

`IDatabase` and the values in `Types.h` form the general storage boundary used by current integration paths. More focused interfaces separate concerns that otherwise tend to collect inside a concrete database wrapper:

| Contract | Responsibility |
| --- | --- |
| `IAsyncQueryExecutor` | Submit database work without turning a backend connection into shared caller state. |
| `IDatabaseConnectionPool` | Acquire/release bounded connections and expose pool status. |
| `ISchemaMigrator` | Inspect and apply an explicitly selected schema transition. |
| `ISessionCache` | Store short-lived session-oriented values behind a cache contract. |
| `IPresenceTracker` | Present an account/zone heartbeat view over session state. |
| `IPubSubBus` | Publish and subscribe to transient messages without pretending they are durable events. |
| `EventLog` | Queue and flush ordered event records through the configured storage path. |

Callers should depend on the narrowest contract that expresses their operation. Taking `PostgreSQLImpl` or `RedisImpl` merely to reach one of these behaviors couples domain code to connection and vendor policy.

## Asynchronous ownership and backpressure

An asynchronous result needs an explicit completion state, error representation, and lifetime. Work accepted into a queue is not equivalent to durable storage. The caller or owning service must know whether a request was rejected, queued, completed, failed, or cancelled during shutdown.

Connection pools bound scarce backend resources. A pool must not hand the same mutable connection to unrelated concurrent callers without a documented synchronization model. Exhaustion and shutdown are ordinary failure paths and should not degrade into an unbounded wait.

The current EventLog tests cover initialization, single and multiple append, queue backpressure, flush, concurrent append, and statistics. They demonstrate the in-process queue/flush contract. They do not prove crash-consistent durability, exactly-once delivery, replication, backup, or recovery after process termination.

## Schema migration

`ISchemaMigrator` makes schema transition an explicit operation rather than a side effect of an arbitrary query. A migration workflow should identify the current and requested versions, order steps deterministically, stop on failure, and report what was and was not applied.

The presence of this interface is not evidence of a supported rolling-upgrade policy or a complete migration catalog. Compatibility windows, downgrade policy, locking, backup, and operational approval require their own implemented workflow and evidence.

## Cache, presence, and pub/sub

Session cache data is short-lived operational state, not the canonical project or world database. Presence builds a view over that state using account identity, zone/process ownership, and heartbeat freshness. A stale heartbeat is different from an explicit offline record, and a forced takeover is a policy decision rather than an incidental overwrite.

Presence does not authenticate an account, implement a social graph, or prove that a zone service is healthy. Pub/sub does not make a message durable. If an operation must survive process loss, the owning design needs a durable record and replay/idempotency policy rather than relying on a transient channel.

## Event-log identity

An event record needs a stable identity, ordering context, payload contract, and bounded metadata. The log owner must define duplicate handling and how a consumer resumes. Appending bytes successfully is not sufficient evidence that replay will reproduce authoritative state.

Event sourcing is therefore an available foundation, not a blanket architecture requirement. State that is already authoritative elsewhere should not be forced through EventLog without a clear recovery or audit use case.

## Concrete backend visibility

The checked-in boundary is vendor-neutral public contracts with `PostgreSQLImpl` and `RedisImpl` adapters wholly under `Private/Backends`. Public Persistence headers do not expose pqxx, hiredis, redis++, or other vendor implementation types. Architecture checks protect that type/include boundary; tests that intentionally exercise concrete adapters use private test configuration and do not turn those headers into installed API.

## Failure and evidence boundary

Persistence errors should retain the operation class and stable diagnostic context while avoiding credentials, raw secrets, or unrestricted query contents. Timeouts, pool exhaustion, malformed records, schema mismatch, duplicate identity, backend unavailability, and shutdown cancellation are distinct outcomes.

PostgreSQL integration tests exercise initialization, entity write/read, statistics, and queue pressure in their configured environment. Passing them proves the named adapter path against that environment. It does not establish production topology, failover, encryption, access control, monitoring, retention, backup, or capacity.

For project/package persistence boundaries see [Persistence, assets, and packages](../runtime/persistence-and-packages.md). For online authority limits see [Replication, networking, and server authority](replication-networking-and-authority.md).
