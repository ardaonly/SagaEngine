/// @file StressTier.cpp
/// @brief Implements bounded SagaStressArena stress tier metadata.

#include "SagaStressArena/StressTier.hpp"

namespace SagaStressArena
{

const char* ToString(StressTier tier) noexcept
{
    switch (tier)
    {
        case StressTier::Smoke:
            return "smoke";
        case StressTier::MiniSoak:
            return "mini_soak";
        case StressTier::Heavy:
            return "heavy";
    }
    return "unknown";
}

std::optional<StressTier> ParseStressTier(std::string_view value) noexcept
{
    if (value == "smoke")
    {
        return StressTier::Smoke;
    }
    if (value == "mini_soak")
    {
        return StressTier::MiniSoak;
    }
    if (value == "heavy")
    {
        return StressTier::Heavy;
    }
    return std::nullopt;
}

StressTierDefinition DefinitionForTier(StressTier tier) noexcept
{
    switch (tier)
    {
        case StressTier::Smoke:
            return StressTierDefinition{
                StressTier::Smoke,
                5,
                50,
                10,
                10,
                100,
                10,
                false};
        case StressTier::MiniSoak:
            return StressTierDefinition{
                StressTier::MiniSoak,
                25,
                1000,
                60,
                50,
                2000,
                60,
                false};
        case StressTier::Heavy:
            return StressTierDefinition{
                StressTier::Heavy,
                100,
                5000,
                300,
                250,
                10000,
                300,
                true};
    }
    return DefinitionForTier(StressTier::Smoke);
}

} // namespace SagaStressArena
