// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <algorithm>
#include <cctype>
#include <string>

namespace SagaShared::Api
{

/// Opaque stable semantic operation identity. It is never derived from a source symbol or binding.
struct SagaOperationId
{
    std::string value;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return !value.empty() && std::none_of(value.begin(), value.end(),
            [](const unsigned char ch) { return std::isspace(ch) != 0; });
    }

    bool operator==(const SagaOperationId&) const = default;
};

} // namespace SagaShared::Api
