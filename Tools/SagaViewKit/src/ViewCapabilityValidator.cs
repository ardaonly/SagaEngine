using System.Text.Json;

namespace SagaViewKit;

internal static class ViewCapabilityValidator
{
    private static readonly HashSet<string> ViewKinds = new(StringComparer.Ordinal)
    {
        "Simple", "Pro", "CSharpSource", "Diagnostics", "TeamRoom"
    };

    private static readonly HashSet<string> ApiLevels = new(StringComparer.Ordinal)
    {
        "High", "Low"
    };

    private static readonly HashSet<string> ApiDomains = new(StringComparer.Ordinal)
    {
        "Gameplay", "Runtime", "Server", "Networking", "UI", "Diagnostics", "Assets", "Packaging"
    };

    public static ViewCapabilityManifest LoadManifest(string path)
    {
        var text = File.ReadAllText(path);
        return JsonSerializer.Deserialize<ViewCapabilityManifest>(text, ReportWriter.Options)
            ?? throw new InvalidOperationException("View capability manifest is empty.");
    }

    public static ValidationResult Validate(ViewCapabilityManifest manifest, string evidenceRoot)
    {
        var violations = new List<Violation>();
        var diagnostics = new List<Diagnostic>();

        if (manifest.SchemaVersion != 1)
        {
            violations.Add(Violate("View.Schema.Unsupported", "Only schemaVersion 1 is supported.", manifest));
        }
        if (string.IsNullOrWhiteSpace(manifest.ViewId))
        {
            violations.Add(Violate("View.Manifest.MissingViewId", "viewId is required.", manifest));
        }
        if (!ViewKinds.Contains(manifest.ViewKind))
        {
            violations.Add(Violate("View.Manifest.UnsupportedViewKind", $"Unsupported viewKind '{manifest.ViewKind}'.", manifest));
        }
        foreach (var level in manifest.AllowedApiLevels)
        {
            if (!ApiLevels.Contains(level))
            {
                violations.Add(Violate("View.Manifest.UnsupportedApiLevel", $"Unsupported API level '{level}'.", manifest));
            }
        }
        foreach (var domain in manifest.AllowedApiDomains)
        {
            if (!ApiDomains.Contains(domain))
            {
                violations.Add(Violate("View.Manifest.UnsupportedApiDomain", $"Unsupported API domain '{domain}'.", manifest));
            }
        }

        if (manifest.ViewKind == "Simple")
        {
            if (manifest.CanEditUnsupportedNodes)
            {
                violations.Add(Violate("View.Simple.UnsupportedNodeMarkedEditable", "Simple View cannot mark unsupported nodes editable.", manifest));
            }
            if (manifest.CanMutateSource || manifest.CanRegenerateSource)
            {
                violations.Add(Violate("View.Simple.SourceMutationCapabilityForbidden", "Simple View cannot claim source rewrite or regeneration.", manifest));
            }
            if (manifest.CanApplyPatch)
            {
                violations.Add(Violate("View.Simple.PatchApplyCapabilityForbidden", "Simple View cannot claim patch application.", manifest));
            }
            if (manifest.CanShowOpaqueNodes && !manifest.MustDiscloseOpaqueNodes)
            {
                violations.Add(Violate("View.Simple.OpaqueDisclosureMissing", "Simple View must disclose opaque nodes when it can show them.", manifest));
            }
        }

        if (!string.IsNullOrWhiteSpace(evidenceRoot))
        {
            foreach (var artifact in manifest.RequiredEvidenceArtifacts.Where(item => item.Required))
            {
                var fullPath = Path.GetFullPath(Path.Combine(evidenceRoot, artifact.Path));
                if (!File.Exists(fullPath))
                {
                    violations.Add(new Violation
                    {
                        Code = "View.Simple.RequiredEvidenceMissing",
                        Message = $"Required evidence artifact is missing: {artifact.Path}",
                        ViewId = manifest.ViewId,
                        Path = artifact.Path,
                    });
                }
            }
        }

        diagnostics.Add(new Diagnostic
        {
            Severity = violations.Count == 0 ? "Info" : "Error",
            Code = violations.Count == 0 ? "View.Manifest.Valid" : "View.Manifest.Invalid",
            Message = violations.Count == 0 ? "View capability manifest passed." : "View capability manifest failed.",
        });
        return new ValidationResult(violations.Count == 0, violations, diagnostics);
    }

    public static ProfileSummary Summarize(ViewCapabilityManifest manifest) => new()
    {
        ViewId = manifest.ViewId,
        ViewKind = manifest.ViewKind,
        AllowedApiDomains = manifest.AllowedApiDomains.Order(StringComparer.Ordinal).ToArray(),
        AllowedApiLevels = manifest.AllowedApiLevels.Order(StringComparer.Ordinal).ToArray(),
        CanEditUnsupportedNodes = manifest.CanEditUnsupportedNodes,
        CanApplyPatch = manifest.CanApplyPatch,
        CanMutateSource = manifest.CanMutateSource,
        CanRegenerateSource = manifest.CanRegenerateSource,
    };

    private static Violation Violate(string code, string message, ViewCapabilityManifest manifest) => new()
    {
        Code = code,
        Message = message,
        ViewId = manifest.ViewId,
    };
}
