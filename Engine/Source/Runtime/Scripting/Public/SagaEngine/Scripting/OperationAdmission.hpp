// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "SagaShared/Api/SagaOperationDescriptor.hpp"

#include <map>
#include <string>
#include <string_view>
#include <unordered_set>

namespace SagaEngine::Scripting
{

enum class OperationAdmissionFailure
{
    None,
    UnknownOperation,
    UnavailableOperation,
    MissingCapability,
    PackageGenerationMismatch,
};

[[nodiscard]] std::string_view ToString(OperationAdmissionFailure failure) noexcept;

struct OperationAdmissionDecision
{
    bool allowed = false;
    OperationAdmissionFailure failure = OperationAdmissionFailure::UnknownOperation;
    std::string_view missingCapability;
};

class SagaOperationCatalog
{
public:
    [[nodiscard]] bool Register(SagaShared::Api::SagaOperationDescriptor descriptor);
    [[nodiscard]] const SagaShared::Api::SagaOperationDescriptor* Find(
        std::string_view operationId) const noexcept;
    [[nodiscard]] std::size_t Size() const noexcept { return operations_.size(); }

private:
    std::map<std::string, SagaShared::Api::SagaOperationDescriptor, std::less<>> operations_;
};

struct OperationAdmissionContext
{
    const SagaOperationCatalog& catalog;
    const std::unordered_set<std::string>& effectiveCapabilities;
    std::string_view packageGenerationId;
    std::string_view packageGenerationHash;
    std::string_view runtimeBinding;
};

class OperationAdmissionEvaluator
{
public:
    [[nodiscard]] static OperationAdmissionDecision Evaluate(
        const OperationAdmissionContext& context,
        const SagaShared::Api::SagaOperationId& operationId) noexcept;
};

} // namespace SagaEngine::Scripting
