// SPDX-License-Identifier: MPL-2.0
/// @file BehaviorSourceIdentity.hpp
/// @brief Backend source identity distinct from logical module identity.

#pragma once

#include "SagaShared/Behavior/BehaviorLanguageId.hpp"

#include <string>

namespace SagaShared::Behavior
{

enum class BehaviorSourceIdentityKind
{
    CanonicalModuleMetadata,
    LegacyCSharpTypeIdentity,
};

struct BehaviorSourceIdentity
{
    BehaviorLanguageId language;
    std::string sourceSymbolId;
    std::string sourcePath;
    BehaviorSourceIdentityKind kind = BehaviorSourceIdentityKind::CanonicalModuleMetadata;
};

} // namespace SagaShared::Behavior
