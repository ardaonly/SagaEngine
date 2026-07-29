// SPDX-License-Identifier: MPL-2.0
/// @file CSharpScriptHost.cpp
/// @brief Public forwarding for the hostfxr-backed C# SagaScript host.

#include <SagaEngine/Scripting/CSharpScriptHost.hpp>
#include <memory>
#include <utility>

#include "CSharpScriptHostImpl.hpp"

namespace SagaEngine::Scripting
{

CSharpScriptHost::CSharpScriptHost(CSharpScriptHostOptions options)
    : impl_(std::make_unique<Impl>(std::move(options)))
{}

CSharpScriptHost::~CSharpScriptHost() = default;

ScriptPackageLoadResult CSharpScriptHost::LoadPackage(const ScriptPackageLoadRequest& request)
{
    return impl_->LoadPackage(request);
}

ScriptPackageLoadResult CSharpScriptHost::LoadVerifiedGeneration(
    const VerifiedScriptGenerationLoadRequest& request)
{
    return impl_->LoadVerifiedGeneration(request);
}

ScriptInstanceCreateResult CSharpScriptHost::CreateInstance(const ScriptPackageHandle package,
                                                            const ScriptClassId& classId)
{
    return impl_->CreateInstance(package, classId);
}

ScriptInstanceCreateResult CSharpScriptHost::CreateInstance(
    const ScriptInstanceCreateRequest& request)
{
    return impl_->CreateInstance(request);
}

ScriptHostOperationResult CSharpScriptHost::InvokeLifecycle(
    const ScriptLifecycleInvocation& invocation)
{
    return impl_->InvokeLifecycle(invocation);
}

ScriptHostOperationResult CSharpScriptHost::InvokeUiNamedAction(
    const ScriptUiNamedActionInvocation& invocation)
{
    return impl_->InvokeUiNamedAction(invocation);
}

ScriptHostOperationResult CSharpScriptHost::InvokeNamedAction(
    const ScriptNamedActionInvocation& invocation)
{
    return impl_->InvokeNamedAction(invocation);
}

CSharpInt32BinaryMethodInvocationResult CSharpScriptHost::InvokeInt32BinaryMethod(
    const CSharpInt32BinaryMethodInvocation& invocation)
{
    return impl_->InvokeInt32BinaryMethod(invocation);
}

std::size_t CSharpScriptHost::InstanceCount() const noexcept
{
    return impl_->InstanceCount();
}

} // namespace SagaEngine::Scripting
