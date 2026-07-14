/// @file ScenarioRegistry.cpp
/// @brief Global self-registration list and bulk-register helper.

#include "SagaSandbox/Core/ScenarioRegistry.h"

namespace SagaSandbox
{

std::vector<ScenarioRegistry::RegistrationFn>& ScenarioRegistry::GetRegistrations() noexcept
{
    // Meyers singleton — constructed before any static that uses it.
    static std::vector<RegistrationFn> s_registrations;
    return s_registrations;
}

void ScenarioRegistry::RegisterAll(ScenarioManager& manager)
{
    for (const auto& fn : GetRegistrations())
    {
        fn(manager);
    }
}

} // namespace SagaSandbox