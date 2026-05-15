/// @file Version.cpp
/// @brief Version contract helpers.

#include "SDE/Core/Version.h"

#include <sstream>

namespace SDE
{

std::string SemanticVersion::ToString() const
{
    std::ostringstream out;
    out << major << '.' << minor << '.' << patch;
    return out.str();
}

bool IsSameMajorCompatible(const SemanticVersion& expected,
                           const SemanticVersion& actual) noexcept
{
    return expected.major == actual.major;
}

CompilerVersion CurrentCompilerVersion() noexcept
{
    return {0, 1, 0};
}

LanguageVersion CurrentLanguageVersion() noexcept
{
    return {1, 0, 0};
}

ArtifactFormatVersion CurrentArtifactFormatVersion() noexcept
{
    return {1, 0, 0};
}

} // namespace SDE
