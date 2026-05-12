/// @file ModelDefinition.cpp
/// @brief ModelDefinition field lookup implementations.

#include "SDE/Model/ModelDefinition.h"
#include "SDE/Validation/Rule.h"

#include <algorithm>

namespace SDE
{

const FieldDefinition* ModelDefinition::FindField(const std::string& fieldId) const noexcept
{
    auto it = std::find_if(fields.begin(), fields.end(),
                           [&fieldId](const FieldDefinition& f) { return f.id == fieldId; });
    return (it != fields.end()) ? &*it : nullptr;
}

bool ModelDefinition::HasField(const std::string& fieldId) const noexcept
{
    return FindField(fieldId) != nullptr;
}

} // namespace SDE
