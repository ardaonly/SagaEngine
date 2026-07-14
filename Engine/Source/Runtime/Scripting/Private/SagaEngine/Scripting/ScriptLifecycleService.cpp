/// @file ScriptLifecycleService.cpp
/// @brief Implements SagaScript lifecycle orchestration over ISagaScriptHost.

#include <SagaEngine/Scripting/ScriptLifecycleService.hpp>

#include "SagaShared/Diagnostics/DiagnosticCategory.hpp"
#include "SagaShared/Diagnostics/DiagnosticCode.hpp"
#include "SagaShared/Diagnostics/DiagnosticSeverity.hpp"
#include "SagaShared/Diagnostics/DiagnosticSource.hpp"

#include <string>
#include <utility>

namespace SagaEngine::Scripting
{

namespace
{

/// Build a deterministic host diagnostic for lifecycle orchestration failures.
[[nodiscard]] ScriptDiagnostic MakeHostDiagnostic(
    std::string code,
    std::string title,
    std::string message)
{
    ScriptDiagnostic diagnostic;
    diagnostic.diagnostic.severity =
        SagaShared::Diagnostics::DiagnosticSeverity::Error;
    diagnostic.diagnostic.category =
        SagaShared::Diagnostics::DiagnosticCategory::Script;
    diagnostic.diagnostic.source =
        SagaShared::Diagnostics::DiagnosticSource::Runtime;
    diagnostic.diagnostic.code.value = std::move(code);
    diagnostic.diagnostic.title = std::move(title);
    diagnostic.diagnostic.message = std::move(message);
    return diagnostic;
}

/// Return a failed operation result containing one diagnostic.
[[nodiscard]] ScriptHostOperationResult FailedOperation(
    ScriptDiagnostic diagnostic)
{
    ScriptHostOperationResult result;
    result.succeeded = false;
    result.diagnostics.push_back(std::move(diagnostic));
    return result;
}

} // namespace

ScriptLifecycleService::ScriptLifecycleService(
    ISagaScriptHost& host,
    ScriptDiagnosticSink diagnosticSink,
    ScriptPackageValidationOptions validationOptions)
    : host_(host)
    , diagnosticSink_(std::move(diagnosticSink))
    , validationOptions_(std::move(validationOptions))
{
}

ScriptPackageLoadResult ScriptLifecycleService::LoadPackage(
    const ScriptPackageLoadRequest& request) const
{
    const auto validation =
        ScriptPackageValidator::ValidateLoadRequest(request, validationOptions_);
    if (!validation.Succeeded())
    {
        ScriptPackageLoadResult result;
        result.succeeded = false;
        result.diagnostics = validation.diagnostics;
        EmitDiagnostics(result);
        return result;
    }

    auto result = host_.LoadPackage(request);
    EmitDiagnostics(result);
    return result;
}

ScriptInstanceCreateResult ScriptLifecycleService::CreateInstance(
    const ScriptPackageHandle package,
    const ScriptClassId& classId) const
{
    ScriptInstanceCreateRequest request;
    request.package = package;
    request.classId = classId;
    return CreateInstance(request);
}

ScriptInstanceCreateResult ScriptLifecycleService::CreateInstance(
    const ScriptInstanceCreateRequest& request) const
{
    const auto package = request.package;
    const auto& classId = request.classId;
    if (!package.IsValid())
    {
        ScriptInstanceCreateResult result;
        result.succeeded = false;
        result.diagnostics.push_back(MakeHostDiagnostic(
            ScriptHostDiagnostics::InvalidPackageHandle,
            "Invalid script package handle",
            "Cannot create a script instance from an invalid package handle."));
        EmitDiagnostics(result);
        return result;
    }

    if (!classId.IsValid())
    {
        ScriptInstanceCreateResult result;
        result.succeeded = false;
        result.diagnostics.push_back(MakeHostDiagnostic(
            ScriptHostDiagnostics::InvalidClassId,
            "Invalid script class id",
            "Cannot create a script instance without a script class id."));
        EmitDiagnostics(result);
        return result;
    }

    auto result = host_.CreateInstance(request);
    EmitDiagnostics(result);
    return result;
}

ScriptHostOperationResult ScriptLifecycleService::StartInstance(
    const ScriptInstanceHandle instance) const
{
    auto createResult =
        InvokeLifecycle(instance, ScriptLifecycleMethod::OnCreate, 0.0);
    if (!createResult.Succeeded())
    {
        return createResult;
    }

    return InvokeLifecycle(instance, ScriptLifecycleMethod::OnStart, 0.0);
}

ScriptHostOperationResult ScriptLifecycleService::UpdateInstance(
    const ScriptInstanceHandle instance,
    const double deltaTimeSeconds) const
{
    return InvokeLifecycle(
        instance,
        ScriptLifecycleMethod::OnUpdate,
        deltaTimeSeconds);
}

ScriptHostOperationResult ScriptLifecycleService::DestroyInstance(
    const ScriptInstanceHandle instance) const
{
    return InvokeLifecycle(instance, ScriptLifecycleMethod::OnDestroy, 0.0);
}

ScriptHostOperationResult ScriptLifecycleService::InvokeLifecycle(
    const ScriptInstanceHandle instance,
    const ScriptLifecycleMethod method,
    const double deltaTimeSeconds) const
{
    if (!instance.IsValid())
    {
        auto result = FailedOperation(MakeHostDiagnostic(
            ScriptHostDiagnostics::InvalidInstanceHandle,
            "Invalid script instance handle",
            "Cannot invoke script lifecycle on an invalid instance handle."));
        result.diagnostics.front().metadata["lifecycleMethod"] =
            std::string(ToString(method));
        EmitDiagnostics(result);
        return result;
    }

    ScriptLifecycleInvocation invocation;
    invocation.instance = instance;
    invocation.method = method;
    invocation.deltaTimeSeconds = deltaTimeSeconds;

    auto result = host_.InvokeLifecycle(invocation);
    EmitDiagnostics(result);
    return result;
}

void ScriptLifecycleService::EmitDiagnostics(
    const ScriptHostOperationResult& result) const
{
    if (!diagnosticSink_)
    {
        return;
    }

    for (const auto& diagnostic : result.diagnostics)
    {
        diagnosticSink_(diagnostic);
    }
}

} // namespace SagaEngine::Scripting
