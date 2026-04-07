/// @file ScenarioRegistry.h
/// @brief Self-registration mechanism for sandbox scenarios.
///
/// Layer  : Sandbox / Core
/// Purpose: Scenarios declare themselves via the SAGA_SANDBOX_REGISTER_SCENARIO
///          macro. This avoids any central hardcoded list in ScenarioManager.
///          The host calls ScenarioRegistry::RegisterAll() once at startup.
///
/// Usage (in a scenario .cpp file):
///
///   SAGA_SANDBOX_REGISTER_SCENARIO(InputProbeScenario)
///
/// That single line is sufficient. No changes needed in any other file.

#pragma once

#include "SagaSandbox/Core/ScenarioManager.h"
#include <functional>
#include <vector>

namespace SagaSandbox
{

// ─── ScenarioRegistry ─────────────────────────────────────────────────────────

/// Collects self-registering scenarios and bulk-registers them with a manager.
class ScenarioRegistry
{
public:
    using RegistrationFn = std::function<void(ScenarioManager&)>;

    /// Returns the global list of self-registration callbacks.
    static std::vector<RegistrationFn>& GetRegistrations() noexcept;

    /// Push all collected registrations into a ScenarioManager instance.
    static void RegisterAll(ScenarioManager& manager);

private:
    ScenarioRegistry() = delete;
};

// ─── Self-registration helper ─────────────────────────────────────────────────

/// Internal helper instantiated by the macro below.
struct ScenarioAutoRegistrar
{
    explicit ScenarioAutoRegistrar(ScenarioRegistry::RegistrationFn fn)
    {
        ScenarioRegistry::GetRegistrations().push_back(std::move(fn));
    }
};

} // namespace SagaSandbox

// ─── Registration Macro ───────────────────────────────────────────────────────

/// Place this macro once in the .cpp file of any scenario class.
/// @param ScenarioClass  The concrete class name (must be default-constructible).
///
/// Example:
///   SAGA_SANDBOX_REGISTER_SCENARIO(InputProbeScenario)
///
/// This generates a file-scope static that registers the factory before main().

#define SAGA_SANDBOX_REGISTER_SCENARIO(ScenarioClass)                          \
    static ::SagaSandbox::ScenarioAutoRegistrar                                \
        s_AutoRegister_##ScenarioClass(                                        \
            [](::SagaSandbox::ScenarioManager& mgr)                            \
            {                                                                  \
                mgr.RegisterScenario(                                          \
                    ScenarioClass::kDescriptor.id,                             \
                    []() -> std::unique_ptr<::SagaSandbox::IScenario>          \
                    {                                                          \
                        return std::make_unique<ScenarioClass>();              \
                    }                                                          \
                );                                                             \
            }                                                                  \
        )