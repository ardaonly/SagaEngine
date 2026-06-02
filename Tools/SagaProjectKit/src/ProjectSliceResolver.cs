using System.Text.Json;

namespace SagaProjectKit;

internal sealed class ProjectSliceResolutionResult
{
    public ProjectSliceSummary Slice { get; init; } = new();
    public List<ProjectDiagnostic> Diagnostics { get; init; } = [];
    public List<SliceResolvedResource> VisibleArtifacts { get; init; } = [];
    public List<SliceResolvedResource> RestrictedArtifacts { get; init; } = [];
    public List<SliceResolvedResource> MissingArtifacts { get; init; } = [];
    public List<SliceResolvedResource> ExcludedArtifacts { get; init; } = [];
    public List<SliceResolvedResource> ResourceVisibility { get; init; } = [];
    public bool Passed => Diagnostics.All(d => d.Severity != "Error");
}

internal sealed class ProjectSliceResolver
{
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNameCaseInsensitive = true,
    };

    private static readonly HashSet<string> ResourceKinds = new(StringComparer.Ordinal)
    {
        "Project",
        "ScriptFolder",
        "ScriptFile",
        "Behavior",
        "Node",
        "AssetRoot",
        "AssetFile",
        "PackageProfile",
        "LaunchProfile",
        "ReportArtifact",
        "CollaborationArtifact",
    };

    private static readonly HashSet<string> VisibilityLevels = new(StringComparer.Ordinal)
    {
        "FullSource",
        "SourceLinkOnly",
        "SummaryOnly",
        "OpaqueRestricted",
        "Hidden",
    };

    public ProjectSliceResolutionResult Resolve(ManifestLoadResult project, string slicePath)
    {
        var diagnostics = new List<ProjectDiagnostic>();
        var slice = LoadSlice(slicePath, diagnostics);
        if (slice is null)
        {
            return new ProjectSliceResolutionResult
            {
                Slice = new ProjectSliceSummary { SlicePath = NormalizeAbsolute(slicePath) },
                Diagnostics = SortDiagnostics(project.Diagnostics.Concat(diagnostics)),
            };
        }

        ValidateSlice(slice, slicePath, project.ProjectRoot, diagnostics);
        var summary = new ProjectSliceSummary
        {
            SliceId = slice.SliceId,
            DisplayName = slice.DisplayName,
            IntendedRole = slice.IntendedRole,
            SlicePath = NormalizeAbsolute(slicePath),
            SchemaVersion = slice.SchemaVersion,
        };

        var visibilityRules = slice.VisibilityRules
            .Where(rule => !string.IsNullOrWhiteSpace(rule.Visibility))
            .GroupBy(TargetKey, StringComparer.Ordinal)
            .ToDictionary(group => group.Key, group => group.Last().Visibility, StringComparer.Ordinal);

        var included = slice.IncludedResources
            .Select(resource => ResolveResource(project, resource, visibilityRules, "Included", slicePath, diagnostics))
            .ToList();
        var excluded = slice.ExcludedResources
            .Select(resource => ResolveResource(project, resource, visibilityRules, "Excluded", slicePath, diagnostics))
            .ToList();

        var allResources = included.Concat(excluded)
            .OrderBy(resource => resource.Kind, StringComparer.Ordinal)
            .ThenBy(resource => resource.Id, StringComparer.Ordinal)
            .ThenBy(resource => resource.RelativePath, StringComparer.Ordinal)
            .ThenBy(resource => resource.Status, StringComparer.Ordinal)
            .ToList();

        return new ProjectSliceResolutionResult
        {
            Slice = summary,
            Diagnostics = SortDiagnostics(project.Diagnostics.Concat(diagnostics)),
            VisibleArtifacts = allResources
                .Where(resource => resource.Status == "Included")
                .ToList(),
            RestrictedArtifacts = allResources
                .Where(resource => resource.Status == "Restricted")
                .ToList(),
            MissingArtifacts = allResources
                .Where(resource => resource.Status == "Missing")
                .ToList(),
            ExcludedArtifacts = allResources
                .Where(resource => resource.Status == "Excluded")
                .ToList(),
            ResourceVisibility = allResources,
        };
    }

    private static ProjectSliceDefinition? LoadSlice(
        string slicePath,
        List<ProjectDiagnostic> diagnostics)
    {
        if (string.IsNullOrWhiteSpace(slicePath))
        {
            diagnostics.Add(Error(
                "ProjectSlice.Input.Missing",
                "Slice",
                "",
                "A slice path is required."));
            return null;
        }
        if (!File.Exists(slicePath))
        {
            diagnostics.Add(Error(
                "ProjectSlice.Input.Missing",
                "Slice",
                slicePath,
                "Project slice file does not exist."));
            return null;
        }

        try
        {
            using var input = File.OpenRead(slicePath);
            using var document = JsonDocument.Parse(input);
            if (document.RootElement.ValueKind != JsonValueKind.Object)
            {
                diagnostics.Add(Error(
                    "ProjectSlice.InvalidRoot",
                    "Slice",
                    slicePath,
                    "Project slice root must be a JSON object."));
                return null;
            }

            return document.RootElement.Deserialize<ProjectSliceDefinition>(JsonOptions);
        }
        catch (JsonException e)
        {
            diagnostics.Add(Error(
                "ProjectSlice.ParseFailed",
                "Slice",
                slicePath,
                $"Project slice JSON is invalid: {e.Message}"));
            return null;
        }
    }

    private static void ValidateSlice(
        ProjectSliceDefinition slice,
        string slicePath,
        string projectRoot,
        List<ProjectDiagnostic> diagnostics)
    {
        if (slice.SchemaVersion != 0)
        {
            diagnostics.Add(Error(
                "ProjectSlice.SchemaVersionUnsupported",
                "Slice",
                "schemaVersion",
                "Only project slice schemaVersion 0 is supported."));
        }
        RequireText(slice.SliceId, "sliceId", slicePath, diagnostics);
        RequireText(slice.DisplayName, "displayName", slicePath, diagnostics);
        RequireText(slice.IntendedRole, "intendedRole", slicePath, diagnostics);

        ValidateResources(slice.IncludedResources, "includedResources", projectRoot, diagnostics);
        ValidateResources(slice.ExcludedResources, "excludedResources", projectRoot, diagnostics);
        ValidateResources(slice.VisibilityRules, "visibilityRules", projectRoot, diagnostics);

        foreach (var duplicate in slice.IncludedResources
                     .Concat(slice.ExcludedResources)
                     .GroupBy(TargetKey, StringComparer.Ordinal)
                     .Where(group => group.Key.Length > 0 && group.Count() > 1)
                     .Select(group => group.Key)
                     .OrderBy(key => key, StringComparer.Ordinal))
        {
            diagnostics.Add(Error(
                "ProjectSlice.Resource.Duplicate",
                "Slice",
                duplicate,
                "Project slice resource target is duplicated."));
        }
    }

    private static void ValidateResources(
        IEnumerable<ProjectSliceResource> resources,
        string field,
        string projectRoot,
        List<ProjectDiagnostic> diagnostics)
    {
        var index = 0;
        foreach (var resource in resources)
        {
            var diagnosticPath = $"{field}[{index}]";
            if (!ResourceKinds.Contains(resource.Kind))
            {
                diagnostics.Add(Error(
                    "ProjectSlice.Resource.UnknownKind",
                    "Slice",
                    diagnosticPath,
                    $"Unknown project slice resource kind: {resource.Kind}."));
            }
            if (string.IsNullOrWhiteSpace(resource.Id) && string.IsNullOrWhiteSpace(resource.Path))
            {
                diagnostics.Add(Error(
                    "ProjectSlice.Resource.MissingTarget",
                    "Slice",
                    diagnosticPath,
                    "Project slice resource must provide id, path, or both."));
            }
            if (!string.IsNullOrWhiteSpace(resource.Path))
            {
                ValidateProjectRelativePath(resource.Path, projectRoot, diagnosticPath, diagnostics);
            }
            if (!string.IsNullOrWhiteSpace(resource.Visibility) && !VisibilityLevels.Contains(resource.Visibility))
            {
                diagnostics.Add(Error(
                    "ProjectSlice.Visibility.UnknownLevel",
                    "Slice",
                    diagnosticPath,
                    $"Unknown source visibility level: {resource.Visibility}."));
            }
            ++index;
        }
    }

    private static SliceResolvedResource ResolveResource(
        ManifestLoadResult project,
        ProjectSliceResource resource,
        IReadOnlyDictionary<string, string> visibilityRules,
        string requestedStatus,
        string slicePath,
        List<ProjectDiagnostic> diagnostics)
    {
        var visibility = ResolveVisibility(resource, visibilityRules, requestedStatus);
        var status = requestedStatus == "Excluded"
            ? "Excluded"
            : visibility is "OpaqueRestricted" or "Hidden" ? "Restricted" : "Included";
        var relativePath = NormalizeRelative(resource.Path);
        var exists = ResourceExists(project, resource, relativePath);
        var invalid = false;

        if (!ResourceKinds.Contains(resource.Kind))
        {
            status = "Unknown";
            invalid = true;
        }
        if (string.IsNullOrWhiteSpace(resource.Id) && string.IsNullOrWhiteSpace(resource.Path))
        {
            status = "Invalid";
            invalid = true;
        }
        if (!string.IsNullOrWhiteSpace(resource.Path) &&
            !ValidateProjectRelativePath(resource.Path, project.ProjectRoot, TargetKey(resource), diagnostics))
        {
            status = "Invalid";
            invalid = true;
        }
        if (!VisibilityLevels.Contains(visibility))
        {
            visibility = "Hidden";
            status = "Invalid";
            invalid = true;
        }
        if (!invalid && requestedStatus != "Excluded" && !exists)
        {
            status = "Missing";
            diagnostics.Add(Error(
                "ProjectSlice.Resource.Missing",
                "Slice",
                TargetKey(resource),
                "Project slice resource target could not be resolved."));
        }

        var hidesPath = visibility is "OpaqueRestricted" or "Hidden";
        return new SliceResolvedResource
        {
            Kind = resource.Kind,
            Id = resource.Id,
            RelativePath = hidesPath ? "" : relativePath,
            Status = status,
            Visibility = visibility,
            Placeholder = status is "Restricted" or "Excluded" ? "Visibility is limited by local slice policy." : "",
            Exists = exists,
        };
    }

    private static bool ResourceExists(ManifestLoadResult project, ProjectSliceResource resource, string relativePath)
    {
        if (resource.Kind == "Project")
        {
            return string.IsNullOrWhiteSpace(resource.Id) ||
                   string.Equals(project.Descriptor?.ProjectId, resource.Id, StringComparison.Ordinal);
        }

        if (!string.IsNullOrWhiteSpace(relativePath))
        {
            var fullPath = Path.GetFullPath(Path.Combine(project.ProjectRoot, relativePath));
            return File.Exists(fullPath) || Directory.Exists(fullPath);
        }

        var catalogKind = resource.Kind switch
        {
            "ScriptFolder" => "scriptFolder",
            "AssetRoot" => "asset",
            "LaunchProfile" => "launchProfile",
            "PackageProfile" => "packageProfile",
            "ReportArtifact" => "generatedReports",
            _ => "",
        };
        return catalogKind.Length > 0 &&
               project.ResolvedPaths.Any(path =>
                   path.Category == catalogKind &&
                   string.Equals(path.Id, resource.Id, StringComparison.Ordinal) &&
                   path.Exists);
    }

    private static string ResolveVisibility(
        ProjectSliceResource resource,
        IReadOnlyDictionary<string, string> visibilityRules,
        string requestedStatus)
    {
        if (!string.IsNullOrWhiteSpace(resource.Visibility))
        {
            return resource.Visibility;
        }
        return visibilityRules.TryGetValue(TargetKey(resource), out var visibility)
            ? visibility
            : requestedStatus == "Excluded" ? "Hidden" : "FullSource";
    }

    private static void RequireText(
        string value,
        string field,
        string slicePath,
        List<ProjectDiagnostic> diagnostics)
    {
        if (!string.IsNullOrWhiteSpace(value))
        {
            return;
        }
        diagnostics.Add(Error(
            "ProjectSlice.MissingField",
            "Slice",
            slicePath,
            $"Required project slice field is missing: {field}."));
    }

    private static bool ValidateProjectRelativePath(
        string value,
        string projectRoot,
        string diagnosticPath,
        List<ProjectDiagnostic> diagnostics)
    {
        var normalized = NormalizeRelative(value);
        if (Path.IsPathRooted(value) ||
            value.StartsWith("/", StringComparison.Ordinal) ||
            value.StartsWith("\\", StringComparison.Ordinal) ||
            (value.Length >= 2 && char.IsLetter(value[0]) && value[1] == ':'))
        {
            diagnostics.Add(Error(
                "ProjectSlice.Path.AbsoluteNotAllowed",
                "Path",
                diagnosticPath,
                "Project slice paths must be project-relative."));
            return false;
        }

        var segments = normalized.Split('/', StringSplitOptions.RemoveEmptyEntries);
        if (segments.Any(segment => segment == ".."))
        {
            diagnostics.Add(Error(
                "ProjectSlice.Path.ParentTraversalNotAllowed",
                "Path",
                diagnosticPath,
                "Project slice paths must not contain parent traversal."));
            return false;
        }

        var fullPath = Path.GetFullPath(Path.Combine(projectRoot, normalized));
        var relative = Path.GetRelativePath(projectRoot, fullPath);
        if (relative.StartsWith("..", StringComparison.Ordinal) || Path.IsPathRooted(relative))
        {
            diagnostics.Add(Error(
                "ProjectSlice.Path.ParentTraversalNotAllowed",
                "Path",
                diagnosticPath,
                "Project slice paths must remain under the project root."));
            return false;
        }

        return true;
    }

    private static string TargetKey(ProjectSliceResource resource)
    {
        return $"{resource.Kind}|{resource.Id}|{NormalizeRelative(resource.Path)}";
    }

    private static ProjectDiagnostic Error(string code, string category, string path, string message)
    {
        return new ProjectDiagnostic
        {
            Severity = "Error",
            Code = code,
            Category = category,
            Path = NormalizeRelative(path),
            Message = message,
        };
    }

    private static List<ProjectDiagnostic> SortDiagnostics(IEnumerable<ProjectDiagnostic> diagnostics)
    {
        return diagnostics
            .OrderByDescending(d => d.Severity == "Error")
            .ThenBy(d => d.Code, StringComparer.Ordinal)
            .ThenBy(d => d.Path, StringComparer.Ordinal)
            .ThenBy(d => d.Message, StringComparer.Ordinal)
            .ToList();
    }

    private static string NormalizeRelative(string path)
    {
        return path.Replace('\\', '/').Trim();
    }

    private static string NormalizeAbsolute(string path)
    {
        return Path.GetFullPath(path).Replace('\\', '/');
    }
}
