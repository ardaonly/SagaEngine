/// @file DeterminismVerifier.h
/// @brief Cross-platform determinism enforcement for replication apply.
///
/// Layer  : SagaEngine / Client / Replication
/// Purpose: Ensures that the client-side ECS apply pipeline produces
///          identical results regardless of CPU architecture (x86 vs ARM),
///          compiler optimisation level, or floating-point unit quirks.
///          Without this guarantee, reconciliation and prediction are
///          fundamentally unreliable.
///
/// What this module enforces:
///   - Floating-point rounding mode is locked to FE_TONEAREST.
///   - Denormal inputs are flushed to zero before any arithmetic.
///   - Fused-multiply-add is disabled where it would diverge from the
///     server's unfused evaluation.
///   - Component payloads are canonicalised (NaN elimination, denormal
///     flush) before being written to the WorldState.
///
/// What this module does NOT do:
///   - Replace the fixed-point math layer (DeterministicMath).  This is
///     an enforcement layer on top of whatever math backend is active.
///   - Verify determinism at runtime.  That is the job of the periodic
///     world hash check in the replication state machine.

#pragma once

#include <cstddef>
#include <cstdint>

namespace SagaEngine::Client::Replication {

// ─── Determinism contract ──────────────────────────────────────────────────

/// Enforce floating-point determinism for the calling thread.
///
/// Must be called once at startup (before the first tick) and
/// re-applied after any external library that might mutate the FPU
/// control word returns control.
void EnforceDeterministicFPMode() noexcept;

/// Canonicalise a float32 value for replication apply.
///
/// - NaN inputs are replaced with zero.
/// - Denormal (subnormal) inputs are flushed to zero.
/// - Infinity values are clamped to the largest finite float.
///
/// This function is the gate between every deserialised component
/// field and the WorldState write.  It is intentionally cheap: a
/// bit test and a conditional move.
[[nodiscard]] float CanonicaliseFloat32(float value) noexcept;

/// Canonicalise a 3-vector of float32 values in place.
void CanonicaliseVec3(float* x, float* y, float* z) noexcept;

/// Canonicalise a quaternion (4 floats) in place.
void CanonicaliseQuat(float* x, float* y, float* z, float* w) noexcept;

/// Canonicalise an entire component blob.
///
/// Scans the raw bytes for float32 fields and applies CanonicaliseFloat32
/// to each.  The caller must provide the component's type ID so the
/// function knows which fields are floats (via the ComponentRegistry's
/// reflection metadata — currently a simple "all bytes are floats"
/// heuristic; a production system would have per-field type info).
///
/// @param data       Raw component bytes (mutable).
/// @param size       Byte count.
void CanonicaliseComponentBlob(void* data, std::size_t size) noexcept;

/// Verify that two component blobs are bit-identical.
///
/// Used by the world hash check to confirm that a client's local
/// component matches the server's authoritative bytes.
[[nodiscard]] bool ComponentsMatch(const void* a, const void* b, std::size_t size) noexcept;

} // namespace SagaEngine::Client::Replication
