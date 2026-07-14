/// @file RuntimeServiceRegistry.hpp
/// @brief Runtime-owned service lifecycle registry.

#pragma once

#include <SagaRuntime/RuntimeServiceLifecycle.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace SagaRuntime
{

/// Runtime service contract used by the lifecycle registry.
class IRuntimeService
{
public:
    virtual ~IRuntimeService() = default;

    /// Return the stable descriptor used for lifecycle ordering and diagnostics.
    [[nodiscard]] virtual const RuntimeServiceDescriptor& Descriptor() const noexcept = 0;

    /// Start the service.
    [[nodiscard]] virtual RuntimeServiceLifecycleResult Start() = 0;

    /// Tick the service once.
    [[nodiscard]] virtual RuntimeServiceLifecycleResult Tick() = 0;

    /// Stop the service.
    [[nodiscard]] virtual RuntimeServiceLifecycleResult Stop() = 0;
};

/// Aggregated lifecycle report for registry operations.
struct RuntimeServiceRegistryReport
{
    std::vector<RuntimeServiceLifecycleResult> results;
    std::vector<RuntimeServiceDiagnostic> diagnostics;

    /// Return true when every service result succeeded and no error diagnostic was reported.
    [[nodiscard]] bool Succeeded() const noexcept;
};

/// Runtime-owned registry that executes services in deterministic registration order.
class RuntimeServiceRegistry
{
public:
    RuntimeServiceRegistry() = default;
    ~RuntimeServiceRegistry();

    RuntimeServiceRegistry(const RuntimeServiceRegistry&) = delete;
    RuntimeServiceRegistry& operator=(const RuntimeServiceRegistry&) = delete;
    RuntimeServiceRegistry(RuntimeServiceRegistry&&) noexcept;
    RuntimeServiceRegistry& operator=(RuntimeServiceRegistry&&) noexcept;

    /// Register a service in deterministic lifecycle order.
    [[nodiscard]] RuntimeServiceLifecycleResult Register(
        std::unique_ptr<IRuntimeService> service);

    /// Start registered services in registration order.
    [[nodiscard]] RuntimeServiceRegistryReport StartAll();

    /// Tick services that are currently started.
    [[nodiscard]] RuntimeServiceRegistryReport TickStarted();

    /// Stop started services in reverse registration order.
    [[nodiscard]] RuntimeServiceRegistryReport StopAll();

    /// Return the number of registered services.
    [[nodiscard]] std::size_t ServiceCount() const noexcept;

    /// Return the current state for a service id when registered.
    [[nodiscard]] std::optional<RuntimeServiceState> GetState(
        const std::string& serviceId) const;

    /// Return descriptors in deterministic registration order.
    [[nodiscard]] std::vector<RuntimeServiceDescriptor> GetDescriptorsInOrder() const;

private:
    struct Entry
    {
        std::unique_ptr<IRuntimeService> service;
        RuntimeServiceDescriptor descriptor;
        RuntimeServiceState state = RuntimeServiceState::Registered;
    };

    [[nodiscard]] Entry* FindEntry(const std::string& serviceId) noexcept;
    [[nodiscard]] const Entry* FindEntry(const std::string& serviceId) const noexcept;

    std::vector<Entry> m_entries;
};

} // namespace SagaRuntime
