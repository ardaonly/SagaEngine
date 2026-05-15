/// @file IEditorEngineBridge.h
/// @brief Editor-owned boundary for talking to engine/runtime services.

#pragma once

#include <cstdint>
#include <string>

namespace SagaEditor
{

// ─── Bridge State ────────────────────────────────────────────────────────────

enum class EditorEngineBridgeState : std::uint8_t
{
    Offline,
    Starting,
    Ready,
    Failed,
};

[[nodiscard]] const char* ToString(EditorEngineBridgeState state) noexcept;

struct EditorEngineBridgeSnapshot
{
    EditorEngineBridgeState state = EditorEngineBridgeState::Offline;
    std::string             displayName;
    std::string             runtimeRole;
    std::string             engineVersion;
    std::string             gitCommit;
    std::string             message;
};

// ─── Engine Bridge Interface ─────────────────────────────────────────────────

/// The editor talks to engine/runtime services through this interface only.
/// Customisation, profiles, and shell composition may observe this state but
/// must not mutate engine interaction semantics through it.
class IEditorEngineBridge
{
public:
    virtual ~IEditorEngineBridge() = default;

    [[nodiscard]] virtual bool Init() = 0;
    virtual void Shutdown() = 0;

    [[nodiscard]] virtual EditorEngineBridgeSnapshot Snapshot() const = 0;
    [[nodiscard]] virtual std::string StableIdentity() const = 0;
};

} // namespace SagaEditor
