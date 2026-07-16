using System.Text.Json;

namespace SagaProjectKit;

internal sealed class ProjectManifestLoader
{
    public ManifestLoadResult Load(string projectInput, bool checkReferences)
    {
        var diagnostics = new List<ProjectDiagnostic>();
        var manifestPath = ResolveManifestPath(projectInput, diagnostics);
        if (manifestPath.Length == 0)
        {
            return new ManifestLoadResult { Diagnostics = SortDiagnostics(diagnostics) };
        }

        var projectRoot = Path.GetFullPath(Path.GetDirectoryName(manifestPath) ?? ".");
        if (!File.Exists(manifestPath))
        {
            diagnostics.Add(Error(
                "Project.Manifest.Missing",
                "Manifest",
                manifestPath,
                "Project manifest is missing."));
            return new ManifestLoadResult
            {
                ProjectRoot = NormalizeAbsolute(projectRoot),
                ManifestPath = NormalizeAbsolute(manifestPath),
                Diagnostics = SortDiagnostics(diagnostics),
            };
        }

        JsonDocument document;
        try
        {
            using var input = File.OpenRead(manifestPath);
            document = JsonDocument.Parse(input);
        }
        catch (JsonException e)
        {
            diagnostics.Add(Error(
                "Project.Manifest.ParseFailed",
                "Manifest",
                manifestPath,
                $"Project manifest JSON is invalid: {e.Message}"));
            return new ManifestLoadResult
            {
                ProjectRoot = NormalizeAbsolute(projectRoot),
                ManifestPath = NormalizeAbsolute(manifestPath),
                Diagnostics = SortDiagnostics(diagnostics),
            };
        }

        using (document)
        {
            if (document.RootElement.ValueKind != JsonValueKind.Object)
            {
                diagnostics.Add(Error(
                    "Project.Manifest.InvalidField",
                    "Manifest",
                    manifestPath,
                    "Project manifest root must be a JSON object."));
                return FailedResult(projectRoot, manifestPath, diagnostics);
            }

            var root = document.RootElement;
            var schemaVersion = ReadRequiredInt(root, "schemaVersion", manifestPath, diagnostics);
            if (schemaVersion.HasValue && schemaVersion.Value != 0)
            {
                diagnostics.Add(Error(
                    "Project.Manifest.SchemaVersionUnsupported",
                    "Manifest",
                    "schemaVersion",
                    "Only .sagaproj schemaVersion 0 is supported."));
            }

            var descriptor = new SagaProjectDescriptor
            {
                SchemaVersion = schemaVersion ?? 0,
                ProjectId = ReadRequiredString(root, "projectId", manifestPath, diagnostics),
                DisplayName = ReadRequiredString(root, "displayName", manifestPath, diagnostics),
                EngineCompatibility = ReadEngineCompatibility(root, manifestPath, diagnostics),
                Paths = ReadProjectPaths(root, projectRoot, manifestPath, diagnostics),
                Scenes = ReadReferences(root, "scenes", false, projectRoot, manifestPath, diagnostics),
                Assets = ReadReferences(root, "assets", true, projectRoot, manifestPath, diagnostics),
                ScriptFolders = ReadReferences(root, "scriptFolders", false, projectRoot, manifestPath, diagnostics),
                LaunchProfiles = ReadReferences(root, "launchProfiles", false, projectRoot, manifestPath, diagnostics),
                PackageProfiles = ReadReferences(root, "packageProfiles", false, projectRoot, manifestPath, diagnostics),
            };

            var resolved = BuildResolvedPaths(projectRoot, descriptor);
            if (checkReferences)
            {
                CheckResolvedReferences(resolved, diagnostics);
            }

            return new ManifestLoadResult
            {
                ProjectRoot = NormalizeAbsolute(projectRoot),
                ManifestPath = NormalizeAbsolute(manifestPath),
                Descriptor = descriptor,
                Diagnostics = SortDiagnostics(diagnostics),
                ResolvedPaths = resolved,
            };
        }
    }

    public List<ProjectDiagnostic> CheckDoctorProfileJson(ManifestLoadResult load)
    {
        var diagnostics = new List<ProjectDiagnostic>();
        foreach (var resolved in load.ResolvedPaths.Where(p =>
                     p.Category is "launchProfile" or "packageProfile"))
        {
            if (!File.Exists(resolved.AbsolutePath))
            {
                continue;
            }

            try
            {
                using var input = File.OpenRead(resolved.AbsolutePath);
                using var _ = JsonDocument.Parse(input);
            }
            catch (JsonException e)
            {
                diagnostics.Add(Error(
                    "Project.Profile.ParseFailed",
                    "Profile",
                    resolved.RelativePath,
                    $"Profile JSON is invalid: {e.Message}"));
            }
        }

        return SortDiagnostics(diagnostics);
    }

    private static ManifestLoadResult FailedResult(
        string projectRoot,
        string manifestPath,
        List<ProjectDiagnostic> diagnostics)
    {
        return new ManifestLoadResult
        {
            ProjectRoot = NormalizeAbsolute(projectRoot),
            ManifestPath = NormalizeAbsolute(manifestPath),
            Diagnostics = SortDiagnostics(diagnostics),
        };
    }

    private static string ResolveManifestPath(
        string projectInput,
        List<ProjectDiagnostic> diagnostics)
    {
        if (string.IsNullOrWhiteSpace(projectInput))
        {
            diagnostics.Add(Error(
                "Project.Input.Missing",
                "Input",
                "",
                "A project path is required."));
            return "";
        }

        var fullInput = Path.GetFullPath(projectInput);
        if (File.Exists(fullInput))
        {
            if (!string.Equals(Path.GetExtension(fullInput), ".sagaproj",
                               StringComparison.OrdinalIgnoreCase))
            {
                diagnostics.Add(Error(
                    "Project.Input.UnsupportedFileType",
                    "Input",
                    fullInput,
                    "Explicit project input must be a .sagaproj file."));
                return "";
            }
            return fullInput;
        }

        if (!Directory.Exists(fullInput))
        {
            diagnostics.Add(Error(
                "Project.Input.Missing",
                "Input",
                projectInput,
                "Project path does not exist."));
            return "";
        }

        var manifests = Directory.GetFiles(fullInput, "*.sagaproj", SearchOption.TopDirectoryOnly)
            .OrderBy(NormalizeAbsolute, StringComparer.Ordinal)
            .ToList();
        if (manifests.Count == 1)
        {
            return manifests[0];
        }

        diagnostics.Add(Error(
            manifests.Count == 0 ? "Project.Manifest.Missing" : "Project.Manifest.Ambiguous",
            "Manifest",
            fullInput,
            manifests.Count == 0
                ? "Project directory does not contain a .sagaproj manifest."
                : "Project directory contains more than one .sagaproj manifest."));
        return "";
    }

    private static int? ReadRequiredInt(
        JsonElement root,
        string name,
        string path,
        List<ProjectDiagnostic> diagnostics)
    {
        if (!root.TryGetProperty(name, out var value))
        {
            diagnostics.Add(MissingField(name, path));
            return null;
        }
        if (value.ValueKind != JsonValueKind.Number || !value.TryGetInt32(out var result))
        {
            diagnostics.Add(InvalidField(name, path, "Field must be an integer."));
            return null;
        }
        return result;
    }

    private static string ReadRequiredString(
        JsonElement root,
        string name,
        string path,
        List<ProjectDiagnostic> diagnostics)
    {
        if (!root.TryGetProperty(name, out var value))
        {
            diagnostics.Add(MissingField(name, path));
            return "";
        }
        if (value.ValueKind != JsonValueKind.String)
        {
            diagnostics.Add(InvalidField(name, path, "Field must be a string."));
            return "";
        }
        var result = value.GetString() ?? "";
        if (string.IsNullOrWhiteSpace(result))
        {
            diagnostics.Add(InvalidField(name, path, "Field must not be empty."));
        }
        return result;
    }

    private static EngineCompatibility ReadEngineCompatibility(
        JsonElement root,
        string path,
        List<ProjectDiagnostic> diagnostics)
    {
        if (!root.TryGetProperty("engineCompatibility", out var value))
        {
            diagnostics.Add(MissingField("engineCompatibility", path));
            return new EngineCompatibility();
        }
        if (value.ValueKind != JsonValueKind.Object)
        {
            diagnostics.Add(InvalidField("engineCompatibility", path, "Field must be an object."));
            return new EngineCompatibility();
        }
        return new EngineCompatibility
        {
            MinimumVersion = ReadRequiredString(value, "minimumVersion", "engineCompatibility.minimumVersion", diagnostics),
            TargetVersion = ReadRequiredString(value, "targetVersion", "engineCompatibility.targetVersion", diagnostics),
            Channel = ReadRequiredString(value, "channel", "engineCompatibility.channel", diagnostics),
        };
    }

    private static ProjectPaths ReadProjectPaths(
        JsonElement root,
        string projectRoot,
        string path,
        List<ProjectDiagnostic> diagnostics)
    {
        if (!root.TryGetProperty("paths", out var value))
        {
            diagnostics.Add(MissingField("paths", path));
            return new ProjectPaths();
        }
        if (value.ValueKind != JsonValueKind.Object)
        {
            diagnostics.Add(InvalidField("paths", path, "Field must be an object."));
            return new ProjectPaths();
        }

        var diagnosticsPath = ReadRequiredPath(value, "diagnostics", projectRoot, "paths.diagnostics", diagnostics);
        var reportsPath = ReadRequiredPath(value, "generatedReports", projectRoot, "paths.generatedReports", diagnostics);
        return new ProjectPaths
        {
            Diagnostics = diagnosticsPath,
            GeneratedReports = reportsPath,
        };
    }

    private static List<ProjectPathReference> ReadReferences(
        JsonElement root,
        string name,
        bool requireKind,
        string projectRoot,
        string path,
        List<ProjectDiagnostic> diagnostics)
    {
        if (!root.TryGetProperty(name, out var value))
        {
            diagnostics.Add(MissingField(name, path));
            return [];
        }
        if (value.ValueKind != JsonValueKind.Array)
        {
            diagnostics.Add(InvalidField(name, path, "Field must be an array."));
            return [];
        }

        var references = new List<ProjectPathReference>();
        var index = 0;
        foreach (var item in value.EnumerateArray())
        {
            var prefix = $"{name}[{index}]";
            if (item.ValueKind != JsonValueKind.Object)
            {
                diagnostics.Add(InvalidField(prefix, path, "Reference must be an object."));
                ++index;
                continue;
            }

            var id = ReadRequiredString(item, "id", $"{prefix}.id", diagnostics);
            var referencePath = ReadRequiredPath(item, "path", projectRoot, $"{prefix}.path", diagnostics);
            var kind = requireKind
                ? ReadRequiredString(item, "kind", $"{prefix}.kind", diagnostics)
                : ReadOptionalString(item, "kind");
            references.Add(new ProjectPathReference
            {
                Id = id,
                Path = referencePath,
                Kind = kind,
            });
            ++index;
        }

        return references
            .OrderBy(r => r.Id, StringComparer.Ordinal)
            .ThenBy(r => r.Path, StringComparer.Ordinal)
            .ToList();
    }

    private static string ReadRequiredPath(
        JsonElement root,
        string name,
        string projectRoot,
        string diagnosticPath,
        List<ProjectDiagnostic> diagnostics)
    {
        var value = ReadRequiredString(root, name, diagnosticPath, diagnostics);
        if (value.Length == 0)
        {
            return "";
        }
        return ValidateProjectRelativePath(value, projectRoot, diagnosticPath, diagnostics);
    }

    private static string ReadOptionalString(JsonElement root, string name)
    {
        return root.TryGetProperty(name, out var value) && value.ValueKind == JsonValueKind.String
            ? value.GetString() ?? ""
            : "";
    }

    private static string ValidateProjectRelativePath(
        string value,
        string projectRoot,
        string diagnosticPath,
        List<ProjectDiagnostic> diagnostics)
    {
        var normalized = NormalizeRelative(value);
        if (IsAbsoluteLike(value))
        {
            diagnostics.Add(Error(
                "Project.Path.AbsoluteNotAllowed",
                "Path",
                diagnosticPath,
                "Project paths must be project-relative."));
            return normalized;
        }

        var segments = normalized.Split('/', StringSplitOptions.RemoveEmptyEntries);
        if (segments.Any(segment => segment == ".."))
        {
            diagnostics.Add(Error(
                "Project.Path.ParentTraversalNotAllowed",
                "Path",
                diagnosticPath,
                "Project paths must not contain parent traversal."));
            return normalized;
        }

        var fullPath = Path.GetFullPath(Path.Combine(projectRoot, normalized));
        var relative = Path.GetRelativePath(projectRoot, fullPath);
        if (relative.StartsWith("..", StringComparison.Ordinal) || Path.IsPathRooted(relative))
        {
            diagnostics.Add(Error(
                "Project.Path.ParentTraversalNotAllowed",
                "Path",
                diagnosticPath,
                "Project paths must remain under the project root."));
        }

        return normalized;
    }

    private static bool IsAbsoluteLike(string value)
    {
        return Path.IsPathRooted(value) ||
               value.StartsWith("/", StringComparison.Ordinal) ||
               value.StartsWith("\\", StringComparison.Ordinal) ||
               (value.Length >= 2 && char.IsLetter(value[0]) && value[1] == ':');
    }

    private static List<ResolvedProjectPath> BuildResolvedPaths(
        string projectRoot,
        SagaProjectDescriptor descriptor)
    {
        var resolved = new List<ResolvedProjectPath>
        {
            Resolve(projectRoot, "diagnostics", "diagnostics", "directory", descriptor.Paths.Diagnostics),
            Resolve(projectRoot, "generatedReports", "generatedReports", "directory", descriptor.Paths.GeneratedReports),
        };
        resolved.AddRange(descriptor.Scenes.Select(r => Resolve(projectRoot, "scene", r.Id, r.Kind, r.Path)));
        resolved.AddRange(descriptor.Assets.Select(r => Resolve(projectRoot, "asset", r.Id, r.Kind, r.Path)));
        resolved.AddRange(descriptor.ScriptFolders.Select(r => Resolve(projectRoot, "scriptFolder", r.Id, "directory", r.Path)));
        resolved.AddRange(descriptor.LaunchProfiles.Select(r => Resolve(projectRoot, "launchProfile", r.Id, "json", r.Path)));
        resolved.AddRange(descriptor.PackageProfiles.Select(r => Resolve(projectRoot, "packageProfile", r.Id, "json", r.Path)));
        return resolved
            .Where(p => p.RelativePath.Length > 0)
            .OrderBy(p => p.Category, StringComparer.Ordinal)
            .ThenBy(p => p.Id, StringComparer.Ordinal)
            .ThenBy(p => p.RelativePath, StringComparer.Ordinal)
            .ToList();
    }

    private static ResolvedProjectPath Resolve(
        string projectRoot,
        string category,
        string id,
        string kind,
        string relativePath)
    {
        var absolutePath = Path.GetFullPath(Path.Combine(projectRoot, relativePath));
        return new ResolvedProjectPath
        {
            Category = category,
            Id = id,
            Kind = kind,
            RelativePath = NormalizeRelative(relativePath),
            AbsolutePath = NormalizeAbsolute(absolutePath),
            Exists = File.Exists(absolutePath) || Directory.Exists(absolutePath),
        };
    }

    private static void CheckResolvedReferences(
        List<ResolvedProjectPath> resolved,
        List<ProjectDiagnostic> diagnostics)
    {
        foreach (var path in resolved)
        {
            if (path.Exists)
            {
                continue;
            }
            diagnostics.Add(Error(
                "Project.Reference.Missing",
                "Reference",
                path.RelativePath,
                $"Referenced {path.Category} path is missing."));
        }
    }

    private static ProjectDiagnostic MissingField(string field, string path)
    {
        return Error(
            "Project.Manifest.MissingField",
            "Manifest",
            path,
            $"Required field is missing: {field}.");
    }

    private static ProjectDiagnostic InvalidField(string field, string path, string message)
    {
        return Error(
            "Project.Manifest.InvalidField",
            "Manifest",
            path,
            $"{field}: {message}");
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
