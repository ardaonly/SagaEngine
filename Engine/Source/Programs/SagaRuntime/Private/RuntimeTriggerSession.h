// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <SagaRuntime/Play/RuntimePlayContracts.hpp>

#include <set>
#include <string>
#include <utility>
#include <vector>

namespace SagaRuntimeApp
{
class RuntimeScriptWorld;

/// Deterministic overlap-set owner for verified scene trigger components.
class RuntimeTriggerSession
{
public:
    RuntimeTriggerSession(
        RuntimeScriptWorld& world,
        SagaRuntime::Play::ScriptEventBindingDispatcher& dispatcher)
        : world_(world), dispatcher_(dispatcher) {}

    [[nodiscard]] std::vector<SagaRuntime::Play::RuntimeDiagnostic> Tick();
    void Reset() noexcept { overlaps_.clear(); dispatcher_.Reset(); }
    [[nodiscard]] std::size_t OverlapCount() const noexcept { return overlaps_.size(); }

private:
    RuntimeScriptWorld& world_;
    SagaRuntime::Play::ScriptEventBindingDispatcher& dispatcher_;
    std::set<std::pair<std::string, std::string>> overlaps_;
};
} // namespace SagaRuntimeApp
