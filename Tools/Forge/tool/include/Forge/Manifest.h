/// @file Manifest.h
/// @brief Minimal forge.toml loader / mutator / serializer.

#pragma once

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Forge
{

// ─── Manifest ─────────────────────────────────────────────────────────────────

/// In-memory representation of a project's `forge.toml`.
///
/// Schema (every section optional; every value is a string):
///
///   [project]
///   name = "MyGame"
///
///   [toolchain]                  # consumed by reproducible-build mode
///   cmake    = "3.28"
///   conan    = "2.3"
///   compiler = "clang-17"
///
///   [build]
///   backend = "cmake"            # currently only "cmake" is implemented
///   preset  = "release"
///   source  = "."
///   build   = "build"
///
///   [deps]                       # ordered name → version map
///   sfml = "2.6"
///   boost = "1.83"
///
/// The parser tolerates `# line comments`, single- or double-quoted
/// values, and bare tokens. It is intentionally not a general TOML
/// parser — anything beyond the schema above is silently skipped.
class Manifest
{
public:
    using DepEntry = std::pair<std::string, std::string>;

    /// Default empty manifest (no project name, no deps).
    [[nodiscard]] static Manifest Default() noexcept { return Manifest{}; }

    /// Load from disk. Returns nullopt on I/O error; `outError` (when non-null)
    /// receives the reason. Parser errors are recovered from line-by-line:
    /// malformed lines are dropped, never aborting the load.
    [[nodiscard]] static std::optional<Manifest> LoadFromFile(const std::string& path,
                                                              std::string*       outError);

    /// Serialise the manifest back to disk in the canonical section order.
    [[nodiscard]] bool SaveToFile(const std::string& path, std::string* outError) const;

    /// Read a key from a section. Empty string when missing.
    [[nodiscard]] std::string Get(const std::string& section,
                                  const std::string& key,
                                  const std::string& fallback = "") const;

    /// Set a single scalar key. Creates the section if needed.
    void Set(const std::string& section, const std::string& key, std::string value);

    /// Ordered view of `[deps]`.
    [[nodiscard]] const std::vector<DepEntry>& Deps() const noexcept { return mDeps; }

    /// Append (or update) a dep entry. Existing names are replaced in place
    /// to keep the manifest stable line-wise.
    void AddDep(const std::string& name, std::string version);

private:
    using ScalarSection = std::map<std::string, std::string>;
    std::map<std::string, ScalarSection> mScalars;
    std::vector<DepEntry>                mDeps;
};

} // namespace Forge
