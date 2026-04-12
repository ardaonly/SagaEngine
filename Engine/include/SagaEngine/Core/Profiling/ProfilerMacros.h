/// @file ProfilerMacros.h
/// @brief Zero-overhead scoped profiling macros for hot-path instrumentation.
///
/// Purpose: Wraps Profiler::BeginSample / EndSample into RAII guards so
///          every exit path (including early returns and exceptions) closes
///          the sample automatically.  The macros compile to nothing when
///          SAGA_PROFILING_ENABLED is not defined, letting release builds
///          strip every instruction without touching call sites.
///
/// Usage:
///     void Tick(float dt) {
///         SAGA_PROFILE_FUNCTION();            // uses __FUNCTION__
///         {
///             SAGA_PROFILE_BLOCK("Physics");  // named sub-scope
///             StepPhysics(dt);
///         }
///     }

#pragma once

#include "SagaEngine/Core/Profiling/Profiler.h"

// ─── Build-time gate ─────────────────────────────────────────────────────────

#ifndef SAGA_PROFILING_ENABLED
    #if !defined(NDEBUG) || defined(SAGA_FORCE_PROFILING)
        #define SAGA_PROFILING_ENABLED 1
    #else
        #define SAGA_PROFILING_ENABLED 0
    #endif
#endif

namespace SagaEngine::Core::Detail {

// ─── ScopedProfileGuard ──────────────────────────────────────────────────────

/// RAII guard that calls EndSample on destruction.  Not intended for direct
/// use — always go through the SAGA_PROFILE_* macros so the build-time gate
/// can strip the guard entirely in shipping builds.
class ScopedProfileGuard final
{
public:
    explicit ScopedProfileGuard(const char* name) noexcept
        : name_(name)
    {
        Profiler::Instance().BeginSample(name_);
    }

    ~ScopedProfileGuard() noexcept
    {
        Profiler::Instance().EndSample(name_);
    }

    ScopedProfileGuard(const ScopedProfileGuard&)            = delete;
    ScopedProfileGuard& operator=(const ScopedProfileGuard&) = delete;

private:
    const char* name_;
};

} // namespace SagaEngine::Core::Detail

// ─── Token-paste helpers ─────────────────────────────────────────────────────

#define SAGA_PROFILE_CONCAT_INNER(a, b) a##b
#define SAGA_PROFILE_CONCAT(a, b) SAGA_PROFILE_CONCAT_INNER(a, b)
#define SAGA_PROFILE_UNIQUE(prefix) SAGA_PROFILE_CONCAT(prefix, __LINE__)

// ─── Public macros ───────────────────────────────────────────────────────────

#if SAGA_PROFILING_ENABLED

/// Profile the enclosing scope under a user-supplied name string.
#define SAGA_PROFILE_BLOCK(name) \
    ::SagaEngine::Core::Detail::ScopedProfileGuard \
        SAGA_PROFILE_UNIQUE(_sagaProfileGuard_)(name)

/// Profile the enclosing function — uses the compiler's function name.
#define SAGA_PROFILE_FUNCTION() \
    SAGA_PROFILE_BLOCK(__FUNCTION__)

/// One-shot sample begin — caller is responsible for the matching EndSample.
/// Prefer SAGA_PROFILE_BLOCK unless the scope boundary is non-trivial.
#define SAGA_PROFILE_BEGIN(name) \
    ::SagaEngine::Core::Profiler::Instance().BeginSample(name)

/// Matching end for SAGA_PROFILE_BEGIN.
#define SAGA_PROFILE_END(name) \
    ::SagaEngine::Core::Profiler::Instance().EndSample(name)

/// Record a frame boundary for per-frame timing diagnostics.
#define SAGA_PROFILE_FRAME_BEGIN() \
    ::SagaEngine::Core::Profiler::Instance().BeginFrame()

#define SAGA_PROFILE_FRAME_END() \
    ::SagaEngine::Core::Profiler::Instance().EndFrame()

#else // profiling disabled

#define SAGA_PROFILE_BLOCK(name)     ((void)0)
#define SAGA_PROFILE_FUNCTION()      ((void)0)
#define SAGA_PROFILE_BEGIN(name)     ((void)0)
#define SAGA_PROFILE_END(name)       ((void)0)
#define SAGA_PROFILE_FRAME_BEGIN()   ((void)0)
#define SAGA_PROFILE_FRAME_END()     ((void)0)

#endif // SAGA_PROFILING_ENABLED
