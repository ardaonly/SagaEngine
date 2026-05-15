/// @file ArtifactReader.h
/// @brief Stable consumer-facing SDE artifact manifest reader.

#pragma once

#include "SDE/Artifacts/ArtifactManifest.h"
#include "SDE/Validation/Diagnostic.h"

#include <filesystem>
#include <optional>
#include <vector>

namespace SDE
{

class ArtifactReader
{
public:
    [[nodiscard]] std::optional<ArtifactManifest> ReadManifest(
        const std::filesystem::path& path,
        std::vector<Diagnostic>& diagnostics) const;
};

} // namespace SDE
