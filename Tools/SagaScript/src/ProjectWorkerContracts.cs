// SPDX-License-Identifier: MPL-2.0

using Microsoft.CodeAnalysis.CSharp;
using SagaProjectKit.Core;

namespace SagaScript;

internal sealed record WorkerProjectInput
{
    public string ProjectId { get; init; } = "";
    public string Path { get; init; } = "";
    public string Hash { get; init; } = "";
}

internal sealed record WorkerSourceInput
{
    public string Path { get; init; } = "";
    public string Hash { get; init; } = "";
}

internal sealed record WorkerVerifiedInput
{
    public string Path { get; init; } = "";
    public string Hash { get; init; } = "";
}

internal sealed record WorkerCancellationInput
{
    public string OperationId { get; init; } = "";
    public string Path { get; init; } = "";
}

internal sealed record WorkerRuntimeBridgeInput
{
    public string LogicalReferenceId { get; init; } = "";
    public string DescriptorPath { get; init; } = "";
    public string DescriptorHash { get; init; } = "";
    public string AssemblyPath { get; init; } = "";
    public string AssemblyHash { get; init; } = "";
    public string TargetFramework { get; init; } = "";
    public string RuntimeCompatibility { get; init; } = "";
    public string Trust { get; init; } = "";
}

internal sealed record ProjectWorkerRequest
{
    public int SchemaVersion { get; init; }
    public string OperationId { get; init; } = "";
    public WorkerProjectInput Project { get; init; } = new();
    public string SourceRevision { get; init; } = "";
    public IReadOnlyList<WorkerSourceInput> Sources { get; init; } = [];
    public WorkerVerifiedInput PackageLock { get; init; } = new();
    public string PackageGenerationId { get; init; } = "";
    public string PackageGenerationHash { get; init; } = "";
    public string PackageGenerationManifestHash { get; init; } = "";
    public string PackageGraphHash { get; init; } = "";
    public WorkerVerifiedInput ReferenceManifest { get; init; } = new();
    public WorkerVerifiedInput ApiSurfaceManifest { get; init; } = new();
    public WorkerRuntimeBridgeInput RuntimeBridge { get; init; } = new();
    public IReadOnlyList<string> Operations { get; init; } = [];
    public string OutputRoot { get; init; } = "";
    public WorkerCancellationInput Cancellation { get; init; } = new();
}

internal sealed record WorkerDiagnostic
{
    public string Severity { get; init; } = "Error";
    public string Code { get; init; } = "";
    public string Path { get; init; } = "";
    public string Message { get; init; } = "";
}

internal sealed record ProjectWorkerResult
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = ToolInfo.Name;
    public string ToolVersion { get; init; } = ToolInfo.Version;
    public string CompilerVersion { get; init; } = typeof(CSharpCompilation).Assembly.GetName().Version?.ToString() ?? "unknown";
    public string OperationId { get; init; } = "";
    public string SourceRevision { get; init; } = "";
    public string Status { get; init; } = "Failed";
    public string FailureClass { get; init; } = "";
    public bool Cancelled { get; init; }
    public bool Stale { get; init; }
    public string ProjectId { get; init; } = "";
    public string ProjectHash { get; init; } = "";
    public string SourceSetHash { get; init; } = "";
    public string PackageLockHash { get; init; } = "";
    public string PackageGenerationId { get; init; } = "";
    public string PackageGenerationHash { get; init; } = "";
    public string PackageGenerationManifestHash { get; init; } = "";
    public string PackageGraphHash { get; init; } = "";
    public string ReferenceManifestHash { get; init; } = "";
    public string ApiSurfaceManifestHash { get; init; } = "";
    public string RuntimeBridgeAssemblyHash { get; init; } = "";
    public string CurrentManifestPath { get; init; } = "";
    public string CurrentManifestHash { get; init; } = "";
    public IReadOnlyList<PublishedWorkerArtifact> Artifacts { get; init; } = [];
    public IReadOnlyList<WorkerDiagnostic> Diagnostics { get; init; } = [];
}

internal sealed record PublishedWorkerArtifact
{
    public string ArtifactId { get; init; } = "";
    public string Kind { get; init; } = "";
    public string ObjectPath { get; init; } = "";
    public string ContentHash { get; init; } = "";
    public string ArtifactHash { get; init; } = "";
}

internal sealed record StagedWorkerArtifact(string ArtifactId, string Kind, string FileName,
    string ContentHash, string ArtifactHash, byte[] Bytes);

internal sealed record WorkerMethod(string Name, SourceSpan Span);
internal sealed record WorkerProjectionStep(string StepId, string Kind, string DisplayName,
    string StateKey, SourceSpan Span);
internal sealed record WorkerAction(string ActionId, string Method, string AttributeTypeId,
    bool PubliclyInvocable, SourceSpan Span, IReadOnlyList<WorkerProjectionStep> Steps);
internal sealed record WorkerBehavior(string BehaviorId, string DeclaringType, string SourceFile,
    string SourceHash, SourceSpan Span, IReadOnlyList<WorkerMethod> LifecycleMethods,
    IReadOnlyList<WorkerAction> Actions, IReadOnlyList<string> Capabilities);
