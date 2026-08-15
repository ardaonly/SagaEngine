// SPDX-License-Identifier: MPL-2.0
#include "SagaEngine/Scripting/OperationAdmission.hpp"

#include <algorithm>

namespace SagaEngine::Scripting
{

std::string_view ToString(const OperationAdmissionFailure failure) noexcept
{
    switch (failure)
    {
    case OperationAdmissionFailure::None: return "None";
    case OperationAdmissionFailure::UnknownOperation: return "UnknownOperation";
    case OperationAdmissionFailure::UnavailableOperation: return "UnavailableOperation";
    case OperationAdmissionFailure::MissingCapability: return "MissingCapability";
    case OperationAdmissionFailure::PackageGenerationMismatch:
        return "PackageGenerationMismatch";
    }
    return "UnknownOperation";
}

bool SagaOperationCatalog::Register(SagaShared::Api::SagaOperationDescriptor descriptor)
{
    if (!descriptor.operationId.IsValid() || !descriptor.apiId.IsValid() ||
        descriptor.packageId.empty() || descriptor.moduleId.empty() ||
        descriptor.runtimeBinding.empty() || descriptor.requiredCapabilities.empty() ||
        descriptor.packageGenerationId.empty() || descriptor.packageGenerationHash.empty() ||
        std::any_of(descriptor.requiredCapabilities.begin(),
            descriptor.requiredCapabilities.end(), [](const std::string& value)
            { return value.empty(); }) ||
        std::unordered_set<std::string>(descriptor.requiredCapabilities.begin(),
            descriptor.requiredCapabilities.end()).size() !=
                descriptor.requiredCapabilities.size())
        return false;
    return operations_.emplace(descriptor.operationId.value, std::move(descriptor)).second;
}

const SagaShared::Api::SagaOperationDescriptor* SagaOperationCatalog::Find(
    const std::string_view operationId) const noexcept
{
    const auto found = operations_.find(operationId);
    return found == operations_.end() ? nullptr : &found->second;
}

OperationAdmissionDecision OperationAdmissionEvaluator::Evaluate(
    const OperationAdmissionContext& context,
    const SagaShared::Api::SagaOperationId& operationId) noexcept
{
    const auto* descriptor = operationId.IsValid()
        ? context.catalog.Find(operationId.value) : nullptr;
    if (descriptor == nullptr)
        return {false, OperationAdmissionFailure::UnknownOperation, {}};
    if (descriptor->packageGenerationId != context.packageGenerationId ||
        descriptor->packageGenerationHash != context.packageGenerationHash)
        return {false, OperationAdmissionFailure::PackageGenerationMismatch, {}};
    if (!descriptor->csharpAvailable || descriptor->runtimeBinding != context.runtimeBinding)
        return {false, OperationAdmissionFailure::UnavailableOperation, {}};
    for (const auto& capability : descriptor->requiredCapabilities)
        if (!context.effectiveCapabilities.contains(capability))
            return {false, OperationAdmissionFailure::MissingCapability, capability};
    return {true, OperationAdmissionFailure::None, {}};
}

} // namespace SagaEngine::Scripting
