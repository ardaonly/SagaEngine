using System.Text.Json;

namespace SagaViewKit;

internal static class SimpleViewHonestyValidator
{
    public static SimpleViewHonestyReport Check(
        string projectionPath,
        ViewCapabilityManifest profile,
        string nodeMetadataPath,
        string evidenceRoot)
    {
        var violations = new List<Violation>();
        var diagnostics = new List<Diagnostic>();

        var profileValidation = ViewCapabilityValidator.Validate(
            ProfileWithCheckedArtifacts(profile, projectionPath, nodeMetadataPath),
            evidenceRoot);
        violations.AddRange(profileValidation.Violations);

        var projectionNodes = LoadProjectionNodes(projectionPath);
        var projectionNodeIds = projectionNodes.Select(node => node.NodeId).ToHashSet(StringComparer.Ordinal);
        var opaqueCount = 0;
        var unsupportedCount = 0;

        foreach (var node in projectionNodes)
        {
            var advanced = IsAdvanced(node, profile);
            if (!advanced)
            {
                continue;
            }

            if (node.Kind == "Opaque" || !string.IsNullOrWhiteSpace(node.OpaqueReason))
            {
                ++opaqueCount;
            }
            if (node.BehaviorCompatibility != "Supported" ||
                node.Kind == "Opaque" ||
                !string.IsNullOrWhiteSpace(node.OpaqueReason))
            {
                ++unsupportedCount;
            }

            if (!node.ReadOnly || profile.CanEditUnsupportedNodes)
            {
                violations.Add(new Violation
                {
                    Code = "View.Simple.UnsupportedNodeMarkedEditable",
                    Message = "Advanced or unsupported node is editable in Simple View.",
                    ViewId = profile.ViewId,
                    NodeId = node.NodeId,
                });
            }
            if (profile.MustDiscloseOpaqueNodes && string.IsNullOrWhiteSpace(node.OpaqueReason))
            {
                violations.Add(new Violation
                {
                    Code = "View.Simple.OpaqueDisclosureMissing",
                    Message = "Advanced or unsupported node is missing an opaque disclosure reason.",
                    ViewId = profile.ViewId,
                    NodeId = node.NodeId,
                });
            }
        }

        if (!string.IsNullOrWhiteSpace(nodeMetadataPath))
        {
            foreach (var node in LoadMetadataNodes(nodeMetadataPath))
            {
                if (IsAdvanced(node, profile) && !projectionNodeIds.Contains(node.NodeId))
                {
                    violations.Add(new Violation
                    {
                        Code = "View.Simple.AdvancedRegionHidden",
                        Message = "Advanced node metadata is not represented in the Simple View projection.",
                        ViewId = profile.ViewId,
                        NodeId = node.NodeId,
                    });
                }
            }
        }

        diagnostics.Add(new Diagnostic
        {
            Severity = violations.Count == 0 ? "Info" : "Error",
            Code = violations.Count == 0 ? "View.Simple.Passed" : "View.Simple.Failed",
            Message = violations.Count == 0 ? "Simple View honesty check passed." : "Simple View honesty check failed.",
            Path = projectionPath,
        });

        return new SimpleViewHonestyReport
        {
            Status = violations.Count == 0 ? "Passed" : "Failed",
            CheckedProjection = Path.GetFullPath(projectionPath),
            ViewProfile = ViewCapabilityValidator.Summarize(profile),
            Violations = violations.OrderBy(v => v.Code, StringComparer.Ordinal)
                .ThenBy(v => v.NodeId, StringComparer.Ordinal)
                .ThenBy(v => v.Path, StringComparer.Ordinal)
                .ToArray(),
            OpaqueNodeCount = opaqueCount,
            UnsupportedNodeCount = unsupportedCount,
            Diagnostics = diagnostics,
        };
    }

    private static bool IsAdvanced(ProjectionNode node, ViewCapabilityManifest profile)
    {
        return !profile.AllowedApiLevels.Contains(node.ApiLevel, StringComparer.Ordinal) ||
               !profile.AllowedApiDomains.Contains(node.ApiDomain, StringComparer.Ordinal) ||
               node.BehaviorCompatibility != "Supported" ||
               node.Capability == "Deferred" ||
               node.ProjectionCompatibility is "Deferred" or "Unsupported" or "Opaque" ||
               node.Kind == "Opaque" ||
               !string.IsNullOrWhiteSpace(node.OpaqueReason);
    }

    private static bool IsAdvanced(MetadataNode node, ViewCapabilityManifest profile)
    {
        var apiLevel = node.Level == "HighLevel" ? "High" :
            node.Level == "LowLevel" ? "Low" : node.Level;
        return !profile.AllowedApiLevels.Contains(apiLevel, StringComparer.Ordinal) ||
               !profile.AllowedApiDomains.Contains(node.Domain, StringComparer.Ordinal) ||
               node.Capability == "Deferred" ||
               node.ProjectionCompatibility is "Deferred" or "Unsupported" or "Opaque" ||
               !node.ReadOnly;
    }

    private static ViewCapabilityManifest ProfileWithCheckedArtifacts(
        ViewCapabilityManifest profile,
        string projectionPath,
        string nodeMetadataPath)
    {
        var artifacts = profile.RequiredEvidenceArtifacts
            .Select(artifact => artifact.Kind switch
            {
                "projection" => artifact with { Path = Path.GetFullPath(projectionPath) },
                "nodeMetadata" when !string.IsNullOrWhiteSpace(nodeMetadataPath) =>
                    artifact with { Path = Path.GetFullPath(nodeMetadataPath) },
                _ => artifact,
            })
            .ToArray();
        return profile with { RequiredEvidenceArtifacts = artifacts };
    }

    private static IReadOnlyList<ProjectionNode> LoadProjectionNodes(string path)
    {
        using var document = JsonDocument.Parse(File.ReadAllText(path));
        var root = document.RootElement;
        var result = new List<ProjectionNode>();

        foreach (var behavior in root.GetProperty("behaviors").EnumerateArray())
        {
            var compatibility = ReadString(behavior, "compatibility", "Supported");
            var behaviorLevel = ReadString(behavior, "apiLevel", "");
            var behaviorDomain = ReadString(behavior, "apiDomain", "");
            foreach (var node in behavior.GetProperty("nodes").EnumerateArray())
            {
                result.Add(new ProjectionNode(
                    ReadString(node, "nodeId", ""),
                    ReadString(node, "kind", ""),
                    ReadString(node, "apiLevel", behaviorLevel),
                    ReadString(node, "apiDomain", behaviorDomain),
                    ReadString(node, "capability", "ProjectionOnly"),
                    ReadString(node, "projectionCompatibility", "ReadOnly"),
                    ReadBool(node, "readOnly", true),
                    ReadString(node, "opaqueReason", ""),
                    compatibility));
            }
        }

        return result.OrderBy(node => node.NodeId, StringComparer.Ordinal).ToArray();
    }

    private static IReadOnlyList<MetadataNode> LoadMetadataNodes(string path)
    {
        using var document = JsonDocument.Parse(File.ReadAllText(path));
        var result = new List<MetadataNode>();

        foreach (var node in document.RootElement.GetProperty("nodes").EnumerateArray())
        {
            result.Add(new MetadataNode(
                ReadString(node, "nodeId", ""),
                ReadString(node, "level", ""),
                ReadString(node, "domain", ""),
                ReadString(node, "capability", ""),
                ReadString(node, "projectionCompatibility", ""),
                ReadBool(node, "readOnly", true)));
        }

        return result.OrderBy(node => node.NodeId, StringComparer.Ordinal).ToArray();
    }

    private static string ReadString(JsonElement element, string property, string fallback)
    {
        return element.TryGetProperty(property, out var value) && value.ValueKind == JsonValueKind.String
            ? value.GetString() ?? fallback
            : fallback;
    }

    private static bool ReadBool(JsonElement element, string property, bool fallback)
    {
        return element.TryGetProperty(property, out var value) && value.ValueKind is JsonValueKind.True or JsonValueKind.False
            ? value.GetBoolean()
            : fallback;
    }
}
