/// @file CSharpScriptHost.hpp
/// @brief C# SagaScript host implementation over hostfxr.

#pragma once

#include "SagaEngine/Scripting/Namespace.hpp"
#include "SagaEngine/Scripting/ISagaScriptHost.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace SagaEngine::Scripting
{

/// Configuration for the production-oriented C# script host bridge.
struct CSharpScriptHostOptions
{
    std::filesystem::path runtimeBridgeAssembly; ///< SagaScript.RuntimeBridge.dll path.
    std::filesystem::path dotnetRoot;            ///< Optional dotnet root override.
    ISagaScriptWorld* world = nullptr;           ///< Optional engine world facade.
    ScriptLogSink logSink;                       ///< Optional managed script log sink.
};

/// Explicit native request for one public instance int Method(int, int) call.
struct CSharpInt32BinaryMethodInvocation
{
    ScriptInstanceHandle instance;
    std::string methodName;
    std::int32_t left = 0;
    std::int32_t right = 0;
};

/// Result for a CSharpInt32BinaryMethodInvocation.
struct CSharpInt32BinaryMethodInvocationResult : ScriptHostOperationResult
{
    std::int32_t value = 0;
};

/// Native hostfxr-backed C# implementation of ISagaScriptHost.
class CSharpScriptHost final : public ISagaScriptHost
{
public:
    explicit CSharpScriptHost(CSharpScriptHostOptions options = {});
    ~CSharpScriptHost() override;

    CSharpScriptHost(const CSharpScriptHost&) = delete;
    CSharpScriptHost& operator=(const CSharpScriptHost&) = delete;

    [[nodiscard]] ScriptPackageLoadResult LoadPackage(
        const ScriptPackageLoadRequest& request) override;

    [[nodiscard]] ScriptInstanceCreateResult CreateInstance(
        ScriptPackageHandle package,
        const ScriptClassId& classId) override;

    [[nodiscard]] ScriptInstanceCreateResult CreateInstance(
        const ScriptInstanceCreateRequest& request) override;

    [[nodiscard]] ScriptHostOperationResult InvokeLifecycle(
        const ScriptLifecycleInvocation& invocation) override;

    [[nodiscard]] ScriptHostOperationResult InvokeUiNamedAction(
        const ScriptUiNamedActionInvocation& invocation) override;

    [[nodiscard]] CSharpInt32BinaryMethodInvocationResult InvokeInt32BinaryMethod(
        const CSharpInt32BinaryMethodInvocation& invocation);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace SagaEngine::Scripting
