using System.Text.Json;
using SagaProjectKit.Core;

namespace SagaProjectKit;

internal sealed class ProjectManifestLoader
{
    public ManifestLoadResult Load(string projectInput, bool checkReferences)
    {
        var load = new ProjectLoader().Load(projectInput, checkReferences);
        return new ManifestLoadResult
        {
            ProjectRoot = load.ProjectRoot,
            ManifestPath = load.ManifestPath,
            Descriptor = load.Manifest is null ? null : Map(load.Manifest),
            Diagnostics = load.Diagnostics.Select(Map).ToList(),
            ResolvedPaths = load.References.Select(item => new ResolvedProjectPath
            {
                Category = item.Category,
                Id = item.Id,
                Kind = item.Kind,
                RelativePath = item.RelativePath,
                AbsolutePath = item.AbsolutePath,
                Exists = item.Exists,
            }).ToList(),
        };
    }

    public List<ProjectDiagnostic> CheckDoctorProfileJson(ManifestLoadResult load)
    {
        var diagnostics = new List<ProjectDiagnostic>();
        foreach (var resolved in load.ResolvedPaths.Where(item =>
                     item.Category is "launchProfile" or "packageProfile"))
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
            catch (JsonException exception)
            {
                diagnostics.Add(new ProjectDiagnostic
                {
                    Severity = "Error",
                    Code = "Project.Profile.ParseFailed",
                    Category = "Profile",
                    Path = resolved.RelativePath,
                    Message = $"Profile JSON is invalid: {exception.Message}",
                });
            }
        }
        return diagnostics.OrderBy(item => item.Code, StringComparer.Ordinal)
            .ThenBy(item => item.Path, StringComparer.Ordinal).ToList();
    }

    private static SagaProjectDescriptor Map(ProjectManifest manifest) => new()
    {
        SchemaVersion = manifest.SchemaVersion,
        ProjectId = manifest.ProjectId,
        DisplayName = manifest.DisplayName,
        EngineCompatibility = new EngineCompatibility
        {
            MinimumVersion = manifest.EngineCompatibility.MinimumVersion,
            TargetVersion = manifest.EngineCompatibility.TargetVersion,
            Channel = manifest.EngineCompatibility.Channel,
        },
        Paths = new ProjectPaths
        {
            Diagnostics = manifest.Paths.Diagnostics,
            GeneratedReports = manifest.Paths.GeneratedReports,
        },
        Scenes = Map(manifest.Scenes),
        Assets = Map(manifest.Assets),
        ScriptFolders = Map(manifest.ScriptFolders),
        LaunchProfiles = Map(manifest.LaunchProfiles),
        PackageProfiles = Map(manifest.PackageProfiles),
    };

    private static List<ProjectPathReference> Map(IReadOnlyList<SagaProjectKit.Core.ProjectReference> references) =>
        references.Select(item => new ProjectPathReference
        {
            Id = item.Id,
            Path = item.Path,
            Kind = item.Kind,
        }).OrderBy(item => item.Id, StringComparer.Ordinal)
          .ThenBy(item => item.Path, StringComparer.Ordinal).ToList();

    private static ProjectDiagnostic Map(SagaProjectKit.Core.ProjectDiagnostic diagnostic) => new()
    {
        Severity = diagnostic.Severity,
        Code = diagnostic.Code,
        Category = diagnostic.Code.Contains("Reference", StringComparison.Ordinal) ? "Reference" : "Manifest",
        Path = diagnostic.Path,
        Message = diagnostic.Message,
    };
}
