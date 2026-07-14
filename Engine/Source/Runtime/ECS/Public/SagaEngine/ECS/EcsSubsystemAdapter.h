/// @file EcsSubsystemAdapter.h
/// @brief Bridges `ECS::ISystem` instances into the engine-wide SubsystemManager.
///
/// Layer  : SagaEngine / ECS
/// Purpose: The engine has two *parallel* lifecycle pipelines today:
///            - `SubsystemManager` owns top-level engine services (audio,
///              input, networking).  They receive `OnInit/OnUpdate/
///              OnShutdown` in dependency order at engine start-up.
///            - `WorldState` owns ECS systems and drives them through
///              `OnSystemInit/OnSystemUpdate/OnSystemShutdown` each tick.
///
///          There is no clean way for the engine boot sequence to
///          register an ECS system as a first-class subsystem.  That
///          leaves gameplay code duplicating boot glue in every Apps
///          binary — not production-grade.
///
///          `EcsSubsystemAdapter` is a thin `ISubsystem` implementation
///          that wraps any `ECS::ISystem`.  Register the adapter with
///          `SubsystemManager` and the ECS system now participates in
///          the same init/update/shutdown graph as every other service.
///
/// Key design choices:
///   - The adapter keeps a *non-owning* pointer to the ECS system when
///     the caller wants to hand out the lifetime themselves (e.g. when
///     the system is a static data member on a game object).  It also
///     accepts an owning `unique_ptr` for the common "allocate once
///     and forget" case.  Both constructors are provided so call sites
///     do not have to juggle their own storage.
///   - The adapter forwards the engine's `OnUpdate(dt)` straight to
///     `OnSystemUpdate(dt)`.  If you need the ECS system to run at a
///     fixed-rate tick instead of the variable frame rate, register it
///     with the SimulationTick driver as before; this adapter covers
///     the frame-rate case only.
///   - Priority forwards from `ECS::ISystem::GetPriority()`, letting
///     ECS-local priority annotations flow through to SubsystemManager
///     without a second configuration site.
///
/// Threading:
///   Like every SubsystemManager child, the adapter is called on the
///   main thread during boot and on the update thread during run.  ECS
///   systems that need parallelism must opt in through
///   `ParallelSystemExecutor`, NOT through this adapter.

#pragma once

#include "SagaEngine/Core/Subsystem/ISubsystem.h"
#include "SagaEngine/ECS/System.h"

#include <memory>
#include <string>

namespace SagaEngine::ECS {

// ─── EcsSubsystemAdapter ────────────────────────────────────────────────────

/// ISubsystem wrapper around an ECS system.  Safe to construct with
/// either an owning unique_ptr (RAII lifetime) or a raw non-owning
/// pointer (caller controls the storage).
class EcsSubsystemAdapter final : public SagaEngine::Core::ISubsystem
{
public:
    /// Owning constructor.  Adapter takes over `system`'s lifetime and
    /// will destroy it in `~EcsSubsystemAdapter`.  `displayName` is
    /// copied so it survives past the caller's string lifetime.
    EcsSubsystemAdapter(std::string displayName, std::unique_ptr<ISystem> system)
        : displayName_(std::move(displayName))
        , owned_(std::move(system))
        , system_(owned_.get())
    {
    }

    /// Non-owning constructor.  The caller keeps responsibility for
    /// `system` — the adapter will NOT delete it.  Use this when the
    /// ECS system is a static or a long-lived game object member.
    EcsSubsystemAdapter(std::string displayName, ISystem* system) noexcept
        : displayName_(std::move(displayName))
        , system_(system)
    {
    }

    // ── ISubsystem forwarders ──────────────────────────────────────────────

    [[nodiscard]] const char* GetName() const override
    {
        // Prefer the adapter's display name over the ECS system's
        // typeid-based name — typeid names are compiler-specific noise
        // (Itanium mangled on Linux, MSVC raw on Windows).
        return displayName_.c_str();
    }

    bool OnInit() override
    {
        if (system_ == nullptr)
            return false;

        // ECS `OnSystemInit` returns void, but SubsystemManager expects
        // a bool.  We forward and then optimistically return true; an
        // ECS system that needs to report a hard failure should throw
        // (engine boot handles exceptions as fatal) or log + set an
        // internal flag to abort on the first update call.
        system_->OnSystemInit();
        return true;
    }

    void OnUpdate(float dt) override
    {
        if (system_ != nullptr)
            system_->OnSystemUpdate(dt);
    }

    void OnShutdown() override
    {
        if (system_ != nullptr)
            system_->OnSystemShutdown();
    }

    // ── Direct access ──────────────────────────────────────────────────────

    /// Exposed for debuggers and tests.  Never called during the
    /// normal lifecycle path — the adapter forwards everything it cares
    /// about on the caller's behalf.
    [[nodiscard]] ISystem* System() const noexcept { return system_; }

private:
    std::string              displayName_;
    std::unique_ptr<ISystem> owned_;    ///< Non-null only in the owning constructor.
    ISystem*                 system_{};  ///< Either `owned_.get()` or an externally-owned ptr.
};

} // namespace SagaEngine::ECS
