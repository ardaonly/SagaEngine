// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <SagaEngine/Scripting/LowLevel/ISagaScriptHost.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <variant>

namespace SagaRuntimeApp
{

struct RuntimeBoolStateSnapshot
{
    bool value = false;
    std::uint64_t revision = 0;
    std::uint64_t activeInstanceToken = 0;
};

using RuntimeStateValue = std::variant<bool, std::int32_t, double, std::string>;
struct RuntimeTypedStateSnapshot
{
    RuntimeStateValue value;
    std::uint64_t revision = 0;
    std::uint64_t activeInstanceToken = 0;
};

/// Runtime-owned, identity-scoped state published by verified script instances.
class RuntimeStateStore final : public SagaEngine::Scripting::ISagaScriptStatePort
{
public:
    using Context = SagaEngine::Scripting::ScriptStateContext;
    using BoolWritePolicy = std::map<std::string, std::set<std::string>>;
    using TypedWritePolicy =
        std::map<std::string, std::map<std::string, std::string>>;

    explicit RuntimeStateStore(BoolWritePolicy boolWrites = {})
        : boolWrites_(std::move(boolWrites))
    {}
    explicit RuntimeStateStore(TypedWritePolicy typedWrites)
        : typedWrites_(std::move(typedWrites))
    {}

    [[nodiscard]] std::optional<RuntimeBoolStateSnapshot> Snapshot(
        SagaEngine::Scripting::ScriptEntityHandle entity,
        const std::string& behaviorId, const std::string& key) const;
    [[nodiscard]] std::optional<RuntimeTypedStateSnapshot> SnapshotValue(
        SagaEngine::Scripting::ScriptEntityHandle entity,
        const std::string& behaviorId, const std::string& key) const;

    [[nodiscard]] SagaEngine::Scripting::ScriptStateBoolResult GetBool(
        const std::string& key) const override;
    [[nodiscard]] SagaEngine::Scripting::ScriptStateInt32Result AddInt32(
        const std::string& key, std::int32_t delta) override;
    [[nodiscard]] SagaEngine::Scripting::ScriptHostOperationResult SetBool(
        const std::string& key, bool value) override;
    [[nodiscard]] SagaEngine::Scripting::ScriptHostOperationResult SetString(
        const std::string& key, const std::string& value) override;

    [[nodiscard]] SagaEngine::Scripting::ScriptStateBoolResult GetBool(
        const Context& context, const std::string& key) const override;
    [[nodiscard]] SagaEngine::Scripting::ScriptStateInt32Result AddInt32(
        const Context& context, const std::string& key, std::int32_t delta) override;
    [[nodiscard]] SagaEngine::Scripting::ScriptHostOperationResult SetBool(
        const Context& context, const std::string& key, bool value) override;
    [[nodiscard]] SagaEngine::Scripting::ScriptHostOperationResult SetString(
        const Context& context, const std::string& key, const std::string& value) override;
    [[nodiscard]] SagaEngine::Scripting::ScriptStateInt32Result GetInt32(
        const Context&, const std::string&) const override;
    [[nodiscard]] SagaEngine::Scripting::ScriptStateDoubleResult GetDouble(
        const Context&, const std::string&) const override;
    [[nodiscard]] SagaEngine::Scripting::ScriptStateStringResult GetString(
        const Context&, const std::string&) const override;
    [[nodiscard]] SagaEngine::Scripting::ScriptHostOperationResult SetInt32(
        const Context&, const std::string&, std::int32_t) override;
    [[nodiscard]] SagaEngine::Scripting::ScriptHostOperationResult SetDouble(
        const Context&, const std::string&, double) override;
    void OnInstanceActivated(const Context& context) override;
    void OnInstanceDeactivated(const Context& context) override;

private:
    struct InstanceIdentity
    {
        std::uint64_t entity = 0;
        std::string behavior;
        bool operator<(const InstanceIdentity& other) const noexcept;
    };
    struct StateIdentity
    {
        InstanceIdentity instance;
        std::string key;
        bool operator<(const StateIdentity& other) const noexcept;
    };
    [[nodiscard]] bool IsActive(const Context& context) const;

    std::map<InstanceIdentity, std::uint64_t> active_;
    std::map<StateIdentity, RuntimeTypedStateSnapshot> values_;
    BoolWritePolicy boolWrites_;
    TypedWritePolicy typedWrites_;
    std::uint64_t nextRevision_ = 1;
};

} // namespace SagaRuntimeApp
