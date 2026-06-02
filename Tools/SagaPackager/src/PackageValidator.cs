namespace SagaPackager;

internal static class PackageValidator
{
    public static PackageReport Validate(string projectPath, string profileId)
    {
        var resolved = new ProjectResolver().Resolve(projectPath, profileId);
        var diagnostics = resolved.Diagnostics.ToList();
        diagnostics.AddRange(BuildMetadataDiagnostics(resolved));
        diagnostics = ProjectResolver.Sort(diagnostics);

        return new PackageReport
        {
            Command = "validate",
            Status = ProjectResolver.HasErrors(diagnostics) ? "Failed" : "Passed",
            Project = ProjectResolver.ProjectSummary(resolved),
            PackageProfile = ProjectResolver.ProfileSummary(resolved.Profile),
            CheckedInputs = BuildCheckedInputs(resolved),
            Diagnostics = diagnostics,
        };
    }

    public static List<CheckedInput> BuildCheckedInputs(ProjectResolution resolved)
    {
        var inputs = new List<CheckedInput>();
        Add(inputs, "projectManifest", resolved.ManifestPath);
        Add(inputs, "packageProfiles", resolved.PackageProfilesPath);
        if (resolved.Project is not null)
        {
            foreach (var item in resolved.Project.LaunchProfiles.OrderBy(p => p.Id, StringComparer.Ordinal))
            {
                Add(inputs, $"launchProfile:{item.Id}", Path.Combine(resolved.ProjectRoot, item.Path));
            }
            Add(inputs, "assets", Path.Combine(resolved.ProjectRoot, "Assets"));
            Add(inputs, "scripts", Path.Combine(resolved.ProjectRoot, "Scripts"));
        }
        return inputs.OrderBy(i => i.Kind, StringComparer.Ordinal).ToList();
    }

    public static List<PackageDiagnostic> BuildMetadataDiagnostics(ProjectResolution resolved)
    {
        var diagnostics = new List<PackageDiagnostic>();
        if (resolved.Profile is null)
        {
            return diagnostics;
        }

        foreach (var (path, code, label) in new[]
                 {
                     (resolved.Profile.ScriptMetadataPath, "Package.Metadata.ScriptPlaceholder", "Script metadata"),
                     (resolved.Profile.RuntimeMetadataPath, "Package.Metadata.RuntimePlaceholder", "Runtime metadata"),
                 })
        {
            var stagedPath = Path.Combine(resolved.ProjectRoot, resolved.Profile.OutputDirectory, path);
            if (!File.Exists(stagedPath))
            {
                diagnostics.Add(ProjectResolver.Warning(
                    code,
                    "Metadata",
                    path,
                    $"{label} is not required for the server/headless package profile and will be staged as placeholder metadata."));
            }
        }
        return diagnostics;
    }

    private static void Add(List<CheckedInput> inputs, string kind, string path)
    {
        if (string.IsNullOrWhiteSpace(path))
        {
            return;
        }
        var fullPath = Path.GetFullPath(path);
        var exists = File.Exists(fullPath) || Directory.Exists(fullPath);
        inputs.Add(new CheckedInput
        {
            Kind = kind,
            Path = ProjectResolver.Normalize(fullPath),
            Exists = exists,
            Status = exists ? "Present" : "Missing",
        });
    }
}
