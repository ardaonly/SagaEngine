using System.Text.Json;
using System.Text.Json.Nodes;

namespace SagaPackager;

internal static class SourceTruthAlignmentReporter
{
    public static PackageSourceTruthAlignmentReport Build(
        string projectPath,
        string sourceTruthGatePath,
        string assetReferenceGatePath)
    {
        var resolver = new ProjectResolver();
        var resolution = resolver.ResolveProject(projectPath);
        var diagnostics = resolution.Diagnostics.ToList();
        var profiles = new List<PackageProfileSummary>();
        var launchProfileIds = new List<string>();
        var rejected = new List<PackageManifestCanonicalRejection>();

        if (resolution.Project is not null)
        {
            foreach (var reference in resolution.Project.PackageProfiles.OrderBy(item => item.Id, StringComparer.Ordinal))
            {
                var document = resolver.ReadPackageProfiles(resolution, reference, diagnostics, out _);
                if (document is null)
                {
                    continue;
                }

                foreach (var profile in document.Profiles.OrderBy(item => item.Id, StringComparer.Ordinal))
                {
                    profiles.Add(ProjectResolver.ProfileSummary(profile));
                    if (!string.IsNullOrWhiteSpace(profile.LaunchProfileId))
                    {
                        launchProfileIds.Add(profile.LaunchProfileId);
                    }
                    rejected.Add(new PackageManifestCanonicalRejection
                    {
                        ProfileId = profile.Id,
                        PackageManifestPath = ProjectResolver.Normalize(profile.PackageManifestPath),
                        AssetManifestPath = ProjectResolver.Normalize(profile.AssetManifestPath),
                        Reason = "Package and generated manifests are package evidence only and cannot be canonical source truth.",
                    });
                }
            }
        }

        var sourceTruthGate = ReadGate("SourceTruthGate", sourceTruthGatePath, diagnostics);
        var assetReferenceGate = ReadGate("AssetReferenceGate", assetReferenceGatePath, diagnostics);
        var failed = ProjectResolver.HasErrors(diagnostics) ||
            sourceTruthGate.Status is "Failed" or "Missing" ||
            assetReferenceGate.Status is "Failed" or "Missing";

        return new PackageSourceTruthAlignmentReport
        {
            Status = failed ? "Failed" : "Passed",
            Project = ProjectResolver.ProjectSummary(resolution),
            PackageProfiles = profiles
                .OrderBy(item => item.Id, StringComparer.Ordinal)
                .ToList(),
            ReferencedLaunchProfileIds = launchProfileIds
                .Distinct(StringComparer.Ordinal)
                .OrderBy(item => item, StringComparer.Ordinal)
                .ToList(),
            SourceTruthGate = sourceTruthGate,
            AssetReferenceGate = assetReferenceGate,
            PackageGeneratedManifestCanonicalRejections = rejected
                .OrderBy(item => item.ProfileId, StringComparer.Ordinal)
                .ToList(),
            Diagnostics = ProjectResolver.Sort(diagnostics),
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    private static GateResult ReadGate(string name, string path, List<PackageDiagnostic> diagnostics)
    {
        if (string.IsNullOrWhiteSpace(path) || !File.Exists(path))
        {
            diagnostics.Add(ProjectResolver.Error(
                "Package.SourceTruthAlignment.GateMissing",
                "SourceTruthAlignment",
                path,
                $"{name} evidence is required."));
            return new GateResult { Name = name, Status = "Missing", EvidencePath = ProjectResolver.Normalize(path), Message = "Evidence missing." };
        }

        try
        {
            var node = JsonNode.Parse(File.ReadAllText(path)) as JsonObject;
            var status = ReadString(node, "status");
            return new GateResult
            {
                Name = name,
                Status = string.IsNullOrWhiteSpace(status) ? "Malformed" : status,
                EvidencePath = ProjectResolver.Normalize(path),
                Message = string.IsNullOrWhiteSpace(status) ? "Evidence status missing." : "Evidence read without package staging.",
            };
        }
        catch (Exception e) when (e is JsonException or IOException or UnauthorizedAccessException)
        {
            diagnostics.Add(ProjectResolver.Error(
                "Package.SourceTruthAlignment.GateMalformed",
                "SourceTruthAlignment",
                path,
                $"Gate evidence could not be read: {e.Message}"));
            return new GateResult { Name = name, Status = "Malformed", EvidencePath = ProjectResolver.Normalize(path), Message = "Evidence malformed." };
        }
    }

    private static string ReadString(JsonObject? obj, string key) =>
        obj is not null &&
        obj.TryGetPropertyValue(key, out var node) &&
        node is JsonValue value &&
        value.TryGetValue<string>(out var text)
            ? text
            : "";
}
