using System.Text.Json;

namespace SagaPackager;

internal static class PackageStager
{
    public static PackageReport Stage(string projectPath, string profileId)
    {
        var resolved = new ProjectResolver().Resolve(projectPath, profileId);
        var diagnostics = resolved.Diagnostics.ToList();
        if (resolved.Profile is null || ProjectResolver.HasErrors(diagnostics))
        {
            diagnostics = ProjectResolver.Sort(diagnostics);
            return new PackageReport
            {
                Command = "stage",
                Status = "Failed",
                Project = ProjectResolver.ProjectSummary(resolved),
                PackageProfile = ProjectResolver.ProfileSummary(resolved.Profile),
                CheckedInputs = PackageValidator.BuildCheckedInputs(resolved),
                Diagnostics = diagnostics,
            };
        }

        var outputRoot = Path.GetFullPath(Path.Combine(resolved.ProjectRoot, resolved.Profile.OutputDirectory));
        var allowedRoot = Path.GetFullPath(Path.Combine(resolved.ProjectRoot, "Build", "Packages"));
        if (!IsUnder(outputRoot, allowedRoot))
        {
            diagnostics.Add(ProjectResolver.Error(
                "Package.Stage.OutputOutsideBuildPackages",
                "Stage",
                outputRoot,
                "Package outputDirectory must stay under Build/Packages."));
        }

        if (ProjectResolver.HasErrors(diagnostics))
        {
            diagnostics = ProjectResolver.Sort(diagnostics);
            return new PackageReport
            {
                Command = "stage",
                Status = "Failed",
                Project = ProjectResolver.ProjectSummary(resolved),
                PackageProfile = ProjectResolver.ProfileSummary(resolved.Profile),
                CheckedInputs = PackageValidator.BuildCheckedInputs(resolved),
                Diagnostics = diagnostics,
            };
        }

        if (Directory.Exists(outputRoot))
        {
            Directory.Delete(outputRoot, recursive: true);
        }
        Directory.CreateDirectory(outputRoot);

        CopyProjectFiles(resolved, outputRoot);
        WriteManifests(resolved, outputRoot);
        WritePackageManifest(resolved, outputRoot);

        var stageManifestPath = Path.Combine(outputRoot, "package_stage_manifest.json");
        var stagedFiles = ListStagedFiles(outputRoot);
        ReportWriter.Write(stageManifestPath, new
        {
            schemaVersion = 1,
            tool = "sagapack",
            status = "Passed",
            stagedPackagePath = Normalize(outputRoot),
            files = stagedFiles,
        });
        stagedFiles = ListStagedFiles(outputRoot);

        return new PackageReport
        {
            Command = "stage",
            Status = "Passed",
            Project = ProjectResolver.ProjectSummary(resolved),
            PackageProfile = ProjectResolver.ProfileSummary(resolved.Profile),
            CheckedInputs = PackageValidator.BuildCheckedInputs(resolved),
            StagedPackagePath = Normalize(outputRoot),
            PackageManifestPath = Normalize(Path.Combine(outputRoot, resolved.Profile.PackageManifestPath)),
            StageManifestPath = Normalize(stageManifestPath),
            StagedFiles = stagedFiles,
            Diagnostics = ProjectResolver.Sort(PackageValidator.BuildMetadataDiagnostics(resolved)),
        };
    }

    private static void CopyProjectFiles(ProjectResolution resolved, string outputRoot)
    {
        var projectRoot = resolved.ProjectRoot;
        var projectOut = Path.Combine(outputRoot, "Project");
        Directory.CreateDirectory(projectOut);
        File.Copy(resolved.ManifestPath, Path.Combine(projectOut, Path.GetFileName(resolved.ManifestPath)), overwrite: true);
        CopyIfExists(Path.Combine(projectRoot, "launch_profiles.json"), Path.Combine(projectOut, "launch_profiles.json"));
        CopyIfExists(Path.Combine(projectRoot, "package_profiles.json"), Path.Combine(projectOut, "package_profiles.json"));
        CopyDirectoryIfExists(Path.Combine(projectRoot, "Assets"), Path.Combine(projectOut, "Assets"));
        CopyDirectoryIfExists(Path.Combine(projectRoot, "Scripts"), Path.Combine(projectOut, "Scripts"));
    }

    private static void WriteManifests(ProjectResolution resolved, string outputRoot)
    {
        var profile = resolved.Profile!;
        ReportWriter.Write(Path.Combine(outputRoot, profile.AssetManifestPath), new
        {
            schemaVersion = 1,
            assets = Array.Empty<object>(),
        });
        ReportWriter.Write(Path.Combine(outputRoot, profile.AssetIdentityManifestPath), new
        {
            schemaVersion = 1,
            mappings = Array.Empty<object>(),
        });
        ReportWriter.Write(Path.Combine(outputRoot, profile.ScriptMetadataPath), new
        {
            schemaVersion = 1,
            status = "Placeholder",
            message = "No script metadata is packaged for the server/headless proof.",
        });
        ReportWriter.Write(Path.Combine(outputRoot, profile.RuntimeMetadataPath), new
        {
            schemaVersion = 1,
            status = "Placeholder",
            message = "No runtime metadata beyond package manifest validation is packaged in this phase.",
        });
    }

    private static void WritePackageManifest(ProjectResolution resolved, string outputRoot)
    {
        var profile = resolved.Profile!;
        var packageKind = profile.Role == "client" ? "client" : "server";
        ReportWriter.Write(Path.Combine(outputRoot, profile.PackageManifestPath), new
        {
            schemaVersion = 1,
            packageId = $"{resolved.Project?.ProjectId ?? "project"}.{packageKind}.{profile.Id}",
            packageKind,
            buildProfile = profile.Id,
            targetPlatform = "local",
            runtimeCompatibilityVersion = "0.0.9",
            assetIdentityManifest = profile.AssetIdentityManifestPath,
            assetManifests = new[]
            {
                new { id = "assets.multiplayer_sandbox", path = profile.AssetManifestPath },
            },
            artifactManifests = Array.Empty<object>(),
        });
    }

    private static List<StagedFile> ListStagedFiles(string outputRoot)
    {
        return Directory.EnumerateFiles(outputRoot, "*", SearchOption.AllDirectories)
            .Select(path => new StagedFile
            {
                Path = Normalize(Path.GetRelativePath(outputRoot, path)),
                SizeBytes = new FileInfo(path).Length,
            })
            .OrderBy(file => file.Path, StringComparer.Ordinal)
            .ToList();
    }

    private static void CopyIfExists(string source, string destination)
    {
        if (!File.Exists(source))
        {
            return;
        }
        Directory.CreateDirectory(Path.GetDirectoryName(destination) ?? ".");
        File.Copy(source, destination, overwrite: true);
    }

    private static void CopyDirectoryIfExists(string source, string destination)
    {
        if (!Directory.Exists(source))
        {
            return;
        }
        foreach (var file in Directory.EnumerateFiles(source, "*", SearchOption.AllDirectories)
                     .OrderBy(path => path, StringComparer.Ordinal))
        {
            var relative = Path.GetRelativePath(source, file);
            CopyIfExists(file, Path.Combine(destination, relative));
        }
    }

    private static bool IsUnder(string path, string root)
    {
        var normalizedPath = Path.GetFullPath(path).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
        var normalizedRoot = Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
        return normalizedPath.StartsWith(normalizedRoot, StringComparison.Ordinal);
    }

    private static string Normalize(string path)
    {
        return path.Replace('\\', '/');
    }
}
