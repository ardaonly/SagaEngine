using System.Text.Json.Nodes;

namespace SagaPackager;

internal static class AssetWorkflowValidator
{
    private static readonly string[] AcceptedManifestNames =
    [
        "asset_manifest.json",
        "assets.source.json",
    ];

    public static AssetWorkflowReport Validate(string projectPath, string packageReportPath)
    {
        var resolved = new ProjectResolver().ResolveProject(projectPath);
        var diagnostics = resolved.Diagnostics.ToList();
        var checks = BuildAssetChecks(resolved, diagnostics);
        var packageEvidence = BuildPackageEvidence(packageReportPath, diagnostics);

        var hasAcceptedSourceTruth = checks.Any(check =>
            check.Exists &&
            (File.Exists(check.Path) || AcceptedManifestNames.Any(name => File.Exists(Path.Combine(check.Path, name)))));
        if (resolved.Project is not null && !hasAcceptedSourceTruth)
        {
            diagnostics.Add(ProjectResolver.Error(
                "Asset.Workflow.MissingSourceOfTruth",
                "Asset",
                resolved.ManifestPath,
                "No accepted asset manifest/source-truth file was found; generated package manifests are evidence only."));
        }

        diagnostics = ProjectResolver.Sort(diagnostics);
        return new AssetWorkflowReport
        {
            Status = ProjectResolver.HasErrors(diagnostics)
                ? diagnostics.Any(d => d.Code == "Asset.Workflow.MissingSourceOfTruth") &&
                    diagnostics.All(d => d.Severity != "Error" || d.Code == "Asset.Workflow.MissingSourceOfTruth")
                        ? "MissingSourceOfTruth"
                        : "Failed"
                : "Passed",
            Project = ProjectResolver.ProjectSummary(resolved),
            AssetReferences = checks,
            PackageEvidence = packageEvidence,
            Diagnostics = diagnostics,
        };
    }

    private static List<AssetReferenceCheck> BuildAssetChecks(
        ProjectResolution resolved,
        List<PackageDiagnostic> diagnostics)
    {
        var checks = new List<AssetReferenceCheck>();
        if (resolved.Project is null)
        {
            return checks;
        }

        if (resolved.Project.Assets.Count == 0)
        {
            diagnostics.Add(ProjectResolver.Error(
                "Asset.Workflow.AssetReferenceMissing",
                "Asset",
                resolved.ManifestPath,
                ".sagaproj does not declare any asset references."));
            return checks;
        }

        foreach (var duplicate in resolved.Project.Assets
                     .Where(asset => !string.IsNullOrWhiteSpace(asset.Id))
                     .GroupBy(asset => asset.Id, StringComparer.Ordinal)
                     .Where(group => group.Count() > 1)
                     .Select(group => group.Key)
                     .OrderBy(id => id, StringComparer.Ordinal))
        {
            diagnostics.Add(ProjectResolver.Error(
                "Asset.Workflow.DuplicateAssetId",
                "Asset",
                duplicate,
                "Asset reference ids must be unique within the project manifest."));
        }

        foreach (var asset in resolved.Project.Assets.OrderBy(asset => asset.Id, StringComparer.Ordinal))
        {
            if (string.IsNullOrWhiteSpace(asset.Id))
            {
                diagnostics.Add(ProjectResolver.Error(
                    "Asset.Workflow.MissingAssetId",
                    "Asset",
                    resolved.ManifestPath,
                    "Asset reference id is required."));
            }

            var pathValid = ProjectResolver.ValidateProjectRelativePath(
                asset.Path,
                $"assets[{asset.Id}].path",
                diagnostics);
            var fullPath = pathValid ? Path.GetFullPath(Path.Combine(resolved.ProjectRoot, asset.Path)) : "";
            var exists = fullPath.Length > 0 && (File.Exists(fullPath) || Directory.Exists(fullPath));
            if (pathValid && !exists)
            {
                diagnostics.Add(ProjectResolver.Error(
                    "Asset.Workflow.AssetRootMissing",
                    "Asset",
                    fullPath,
                    "Declared asset root does not exist."));
            }

            var placeholderStatus = PlaceholderStatus(fullPath);
            if (placeholderStatus is "PlaceholderOnly" or "Empty")
            {
                diagnostics.Add(ProjectResolver.Warning(
                    "Asset.Workflow.PlaceholderOnly",
                    "Asset",
                    fullPath,
                    "Asset root contains no accepted source assets; placeholder content is reportable but not asset source truth."));
            }

            checks.Add(new AssetReferenceCheck
            {
                Id = asset.Id,
                Kind = asset.Kind,
                Path = ProjectResolver.Normalize(fullPath),
                Exists = exists,
                PlaceholderStatus = placeholderStatus,
            });
        }

        return checks;
    }

    private static PackageAssetEvidence BuildPackageEvidence(
        string packageReportPath,
        List<PackageDiagnostic> diagnostics)
    {
        if (string.IsNullOrWhiteSpace(packageReportPath))
        {
            return new PackageAssetEvidence();
        }

        var fullReportPath = Path.GetFullPath(packageReportPath);
        if (!File.Exists(fullReportPath))
        {
            diagnostics.Add(ProjectResolver.Error(
                "Asset.PackageReport.Missing",
                "Asset",
                fullReportPath,
                "Supplied package stage report does not exist."));
            return new PackageAssetEvidence
            {
                PackageReportPath = ProjectResolver.Normalize(fullReportPath),
                StageStatus = "Missing",
            };
        }

        try
        {
            var report = JsonNode.Parse(File.ReadAllText(fullReportPath)) as JsonObject
                ?? throw new InvalidOperationException("Package report must be a JSON object.");
            var stageStatus = ReadString(report, "status");
            var packageManifestPath = ReadString(report, "packageManifestPath");
            var evidence = new PackageAssetEvidence
            {
                PackageReportPath = ProjectResolver.Normalize(fullReportPath),
                StageStatus = stageStatus,
            };

            if (string.IsNullOrWhiteSpace(packageManifestPath) || !File.Exists(packageManifestPath))
            {
                diagnostics.Add(ProjectResolver.Error(
                    "Asset.PackageManifest.Missing",
                    "Asset",
                    packageManifestPath,
                    "Package stage report does not point to an existing package manifest."));
                return evidence;
            }

            var packageManifest = JsonNode.Parse(File.ReadAllText(packageManifestPath)) as JsonObject
                ?? throw new InvalidOperationException("Package manifest must be a JSON object.");
            var packageRoot = Path.GetDirectoryName(Path.GetFullPath(packageManifestPath)) ?? ".";
            var assetManifestPath = ResolvePackageRelativePath(
                packageRoot,
                ReadFirstAssetManifestPath(packageManifest));
            var identityManifestPath = ResolvePackageRelativePath(
                packageRoot,
                ReadString(packageManifest, "assetIdentityManifest"));
            var assetManifestExists = File.Exists(assetManifestPath);
            var identityManifestExists = File.Exists(identityManifestPath);
            if (!assetManifestExists)
            {
                diagnostics.Add(ProjectResolver.Error(
                    "Asset.PackageAssetManifest.Missing",
                    "Asset",
                    assetManifestPath,
                    "Generated package asset manifest referenced by the package manifest is missing."));
            }
            if (!identityManifestExists)
            {
                diagnostics.Add(ProjectResolver.Error(
                    "Asset.PackageIdentityManifest.Missing",
                    "Asset",
                    identityManifestPath,
                    "Generated package asset identity manifest referenced by the package manifest is missing."));
            }

            return new PackageAssetEvidence
            {
                PackageReportPath = evidence.PackageReportPath,
                StageStatus = evidence.StageStatus,
                AssetManifestPath = ProjectResolver.Normalize(assetManifestPath),
                AssetManifestExists = assetManifestExists,
                AssetIdentityManifestPath = ProjectResolver.Normalize(identityManifestPath),
                AssetIdentityManifestExists = identityManifestExists,
            };
        }
        catch (Exception e) when (e is IOException or InvalidOperationException or System.Text.Json.JsonException)
        {
            diagnostics.Add(ProjectResolver.Error(
                "Asset.PackageEvidence.InvalidJson",
                "Asset",
                fullReportPath,
                $"Unable to read package asset evidence: {e.Message}"));
            return new PackageAssetEvidence
            {
                PackageReportPath = ProjectResolver.Normalize(fullReportPath),
                StageStatus = "Invalid",
            };
        }
    }

    private static string PlaceholderStatus(string path)
    {
        if (string.IsNullOrWhiteSpace(path) || !Directory.Exists(path))
        {
            return "";
        }

        var files = Directory.EnumerateFiles(path, "*", SearchOption.AllDirectories)
            .Select(file => ProjectResolver.Normalize(Path.GetRelativePath(path, file)))
            .OrderBy(file => file, StringComparer.Ordinal)
            .ToArray();
        if (files.Length == 0)
        {
            return "Empty";
        }
        return files.All(file => string.Equals(file, "README.md", StringComparison.OrdinalIgnoreCase))
            ? "PlaceholderOnly"
            : "HasSourceFiles";
    }

    private static string ResolvePackageRelativePath(string packageRoot, string value)
    {
        if (string.IsNullOrWhiteSpace(value))
        {
            return "";
        }
        return Path.IsPathRooted(value)
            ? Path.GetFullPath(value)
            : Path.GetFullPath(Path.Combine(packageRoot, value));
    }

    private static string ReadFirstAssetManifestPath(JsonObject packageManifest)
    {
        var assetManifests = packageManifest["assetManifests"] as JsonArray;
        var first = assetManifests?.OfType<JsonObject>().FirstOrDefault();
        return ReadString(first, "path");
    }

    private static string ReadString(JsonObject? obj, string key)
    {
        return obj is not null &&
            obj.TryGetPropertyValue(key, out var node) &&
            node is JsonValue value &&
            value.TryGetValue<string>(out var text)
                ? text
                : "";
    }
}
