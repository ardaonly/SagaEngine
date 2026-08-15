// SPDX-License-Identifier: MPL-2.0

using System.Text.Json;
using SagaProjectKit.Core;

namespace SagaScript;

internal static class ApiSurfaceAggregation
{
    public static IReadOnlyList<object> Load(
        ProjectWorkerRequest request,
        string distributionRoot,
        List<WorkerDiagnostic> diagnostics)
    {
        var allowed = new List<object>();
        try
        {
            using var lockDocument = JsonDocument.Parse(
                File.ReadAllBytes(request.PackageLock.Path));
            var prefix = distributionRoot.TrimEnd(Path.DirectorySeparatorChar) +
                Path.DirectorySeparatorChar;
            var identities = new Dictionary<string, string>(StringComparer.Ordinal);
            var operationIdentities = new HashSet<string>(StringComparer.Ordinal);
            var lockedManagedReferences = lockDocument.RootElement.GetProperty("packages")
                .EnumerateArray().SelectMany(package =>
                    package.GetProperty("artifacts").EnumerateArray())
                .Where(artifact =>
                    artifact.GetProperty("kind").GetString() == "managedReference")
                .Select(artifact =>
                    artifact.GetProperty("artifactId").GetString() ?? "")
                .ToHashSet(StringComparer.Ordinal);
            var packages = lockDocument.RootElement.GetProperty("packages").EnumerateArray()
                .OrderBy(item => item.GetProperty("packageId").GetString(), StringComparer.Ordinal)
                .ThenBy(item => item.GetProperty("version").GetString(), StringComparer.Ordinal);
            foreach (var package in packages)
            {
                var packageId = package.GetProperty("packageId").GetString() ?? "";
                var packageVersion = package.GetProperty("version").GetString() ?? "";
                var artifacts = package.GetProperty("artifacts").EnumerateArray()
                    .Where(item => item.GetProperty("kind").GetString() == "apiSurface")
                    .OrderBy(item => item.GetProperty("artifactId").GetString(), StringComparer.Ordinal);
                foreach (var artifact in artifacts)
                {
                    var artifactId = artifact.GetProperty("artifactId").GetString() ?? "";
                    var relative = artifact.GetProperty("distributionPath").GetString() ?? "";
                    var surfacePath = Path.GetFullPath(Path.Combine(distributionRoot, relative));
                    if (!surfacePath.StartsWith(prefix, StringComparison.Ordinal) ||
                        !File.Exists(surfacePath) ||
                        CanonicalJson.HashFile(surfacePath) !=
                            artifact.GetProperty("artifactHash").GetString())
                    {
                        diagnostics.Add(Error("Script.Worker.ApiSurfaceTrustFailed", relative,
                            "API surface is missing or changed outside the verified engine distribution."));
                        continue;
                    }

                    using var surface = JsonDocument.Parse(File.ReadAllBytes(surfacePath));
                    var root = surface.RootElement;
                    if (root.GetProperty("schemaVersion").GetInt32() != 1)
                        throw new InvalidOperationException("API surface schema is unsupported.");
                    var module = root.GetProperty("module").GetString() ?? "";
                    var surfaceId = root.GetProperty("surfaceId").GetString() ?? "";
                    var readiness = root.TryGetProperty(
                        "runtimeReadiness", out var readinessElement)
                        ? readinessElement.GetString() ?? ""
                        : "RuntimeBacked";
                    if (string.IsNullOrWhiteSpace(module) ||
                        string.IsNullOrWhiteSpace(surfaceId) ||
                        readiness is not ("RuntimeBacked" or "ContractOnly"))
                        throw new InvalidOperationException(
                            "API surface identity or runtime readiness is invalid.");
                    if (readiness == "RuntimeBacked")
                    {
                        var implementation =
                            root.GetProperty("runtimeImplementation");
                        var owner = implementation.GetProperty(
                            "managedReferenceArtifactId").GetString() ?? "";
                        var implementationType = implementation.GetProperty(
                            "implementationType").GetString() ?? "";
                        var expectedType = module switch
                        {
                            "challenge.core.highlevel" =>
                                "SagaEngine.Scripting.Gameplay.ChallengeHighLevel",
                            "collectibles.highlevel" =>
                                "SagaEngine.Scripting.Gameplay.CollectiblesHighLevel",
                            "collectibles.instrumentation" =>
                                "SagaEngine.Scripting.Gameplay.DevelopmentDiagnostics.CollectibleInstrumentation",
                            _ => "",
                        };
                        if (!lockedManagedReferences.Contains(owner) ||
                            owner !=
                                "com.saga.gameplay.foundation.managed-reference" ||
                            implementationType != expectedType)
                            throw new InvalidOperationException(
                                "RuntimeBacked API surface lacks its locked allowlisted implementation owner.");
                    }
                    AppendApis(
                        root, packageId, packageVersion, artifactId, module,
                        surfaceId, readiness, identities, operationIdentities,
                        allowed, diagnostics);
                }
            }
        }
        catch (Exception exception) when (
            exception is JsonException or InvalidOperationException or KeyNotFoundException)
        {
            diagnostics.Add(Error(
                "Script.Worker.ApiSurfaceInvalid", "apiSurface", exception.Message));
        }
        return allowed;
    }

    private static void AppendApis(
        JsonElement root,
        string packageId,
        string packageVersion,
        string artifactId,
        string module,
        string surfaceId,
        string readiness,
        Dictionary<string, string> identities,
        HashSet<string> operationIdentities,
        List<object> allowed,
        List<WorkerDiagnostic> diagnostics)
    {
        foreach (var api in root.GetProperty("apis").EnumerateArray()
                     .OrderBy(item => item.GetProperty("apiId").GetString(), StringComparer.Ordinal))
        {
            var apiId = api.GetProperty("apiId").GetString() ?? "";
            var visibility = api.GetProperty("visibility").GetString() ?? "";
            var capability = api.GetProperty("capability").GetString() ?? "";
            var projection = api.GetProperty("blockProjection").GetString() ?? "";
            var sourcePath = api.TryGetProperty("sourcePath", out var source)
                ? source.GetString() ?? ""
                : "";
            var hasOperationId = api.TryGetProperty("operationId", out var operation);
            var operationId = hasOperationId
                ? operation.GetString() ?? ""
                : "";
            var apiLevel = api.TryGetProperty("apiLevel", out var level)
                ? level.GetString() ?? ""
                : "";
            var effect = api.TryGetProperty("effect", out var effectElement)
                ? effectElement.GetString() ?? ""
                : "";
            var runtimeBinding = api.TryGetProperty("runtimeBinding", out var binding)
                ? binding.GetString() ?? ""
                : "";
            var requiredCapabilities = api.TryGetProperty(
                    "requiredCapabilities", out var required)
                ? required.EnumerateArray().Select(item => item.GetString() ?? "")
                    .ToArray()
                : Array.Empty<string>();
            if (visibility != "Public" || capability is "Raw" or "Internal" ||
                projection == "Filtered") continue;
            if (hasOperationId &&
                (string.IsNullOrWhiteSpace(operationId) ||
                 operationId.Any(char.IsWhiteSpace) ||
                 apiLevel is not ("High" or "Low") ||
                 effect is not ("ReadAuthoritativeState" or "WriteAuthoritativeState") ||
                 string.IsNullOrWhiteSpace(runtimeBinding) ||
                 requiredCapabilities.Length == 0 ||
                 requiredCapabilities.Any(string.IsNullOrWhiteSpace) ||
                 requiredCapabilities.Distinct(StringComparer.Ordinal).Count() !=
                    requiredCapabilities.Length))
                throw new InvalidOperationException(
                    $"Semantic operation descriptor '{operationId}' is invalid.");
            if (hasOperationId && !operationIdentities.Add(operationId))
            {
                diagnostics.Add(Error("Script.Worker.OperationIdentityDuplicate", operationId,
                    "Semantic operation identity must be globally unique in the verified package graph."));
                continue;
            }
            var identity = $"{surfaceId}/{apiId}";
            var valueIdentity =
                $"{packageId}\n{packageVersion}\n{artifactId}\n{module}\n{readiness}\n{capability}\n{projection}\n{sourcePath}\n{operationId}\n{apiLevel}\n{effect}\n{runtimeBinding}\n{string.Join(',', requiredCapabilities)}";
            if (identities.TryGetValue(identity, out var existing))
            {
                if (existing != valueIdentity)
                    diagnostics.Add(Error("Script.Worker.ApiIdentityConflict", identity,
                        "Duplicate API identity has conflicting descriptors."));
                continue;
            }
            identities.Add(identity, valueIdentity);
            allowed.Add(new
            {
                packageId,
                packageVersion,
                artifactId,
                module,
                surfaceId,
                apiId,
                runtimeReadiness = readiness,
                capability,
                blockProjection = projection,
                sourcePath,
                operationId,
                apiLevel,
                effect,
                requiredCapabilities = requiredCapabilities
                    .Order(StringComparer.Ordinal).ToArray(),
                runtimeBinding,
            });
        }
    }

    private static WorkerDiagnostic Error(string code, string path, string message) => new()
    {
        Code = code,
        Path = path,
        Message = message,
    };
}
