/// @file ConanAdapter.h
/// @brief Conan backend adapter — lowers BuildModel dependency info into conan invocations.
///
/// All conan process spawning is centralised here. Writes forge.lock after a
/// successful install so that --strict --frozen can verify reproducibility.

#pragma once

#include "BuildModel.h"
#include <string>
#include <vector>

namespace Forge
{

// ─── ConanAdapter ─────────────────────────────────────────────────────────────

class ConanAdapter
{
public:
    ConanAdapter()                               = delete;
    ConanAdapter(const ConanAdapter&)            = delete;
    ConanAdapter& operator=(const ConanAdapter&) = delete;

    // ─── Profile options ──────────────────────────────────────────────────────

    /// Named Conan profiles for the install invocation.
    /// Values are passed verbatim as --profile:host / --profile:build.
    struct ProfileOptions
    {
        std::string host;   ///<  --profile:host=<value>; empty = omit
        std::string build;  ///<  --profile:build=<value>; empty = omit
    };

    /// Install dependencies.
    ///
    ///   A) host profile set → conan install . --profile:host=… [--profile:build=…] --build=missing
    ///   B) model.deps non-empty → conan install . --requires=… --build=missing
    ///   C) conanfile.py present → conan install . --build=missing
    ///
    /// On success, writes forge.lock to the working directory.
    [[nodiscard]] static int Install(const BuildModel&              model,
                                     const ProfileOptions&          profileOpts,
                                     const std::vector<std::string>& extra   = {},
                                     bool                            explain = false);
};

} // namespace Forge
