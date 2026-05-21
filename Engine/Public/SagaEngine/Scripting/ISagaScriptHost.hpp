/// @file ISagaScriptHost.hpp
/// @brief Compatibility include for low-level SagaScript host contracts.

#pragma once

#include "SagaEngine/Scripting/LowLevel/ISagaScriptHost.hpp"

namespace SagaEngine::Scripting::ScriptHostDiagnostics
{

inline constexpr const char* HostFxrMissing =
    "Script.Host.HostFxrMissing";
inline constexpr const char* HostFxrInitializationFailed =
    "Script.Host.HostFxrInitializationFailed";
inline constexpr const char* RuntimeDelegateResolutionFailed =
    "Script.Host.RuntimeDelegateResolutionFailed";
inline constexpr const char* ManagedBridgeLoadFailed =
    "Script.Host.ManagedBridgeLoadFailed";
inline constexpr const char* AssemblyLoadFailed =
    "Script.Host.AssemblyLoadFailed";
inline constexpr const char* ClassInvalid =
    "Script.Host.ClassInvalid";
inline constexpr const char* LifecycleMethodMissing =
    "Script.Host.LifecycleMethodMissing";
inline constexpr const char* ManagedException =
    "Script.Host.ManagedException";

} // namespace SagaEngine::Scripting::ScriptHostDiagnostics
