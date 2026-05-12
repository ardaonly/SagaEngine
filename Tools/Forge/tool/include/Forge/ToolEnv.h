/// @file ToolEnv.h
/// @brief Executable resolution table — overrides default tool names from the .forge file.
///
/// Adapters query ToolEnv::Active() instead of hardcoding "cmake" / "conan" so that
/// projects can pin exact binary paths without touching forge.toml or PATH.

#pragma once

#include <string>

namespace Forge
{

// ─── ToolEnv ──────────────────────────────────────────────────────────────────

/// Holds the resolved executable names (or full paths) for each backend tool.
///
/// Loaded once at startup from the `.forge` file in the working directory.
/// Keys in that file: CMAKE, CONAN, CTEST, CLANG_FORMAT.
/// Any key absent from the file keeps its default value.
struct ToolEnv
{
    std::string cmake     = "cmake";
    std::string conan     = "conan";
    std::string ctest     = "ctest";
    std::string clangFmt  = "clang-format";

    /// True when all fields still hold their default values.
    [[nodiscard]] bool IsDefault() const noexcept;

    /// Load from a `.forge` key=value file. Missing keys keep defaults.
    /// Returns a default ToolEnv when the file cannot be opened.
    [[nodiscard]] static ToolEnv LoadFromFile(const std::string& path,
                                              std::string*       outError = nullptr);

    /// Replace the process-wide active environment.
    static void          SetActive(ToolEnv env) noexcept;

    /// Retrieve the active environment. Initialised to all-defaults.
    [[nodiscard]] static const ToolEnv& Active() noexcept;
};

} // namespace Forge
