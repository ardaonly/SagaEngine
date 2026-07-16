using System.Text.Json;

namespace SagaPackager;

internal sealed class ProjectResolver
{
    private static readonly JsonSerializerOptions Options = new()
    {
        PropertyNameCaseInsensitive = true,
    };

    public ProjectResolution Resolve(string projectInput, string profileId)
    {
        var diagnostics = new List<PackageDiagnostic>();
        var manifestPath = ResolveManifestPath(projectInput, diagnostics);
        if (manifestPath.Length == 0)
        {
            return new ProjectResolution { Diagnostics = Sort(diagnostics) };
        }

        var projectRoot = Path.GetFullPath(Path.GetDirectoryName(manifestPath) ?? ".");
        var project = ReadJson<SagaProjectManifest>(
            manifestPath,
            "Package.Project.ManifestParseFailed",
            diagnostics);
        if (project is null)
        {
            return Result(projectRoot, manifestPath, null, null, "", diagnostics);
        }

        if (project.SchemaVersion != 0)
        {
            diagnostics.Add(Error(
                "Package.Project.UnsupportedSchemaVersion",
                "Project",
                "schemaVersion",
                "Only .sagaproj schemaVersion 0 is supported."));
        }

        var profileReference = project.PackageProfiles
            .OrderBy(p => p.Id, StringComparer.Ordinal)
            .FirstOrDefault(p => p.Id == profileId);
        if (profileReference is null)
        {
            diagnostics.Add(Error(
                "Package.Profile.NotFound",
                "Profile",
                profileId,
                "Package profile id was not found in the project manifest."));
            return Result(projectRoot, manifestPath, project, null, "", diagnostics);
        }

        if (!ValidateProjectRelativePath(profileReference.Path, "packageProfiles.path", diagnostics))
        {
            return Result(projectRoot, manifestPath, project, null, profileReference.Path, diagnostics);
        }

        var packageProfilesPath = Path.GetFullPath(Path.Combine(projectRoot, profileReference.Path));
        var document = ReadJson<PackageProfilesDocument>(
            packageProfilesPath,
            "Package.Profile.ParseFailed",
            diagnostics);
        if (document is null)
        {
            return Result(projectRoot, manifestPath, project, null, packageProfilesPath, diagnostics);
        }

        if (document.SchemaVersion != 0)
        {
            diagnostics.Add(Error(
                "Package.Profile.UnsupportedSchemaVersion",
                "Profile",
                packageProfilesPath,
                "Only package profile schemaVersion 0 is supported."));
        }

        var profile = document.Profiles
            .OrderBy(p => p.Id, StringComparer.Ordinal)
            .FirstOrDefault(p => p.Id == profileId);
        if (profile is null)
        {
            diagnostics.Add(Error(
                "Package.Profile.NotFound",
                "Profile",
                packageProfilesPath,
                "Package profile id was not found in package_profiles.json."));
        }

        if (profile is not null)
        {
            ValidateProfile(projectRoot, project, profile, packageProfilesPath, diagnostics);
        }

        return Result(projectRoot, manifestPath, project, profile, packageProfilesPath, diagnostics);
    }

    public ProjectResolution ResolveProject(string projectInput)
    {
        var diagnostics = new List<PackageDiagnostic>();
        var manifestPath = ResolveManifestPath(projectInput, diagnostics);
        if (manifestPath.Length == 0)
        {
            return new ProjectResolution { Diagnostics = Sort(diagnostics) };
        }

        var projectRoot = Path.GetFullPath(Path.GetDirectoryName(manifestPath) ?? ".");
        var project = ReadJson<SagaProjectManifest>(
            manifestPath,
            "Package.Project.ManifestParseFailed",
            diagnostics);
        if (project is null)
        {
            return Result(projectRoot, manifestPath, null, null, "", diagnostics);
        }

        if (project.SchemaVersion != 0)
        {
            diagnostics.Add(Error(
                "Package.Project.UnsupportedSchemaVersion",
                "Project",
                "schemaVersion",
                "Only .sagaproj schemaVersion 0 is supported."));
        }

        return Result(projectRoot, manifestPath, project, null, "", diagnostics);
    }

    public PackageProfilesDocument? ReadPackageProfiles(
        ProjectResolution resolution,
        ProjectPathReference reference,
        List<PackageDiagnostic> diagnostics,
        out string packageProfilesPath)
    {
        packageProfilesPath = "";
        if (!ValidateProjectRelativePath(reference.Path, "packageProfiles.path", diagnostics))
        {
            return null;
        }

        packageProfilesPath = Path.GetFullPath(Path.Combine(resolution.ProjectRoot, reference.Path));
        var document = ReadJson<PackageProfilesDocument>(
            packageProfilesPath,
            "Package.Profile.ParseFailed",
            diagnostics);
        if (document is null)
        {
            return null;
        }

        if (document.SchemaVersion != 0)
        {
            diagnostics.Add(Error(
                "Package.Profile.UnsupportedSchemaVersion",
                "Profile",
                packageProfilesPath,
                "Only package profile schemaVersion 0 is supported."));
        }

        return document;
    }

    public static ProjectSummary ProjectSummary(ProjectResolution resolution)
    {
        return new ProjectSummary
        {
            ProjectId = resolution.Project?.ProjectId ?? "",
            DisplayName = resolution.Project?.DisplayName ?? "",
            ProjectRoot = Normalize(resolution.ProjectRoot),
            ManifestPath = Normalize(resolution.ManifestPath),
        };
    }

    public static PackageProfileSummary ProfileSummary(PackageProfile? profile)
    {
        return new PackageProfileSummary
        {
            Id = profile?.Id ?? "",
            DisplayName = profile?.DisplayName ?? "",
            Role = profile?.Role ?? "",
            OutputDirectory = Normalize(profile?.OutputDirectory ?? ""),
            PackageManifestPath = Normalize(profile?.PackageManifestPath ?? ""),
            LaunchProfileId = profile?.LaunchProfileId ?? "",
        };
    }

    public static PackageDiagnostic Error(
        string code,
        string category,
        object path,
        string message)
    {
        return new PackageDiagnostic
        {
            Severity = "Error",
            Code = code,
            Category = category,
            Path = Normalize(path.ToString() ?? ""),
            Message = message,
        };
    }

    public static PackageDiagnostic Warning(
        string code,
        string category,
        object path,
        string message)
    {
        return new PackageDiagnostic
        {
            Severity = "Warning",
            Code = code,
            Category = category,
            Path = Normalize(path.ToString() ?? ""),
            Message = message,
        };
    }

    public static List<PackageDiagnostic> Sort(IEnumerable<PackageDiagnostic> diagnostics)
    {
        return diagnostics
            .OrderByDescending(d => d.Severity == "Error")
            .ThenBy(d => d.Code, StringComparer.Ordinal)
            .ThenBy(d => d.Path, StringComparer.Ordinal)
            .ThenBy(d => d.Message, StringComparer.Ordinal)
            .ToList();
    }

    public static bool HasErrors(IEnumerable<PackageDiagnostic> diagnostics)
    {
        return diagnostics.Any(d => d.Severity == "Error");
    }

    public static string Normalize(string path)
    {
        return path.Replace('\\', '/').Trim();
    }

    public static bool ValidateProjectRelativePath(
        string value,
        string field,
        List<PackageDiagnostic> diagnostics)
    {
        if (string.IsNullOrWhiteSpace(value))
        {
            diagnostics.Add(Error(
                "Package.Profile.MissingField",
                "Profile",
                field,
                "Required path field is missing or empty."));
            return false;
        }
        if (IsAbsoluteLike(value))
        {
            diagnostics.Add(Error(
                "Package.Path.AbsoluteNotAllowed",
                "Path",
                field,
                "Package paths must be project-relative."));
            return false;
        }

        var normalized = Normalize(value);
        var segments = normalized.Split('/', StringSplitOptions.RemoveEmptyEntries);
        if (segments.Any(segment => segment == ".."))
        {
            diagnostics.Add(Error(
                "Package.Path.ParentTraversalNotAllowed",
                "Path",
                field,
                "Package paths must not contain parent traversal."));
            return false;
        }
        return true;
    }

    private static ProjectResolution Result(
        string projectRoot,
        string manifestPath,
        SagaProjectManifest? project,
        PackageProfile? profile,
        string packageProfilesPath,
        List<PackageDiagnostic> diagnostics)
    {
        return new ProjectResolution
        {
            ProjectRoot = Normalize(projectRoot),
            ManifestPath = Normalize(manifestPath),
            Project = project,
            Profile = profile,
            PackageProfilesPath = Normalize(packageProfilesPath),
            Diagnostics = Sort(diagnostics),
        };
    }

    private static void ValidateProfile(
        string projectRoot,
        SagaProjectManifest project,
        PackageProfile profile,
        string profilePath,
        List<PackageDiagnostic> diagnostics)
    {
        Require(profile.Id, "profiles[].id", profilePath, diagnostics);
        Require(profile.DisplayName, "displayName", profilePath, diagnostics);
        Require(profile.OutputDirectory, "outputDirectory", profilePath, diagnostics);
        Require(profile.DiagnosticsDirectory, "diagnosticsDirectory", profilePath, diagnostics);
        Require(profile.ReportsDirectory, "reportsDirectory", profilePath, diagnostics);
        Require(profile.PackageManifestPath, "packageManifestPath", profilePath, diagnostics);
        Require(profile.AssetManifestPath, "assetManifestPath", profilePath, diagnostics);
        Require(profile.AssetIdentityManifestPath, "assetIdentityManifestPath", profilePath, diagnostics);
        Require(profile.ScriptMetadataPath, "scriptMetadataPath", profilePath, diagnostics);
        Require(profile.RuntimeMetadataPath, "runtimeMetadataPath", profilePath, diagnostics);

        if (profile.Role is not ("client" or "server"))
        {
            diagnostics.Add(Error(
                "Package.Profile.InvalidRole",
                "Profile",
                "role",
                "Package profile role must be client or server."));
        }

        ValidateProjectRelativePath(profile.OutputDirectory, "outputDirectory", diagnostics);
        ValidateProjectRelativePath(profile.DiagnosticsDirectory, "diagnosticsDirectory", diagnostics);
        ValidateProjectRelativePath(profile.ReportsDirectory, "reportsDirectory", diagnostics);
        ValidateProjectRelativePath(profile.PackageManifestPath, "packageManifestPath", diagnostics);
        ValidateProjectRelativePath(profile.AssetManifestPath, "assetManifestPath", diagnostics);
        ValidateProjectRelativePath(profile.AssetIdentityManifestPath, "assetIdentityManifestPath", diagnostics);
        ValidateProjectRelativePath(profile.ScriptMetadataPath, "scriptMetadataPath", diagnostics);
        ValidateProjectRelativePath(profile.RuntimeMetadataPath, "runtimeMetadataPath", diagnostics);

        var launchReference = project.LaunchProfiles
            .FirstOrDefault(p => p.Id == profile.LaunchProfileId);
        if (string.IsNullOrWhiteSpace(profile.LaunchProfileId) || launchReference is null)
        {
            diagnostics.Add(Error(
                "Package.Profile.LaunchProfileMissing",
                "Profile",
                profile.LaunchProfileId,
                "Package profile must reference an existing launch profile."));
        }
        else
        {
            var launchProfilePath = Path.GetFullPath(Path.Combine(projectRoot, launchReference.Path));
            if (!File.Exists(launchProfilePath))
            {
                diagnostics.Add(Error(
                    "Package.Profile.LaunchProfileFileMissing",
                    "Profile",
                    launchProfilePath,
                    "Referenced launch profile document is missing."));
            }
            else if (!File.ReadAllText(launchProfilePath).Contains($"\"id\": \"{profile.LaunchProfileId}\"", StringComparison.Ordinal))
            {
                diagnostics.Add(Error(
                    "Package.Profile.LaunchProfileMissing",
                    "Profile",
                    launchProfilePath,
                    "Referenced launch profile id was not found in launch profile document."));
            }
        }
    }

    private static void Require(
        string value,
        string field,
        string path,
        List<PackageDiagnostic> diagnostics)
    {
        if (string.IsNullOrWhiteSpace(value))
        {
            diagnostics.Add(Error(
                "Package.Profile.MissingField",
                "Profile",
                path,
                $"Required field is missing or empty: {field}."));
        }
    }

    private static string ResolveManifestPath(
        string projectInput,
        List<PackageDiagnostic> diagnostics)
    {
        if (string.IsNullOrWhiteSpace(projectInput))
        {
            diagnostics.Add(Error(
                "Package.Project.MissingInput",
                "Project",
                "",
                "Project path is required."));
            return "";
        }

        var fullInput = Path.GetFullPath(projectInput);
        if (File.Exists(fullInput))
        {
            if (!string.Equals(Path.GetExtension(fullInput), ".sagaproj",
                               StringComparison.OrdinalIgnoreCase))
            {
                diagnostics.Add(Error(
                    "Package.Project.UnsupportedFileType",
                    "Project",
                    fullInput,
                    "Explicit project input must be a .sagaproj file."));
                return "";
            }
            return fullInput;
        }
        if (!Directory.Exists(fullInput))
        {
            diagnostics.Add(Error(
                "Package.Project.MissingInput",
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
            manifests.Count == 0 ? "Package.Project.ManifestMissing" : "Package.Project.ManifestAmbiguous",
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
        List<PackageDiagnostic> diagnostics)
    {
        if (!File.Exists(path))
        {
            diagnostics.Add(Error(
                "Package.Project.ReferenceMissing",
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

    private static bool IsAbsoluteLike(string value)
    {
        return Path.IsPathRooted(value) ||
               value.StartsWith("/", StringComparison.Ordinal) ||
               value.StartsWith("\\", StringComparison.Ordinal) ||
               (value.Length >= 2 && char.IsLetter(value[0]) && value[1] == ':');
    }
}
