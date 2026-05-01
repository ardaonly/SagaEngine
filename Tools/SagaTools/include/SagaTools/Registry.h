/// @file Registry.h
/// @brief Two-section name→path registry loaded from a tiny JSON file.

#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace SagaTools
{

// ─── Registry ─────────────────────────────────────────────────────────────────

/// In-memory representation of the dispatcher's tool registry.
///
/// On-disk schema (intentionally tiny):
///
///   {
///     "schema_version": "1.0",
///     "tools": {
///       "forge": "${FORGE_BIN}",
///       "prism": "${PRISM_BIN}"
///     },
///     "installers": {
///       "forge": "Tools/Forge/tool/build.py",
///       "prism": "Tools/Prism/build.py"
///     }
///   }
///
/// Each section is a flat name→string map. Both sections are optional;
/// a tool may have only a runtime entry, only an installer entry, or both.
/// The dispatcher loads this file at startup and never mutates it.
class Registry
{
public:
    using Entry = std::pair<std::string /*name*/, std::string /*rawValue*/>;

    /// Documented search order:
    ///   1) explicit `--registry <path>`
    ///   2) `$SAGATOOLS_REGISTRY`
    ///   3) `<exe-dir>/config/tools.registry.json`
    ///   4) `./tools.registry.json`
    [[nodiscard]] static std::string LocateFile(const std::string& explicitPath,
                                                const std::string& executableDir) noexcept;

    /// Parse the given registry file. Returns nullopt on I/O / parse failure;
    /// `outError` (when non-null) receives a human-readable reason.
    [[nodiscard]] static std::optional<Registry> LoadFromFile(const std::string& filePath,
                                                              std::string*       outError);

    /// Tool entries: logical name → raw executable string.
    [[nodiscard]] const std::vector<Entry>& Tools() const noexcept { return mTools; }

    /// Installer entries: logical name → bootstrap script path.
    [[nodiscard]] const std::vector<Entry>& Installers() const noexcept { return mInstallers; }

    /// Lookup helpers (case-sensitive).
    [[nodiscard]] const std::string* FindToolRaw     (const std::string& name) const noexcept;
    [[nodiscard]] const std::string* FindInstallerRaw(const std::string& name) const noexcept;

    /// Absolute directory containing the registry file. Used to resolve
    /// relative paths in either section.
    [[nodiscard]] const std::string& BaseDir() const noexcept { return mBaseDir; }

private:
    std::vector<Entry> mTools;
    std::vector<Entry> mInstallers;
    std::string        mBaseDir;
};

} // namespace SagaTools
