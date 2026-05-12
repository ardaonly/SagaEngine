/// @file ModelLoader.cpp
/// @brief RawValue helper method implementations.

#include "SDE/IO/ModelLoader.h"

#include <variant>

namespace SDE
{

// ─── RawValue helpers ─────────────────────────────────────────────────────────

bool RawValue::IsNull()    const noexcept { return std::holds_alternative<RawNull>(data); }
bool RawValue::IsInteger() const noexcept { return std::holds_alternative<RawInteger>(data); }
bool RawValue::IsBool()    const noexcept { return std::holds_alternative<RawBool>(data); }
bool RawValue::IsText()    const noexcept { return std::holds_alternative<RawText>(data); }
bool RawValue::IsArray()   const noexcept { return std::holds_alternative<RawArray>(data); }
bool RawValue::IsObject()  const noexcept { return std::holds_alternative<RawObject>(data); }

bool RawValue::IsNumber() const noexcept
{
    return std::holds_alternative<RawInteger>(data) ||
           std::holds_alternative<RawNumber>(data);
}

double RawValue::AsDouble() const
{
    if (const auto* i = std::get_if<RawInteger>(&data))
        return static_cast<double>(*i);
    return std::get<RawNumber>(data);
}

int64_t RawValue::AsInteger() const
{
    return std::get<RawInteger>(data);
}

} // namespace SDE
