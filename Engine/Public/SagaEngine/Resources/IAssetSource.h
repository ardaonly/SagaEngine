/// @file IAssetSource.h
/// @brief Pluggable backend contract that fulfils streaming requests.
///
/// Layer  : SagaEngine / Resources
/// Purpose: `StreamingRequest.h` describes *what* callers want.  This
///          header describes *who* actually fetches the bytes.  The
///          streaming manager never touches `fopen`, a package file,
///          or a network socket on its own; it calls into an
///          `IAssetSource` and the source decides how to produce the
///          decoded payload.  That indirection is the single reason
///          the same manager instance can power a desktop build that
///          reads from a loose `assets/` folder, a shipped client
///          that reads from a signed package, and a development
///          build that proxies through an editor's live-reload
///          server.
///
/// Design rules:
///   - One method.  `LoadBlocking` is called on a worker thread and
///     is expected to block that thread until the request is
///     finished or the handle's `CancelRequested()` flag has been
///     observed.  Sources are free to be synchronous internally —
///     the manager already owns the worker pool, the source does
///     not need to spin up its own.
///   - No throwing.  The result is carried in an `AssetLoadResult`
///     so the manager does not have to wrap every call in a
///     `try` / `catch` — the engine's hot path is exception-free
///     by design.
///   - Sources must be reentrant.  The manager will call
///     `LoadBlocking` from several worker threads concurrently on
///     the same instance; implementations own their own
///     synchronisation if they need any.
///   - Cooperative cancellation.  Long-running decodes (a 200 MB
///     mesh LOD coming off spinning rust) should poll the flag and
///     return early with `StreamingStatus::Cancelled` so the
///     manager can free the slot for another request.
///
/// What this header is NOT:
///   - Not an IO library.  `fopen`, `memory-map`, and network
///     primitives live inside concrete source implementations, not
///     on this interface.
///   - Not a cache.  The streaming manager and the resource
///     manager own residency; the source just turns one request
///     into one result, every time.

#pragma once

#include "SagaEngine/Resources/StreamingRequest.h"

#include <cstdint>
#include <string>

namespace SagaEngine::Resources {

// ─── Result ────────────────────────────────────────────────────────────────

/// Outcome of a single `LoadBlocking` call.  Carried back to the
/// streaming manager, which copies the status into the public
/// `StreamingRequestHandle` and moves the payload into the handle.
struct AssetLoadResult
{
    /// Terminal status for this request.  `Pending` is never a valid
    /// return value — the source must pick a concrete code before
    /// returning, otherwise the manager leaves the handle stuck.
    StreamingStatus status = StreamingStatus::InternalError;

    /// Decoded payload, moved out on success.  Callers must set
    /// `kind`, `lod`, `sizeBytes`, and `bytes` together so downstream
    /// consumers can never see a partially-populated result.
    StreamingResultPayload payload;

    /// Optional human-readable diagnostic.  Surfaced in logs when
    /// the source returns a non-`Ok` status.  Empty on success so
    /// the hot path allocates nothing.
    std::string diagnostic;
};

// ─── IAssetSource ──────────────────────────────────────────────────────────

/// Abstract asset source.  Implementations are owned by the
/// streaming manager and live for the manager's full lifetime; the
/// manager retains a `unique_ptr` so destruction is deterministic.
class IAssetSource
{
public:
    virtual ~IAssetSource() = default;

    /// Load one asset synchronously.  Runs on a streaming manager
    /// worker thread; implementations may issue their own blocking
    /// I/O and are expected to poll the handle's
    /// `CancelRequested()` accessor at the granularity that makes
    /// sense for the backend (every few KiB for disk, every chunk
    /// for network sources).
    ///
    /// Ownership:
    ///   - `request` is a read-only view owned by the manager.
    ///   - `handle` is a read-only view owned by the caller; the
    ///     source must not mutate it or publish a payload through
    ///     it — publishing is the manager's job.
    ///   - The returned `AssetLoadResult` owns its `payload.bytes`;
    ///     the manager moves it into the public handle.
    ///
    /// Threading:
    ///   - Reentrant.  Called from several worker threads in
    ///     parallel; implementations that share state must guard
    ///     it themselves.
    ///
    /// Cancellation:
    ///   - `handle.CancelRequested()` becomes true if the caller
    ///     cancels the public handle or the manager shuts down.  A
    ///     source that observes it should return with
    ///     `StreamingStatus::Cancelled` and an empty payload.
    [[nodiscard]] virtual AssetLoadResult LoadBlocking(
        const StreamingRequest&        request,
        const StreamingRequestHandle&  handle) = 0;

    /// Optional hot-path hint.  The streaming manager forwards
    /// prefetch notifications through this entry point so backends
    /// that can warm a cache (page-in a memory-mapped package file,
    /// prime a CDN connection) get a chance to do so.  Default does
    /// nothing, so simple disk sources do not have to override it.
    virtual void Prefetch(AssetId /*assetId*/, AssetKind /*kind*/) noexcept {}

    /// Optional identifier used in logs.  Defaults to an empty
    /// string; concrete sources override with something stable so
    /// operators can tell "the disk source returned AssetNotFound"
    /// from "the package source returned AssetNotFound".
    [[nodiscard]] virtual const char* DebugName() const noexcept { return ""; }
};

} // namespace SagaEngine::Resources
