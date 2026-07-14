/// @file RuntimeServiceRegistry.cpp
/// @brief Runtime service registry implementation.

#include "SagaRuntime/RuntimeServiceRegistry.hpp"

#include <algorithm>
#include <utility>

namespace SagaRuntime
{
namespace
{

[[nodiscard]] bool HasErrorDiagnostic(
    const std::vector<RuntimeServiceDiagnostic>& diagnostics) noexcept
{
    return std::any_of(
        diagnostics.begin(),
        diagnostics.end(),
        [](const RuntimeServiceDiagnostic& diagnostic) {
            return diagnostic.severity == RuntimeServiceDiagnosticSeverity::Error;
        });
}

[[nodiscard]] RuntimeServiceLifecycleResult MakeResult(
    RuntimeServiceDescriptor descriptor,
    RuntimeServiceState state,
    std::vector<RuntimeServiceDiagnostic> diagnostics = {})
{
    RuntimeServiceLifecycleResult result;
    result.descriptor = std::move(descriptor);
    result.state = state;
    result.diagnostics = std::move(diagnostics);
    return result;
}

[[nodiscard]] RuntimeServiceLifecycleResult MakeFailure(
    RuntimeServiceDescriptor descriptor,
    std::string message)
{
    const std::string serviceId = descriptor.serviceId;
    return MakeResult(
        std::move(descriptor),
        RuntimeServiceState::Failed,
        {RuntimeServiceLifecycle::Diagnostic(
            RuntimeServiceDiagnosticSeverity::Error,
            serviceId,
            std::move(message))});
}

void AppendResult(RuntimeServiceRegistryReport& report,
                  RuntimeServiceLifecycleResult result)
{
    for (const RuntimeServiceDiagnostic& diagnostic : result.diagnostics)
    {
        report.diagnostics.push_back(diagnostic);
    }
    report.results.push_back(std::move(result));
}

} // namespace

RuntimeServiceRegistry::~RuntimeServiceRegistry() = default;

RuntimeServiceRegistry::RuntimeServiceRegistry(
    RuntimeServiceRegistry&&) noexcept = default;

RuntimeServiceRegistry& RuntimeServiceRegistry::operator=(
    RuntimeServiceRegistry&&) noexcept = default;

bool RuntimeServiceRegistryReport::Succeeded() const noexcept
{
    if (HasErrorDiagnostic(diagnostics))
    {
        return false;
    }

    return std::all_of(
        results.begin(),
        results.end(),
        [](const RuntimeServiceLifecycleResult& result) {
            return result.Succeeded();
        });
}

RuntimeServiceLifecycleResult RuntimeServiceRegistry::Register(
    std::unique_ptr<IRuntimeService> service)
{
    if (!service)
    {
        return MakeFailure(RuntimeServiceDescriptor{},
                           "Runtime service registration requires a service instance.");
    }

    const RuntimeServiceDescriptor descriptor = service->Descriptor();
    if (!descriptor.IsValid())
    {
        return MakeFailure(
            descriptor,
            "Runtime service descriptor must provide serviceId and displayName.");
    }

    if (FindEntry(descriptor.serviceId) != nullptr)
    {
        return MakeFailure(
            descriptor,
            "Runtime service registration requires a unique serviceId.");
    }

    Entry entry;
    entry.descriptor = descriptor;
    entry.state = RuntimeServiceState::Registered;
    entry.service = std::move(service);
    m_entries.push_back(std::move(entry));

    return MakeResult(descriptor, RuntimeServiceState::Registered);
}

RuntimeServiceRegistryReport RuntimeServiceRegistry::StartAll()
{
    RuntimeServiceRegistryReport report;
    bool blocked = false;

    for (Entry& entry : m_entries)
    {
        if (blocked || entry.state != RuntimeServiceState::Registered)
        {
            continue;
        }

        RuntimeServiceLifecycleResult result = entry.service->Start();
        if (result.descriptor.serviceId.empty())
        {
            result.descriptor = entry.descriptor;
        }

        if (result.Succeeded() && result.state == RuntimeServiceState::Started)
        {
            entry.state = RuntimeServiceState::Started;
        }
        else
        {
            entry.state = RuntimeServiceState::Failed;
            result.state = RuntimeServiceState::Failed;
            blocked = true;
        }

        AppendResult(report, std::move(result));
    }

    return report;
}

RuntimeServiceRegistryReport RuntimeServiceRegistry::TickStarted()
{
    RuntimeServiceRegistryReport report;

    for (Entry& entry : m_entries)
    {
        if (entry.state != RuntimeServiceState::Started)
        {
            continue;
        }

        RuntimeServiceLifecycleResult result = entry.service->Tick();
        if (result.descriptor.serviceId.empty())
        {
            result.descriptor = entry.descriptor;
        }

        if (!result.Succeeded())
        {
            entry.state = RuntimeServiceState::Failed;
            result.state = RuntimeServiceState::Failed;
        }

        AppendResult(report, std::move(result));
    }

    return report;
}

RuntimeServiceRegistryReport RuntimeServiceRegistry::StopAll()
{
    RuntimeServiceRegistryReport report;

    for (auto iterator = m_entries.rbegin(); iterator != m_entries.rend(); ++iterator)
    {
        Entry& entry = *iterator;
        if (entry.state != RuntimeServiceState::Started)
        {
            continue;
        }

        RuntimeServiceLifecycleResult result = entry.service->Stop();
        if (result.descriptor.serviceId.empty())
        {
            result.descriptor = entry.descriptor;
        }

        if (result.Succeeded() && result.state == RuntimeServiceState::Stopped)
        {
            entry.state = RuntimeServiceState::Stopped;
        }
        else
        {
            entry.state = RuntimeServiceState::Failed;
            result.state = RuntimeServiceState::Failed;
        }

        AppendResult(report, std::move(result));
    }

    return report;
}

std::size_t RuntimeServiceRegistry::ServiceCount() const noexcept
{
    return m_entries.size();
}

std::optional<RuntimeServiceState> RuntimeServiceRegistry::GetState(
    const std::string& serviceId) const
{
    const Entry* entry = FindEntry(serviceId);
    if (!entry)
    {
        return std::nullopt;
    }

    return entry->state;
}

std::vector<RuntimeServiceDescriptor>
RuntimeServiceRegistry::GetDescriptorsInOrder() const
{
    std::vector<RuntimeServiceDescriptor> descriptors;
    descriptors.reserve(m_entries.size());
    for (const Entry& entry : m_entries)
    {
        descriptors.push_back(entry.descriptor);
    }
    return descriptors;
}

RuntimeServiceRegistry::Entry* RuntimeServiceRegistry::FindEntry(
    const std::string& serviceId) noexcept
{
    const auto iterator = std::find_if(
        m_entries.begin(),
        m_entries.end(),
        [&serviceId](const Entry& entry) {
            return entry.descriptor.serviceId == serviceId;
        });

    return iterator != m_entries.end() ? &(*iterator) : nullptr;
}

const RuntimeServiceRegistry::Entry* RuntimeServiceRegistry::FindEntry(
    const std::string& serviceId) const noexcept
{
    const auto iterator = std::find_if(
        m_entries.begin(),
        m_entries.end(),
        [&serviceId](const Entry& entry) {
            return entry.descriptor.serviceId == serviceId;
        });

    return iterator != m_entries.end() ? &(*iterator) : nullptr;
}

} // namespace SagaRuntime
