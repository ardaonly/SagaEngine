// SPDX-FileCopyrightText: 2026 Arda Koyuncu
// SPDX-License-Identifier: MPL-2.0

using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;
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
internal sealed record WorkerAction(string ActionId, string Method, string AttributeTypeId,
    bool PubliclyInvocable, SourceSpan Span);
internal sealed record WorkerBehavior(string BehaviorId, string DeclaringType, string SourceFile,
    string SourceHash, SourceSpan Span, IReadOnlyList<WorkerMethod> LifecycleMethods,
    IReadOnlyList<WorkerAction> Actions, IReadOnlyList<string> Capabilities);

internal static class ProjectWorker
{
    private static readonly string[] SupportedOperations = ["analyze", "compile", "project-blocks"];

    public static int Run(string requestPath, string resultPath, string progressPath)
    {
        ProjectWorkerRequest request = new();
        ProjectSourceSelection? selection = null;
        var diagnostics = new List<WorkerDiagnostic>();
        string sourceSetHash = "";
        string staging = "";
        try
        {
            InitializeProgress(progressPath);
            request = ReadRequest(requestPath, diagnostics);
            Progress(progressPath, request, "ValidatingRequest");
            if (diagnostics.Count > 0)
            {
                return Finish(resultPath, Failure(request, "InvalidRequest", diagnostics));
            }
            if (Cancelled(request))
            {
                return Finish(resultPath, Failure(request, "Cancellation", [], cancelled: true));
            }

            selection = LoadProject(request, diagnostics);
            Progress(progressPath, request, "ValidatingProject");
            if (diagnostics.Count > 0 || selection is null)
            {
                return Finish(resultPath, Failure(request, "ProjectValidation", diagnostics));
            }
            if (Cancelled(request))
            {
                return Finish(resultPath, Failure(request, "Cancellation", [], cancelled: true));
            }

            var canonicalSources = CanonicalSources(selection);
            sourceSetHash = CanonicalJson.Hash(CanonicalJson.Serialize(canonicalSources));
            ValidateSelectedSources(request, canonicalSources, diagnostics);
            ValidateVerifiedInputs(request, selection, diagnostics);
            var allowedApis = LoadAllowedApiSurface(request, diagnostics);
            var runtimeBridgeAssembly = ResolveWorkerRuntimeBridgeAssembly(request, diagnostics);
            Progress(progressPath, request, "VerifyingInputs");
            if (diagnostics.Count > 0)
            {
                return Finish(resultPath, Failure(request, "DependencyValidation", diagnostics, sourceSetHash));
            }
            if (Cancelled(request))
            {
                return Finish(resultPath, Failure(request, "Cancellation", [], sourceSetHash, cancelled: true));
            }

            var behaviors = AnalyzeBehaviors(selection, canonicalSources, runtimeBridgeAssembly, diagnostics);
            ValidateSceneBindings(selection, behaviors, diagnostics);
            Progress(progressPath, request, "Analyzing");
            if (diagnostics.Count > 0)
            {
                return Finish(resultPath, Failure(request, "Analysis", diagnostics, sourceSetHash));
            }
            if (Cancelled(request))
            {
                return Finish(resultPath, Failure(request, "Cancellation", [], sourceSetHash, cancelled: true));
            }
            if (!DotNetSdkProbe.HasRequiredSdk())
            {
                diagnostics.Add(Error("Script.Worker.DotNetSdkMissing", "",
                    "SagaScript worker compilation requires the .NET 10 SDK."));
                return Finish(resultPath, Failure(request, "Compilation", diagnostics, sourceSetHash));
            }

            staging = BuildStagingPath(request.OutputRoot, request.OperationId);
            if (Directory.Exists(staging))
            {
                Directory.Delete(staging, recursive: true);
            }
            Directory.CreateDirectory(staging);
            var compileResult = Compile(selection, staging, runtimeBridgeAssembly);
            foreach (var item in compileResult.DiagnosticReport.Diagnostics.Where(SagaScriptAnalyzer.IsBlocking))
            {
                diagnostics.Add(new WorkerDiagnostic
                {
                    Code = item.Code,
                    Path = RelativeSource(selection.ProjectRoot, item.SourceFile ?? ""),
                    Message = item.Message,
                    Severity = item.Severity,
                });
            }
            Progress(progressPath, request, "Compiling");
            if (diagnostics.Count > 0)
            {
                return Finish(resultPath, Failure(request, "Compilation", diagnostics, sourceSetHash));
            }
            if (Cancelled(request))
            {
                return Finish(resultPath, Failure(request, "Cancellation", [], sourceSetHash, cancelled: true));
            }

            var artifacts = BuildArtifacts(request, canonicalSources, sourceSetHash,
                behaviors, allowedApis, staging, runtimeBridgeAssembly, diagnostics);
            Progress(progressPath, request, "Projecting");
            if (diagnostics.Count > 0)
            {
                return Finish(resultPath, Failure(request, "Analysis", diagnostics, sourceSetHash));
            }
            if (Cancelled(request))
            {
                return Finish(resultPath, Failure(request, "Cancellation", [], sourceSetHash, cancelled: true));
            }

            Progress(progressPath, request, "FreshnessCheck");
            if (!IsFresh(request, selection, canonicalSources))
            {
                diagnostics.Add(Error("Script.Worker.StaleResult", "",
                    "Project, source, lock, reference, or surface input changed before publication."));
                return Finish(resultPath, Failure(request, "StaleResult", diagnostics, sourceSetHash, stale: true));
            }
            if (Cancelled(request))
            {
                return Finish(resultPath, Failure(request, "Cancellation", [], sourceSetHash, cancelled: true));
            }

            (string ManifestHash, IReadOnlyList<PublishedWorkerArtifact> Artifacts) publication;
            try
            {
                publication = Publish(request, selection, canonicalSources, sourceSetHash, artifacts);
            }
            catch (OperationCanceledException)
            {
                return Finish(resultPath, Failure(request, "Cancellation", [], sourceSetHash, cancelled: true));
            }
            catch (StaleWorkerResultException exception)
            {
                diagnostics.Add(Error("Script.Worker.StaleResult", "publication", exception.Message));
                return Finish(resultPath, Failure(request, "StaleResult", diagnostics, sourceSetHash, stale: true));
            }
            catch (Exception exception)
            {
                diagnostics.Add(Error("Script.Worker.PublishFailed", "outputRoot", exception.Message));
                return Finish(resultPath, Failure(request, "Publication", diagnostics, sourceSetHash));
            }
            Progress(progressPath, request, "Publishing");
            Progress(progressPath, request, "Completed");
            return Finish(resultPath, new ProjectWorkerResult
            {
                OperationId = request.OperationId,
                SourceRevision = request.SourceRevision,
                Status = "Passed",
                ProjectId = request.Project.ProjectId,
                ProjectHash = request.Project.Hash,
                SourceSetHash = sourceSetHash,
                PackageLockHash = request.PackageLock.Hash,
                ReferenceManifestHash = request.ReferenceManifest.Hash,
                ApiSurfaceManifestHash = request.ApiSurfaceManifest.Hash,
                RuntimeBridgeAssemblyHash = request.RuntimeBridge.AssemblyHash,
                CurrentManifestPath = "manifests/current.json",
                CurrentManifestHash = publication.ManifestHash,
                Artifacts = publication.Artifacts,
            });
        }
        catch (Exception exception)
        {
            diagnostics.Add(Error("Script.Worker.InternalFailure", "", exception.Message));
            return Finish(resultPath, Failure(request, "Internal", diagnostics, sourceSetHash));
        }
        finally
        {
            if (staging.Length > 0 && Directory.Exists(staging))
            {
                Directory.Delete(staging, recursive: true);
            }
        }
    }

    private static ProjectWorkerRequest ReadRequest(string path, List<WorkerDiagnostic> diagnostics)
    {
        if (!File.Exists(path))
        {
            diagnostics.Add(Error("Script.Worker.RequestMissing", path, "Worker request file is missing."));
            return new ProjectWorkerRequest();
        }
        try
        {
            using var document = JsonDocument.Parse(File.ReadAllBytes(path));
            ValidateObject(document.RootElement, "request",
                ["schemaVersion", "operationId", "project", "sourceRevision", "sources", "packageLock",
                    "referenceManifest", "apiSurfaceManifest", "runtimeBridge", "operations", "outputRoot", "cancellation"], diagnostics);
            if (document.RootElement.ValueKind == JsonValueKind.Object)
            {
                if (document.RootElement.TryGetProperty("project", out var project))
                    ValidateObject(project, "project", ["projectId", "path", "hash"], diagnostics);
                if (document.RootElement.TryGetProperty("sources", out var sources) && sources.ValueKind == JsonValueKind.Array)
                    foreach (var source in sources.EnumerateArray()) ValidateObject(source, "sources", ["path", "hash"], diagnostics);
                foreach (var name in new[] { "packageLock", "referenceManifest", "apiSurfaceManifest" })
                    if (document.RootElement.TryGetProperty(name, out var input))
                        ValidateObject(input, name, ["path", "hash"], diagnostics);
                if (document.RootElement.TryGetProperty("runtimeBridge", out var runtimeBridge))
                    ValidateObject(runtimeBridge, "runtimeBridge",
                        ["logicalReferenceId", "descriptorPath", "descriptorHash", "assemblyPath", "assemblyHash",
                         "targetFramework", "runtimeCompatibility", "trust"], diagnostics);
                if (document.RootElement.TryGetProperty("cancellation", out var cancellation))
                    ValidateObject(cancellation, "cancellation", ["operationId", "path"], diagnostics);
            }
            var request = JsonSerializer.Deserialize<ProjectWorkerRequest>(
                File.ReadAllBytes(path), CanonicalJson.SerializerOptions) ?? new ProjectWorkerRequest();
            if (request.SchemaVersion != 1)
                diagnostics.Add(Error("Script.Worker.SchemaUnsupported", "schemaVersion", "Worker request schemaVersion must be 1."));
            Require(request.OperationId, "operationId", diagnostics);
            Require(request.SourceRevision, "sourceRevision", diagnostics);
            Require(request.Project.ProjectId, "project.projectId", diagnostics);
            Require(request.Project.Path, "project.path", diagnostics);
            RequireHash(request.Project.Hash, "project.hash", diagnostics);
            RequireInput(request.PackageLock, "packageLock", diagnostics);
            RequireInput(request.ReferenceManifest, "referenceManifest", diagnostics);
            RequireInput(request.ApiSurfaceManifest, "apiSurfaceManifest", diagnostics);
            Require(request.RuntimeBridge.LogicalReferenceId, "runtimeBridge.logicalReferenceId", diagnostics);
            Require(request.RuntimeBridge.DescriptorPath, "runtimeBridge.descriptorPath", diagnostics);
            RequireHash(request.RuntimeBridge.DescriptorHash, "runtimeBridge.descriptorHash", diagnostics);
            Require(request.RuntimeBridge.AssemblyPath, "runtimeBridge.assemblyPath", diagnostics);
            RequireHash(request.RuntimeBridge.AssemblyHash, "runtimeBridge.assemblyHash", diagnostics);
            Require(request.RuntimeBridge.TargetFramework, "runtimeBridge.targetFramework", diagnostics);
            Require(request.RuntimeBridge.RuntimeCompatibility, "runtimeBridge.runtimeCompatibility", diagnostics);
            Require(request.RuntimeBridge.Trust, "runtimeBridge.trust", diagnostics);
            Require(request.OutputRoot, "outputRoot", diagnostics);
            Require(request.Cancellation.OperationId, "cancellation.operationId", diagnostics);
            Require(request.Cancellation.Path, "cancellation.path", diagnostics);
            if (request.Cancellation.OperationId != request.OperationId)
                diagnostics.Add(Error("Script.Worker.CancellationIdentityMismatch", "cancellation.operationId", "Cancellation identity must match operationId."));
            var operations = request.Operations.Order(StringComparer.Ordinal).ToArray();
            if (!operations.SequenceEqual(SupportedOperations, StringComparer.Ordinal))
                diagnostics.Add(Error("Script.Worker.OperationSetInvalid", "operations", "Worker operation set must contain analyze, compile, and project-blocks exactly once."));
            if (request.Sources.Count == 0)
                diagnostics.Add(Error("Script.Worker.SourceSetMissing", "sources", "Worker request must select at least one source."));
            return request;
        }
        catch (JsonException exception)
        {
            diagnostics.Add(Error("Script.Worker.RequestParseFailed", path, exception.Message));
            return new ProjectWorkerRequest();
        }
    }

    private static ProjectSourceSelection? LoadProject(ProjectWorkerRequest request,
        List<WorkerDiagnostic> diagnostics)
    {
        try
        {
            var selection = ProjectSourceSelection.Load(request.Project.Path);
            if (selection.Project.Manifest?.ProjectId != request.Project.ProjectId)
                diagnostics.Add(Error("Script.Worker.ProjectIdentityMismatch", "project.projectId", "Request projectId does not match the selected project."));
            if (selection.Project.ProjectHash != request.Project.Hash)
                diagnostics.Add(Error("Script.Worker.ProjectHashMismatch", "project.hash", "Request project hash is stale."));
            return selection;
        }
        catch (Exception exception)
        {
            diagnostics.Add(Error("Script.Worker.ProjectInvalid", request.Project.Path, exception.Message));
            return null;
        }
    }

    private static WorkerSourceInput[] CanonicalSources(ProjectSourceSelection selection) =>
        selection.SourceFiles.Select(path => new WorkerSourceInput
        {
            Path = RelativeSource(selection.ProjectRoot, path),
            Hash = CanonicalJson.HashFile(path),
        }).OrderBy(item => item.Path, StringComparer.Ordinal).ToArray();

    private static void ValidateSelectedSources(ProjectWorkerRequest request,
        IReadOnlyList<WorkerSourceInput> actual, List<WorkerDiagnostic> diagnostics)
    {
        var selected = request.Sources.OrderBy(item => item.Path, StringComparer.Ordinal).ToArray();
        if (selected.Select(item => item.Path).Distinct(StringComparer.Ordinal).Count() != selected.Length ||
            !CanonicalJson.Serialize(selected).SequenceEqual(CanonicalJson.Serialize(actual)))
        {
            diagnostics.Add(Error("Script.Worker.SourceSetMismatch", "sources",
                "Selected source paths and hashes must exactly match the canonical project source set."));
        }
    }

    private static void ValidateVerifiedInputs(ProjectWorkerRequest request,
        ProjectSourceSelection selection, List<WorkerDiagnostic> diagnostics)
    {
        foreach (var (name, input) in new[]
        {
            ("packageLock", request.PackageLock),
            ("referenceManifest", request.ReferenceManifest),
            ("apiSurfaceManifest", request.ApiSurfaceManifest),
        })
        {
            if (!File.Exists(input.Path))
            {
                diagnostics.Add(Error("Script.Worker.VerifiedInputMissing", name, "Verified input file is missing."));
            }
            else if (CanonicalJson.HashFile(input.Path) != input.Hash)
            {
                diagnostics.Add(Error("Script.Worker.VerifiedInputHashMismatch", name, "Verified input hash does not match current bytes."));
            }
        }
        if (diagnostics.Count > 0) return;
        try
        {
            using var lockDocument = JsonDocument.Parse(File.ReadAllBytes(request.PackageLock.Path));
            var root = lockDocument.RootElement;
            if (root.GetProperty("schemaVersion").GetInt32() != 1 ||
                root.GetProperty("projectId").GetString() != request.Project.ProjectId ||
                root.GetProperty("projectHash").GetString() != request.Project.Hash)
            {
                diagnostics.Add(Error("Script.Worker.LockProjectMismatch", "packageLock",
                    "Verified package lock does not match the selected project."));
            }
            var graphHash = root.GetProperty("graphHash").GetString();
            var distributionRoot = ResolveDistributionRoot();
            foreach (var package in root.GetProperty("packages").EnumerateArray())
            {
                var trust = package.GetProperty("trust").GetString() ?? "";
                foreach (var artifact in package.GetProperty("artifacts").EnumerateArray())
                {
                    var kind = artifact.GetProperty("kind").GetString() ?? "";
                    var relative = artifact.GetProperty("distributionPath").GetString() ?? "";
                    var expectedHash = artifact.GetProperty("artifactHash").GetString() ?? "";
                    var full = Path.GetFullPath(Path.Combine(distributionRoot, relative));
                    var prefix = distributionRoot.TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
                    if (!full.StartsWith(prefix, StringComparison.Ordinal) || !File.Exists(full) ||
                        CanonicalJson.HashFile(full) != expectedHash)
                    {
                        diagnostics.Add(Error("Script.Worker.LockedArtifactInvalid", relative,
                            "Locked artifact is missing, outside the engine distribution, or hash-mismatched."));
                    }
                    if (kind is "managedReference" or "analyzer" && trust != "TrustedEngineDistribution")
                    {
                        diagnostics.Add(Error("Script.Worker.ExecutableTrustRejected", relative,
                            "Executable references and analyzers require TrustedEngineDistribution."));
                    }
                }
            }
            foreach (var (name, input) in new[]
            {
                ("referenceManifest", request.ReferenceManifest),
                ("apiSurfaceManifest", request.ApiSurfaceManifest),
            })
            {
                using var document = JsonDocument.Parse(File.ReadAllBytes(input.Path));
                var metadata = document.RootElement;
                if (metadata.GetProperty("schemaVersion").GetInt32() != 1 ||
                    metadata.GetProperty("projectId").GetString() != request.Project.ProjectId ||
                    metadata.GetProperty("projectHash").GetString() != request.Project.Hash ||
                    metadata.GetProperty("graphHash").GetString() != graphHash)
                {
                    diagnostics.Add(Error("Script.Worker.GeneratedMetadataMismatch", name,
                        "Generated package metadata is not from the verified project graph."));
                }
            }
        }
        catch (Exception exception) when (exception is JsonException or InvalidOperationException or KeyNotFoundException)
        {
            diagnostics.Add(Error("Script.Worker.VerifiedInputInvalid", "packageGraph", exception.Message));
        }
    }

    private static IReadOnlyList<object> LoadAllowedApiSurface(ProjectWorkerRequest request,
        List<WorkerDiagnostic> diagnostics)
    {
        var allowed = new List<object>();
        try
        {
            using var lockDocument = JsonDocument.Parse(File.ReadAllBytes(request.PackageLock.Path));
            var artifact = lockDocument.RootElement.GetProperty("packages").EnumerateArray()
                .SelectMany(package => package.GetProperty("artifacts").EnumerateArray())
                .Single(item => item.GetProperty("kind").GetString() == "apiSurface");
            var distributionRoot = ResolveDistributionRoot();
            var relative = artifact.GetProperty("distributionPath").GetString() ?? "";
            var surfacePath = Path.GetFullPath(Path.Combine(distributionRoot, relative));
            if (!surfacePath.StartsWith(distributionRoot.TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar,
                    StringComparison.Ordinal) || !File.Exists(surfacePath) ||
                CanonicalJson.HashFile(surfacePath) != artifact.GetProperty("artifactHash").GetString())
            {
                diagnostics.Add(Error("Script.Worker.ApiSurfaceTrustFailed", relative,
                    "API surface is missing or changed outside the verified engine distribution."));
                return allowed;
            }
            using var surface = JsonDocument.Parse(File.ReadAllBytes(surfacePath));
            foreach (var api in surface.RootElement.GetProperty("apis").EnumerateArray())
            {
                var visibility = api.GetProperty("visibility").GetString() ?? "";
                var capability = api.GetProperty("capability").GetString() ?? "";
                var projection = api.GetProperty("blockProjection").GetString() ?? "";
                if (visibility != "Public" || capability is "Raw" or "Internal" || projection == "Filtered") continue;
                allowed.Add(new
                {
                    apiId = api.GetProperty("apiId").GetString() ?? "",
                    capability,
                    blockProjection = projection,
                });
            }
        }
        catch (Exception exception) when (exception is JsonException or InvalidOperationException or KeyNotFoundException)
        {
            diagnostics.Add(Error("Script.Worker.ApiSurfaceInvalid", "apiSurface", exception.Message));
        }
        return allowed;
    }

    private static string ResolveWorkerRuntimeBridgeAssembly(ProjectWorkerRequest request,
        List<WorkerDiagnostic> diagnostics)
    {
        try
        {
            var bridge = request.RuntimeBridge;
            if (bridge.Trust != "TrustedEngineDistribution" || bridge.TargetFramework != ToolInfo.TargetFramework ||
                bridge.RuntimeCompatibility != "0.0.11")
                throw new InvalidOperationException("Runtime bridge trust/framework/runtime compatibility is invalid.");
            if (!File.Exists(bridge.DescriptorPath) || CanonicalJson.HashFile(bridge.DescriptorPath) != bridge.DescriptorHash)
                throw new InvalidOperationException("Runtime bridge descriptor is missing or hash-mismatched.");
            if (!File.Exists(bridge.AssemblyPath) || CanonicalJson.HashFile(bridge.AssemblyPath) != bridge.AssemblyHash)
                throw new InvalidOperationException("Runtime bridge assembly is missing or hash-mismatched.");
            using var references = JsonDocument.Parse(File.ReadAllBytes(request.ReferenceManifest.Path));
            var reference = references.RootElement.GetProperty("references").EnumerateArray()
                .Single(item => item.GetProperty("artifactId").GetString() == bridge.LogicalReferenceId);
            if (reference.GetProperty("artifactHash").GetString() != bridge.DescriptorHash ||
                reference.GetProperty("resolvedAssemblyHash").GetString() != bridge.AssemblyHash ||
                reference.GetProperty("resolvedAssemblyLogicalPath").GetString() is null)
                throw new InvalidOperationException("Runtime bridge request does not match the verified reference manifest.");
            return bridge.AssemblyPath;
        }
        catch (Exception exception) when (exception is JsonException or InvalidOperationException or KeyNotFoundException)
        {
            diagnostics.Add(Error("Script.Worker.ManagedReferenceInvalid", "referenceManifest", exception.Message));
            return "";
        }
    }

    private static IReadOnlyList<WorkerBehavior> AnalyzeBehaviors(ProjectSourceSelection selection,
        IReadOnlyList<WorkerSourceInput> sources, string runtimeBridgeAssembly,
        List<WorkerDiagnostic> diagnostics)
    {
        var sourceHashes = sources.ToDictionary(item => item.Path, item => item.Hash, StringComparer.Ordinal);
        var behaviors = new List<WorkerBehavior>();
        var sourceData = selection.SourceFiles.Order(StringComparer.Ordinal).Select(sourceFile =>
        {
            var relative = RelativeSource(selection.ProjectRoot, sourceFile);
            var text = File.ReadAllText(sourceFile, Encoding.UTF8);
            var tree = CSharpSyntaxTree.ParseText(text, new CSharpParseOptions(LanguageVersion.Latest), relative);
            return (sourceFile, relative, text, tree);
        }).ToArray();
        var trustedAssemblies = (AppContext.GetData("TRUSTED_PLATFORM_ASSEMBLIES") as string ?? "")
            .Split(Path.PathSeparator, StringSplitOptions.RemoveEmptyEntries)
            .Select(path => MetadataReference.CreateFromFile(path)).Cast<MetadataReference>().ToList();
        trustedAssemblies.Add(MetadataReference.CreateFromFile(runtimeBridgeAssembly));
        var compilation = CSharpCompilation.Create("SagaScriptSemanticEvidence", sourceData.Select(item => item.tree),
            trustedAssemblies, new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary));
        foreach (var (sourceFile, relative, text, tree) in sourceData)
        {
            foreach (var parseError in tree.GetDiagnostics().Where(item => item.Severity == DiagnosticSeverity.Error))
            {
                diagnostics.Add(Error("Script.Worker.ParseFailed", relative, parseError.ToString()));
            }
            var root = tree.GetCompilationUnitRoot();
            var model = compilation.GetSemanticModel(tree);
            foreach (var type in root.DescendantNodes().OfType<ClassDeclarationSyntax>())
            {
                if (model.GetDeclaredSymbol(type) is not INamedTypeSymbol typeSymbol) continue;
                var baseType = typeSymbol.BaseType;
                var derives = false;
                while (baseType is not null)
                {
                    if (baseType.ToDisplayString() == "SagaEngine.Scripting.SagaBehavior") { derives = true; break; }
                    baseType = baseType.BaseType;
                }
                if (!derives) continue;
                var declaringType = typeSymbol.ToDisplayString();
                var lifecycle = type.Members.OfType<MethodDeclarationSyntax>()
                    .Where(method => method.Identifier.ValueText is "OnCreate" or "OnStart" or "OnUpdate" or "OnDestroy")
                    .Select(method => new WorkerMethod(method.Identifier.ValueText, Span(text, tree, method)))
                    .OrderBy(method => method.Name, StringComparer.Ordinal).ToArray();
                var actions = new List<WorkerAction>();
                foreach (var method in type.Members.OfType<MethodDeclarationSyntax>())
                {
                    if (model.GetDeclaredSymbol(method) is not IMethodSymbol methodSymbol) continue;
                    var actionData = methodSymbol.GetAttributes().FirstOrDefault(attribute =>
                        attribute.AttributeClass?.ToDisplayString() == "SagaEngine.Scripting.SagaActionAttribute");
                    if (actionData is null) continue;
                    if (methodSymbol.DeclaredAccessibility != Accessibility.Public || methodSymbol.IsStatic ||
                        !methodSymbol.ReturnsVoid || methodSymbol.Parameters.Length != 0)
                    {
                        diagnostics.Add(Error("Script.Worker.ActionNotInvocable", relative,
                            "SagaAction methods must be public instance void methods with no parameters."));
                        continue;
                    }
                    var actionAttribute = method.AttributeLists.SelectMany(list => list.Attributes)
                        .First(attribute => model.GetSymbolInfo(attribute).Symbol is IMethodSymbol constructor &&
                            constructor.ContainingType.ToDisplayString() == "SagaEngine.Scripting.SagaActionAttribute");
                    var actionId = actionAttribute.ArgumentList?.Arguments.FirstOrDefault()?.Expression is LiteralExpressionSyntax literal &&
                        literal.IsKind(SyntaxKind.StringLiteralExpression) ? literal.Token.ValueText : "";
                    if (string.IsNullOrWhiteSpace(actionId))
                    {
                        diagnostics.Add(Error("Script.Worker.ActionIdentityInvalid", relative,
                            "[SagaAction] requires a non-empty string action id."));
                        continue;
                    }
                    actions.Add(new WorkerAction(actionId, method.Identifier.ValueText,
                        "SagaEngine.Scripting.SagaActionAttribute", true, Span(text, tree, method)));
                }
                var capabilities = new SortedSet<string>(StringComparer.Ordinal) { "Gameplay.State.ReadWrite" };
                if (text.Contains("Context.Self.Transform", StringComparison.Ordinal))
                    capabilities.Add("World.Transform.ReadWrite");
                behaviors.Add(new WorkerBehavior(declaringType, declaringType, relative,
                    sourceHashes[relative], Span(text, tree, type), lifecycle,
                    actions.OrderBy(item => item.ActionId, StringComparer.Ordinal).ToArray(), capabilities.ToArray()));
            }
        }
        if (behaviors.Count == 0)
            diagnostics.Add(Error("Script.Worker.BehaviorMissing", "sources", "No SagaBehavior-derived class was found."));
        foreach (var duplicate in behaviors.GroupBy(item => item.BehaviorId, StringComparer.Ordinal).Where(group => group.Count() > 1))
            diagnostics.Add(Error("Script.Worker.BehaviorDuplicate", duplicate.Key, "Behavior identities must be unique."));
        foreach (var duplicate in behaviors.SelectMany(item => item.Actions.Select(action => action.ActionId))
                     .GroupBy(item => item, StringComparer.Ordinal).Where(group => group.Count() > 1))
            diagnostics.Add(Error("Script.Worker.ActionDuplicate", duplicate.Key, "Named action identities must be unique."));
        return behaviors.OrderBy(item => item.BehaviorId, StringComparer.Ordinal).ToArray();
    }

    private static void ValidateSceneBindings(ProjectSourceSelection selection,
        IReadOnlyList<WorkerBehavior> behaviors, List<WorkerDiagnostic> diagnostics)
    {
        if (selection.Project.Manifest is null) return;
        var byId = behaviors.ToDictionary(item => item.BehaviorId, StringComparer.Ordinal);
        foreach (var scene in selection.Project.Manifest.Scenes.OrderBy(item => item.Id, StringComparer.Ordinal))
        {
            try
            {
                using var document = JsonDocument.Parse(File.ReadAllBytes(Path.Combine(selection.ProjectRoot, scene.Path)));
                var root = document.RootElement;
                if (root.TryGetProperty("entities", out var entities))
                {
                    foreach (var component in entities.EnumerateArray()
                                 .SelectMany(entity => entity.GetProperty("components").EnumerateArray())
                                 .Where(component => component.GetProperty("type").GetString() == "ScriptBehavior"))
                    {
                        var behaviorId = component.GetProperty("behaviorTypeId").GetString() ?? "";
                        if (!byId.ContainsKey(behaviorId))
                            diagnostics.Add(Error("Script.Worker.SceneBehaviorMissing", scene.Path,
                                $"Scene references missing behavior '{behaviorId}'."));
                    }
                }
                if (root.TryGetProperty("entities", out var bindingEntities))
                {
                    foreach (var binding in bindingEntities.EnumerateArray()
                                 .SelectMany(entity => entity.GetProperty("components").EnumerateArray())
                                 .Where(component => component.GetProperty("type").GetString() == "ScriptActionBinding"))
                    {
                        var behaviorId = binding.GetProperty("behaviorTypeId").GetString() ?? "";
                        var actionId = binding.GetProperty("namedActionId").GetString() ?? "";
                        if (!byId.TryGetValue(behaviorId, out var behavior) ||
                            !behavior.Actions.Any(action => action.ActionId == actionId))
                        {
                            diagnostics.Add(Error("Script.Worker.SceneActionMissing", scene.Path,
                                $"Scene binding '{actionId}' does not resolve to {behaviorId}."));
                        }
                    }
                }
            }
            catch (Exception exception) when (exception is JsonException or InvalidOperationException or KeyNotFoundException)
            {
                diagnostics.Add(Error("Script.Worker.SceneInvalid", scene.Path, exception.Message));
            }
        }
    }

    private static SagaScriptCompileResult Compile(ProjectSourceSelection selection, string staging,
        string runtimeBridgeAssembly)
    {
        var analysis = new SagaScriptAnalyzer().Analyze(selection.SourceFiles, requireManifestReady: true);
        var assemblyName = "SagaProject_" + selection.Project.Manifest!.ProjectId.Replace("-", "", StringComparison.Ordinal);
        return SagaScriptCompiler.Compile(new SagaScriptCompileRequest
        {
            Analysis = analysis,
            SourceFiles = selection.SourceFiles,
            ManifestOutputDirectory = Path.Combine(staging, "legacy-manifests"),
            ArtifactOutputDirectory = Path.Combine(staging, "compiled"),
            ProjectRoot = selection.ProjectRoot,
            AssemblyName = assemblyName,
            RuntimeBridgeAssemblyPath = runtimeBridgeAssembly,
        });
    }

    private static IReadOnlyList<StagedWorkerArtifact> BuildArtifacts(ProjectWorkerRequest request,
        IReadOnlyList<WorkerSourceInput> sources, string sourceSetHash,
        IReadOnlyList<WorkerBehavior> behaviors, IReadOnlyList<object> allowedApis, string staging,
        string runtimeBridgeAssembly, List<WorkerDiagnostic> diagnostics)
    {
        var common = new
        {
            schemaVersion = 1,
            projectId = request.Project.ProjectId,
            projectHash = request.Project.Hash,
            sourceRevision = request.SourceRevision,
            sourceSetHash,
            toolVersion = ToolInfo.Version,
            compilerVersion = typeof(CSharpCompilation).Assembly.GetName().Version?.ToString() ?? "unknown",
        };
        var artifacts = new List<StagedWorkerArtifact>();
        artifacts.Add(JsonArtifact("scripts.behaviors", "behaviorManifest", "behavior_manifest_v1.json", new
        {
            common.schemaVersion, common.projectId, common.projectHash, common.sourceRevision,
            common.sourceSetHash, common.toolVersion, common.compilerVersion, behaviors,
        }));
        var actions = behaviors.SelectMany(behavior => behavior.Actions.Select(action => new
        {
            action.ActionId,
            behaviorId = behavior.BehaviorId,
            action.Method,
            action.AttributeTypeId,
            action.PubliclyInvocable,
            sourceFile = behavior.SourceFile,
            action.Span,
        })).OrderBy(item => item.ActionId, StringComparer.Ordinal).ToArray();
        artifacts.Add(JsonArtifact("scripts.named-actions", "namedActionManifest", "named_actions_v1.json", new
        {
            common.schemaVersion, common.projectId, common.projectHash, common.sourceRevision,
            common.sourceSetHash, common.toolVersion, actions,
        }));
        var capabilities = behaviors.SelectMany(item => item.Capabilities).Distinct(StringComparer.Ordinal)
            .Order(StringComparer.Ordinal).ToArray();
        artifacts.Add(JsonArtifact("scripts.capabilities", "capabilityManifest", "capabilities_v1.json", new
        {
            common.schemaVersion, common.projectId, common.projectHash, common.sourceRevision,
            common.sourceSetHash, common.toolVersion, capabilities,
            packageGraphHash = ReadString(request.PackageLock.Path, "graphHash"),
        }));
        artifacts.Add(JsonArtifact("scripts.source-map", "sourceMap", "source_map_v1.json", new
        {
            common.schemaVersion, common.projectId, common.projectHash, common.sourceRevision,
            common.sourceSetHash, common.toolVersion, sources,
            behaviors = behaviors.Select(item => new { item.BehaviorId, item.SourceFile, item.SourceHash, item.Span,
                item.LifecycleMethods, item.Actions }).ToArray(),
        }));
        var blocks = behaviors.SelectMany(behavior => behavior.LifecycleMethods
            .Select(method => new { blockId = $"behavior://{behavior.BehaviorId}/{method.Name}", blockKind = "LifecycleMethod",
                displayName = method.Name, classification = "ReadOnlySupported", editable = false,
                sourceFile = behavior.SourceFile, sourceHash = behavior.SourceHash, sourceSpan = method.Span })
            .Concat(behavior.Actions.Select(action => new { blockId = $"action://{behavior.BehaviorId}/{action.ActionId}",
                blockKind = "NamedAction", displayName = action.ActionId, classification = "ReadOnlySupported", editable = false,
                sourceFile = behavior.SourceFile, sourceHash = behavior.SourceHash, sourceSpan = action.Span })))
            .OrderBy(item => item.sourceFile, StringComparer.Ordinal).ThenBy(item => item.sourceSpan.StartByte).ToArray();
        var opaque = behaviors.Select(item => new
        {
            regionId = $"opaque://{item.BehaviorId}/implementation",
            classification = "OpaqueCSharp",
            editable = false,
            item.SourceFile,
            item.SourceHash,
            sourceSpan = item.Span,
            reason = "Unsupported C# remains source-linked and opaque; projection never rewrites authored C#.",
        }).ToArray();
        artifacts.Add(JsonArtifact("scripts.block-projection", "blockProjection", "block_projection_v1.json", new
        {
            common.schemaVersion, common.projectId, common.projectHash, common.sourceRevision,
            common.sourceSetHash, common.toolVersion, sourcePreservation = "SourceNotMutated",
            readOnly = true, blocks, opaqueRegions = opaque, allowedApis,
            filteredApiPolicy = "ExcludeRawAndInternal",
        }));

        var compiledRoot = Path.Combine(staging, "compiled");
        var assemblyPath = Directory.GetFiles(compiledRoot, "*.scripts.dll", SearchOption.TopDirectoryOnly).SingleOrDefault();
        var runtimeConfigPath = Directory.GetFiles(compiledRoot, "*.scripts.runtimeconfig.json", SearchOption.TopDirectoryOnly).SingleOrDefault();
        if (assemblyPath is null || runtimeConfigPath is null)
        {
            diagnostics.Add(Error("Script.Worker.AssemblyMissing", "compile", "Deterministic compilation did not produce assembly outputs."));
            return artifacts;
        }
        var assembly = BinaryArtifact("scripts.assembly", "assembly", Path.GetFileName(assemblyPath), File.ReadAllBytes(assemblyPath));
        var runtimeConfig = BinaryArtifact("scripts.runtime-config", "runtimeConfig", Path.GetFileName(runtimeConfigPath), File.ReadAllBytes(runtimeConfigPath));
        artifacts.Add(assembly);
        artifacts.Add(runtimeConfig);
        artifacts.Add(JsonArtifact("scripts.assembly-manifest", "assemblyManifest", "assembly_manifest_v1.json", new
        {
            common.schemaVersion, common.projectId, common.projectHash, common.sourceRevision,
            common.sourceSetHash, common.toolVersion, common.compilerVersion,
            assemblyId = assembly.ArtifactId, assemblyHash = assembly.ArtifactHash,
            runtimeConfigId = runtimeConfig.ArtifactId, runtimeConfigHash = runtimeConfig.ArtifactHash,
            targetFramework = ToolInfo.TargetFramework,
            referenceManifestHash = request.ReferenceManifest.Hash,
            runtimeBridgeLogicalReferenceId = request.RuntimeBridge.LogicalReferenceId,
            runtimeBridgeDescriptorHash = request.RuntimeBridge.DescriptorHash,
            runtimeBridgeAssemblyLogicalPath = ReadBridgeLogicalPath(request.ReferenceManifest.Path,
                request.RuntimeBridge.LogicalReferenceId),
            runtimeBridgeHash = request.RuntimeBridge.AssemblyHash,
            runtimeCompatibility = request.RuntimeBridge.RuntimeCompatibility,
            trust = request.RuntimeBridge.Trust,
        }));
        return artifacts.OrderBy(item => item.ArtifactId, StringComparer.Ordinal).ToArray();
    }

    private sealed class StaleWorkerResultException(string message) : Exception(message);

    private static (string ManifestHash, IReadOnlyList<PublishedWorkerArtifact> Artifacts) Publish(
        ProjectWorkerRequest request, ProjectSourceSelection selection,
        IReadOnlyList<WorkerSourceInput> originalSources, string sourceSetHash,
        IReadOnlyList<StagedWorkerArtifact> staged)
    {
        var outputRoot = Path.GetFullPath(request.OutputRoot);
        Directory.CreateDirectory(outputRoot);
        var published = new List<PublishedWorkerArtifact>();
        foreach (var artifact in staged.OrderBy(item => item.ArtifactId, StringComparer.Ordinal))
        {
            var hashToken = artifact.ArtifactHash["sha256:".Length..];
            var relative = $"objects/{hashToken}/{artifact.FileName}";
            var target = Path.Combine(outputRoot, relative);
            if (!File.Exists(target)) AtomicWrite(target, artifact.Bytes);
            else if (CanonicalJson.HashFile(target) != artifact.ArtifactHash)
                throw new InvalidOperationException("Content-addressed object hash collision.");
            published.Add(new PublishedWorkerArtifact
            {
                ArtifactId = artifact.ArtifactId,
                Kind = artifact.Kind,
                ObjectPath = relative,
                ContentHash = artifact.ContentHash,
                ArtifactHash = artifact.ArtifactHash,
            });
        }
        PublicationBarrier();
        if (Cancelled(request)) throw new OperationCanceledException();
        if (!IsFresh(request, selection, originalSources))
            throw new StaleWorkerResultException("Verified worker inputs changed after object publication.");
        var manifestBytes = CanonicalJson.Serialize(new
        {
            schemaVersion = 1,
            tool = ToolInfo.Name,
            toolVersion = ToolInfo.Version,
            compilerVersion = typeof(CSharpCompilation).Assembly.GetName().Version?.ToString() ?? "unknown",
            projectId = request.Project.ProjectId,
            projectHash = request.Project.Hash,
            sourceRevision = request.SourceRevision,
            sourceSetHash,
            packageLockHash = request.PackageLock.Hash,
            referenceManifestHash = request.ReferenceManifest.Hash,
            apiSurfaceManifestHash = request.ApiSurfaceManifest.Hash,
            runtimeBridgeLogicalReferenceId = request.RuntimeBridge.LogicalReferenceId,
            runtimeBridgeDescriptorHash = request.RuntimeBridge.DescriptorHash,
            runtimeBridgeAssemblyHash = request.RuntimeBridge.AssemblyHash,
            artifacts = published,
        });
        if (Cancelled(request)) throw new OperationCanceledException();
        if (!IsFresh(request, selection, originalSources))
            throw new StaleWorkerResultException("Verified worker inputs changed immediately before current publication.");
        AtomicWrite(Path.Combine(outputRoot, "manifests", "current.json"), manifestBytes);
        return (CanonicalJson.Hash(manifestBytes), published);
    }

    private static bool IsFresh(ProjectWorkerRequest request, ProjectSourceSelection selection,
        IReadOnlyList<WorkerSourceInput> originalSources)
    {
        if (CanonicalJson.HashFile(selection.Project.ManifestPath) != request.Project.Hash) return false;
        if (!CanonicalJson.Serialize(CanonicalSources(selection)).SequenceEqual(CanonicalJson.Serialize(originalSources))) return false;
        return InputFresh(request.PackageLock) && InputFresh(request.ReferenceManifest) && InputFresh(request.ApiSurfaceManifest) &&
            File.Exists(request.RuntimeBridge.DescriptorPath) && File.Exists(request.RuntimeBridge.AssemblyPath) &&
            CanonicalJson.HashFile(request.RuntimeBridge.DescriptorPath) == request.RuntimeBridge.DescriptorHash &&
            CanonicalJson.HashFile(request.RuntimeBridge.AssemblyPath) == request.RuntimeBridge.AssemblyHash;
    }

    private static bool InputFresh(WorkerVerifiedInput input) =>
        File.Exists(input.Path) && CanonicalJson.HashFile(input.Path) == input.Hash;

    private static void PublicationBarrier()
    {
        var barrier = Environment.GetEnvironmentVariable("SAGASCRIPT_PUBLICATION_BARRIER");
        if (string.IsNullOrWhiteSpace(barrier)) return;
        AtomicWrite(barrier + ".reached", Encoding.UTF8.GetBytes("reached\n"));
        var deadline = DateTime.UtcNow.AddSeconds(30);
        while (!File.Exists(barrier + ".release"))
        {
            if (DateTime.UtcNow >= deadline) throw new TimeoutException("SagaScript publication barrier timed out.");
            Thread.Sleep(10);
        }
    }

    private static StagedWorkerArtifact JsonArtifact(string id, string kind, string name, object payload)
    {
        var node = JsonSerializer.SerializeToNode(payload, CanonicalJson.SerializerOptions)!.AsObject();
        var contentHash = CanonicalJson.Hash(CanonicalJson.Serialize(node));
        node["contentHash"] = contentHash;
        var bytes = CanonicalJson.Serialize(node);
        return new StagedWorkerArtifact(id, kind, name, contentHash, CanonicalJson.Hash(bytes), bytes);
    }

    private static StagedWorkerArtifact BinaryArtifact(string id, string kind, string name, byte[] bytes)
    {
        var hash = CanonicalJson.Hash(bytes);
        return new StagedWorkerArtifact(id, kind, name, hash, hash, bytes);
    }

    private static ProjectWorkerResult Failure(ProjectWorkerRequest request, string failureClass,
        IEnumerable<WorkerDiagnostic> diagnostics, string sourceSetHash = "", bool cancelled = false,
        bool stale = false) => new()
    {
        OperationId = request.OperationId,
        SourceRevision = request.SourceRevision,
        Status = cancelled ? "Cancelled" : stale ? "StaleResult" : "Failed",
        FailureClass = failureClass,
        Cancelled = cancelled,
        Stale = stale,
        ProjectId = request.Project.ProjectId,
        ProjectHash = request.Project.Hash,
        SourceSetHash = sourceSetHash,
        PackageLockHash = request.PackageLock.Hash,
        ReferenceManifestHash = request.ReferenceManifest.Hash,
        ApiSurfaceManifestHash = request.ApiSurfaceManifest.Hash,
        RuntimeBridgeAssemblyHash = request.RuntimeBridge.AssemblyHash,
        Diagnostics = Sort(diagnostics),
    };

    private static int Finish(string path, ProjectWorkerResult result)
    {
        try { AtomicWrite(path, CanonicalJson.Serialize(result)); }
        catch (Exception exception) { Console.Error.WriteLine("sagascript worker result: " + exception.Message); return 2; }
        return result.Status == "Passed" ? 0 : 1;
    }

    private static void InitializeProgress(string path)
    {
        var full = Path.GetFullPath(path);
        Directory.CreateDirectory(Path.GetDirectoryName(full)!);
        File.WriteAllBytes(full, []);
    }

    private static void Progress(string path, ProjectWorkerRequest request, string stage)
    {
        var line = JsonSerializer.Serialize(new
        {
            schemaVersion = 1,
            operationId = request.OperationId,
            sourceRevision = request.SourceRevision,
            stage,
        }) + "\n";
        File.AppendAllText(path, line, new UTF8Encoding(false));
        var raw = Environment.GetEnvironmentVariable("SAGASCRIPT_WORKER_STAGE_DELAY_MS");
        if (int.TryParse(raw, out var delay) && delay > 0) Thread.Sleep(Math.Min(delay, 2000));
    }

    private static bool Cancelled(ProjectWorkerRequest request)
    {
        if (!File.Exists(request.Cancellation.Path)) return false;
        try
        {
            using var document = JsonDocument.Parse(File.ReadAllBytes(request.Cancellation.Path));
            return document.RootElement.GetProperty("operationId").GetString() == request.OperationId;
        }
        catch (Exception) { return false; }
    }

    private static string BuildStagingPath(string outputRoot, string operationId)
    {
        var token = new string(operationId.Where(char.IsLetterOrDigit).Take(24).ToArray());
        if (token.Length == 0) token = "operation";
        var full = Path.GetFullPath(outputRoot);
        var parent = Path.GetDirectoryName(full) ?? Directory.GetCurrentDirectory();
        return Path.Combine(parent, "." + Path.GetFileName(full) + ".sagascript-staging-" + token);
    }

    private static string ResolveDistributionRoot()
    {
        var cursor = new DirectoryInfo(Directory.GetCurrentDirectory());
        while (cursor is not null)
        {
            var candidate = Path.Combine(cursor.FullName, "Engine", "Distribution");
            if (Directory.Exists(candidate)) return Path.GetFullPath(candidate);
            cursor = cursor.Parent;
        }
        throw new InvalidOperationException("Trusted engine distribution root was not found.");
    }

    private static bool DerivesFromSagaBehavior(ClassDeclarationSyntax type) =>
        type.BaseList?.Types.Any(item => item.Type.ToString() is "SagaBehavior" or "SagaEngine.Scripting.SagaBehavior" ||
            item.Type.ToString().EndsWith(".SagaBehavior", StringComparison.Ordinal)) == true;

    private static string AttributeName(AttributeSyntax attribute)
    {
        var name = attribute.Name.ToString().Split('.').Last();
        return name.EndsWith("Attribute", StringComparison.Ordinal) ? name[..^"Attribute".Length] : name;
    }

    private static SourceSpan Span(string text, SyntaxTree tree, SyntaxNode node)
    {
        var line = tree.GetLineSpan(node.Span);
        var startByte = Encoding.UTF8.GetByteCount(text.AsSpan(0, node.Span.Start));
        var endByte = startByte + Encoding.UTF8.GetByteCount(text.AsSpan(node.Span.Start, node.Span.Length));
        return new SourceSpan(line.StartLinePosition.Line + 1, line.StartLinePosition.Character + 1,
            line.EndLinePosition.Line + 1, line.EndLinePosition.Character + 1, startByte, endByte);
    }

    private static string RelativeSource(string root, string path)
    {
        if (string.IsNullOrWhiteSpace(path)) return "";
        var relative = Path.GetRelativePath(Path.GetFullPath(root), Path.GetFullPath(path))
            .Replace(Path.DirectorySeparatorChar, '/');
        return relative.StartsWith("../", StringComparison.Ordinal) ? Path.GetFileName(path) : relative;
    }

    private static string ReadString(string path, string property)
    {
        using var document = JsonDocument.Parse(File.ReadAllBytes(path));
        return document.RootElement.GetProperty(property).GetString() ?? "";
    }

    private static string ReadBridgeLogicalPath(string referenceManifestPath, string logicalReferenceId)
    {
        using var document = JsonDocument.Parse(File.ReadAllBytes(referenceManifestPath));
        return document.RootElement.GetProperty("references").EnumerateArray()
            .Single(item => item.GetProperty("artifactId").GetString() == logicalReferenceId)
            .GetProperty("resolvedAssemblyLogicalPath").GetString() ?? "";
    }

    private static void RequireInput(WorkerVerifiedInput input, string name,
        List<WorkerDiagnostic> diagnostics)
    {
        Require(input.Path, name + ".path", diagnostics);
        RequireHash(input.Hash, name + ".hash", diagnostics);
    }

    private static void ValidateObject(JsonElement value, string path, IReadOnlyList<string> allowed,
        List<WorkerDiagnostic> diagnostics)
    {
        if (value.ValueKind != JsonValueKind.Object)
        {
            diagnostics.Add(Error("Script.Worker.ObjectExpected", path, "Worker request field must be an object."));
            return;
        }
        var names = new HashSet<string>(StringComparer.Ordinal);
        foreach (var property in value.EnumerateObject())
        {
            if (!names.Add(property.Name))
                diagnostics.Add(Error("Script.Worker.DuplicateField", path + "." + property.Name,
                    "Worker request fields must be unique."));
            if (!allowed.Contains(property.Name, StringComparer.Ordinal))
                diagnostics.Add(Error("Script.Worker.UnknownField", path + "." + property.Name,
                    "Unknown worker request field."));
        }
    }

    private static void RequireHash(string value, string path, List<WorkerDiagnostic> diagnostics)
    {
        if (value.Length != 71 || !value.StartsWith("sha256:", StringComparison.Ordinal) ||
            !value[7..].All(ch => ch is >= '0' and <= '9' or >= 'a' and <= 'f'))
            diagnostics.Add(Error("Script.Worker.HashInvalid", path, "Expected lowercase sha256:<64-hex> hash."));
    }

    private static void Require(string value, string path, List<WorkerDiagnostic> diagnostics)
    {
        if (string.IsNullOrWhiteSpace(value))
            diagnostics.Add(Error("Script.Worker.FieldMissing", path, "Required worker request field is missing."));
    }

    private static WorkerDiagnostic Error(string code, string path, string message) => new()
    {
        Code = code,
        Path = path.Replace('\\', '/'),
        Message = message,
    };

    private static WorkerDiagnostic[] Sort(IEnumerable<WorkerDiagnostic> diagnostics) => diagnostics
        .OrderByDescending(item => item.Severity == "Error")
        .ThenBy(item => item.Code, StringComparer.Ordinal)
        .ThenBy(item => item.Path, StringComparer.Ordinal)
        .ThenBy(item => item.Message, StringComparer.Ordinal).ToArray();

    private static void AtomicWrite(string path, byte[] bytes)
    {
        var full = Path.GetFullPath(path);
        Directory.CreateDirectory(Path.GetDirectoryName(full)!);
        var temporary = full + "." + Guid.NewGuid().ToString("N") + ".tmp";
        try
        {
            File.WriteAllBytes(temporary, bytes);
            File.Move(temporary, full, overwrite: true);
        }
        finally
        {
            if (File.Exists(temporary)) File.Delete(temporary);
        }
    }
}
