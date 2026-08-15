// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "SagaShared/Api/SagaApiId.hpp"
#include "SagaShared/Api/SagaOperationId.hpp"
#include "SagaShared/Scripting/ScriptAuthorityDescriptor.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace SagaShared::Api
{

enum class SagaApiLevel : std::uint8_t
{
    High,
    Low,
};

/// Immutable semantic operation derived from one verified authored package surface.
struct SagaOperationDescriptor
{
    SagaOperationId operationId;
    SagaApiId apiId;
    std::string packageId;
    std::string moduleId;
    SagaApiLevel apiLevel = SagaApiLevel::High;
    SagaShared::Scripting::ScriptSideEffect effect =
        SagaShared::Scripting::ScriptSideEffect::Pure;
    std::vector<std::string> requiredCapabilities;
    std::string runtimeBinding;
    std::string csharpBinding;
    bool csharpAvailable = false;
    bool blocksAvailable = false;
    std::string packageGenerationId;
    std::string packageGenerationHash;
};

} // namespace SagaShared::Api
