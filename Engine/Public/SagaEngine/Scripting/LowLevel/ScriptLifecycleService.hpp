/// @file ScriptLifecycleService.hpp
/// @brief Low-level SagaScript lifecycle orchestration contract.

#pragma once

#include "SagaEngine/Scripting/LowLevel/ISagaScriptHost.hpp"
#include "SagaEngine/Scripting/LowLevel/ScriptPackageValidator.hpp"

namespace SagaEngine::Scripting
{

class ScriptLifecycleService
{
public:
    explicit ScriptLifecycleService(
        ISagaScriptHost& host,
        ScriptDiagnosticSink diagnosticSink = {},
        ScriptPackageValidationOptions validationOptions = {});

    [[nodiscard]] ScriptPackageLoadResult LoadPackage(
        const ScriptPackageLoadRequest& request) const;

    [[nodiscard]] ScriptInstanceCreateResult CreateInstance(
        ScriptPackageHandle package,
        const ScriptClassId& classId) const;

    [[nodiscard]] ScriptInstanceCreateResult CreateInstance(
        const ScriptInstanceCreateRequest& request) const;

    [[nodiscard]] ScriptHostOperationResult StartInstance(
        ScriptInstanceHandle instance) const;

    [[nodiscard]] ScriptHostOperationResult UpdateInstance(
        ScriptInstanceHandle instance,
        double deltaTimeSeconds) const;

    [[nodiscard]] ScriptHostOperationResult DestroyInstance(
        ScriptInstanceHandle instance) const;

private:
    [[nodiscard]] ScriptHostOperationResult InvokeLifecycle(
        ScriptInstanceHandle instance,
        ScriptLifecycleMethod method,
        double deltaTimeSeconds) const;

    void EmitDiagnostics(const ScriptHostOperationResult& result) const;

    ISagaScriptHost& host_;
    ScriptDiagnosticSink diagnosticSink_;
    ScriptPackageValidationOptions validationOptions_;
};

} // namespace SagaEngine::Scripting
