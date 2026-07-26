// SPDX-License-Identifier: MPL-2.0

namespace SagaProjectKit.Core;

public sealed record ProjectManifest
{
    public int SchemaVersion { get; init; }
    public string ProjectId { get; init; } = "";
    public string DisplayName { get; init; } = "";
    public EngineCompatibility EngineCompatibility { get; init; } = new();
    public ProjectPaths Paths { get; init; } = new();
    public IReadOnlyList<ProjectReference> Scenes { get; init; } = [];
    public IReadOnlyList<ProjectReference> Assets { get; init; } = [];
    public IReadOnlyList<ProjectReference> ScriptFolders { get; init; } = [];
    public IReadOnlyList<ProjectReference> InputMaps { get; init; } = [];
    public IReadOnlyList<ProjectReference> UiDocuments { get; init; } = [];
    public IReadOnlyList<ProjectReference> LaunchProfiles { get; init; } = [];
    public IReadOnlyList<ProjectReference> PackageProfiles { get; init; } = [];
    public IReadOnlyList<ProjectDependency> Dependencies { get; init; } = [];
    public IReadOnlyList<StatePolicy> StatePolicies { get; init; } = [];
}

public sealed record EngineCompatibility
{
    public string MinimumVersion { get; init; } = "";
    public string TargetVersion { get; init; } = "";
    public string Channel { get; init; } = "";
}

public sealed record ProjectPaths
{
    public string Diagnostics { get; init; } = "";
    public string GeneratedReports { get; init; } = "";
    public string SourceTruth { get; init; } = "";
    public string GeneratedState { get; init; } = "";
    public string PersonalState { get; init; } = "";
    public string SharedWorkspaceState { get; init; } = "";
}

public sealed record ProjectReference
{
    public string Id { get; init; } = "";
    public string Path { get; init; } = "";
    public string Kind { get; init; } = "";
}

public sealed record ProjectDependency
{
    public string PackageId { get; init; } = "";
    public string Version { get; init; } = "";
    public IReadOnlyList<string> Modules { get; init; } = [];
}

public sealed record StatePolicy
{
    public string Path { get; init; } = "";
    public string Classification { get; init; } = "";
    public bool Tracked { get; init; }
    public string Exception { get; init; } = "";
}

public sealed record ProjectDiagnostic
{
    public string Severity { get; init; } = "Error";
    public string Code { get; init; } = "";
    public string Path { get; init; } = "";
    public string Message { get; init; } = "";
}

public sealed record ResolvedProjectReference
{
    public string Category { get; init; } = "";
    public string Id { get; init; } = "";
    public string Kind { get; init; } = "";
    public string RelativePath { get; init; } = "";
    public string AbsolutePath { get; init; } = "";
    public bool Exists { get; init; }
}

public sealed record ProjectLoadResult
{
    public string ProjectRoot { get; init; } = "";
    public string ManifestPath { get; init; } = "";
    public string ProjectHash { get; init; } = "";
    public ProjectManifest? Manifest { get; init; }
    public IReadOnlyList<ResolvedProjectReference> References { get; init; } = [];
    public IReadOnlyList<ProjectDiagnostic> Diagnostics { get; init; } = [];
    public bool Passed => Diagnostics.All(item => item.Severity != "Error");
}
