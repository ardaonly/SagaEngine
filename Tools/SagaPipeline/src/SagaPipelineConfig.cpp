/// @file SagaPipelineConfig.cpp
/// @brief SagaPipeline default path calculation.

#include "SagaPipeline/SagaPipelineConfig.h"

namespace SagaPipeline
{
namespace
{

[[nodiscard]] std::filesystem::path NormalizedRoot(
    const std::filesystem::path& root)
{
    if (root.empty())
    {
        return ".";
    }
    return root.lexically_normal();
}

} // namespace

std::filesystem::path DefaultEditorCompositionWorkspace(
    const std::filesystem::path& projectRoot)
{
    return NormalizedRoot(projectRoot) / "Editor" / "CompositionSources";
}

std::filesystem::path DefaultBuildRoot(const std::filesystem::path& projectRoot)
{
    return NormalizedRoot(projectRoot) / "Build";
}

} // namespace SagaPipeline
