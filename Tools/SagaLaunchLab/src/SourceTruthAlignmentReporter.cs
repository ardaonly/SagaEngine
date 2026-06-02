using System.Text.Json;
using System.Text.Json.Nodes;

namespace SagaLaunchLab;

internal static class SourceTruthAlignmentReporter
{
    public static LaunchSourceTruthAlignmentReport Build(
        string projectPath,
        string sourceTruthGatePath,
        string runtimeReadinessPath)
    {
        var resolver = new LaunchProfileResolver();
        var resolution = resolver.ResolveProject(projectPath);
        var diagnostics = resolution.Diagnostics.ToList();
        var profiles = new List<LaunchSourceTruthProfileItem>();

        if (resolution.Project is not null)
        {
            foreach (var reference in resolution.Project.LaunchProfiles.OrderBy(item => item.Id, StringComparer.Ordinal))
            {
                var document = resolver.ReadLaunchProfiles(resolution, reference, diagnostics, out _);
                if (document is null)
                {
                    continue;
                }

                foreach (var profile in document.Profiles.OrderBy(item => item.Id, StringComparer.Ordinal))
                {
                    var serverEvidence = profile.Role == "server" && profile.Mode.Contains("headless", StringComparison.OrdinalIgnoreCase) ||
                        profile.Role == "server" && profile.Id.Contains("headless", StringComparison.OrdinalIgnoreCase);
                    profiles.Add(new LaunchSourceTruthProfileItem
                    {
                        ProfileId = profile.Id,
                        Role = profile.Role,
                        Mode = profile.Mode,
                        SourceTruthCompatibility = serverEvidence ? "ServerHeadlessEvidenceOnly" : "Deferred",
                        EvidenceStatus = serverEvidence ? "EvidenceOnly" : "Deferred",
                    });
                }
            }
        }

        var sourceTruthGate = ReadEvidence("SourceTruthGate", sourceTruthGatePath, diagnostics);
        var runtimeReadiness = ReadEvidence("RuntimeReadiness", runtimeReadinessPath, diagnostics);
        var deferred = new List<AcceptanceStage>
        {
            new()
            {
                Name = "Client Preview",
                Status = "Deferred",
                Reason = "ClientHost and Runtime launch behavior remain deferred; this command does not launch processes.",
            },
        };
        var failed = diagnostics.Any(item => item.Severity == "Error") ||
            sourceTruthGate.Status is "Failed" or "Missing" or "Malformed" ||
            runtimeReadiness.Status is "Failed" or "Missing" or "Malformed";

        return new LaunchSourceTruthAlignmentReport
        {
            Status = failed ? "Failed" : "PartiallyProven",
            Project = BuildProjectSummary(resolution),
            LaunchProfiles = profiles.OrderBy(item => item.ProfileId, StringComparer.Ordinal).ToList(),
            SourceTruthGate = sourceTruthGate,
            RuntimeReadiness = runtimeReadiness,
            DeferredStages = deferred,
            Diagnostics = LaunchProfileResolver.Sort(diagnostics),
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    private static LaunchAlignmentEvidence ReadEvidence(string name, string path, List<LaunchDiagnostic> diagnostics)
    {
        if (string.IsNullOrWhiteSpace(path) || !File.Exists(path))
        {
            diagnostics.Add(LaunchProfileResolver.Error(
                "Launch.SourceTruthAlignment.EvidenceMissing",
                "SourceTruthAlignment",
                path,
                $"{name} evidence is required."));
            return new LaunchAlignmentEvidence { Name = name, Status = "Missing", EvidencePath = LaunchProfileResolver.Normalize(path) };
        }

        try
        {
            var root = JsonNode.Parse(File.ReadAllText(path)) as JsonObject;
            var status = ReadString(root, "status");
            return new LaunchAlignmentEvidence
            {
                Name = name,
                Status = string.IsNullOrWhiteSpace(status) ? "Malformed" : status,
                EvidencePath = LaunchProfileResolver.Normalize(path),
            };
        }
        catch (Exception e) when (e is JsonException or IOException or UnauthorizedAccessException)
        {
            diagnostics.Add(LaunchProfileResolver.Error(
                "Launch.SourceTruthAlignment.EvidenceMalformed",
                "SourceTruthAlignment",
                path,
                $"Evidence could not be read: {e.Message}"));
            return new LaunchAlignmentEvidence { Name = name, Status = "Malformed", EvidencePath = LaunchProfileResolver.Normalize(path) };
        }
    }

    private static ProjectReportSummary BuildProjectSummary(LaunchResolution resolution) =>
        new()
        {
            ProjectId = resolution.Project?.ProjectId ?? "",
            DisplayName = resolution.Project?.DisplayName ?? "",
            ProjectRoot = resolution.ProjectRoot,
            ManifestPath = resolution.ManifestPath,
        };

    private static string ReadString(JsonObject? obj, string key) =>
        obj is not null &&
        obj.TryGetPropertyValue(key, out var node) &&
        node is JsonValue value &&
        value.TryGetValue<string>(out var text)
            ? text
            : "";
}
