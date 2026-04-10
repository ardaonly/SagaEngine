/// @file ISchemaMigrator.h
/// @brief Versioned schema migration runner for SQL backends.
///
/// Layer  : Backends / Services / Persistence / Database
/// Purpose: Production databases evolve forever.  A running cluster
///          has to apply new migrations on rolling restarts, detect
///          that a peer has already applied them, refuse to start if
///          the on-disk schema is newer than the running binary
///          expects, and leave an audit trail of when each migration
///          ran.  This header defines the interface every backend
///          must satisfy so the rest of the engine can call
///          `MigrateToLatest` at boot without caring whether it is
///          talking to PostgreSQL, SQLite (for tests), or a future
///          backend.
///
/// Design rules:
///   - Migrations are identified by a monotonic 64-bit version.  We
///     do NOT use timestamps as migration ids because rebases in a
///     busy repository generate duplicate timestamps and the ordering
///     turns into a guessing game.  A strict integer sequence is
///     easy to reason about.
///   - Each migration is atomic.  The runner wraps the up-script in a
///     transaction so a failure half-way through rolls back rather
///     than leaving the schema in an undefined state.  Backends that
///     do not support DDL inside a transaction are explicitly
///     rejected at Start time; we would rather fail loud than silently
///     ship half-applied schemas.
///   - The runner takes a cooperative lock before applying anything
///     so two instances starting simultaneously do not race on the
///     migrations table.  The lock is released as soon as the last
///     migration commits.
///   - The schema_migrations table is the source of truth.  The
///     runner compares the on-disk table to the compiled-in
///     migration list and refuses to start if the database has seen
///     a version the binary does not know about — that signals an
///     accidental downgrade which is always operator error.
///
/// What this header is NOT:
///   - Not a query builder.  Migrations are plain SQL strings the
///     application code registers; the runner just executes them.
///   - Not a rollback tool.  We do not ship down-migrations; the
///     recovery story is "restore from backup" because rolling
///     forward is dramatically safer than rolling back in a live
///     MMO.

#pragma once

#include "Services/Persistence/Database/IDatabaseConnectionPool.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace SagaEngine::Persistence::Database {

// ─── Migration descriptor ──────────────────────────────────────────────────

/// One entry in the migration ladder.  Version is the monotonic
/// sequence number; `name` is a short human-readable slug used in
/// logs; `upSql` is the full SQL body (may contain multiple
/// statements separated by semicolons if the backend supports it).
struct SchemaMigration
{
    /// Monotonic version — never reused, never reordered.  Gaps are
    /// allowed (you can skip 7 and jump from 6 to 8) because the
    /// runner compares by "max seen" not by "contiguous range".
    std::uint64_t version = 0;

    /// Short stable slug like "add_inventory_versioning" or
    /// "split_character_positions".  Goes into the schema_migrations
    /// table so operators can eyeball progress without pulling the
    /// SQL body.
    std::string name;

    /// Complete forward migration body.  One or more SQL statements;
    /// the runner executes them inside a single transaction.
    std::string upSql;

    /// Optional free-form tag: ticket id, author, release notes link.
    /// Logged but not otherwise interpreted.
    std::string metadata;
};

// ─── Outcome codes ─────────────────────────────────────────────────────────

/// Status returned by `MigrateToLatest`.  Distinct codes so the
/// boot sequence can decide whether to retry, abort, or escalate to
/// the operator.
enum class MigrationResult : std::uint8_t
{
    Ok = 0,                    ///< All known migrations are present.
    NoChangesNeeded,           ///< Schema was already at the latest version.
    LockAcquisitionFailed,     ///< Another instance holds the migration lock.
    BackendDowngradeDetected,  ///< DB contains versions the binary does not know.
    BackendError,              ///< A specific migration SQL was rejected.
    UnsupportedBackend,        ///< Backend cannot run DDL in a transaction.
    InternalError,
};

/// Per-migration outcome entry used by `LastRunReport()` so operators
/// can see exactly which migrations were applied during the most
/// recent boot.
struct MigrationRunEntry
{
    std::uint64_t version       = 0;
    std::string   name;
    bool          applied       = false; ///< False = already in DB, skipped.
    std::uint64_t durationMicros = 0;
};

// ─── Interface ─────────────────────────────────────────────────────────────

/// Abstract migration runner.  One instance per backend.  Typically
/// invoked once at boot before any gameplay service touches the
/// database.
class ISchemaMigrator
{
public:
    virtual ~ISchemaMigrator() = default;

    /// Bind the migrator to a running connection pool.  The migrator
    /// does not own the pool; it borrows one connection during
    /// execution and releases it on return.
    [[nodiscard]] virtual bool Start(DatabasePoolPtr pool) = 0;

    /// Release any borrowed state.  Safe to call after a failed
    /// Start.  Not strictly necessary for correctness but keeps the
    /// shutdown path symmetrical.
    virtual void Stop() = 0;

    /// Register the migration ladder.  The caller hands over the
    /// full known list; the runner sorts by version and deduplicates.
    /// Must be called before `MigrateToLatest`; calling it afterwards
    /// is a programming error and returns false without touching
    /// state.
    [[nodiscard]] virtual bool RegisterMigrations(
        std::vector<SchemaMigration> migrations) = 0;

    /// Apply every registered migration that is not yet present in
    /// the database.  The runner:
    ///   1. Ensures the schema_migrations table exists.
    ///   2. Takes the cooperative lock.
    ///   3. Reads the set of applied versions.
    ///   4. Fails if any applied version is not in the registered
    ///      list (downgrade detection).
    ///   5. Applies each missing migration in order, inside a
    ///      transaction, recording the row in schema_migrations.
    ///   6. Releases the lock.
    /// Returns the overall result; individual migration outcomes are
    /// available via `LastRunReport()`.
    [[nodiscard]] virtual MigrationResult MigrateToLatest() = 0;

    /// Report of the most recent `MigrateToLatest` call.  Empty if
    /// never called.  Intended for boot logs and for the operator's
    /// `/dbmigrate` command.
    [[nodiscard]] virtual const std::vector<MigrationRunEntry>& LastRunReport() const noexcept = 0;

    /// Highest version currently present in the database.  Queried
    /// lazily the first time it is read and cached until the next
    /// `MigrateToLatest` call.  Zero means "no migrations yet".
    [[nodiscard]] virtual std::uint64_t CurrentVersion() const = 0;

    [[nodiscard]] virtual const char* BackendName() const noexcept = 0;
};

using SchemaMigratorPtr = std::shared_ptr<ISchemaMigrator>;

// ─── Constants ─────────────────────────────────────────────────────────────

/// Name of the bookkeeping table.  Exposed here so operators can
/// find it in psql without grepping the source.  Implementations
/// must use this exact name so migrations written against one
/// backend do not have to be rewritten for another.
inline constexpr const char* kSchemaMigrationsTableName = "schema_migrations";

/// Cooperative advisory lock id used by PostgreSQL-backed
/// implementations.  Picked arbitrarily but pinned so operators can
/// check `pg_locks` and understand what they are seeing.
inline constexpr std::uint64_t kSchemaMigrationsAdvisoryLockId = 0x5A6AC0DE00000001ULL;

} // namespace SagaEngine::Persistence::Database
