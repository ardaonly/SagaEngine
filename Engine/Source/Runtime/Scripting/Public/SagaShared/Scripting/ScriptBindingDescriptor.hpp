/// @file ScriptBindingDescriptor.hpp
/// @brief Shared SagaScript block binding descriptor contract.

#pragma once

#include "SagaShared/Diagnostics/DiagnosticLocation.hpp"
#include "SagaShared/Scripting/GeneratedCodeOriginDescriptor.hpp"
#include "SagaShared/Scripting/ScriptArtifactRef.hpp"
#include "SagaShared/Scripting/ScriptAuthorityDescriptor.hpp"
#include "SagaShared/Scripting/ScriptCapabilityDescriptor.hpp"
#include "SagaShared/Scripting/ScriptId.hpp"

#include <map>
#include <string>
#include <vector>

namespace SagaShared::Scripting
{

/// Parameter exposed by a script binding.
struct ScriptParameterDescriptor
{
    std::string name;              ///< Source parameter name.
    std::string typeName;          ///< Stable SagaScript-facing type name.
    std::string defaultValue;      ///< Optional serialized default value.
    bool required = true;          ///< True when callers must provide the parameter.
};

/// Return value exposed by a script binding.
struct ScriptReturnDescriptor
{
    std::string typeName;          ///< Stable SagaScript-facing type name.
    std::string semanticName;      ///< Optional semantic label for graph or diagnostics display.
};

/// Data-only descriptor for one C# method or generated entry point exposed to SagaScript.
struct ScriptBindingDescriptor
{
    std::string bindingId;         ///< Stable binding identifier.
    ScriptId scriptId;             ///< Source script that owns the binding.
    std::string language = "csharp"; ///< Source language token.
    std::string className;         ///< Fully qualified source class name.
    std::string methodName;        ///< Source method or entry point name.
    std::string blockCategory;     ///< Optional block palette category.
    std::string blockName;         ///< Optional block display name.
    std::vector<ScriptParameterDescriptor> parameters; ///< Parameters exposed to callers.
    std::vector<ScriptReturnDescriptor> returns;       ///< Return values exposed to callers.
    ScriptAuthorityDescriptor authority;               ///< Authority and side-effect requirements.
    ScriptCapabilityDescriptor capabilities;           ///< Requested and granted capabilities.
    ScriptArtifactRef artifact;                        ///< Optional produced artifact reference.
    GeneratedCodeOriginDescriptor generatedOrigin;     ///< Optional generated source origin.
    Diagnostics::DiagnosticLocation sourceLocation;    ///< Source location for diagnostics and navigation.
    std::map<std::string, std::string> metadata;       ///< Serialization-safe extension metadata.
};

} // namespace SagaShared::Scripting
