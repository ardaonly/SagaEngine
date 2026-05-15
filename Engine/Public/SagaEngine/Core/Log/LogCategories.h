/// @file LogCategories.h
/// @brief Canonical log tag strings for every major engine subsystem.
///
/// Purpose: The existing Log macros take a free-form `tag` argument.  That
///          is flexible but leaves room for drift — one module writes "net",
///          another writes "Net", a third writes "network".  Downstream log
///          filters, grep tooling, and spdlog sinks care about exact string
///          equality, so the tags must be standardised in one header.
///
/// Design:
///   - Every category is a `constexpr const char*` at namespace scope so it
///     compiles down to a string literal in the binary with zero overhead.
///   - Category names are short (≤ 8 chars) to keep log lines compact.
///   - A `LOG_CAT_*` macro set wraps the existing `LOG_*` macros to make
///     the category mandatory and stop the drift mentioned above.
///
/// Usage:
///   LOG_CAT_INFO(Network, "accepted connection from %s", addr);
///   LOG_CAT_WARN(Replication, "bandwidth budget exceeded by %u bytes", over);

#pragma once

#include "SagaEngine/Core/Log/Log.h"

namespace SagaEngine::Core::Log::Category
{

// ─── Core engine ──────────────────────────────────────────────────────────────

/// Engine start-up, subsystem bring-up, module lifecycle.
inline constexpr const char* Engine      = "engine";

/// Memory allocator, arena, pool traces.
inline constexpr const char* Memory      = "memory";

/// Job system, worker threads, task graph.
inline constexpr const char* Threading   = "thread";

/// File I/O, resource loading, asset cache.
inline constexpr const char* Resources   = "res";

// ─── Gameplay simulation ─────────────────────────────────────────────────────

/// Simulation tick loop, determinism guards, authority.
inline constexpr const char* Simulation  = "sim";

/// Entity/component/archetype, ECS migrations.
inline constexpr const char* ECS         = "ecs";

/// Client-side prediction, reconciliation, input forwarding.
inline constexpr const char* Prediction  = "predict";

// ─── Networking ──────────────────────────────────────────────────────────────

/// Transport-layer send/receive, handshake, rate limiter.
inline constexpr const char* Network     = "net";

/// Packet framing, reliable channel, fragmentation/reassembly.
inline constexpr const char* Packet      = "packet";

/// Per-entity replication, dirty tracking, snapshot encode/decode.
inline constexpr const char* Replication = "repl";

/// Zone-server boundaries, shard manager, migration hand-off.
inline constexpr const char* Zone        = "zone";

// ─── Persistence ─────────────────────────────────────────────────────────────

/// PostgreSQL / durable storage.
inline constexpr const char* Database    = "db";

/// Redis / hot cache, session state, presence.
inline constexpr const char* Cache       = "cache";

// ─── Security ────────────────────────────────────────────────────────────────

/// TLS handshake, certificate handling, auth token validation.
inline constexpr const char* Security    = "sec";

// ─── Rendering / client ──────────────────────────────────────────────────────

/// RHI, render graph, GPU resource upload.
inline constexpr const char* Render      = "render";

/// Audio pipeline and mixer.
inline constexpr const char* Audio       = "audio";

/// Input backends (keyboard, mouse, gamepad).
inline constexpr const char* Input       = "input";

// ─── Test / development ──────────────────────────────────────────────────────

/// Test harness, scenario runner, mock backends.
inline constexpr const char* Test        = "test";

} // namespace SagaEngine::Core::Log::Category

// ─── Category-aware macros ───────────────────────────────────────────────────
//
// These wrap the plain LOG_* macros so the category is always a canonical
// constant from above.  Use them in new code; the plain LOG_* variants are
// kept for backwards compatibility with existing call sites.

#define LOG_CAT_TRACE(cat, fmt, ...) \
    LOG_TRACE(::SagaEngine::Core::Log::Category::cat, fmt, ##__VA_ARGS__)

#define LOG_CAT_DEBUG(cat, fmt, ...) \
    LOG_DEBUG(::SagaEngine::Core::Log::Category::cat, fmt, ##__VA_ARGS__)

#define LOG_CAT_INFO(cat, fmt, ...) \
    LOG_INFO(::SagaEngine::Core::Log::Category::cat, fmt, ##__VA_ARGS__)

#define LOG_CAT_WARN(cat, fmt, ...) \
    LOG_WARN(::SagaEngine::Core::Log::Category::cat, fmt, ##__VA_ARGS__)

#define LOG_CAT_ERROR(cat, fmt, ...) \
    LOG_ERROR(::SagaEngine::Core::Log::Category::cat, fmt, ##__VA_ARGS__)

#define LOG_CAT_CRITICAL(cat, fmt, ...) \
    LOG_CRITICAL(::SagaEngine::Core::Log::Category::cat, fmt, ##__VA_ARGS__)
