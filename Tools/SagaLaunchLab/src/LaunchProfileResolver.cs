using System.Text.Json;

namespace SagaLaunchLab;

internal sealed class LaunchProfileResolver
{
    private static readonly JsonSerializerOptions Options = new()
    {
        PropertyNameCaseInsensitive = true,
    };

    public LaunchResolution Resolve(string projectInput, string launchProfileId)
    {
        var diagnostics = new List<LaunchDiagnostic>();
        var manifestPath = ResolveManifestPath(projectInput, diagnostics);
        if (manifestPath.Length == 0)
        {
            return new LaunchResolution { Diagnostics = Sort(diagnostics) };
        }

        var projectRoot = Path.GetDirectoryName(manifestPath) ?? Directory.GetCurrentDirectory();
        var project = ReadJson<SagaProjectManifest>(
            manifestPath,
            "Launch.Project.ManifestParseFailed",
            diagnostics);
        if (project is null)
        {
            return new LaunchResolution
            {
                ProjectRoot = Normalize(projectRoot),
                ManifestPath = Normalize(manifestPath),
                Diagnostics = Sort(diagnostics),
            };
        }

        var launchReference = project.LaunchProfiles
            .OrderBy(p => p.Id, StringComparer.Ordinal)
            .FirstOrDefault(p => p.Id == launchProfileId);
        if (launchReference is null)
        {
            diagnostics.Add(Error(
                "Launch.Profile.NotFound",
                "Profile",
                launchProfileId,
                "Launch profile id was not found in the project manifest."));
            return Result(projectRoot, manifestPath, project, null, diagnostics);
        }

        var launchProfilesPath = Path.GetFullPath(Path.Combine(projectRoot, launchReference.Path));
        var document = ReadJson<LaunchProfilesDocument>(
            launchProfilesPath,
            "Launch.Profile.ParseFailed",
            diagnostics);
        if (document is null)
        {
            return Result(projectRoot, manifestPath, project, null, diagnostics);
        }

        var profile = document.Profiles
            .OrderBy(p => p.Id, StringComparer.Ordinal)
            .FirstOrDefault(p => p.Id == launchProfileId);
        if (profile is null)
        {
            diagnostics.Add(Error(
                "Launch.Profile.NotFound",
                "Profile",
                launchProfilesPath,
                "Launch profile id was not found in launch_profiles.json."));
            return Result(projectRoot, manifestPath, project, null, diagnostics);
        }
        if (!string.Equals(profile.Role, "server", StringComparison.Ordinal))
        {
            diagnostics.Add(Error(
                "Launch.Profile.UnsupportedRole",
                "Profile",
                launchProfileId,
                "Only server launch profiles are supported in Phase 22."));
        }
        if (string.IsNullOrWhiteSpace(profile.Executable))
        {
            diagnostics.Add(Error(
                "Launch.Profile.ExecutableMissing",
                "Profile",
                launchProfileId,
                "Launch profile must declare an executable."));
        }

        return Result(projectRoot, manifestPath, project, profile, diagnostics);
    }

    public LaunchResolution ResolveProject(string projectInput)
    {
        var diagnostics = new List<LaunchDiagnostic>();
        var manifestPath = ResolveManifestPath(projectInput, diagnostics);
        if (manifestPath.Length == 0)
        {
            return new LaunchResolution { Diagnostics = Sort(diagnostics) };
        }

        var projectRoot = Path.GetDirectoryName(manifestPath) ?? Directory.GetCurrentDirectory();
        var project = ReadJson<SagaProjectManifest>(
            manifestPath,
            "Launch.Project.ManifestParseFailed",
            diagnostics);
        return project is null
            ? new LaunchResolution
            {
                ProjectRoot = Normalize(projectRoot),
                ManifestPath = Normalize(manifestPath),
                Diagnostics = Sort(diagnostics),
            }
            : Result(projectRoot, manifestPath, project, null, diagnostics);
    }

    public LaunchProfilesDocument? ReadLaunchProfiles(
        LaunchResolution resolution,
        ProjectPathReference reference,
        List<LaunchDiagnostic> diagnostics,
        out string launchProfilesPath)
    {
        launchProfilesPath = Path.GetFullPath(Path.Combine(resolution.ProjectRoot, reference.Path));
        return ReadJson<LaunchProfilesDocument>(
            launchProfilesPath,
            "Launch.Profile.ParseFailed",
            diagnostics);
    }

    private static LaunchResolution Result(
        string projectRoot,
        string manifestPath,
        SagaProjectManifest project,
        LaunchProfile? profile,
        List<LaunchDiagnostic> diagnostics)
    {
        return new LaunchResolution
        {
            ProjectRoot = Normalize(projectRoot),
            ManifestPath = Normalize(manifestPath),
            Project = project,
            Profile = profile,
            Diagnostics = Sort(diagnostics),
        };
    }

    private static string ResolveManifestPath(
        string projectInput,
        List<LaunchDiagnostic> diagnostics)
    {
        if (string.IsNullOrWhiteSpace(projectInput))
        {
            diagnostics.Add(Error(
                "Launch.Project.MissingInput",
                "Project",
                "",
                "Project path is required."));
            return "";
        }

        var fullInput = Path.GetFullPath(projectInput);
        if (File.Exists(fullInput))
        {
            return fullInput;
        }
        if (!Directory.Exists(fullInput))
        {
            diagnostics.Add(Error(
                "Launch.Project.MissingInput",
                "Project",
                projectInput,
                "Project path does not exist."));
            return "";
        }

        var manifests = Directory.GetFiles(fullInput, "*.sagaproj", SearchOption.TopDirectoryOnly)
            .OrderBy(Normalize, StringComparer.Ordinal)
            .ToList();
        if (manifests.Count == 1)
        {
            return manifests[0];
        }

        diagnostics.Add(Error(
            manifests.Count == 0 ? "Launch.Project.ManifestMissing" : "Launch.Project.ManifestAmbiguous",
            "Project",
            fullInput,
            manifests.Count == 0
                ? "Project directory does not contain a .sagaproj manifest."
                : "Project directory contains more than one .sagaproj manifest."));
        return "";
    }

    private static T? ReadJson<T>(
        string path,
        string parseCode,
        List<LaunchDiagnostic> diagnostics)
    {
        if (!File.Exists(path))
        {
            diagnostics.Add(Error(
                "Launch.Project.ReferenceMissing",
                "Project",
                path,
                "Referenced project file is missing."));
            return default;
        }

        try
        {
            return JsonSerializer.Deserialize<T>(File.ReadAllText(path), Options);
        }
        catch (JsonException e)
        {
            diagnostics.Add(Error(
                parseCode,
                "Project",
                path,
                $"JSON parse failed: {e.Message}"));
            return default;
        }
    }

    public static LaunchDiagnostic Error(
        string code,
        string category,
        object path,
        string message)
    {
        return new LaunchDiagnostic
        {
            Severity = "Error",
            Code = code,
            Category = category,
            Path = Normalize(path.ToString() ?? ""),
            Message = message,
        };
    }

    public static List<LaunchDiagnostic> Sort(IEnumerable<LaunchDiagnostic> diagnostics)
    {
        return diagnostics
            .OrderByDescending(d => d.Severity == "Error")
            .ThenBy(d => d.Code, StringComparer.Ordinal)
            .ThenBy(d => d.Path, StringComparer.Ordinal)
            .ThenBy(d => d.Message, StringComparer.Ordinal)
            .ToList();
    }

    public static string Normalize(string path)
    {
        return path.Replace('\\', '/');
    }
}
