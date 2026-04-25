/// @file IEditorExtension.h
/// @brief Base interface for every editor extension (package-provided panel, tool, or command set).

#pragma once

#include <string>

namespace SagaEditor
{

class IExtensionContext;

// ─── Extension Interface ──────────────────────────────────────────────────────

/// An extension bundles panels, commands, themes, and inspectors contributed
/// by a package. ExtensionHost calls these methods at the appropriate points
/// in the editor lifecycle so extensions never need to hook into low-level
/// startup code.
class IEditorExtension
{
public:
    virtual ~IEditorExtension() = default;

    // ─── Identity ─────────────────────────────────────────────────────────────

    /// Stable reverse-DNS extension id (e.g. "com.bytegeist.terraintools").
    [[nodiscard]] virtual std::string GetExtensionId()   const = 0;

    /// Human-readable display name shown in the extension manager.
    [[nodiscard]] virtual std::string GetDisplayName()   const = 0;

    /// Semantic version string (e.g. "1.0.0").
    [[nodiscard]] virtual std::string GetVersion()       const = 0;

    // ─── Lifecycle ────────────────────────────────────────────────────────────

    /// Called once when the extension is loaded. Register panels, commands, and
    /// themes through the provided context.
    virtual void OnLoad(IExtensionContext& ctx)   = 0;

    /// Called once when the extension is unloaded or the editor shuts down.
    /// Unregister everything registered in OnLoad.
    virtual void OnUnload(IExtensionContext& ctx) = 0;
};

} // namespace SagaEditor
