/// @file GeneratedCodeOriginDescriptor.hpp
/// @brief Shared generated script source origin contract.

#pragma once

#include <map>
#include <string>

namespace SagaShared::Scripting
{

/// Describes where generated C# came from without owning generation behavior.
struct GeneratedCodeOriginDescriptor
{
    std::string originId;             ///< Stable generated-code origin identifier.
    std::string sourceResourceId;     ///< Graph, block, or tool resource that produced the code.
    std::string sourceHash;           ///< Content hash of the source resource used for generation.
    std::string generatedPath;        ///< Project or package relative generated source path.
    std::string generatorId;          ///< Tool or step id that produced the generated source.
    std::string generatorVersion;     ///< Generator version token.
    bool editableDetachedCopy = false; ///< True when generated code was explicitly detached for editing.
    std::map<std::string, std::string> metadata; ///< Serialization-safe extension metadata.
};

} // namespace SagaShared::Scripting
