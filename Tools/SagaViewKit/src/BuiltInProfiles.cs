namespace SagaViewKit;

internal static class BuiltInProfiles
{
    public static IReadOnlyList<ViewCapabilityManifest> All { get; } =
    [
        SimpleView(),
        ProView(),
        CSharpSourceView(),
        DiagnosticsView(),
        TeamRoomView(),
    ];

    public static ViewCapabilityManifest SimpleView() => new()
    {
        ViewId = "simple-view",
        ViewKind = "Simple",
        AllowedApiDomains = ["Gameplay"],
        AllowedApiLevels = ["High"],
        CanShowOpaqueNodes = true,
        MustDiscloseOpaqueNodes = true,
        CanEditSupportedNodes = false,
        CanEditUnsupportedNodes = false,
        CanApplyPatch = false,
        CanMutateSource = false,
        CanRegenerateSource = false,
        CanShowSourceLinks = false,
        CanShowDiagnostics = true,
        CanShowCollaborationMetadata = false,
        RequiredEvidenceArtifacts =
        [
            new EvidenceArtifact { Kind = "projection", Path = "Build/SagaScript/projection_report.json" },
            new EvidenceArtifact { Kind = "nodeMetadata", Path = "Build/SagaScript/node_metadata.json", Required = false },
        ],
    };

    public static ViewCapabilityManifest ProView() => new()
    {
        ViewId = "pro-view",
        ViewKind = "Pro",
        AllowedApiDomains = ["Gameplay", "Runtime", "Server", "Networking", "UI", "Diagnostics", "Assets", "Packaging"],
        AllowedApiLevels = ["High", "Low"],
        CanShowOpaqueNodes = true,
        MustDiscloseOpaqueNodes = true,
        CanEditSupportedNodes = false,
        CanEditUnsupportedNodes = false,
        CanApplyPatch = false,
        CanMutateSource = false,
        CanRegenerateSource = false,
        CanShowSourceLinks = true,
        CanShowDiagnostics = true,
        CanShowCollaborationMetadata = false,
        RequiredEvidenceArtifacts =
        [
            new EvidenceArtifact { Kind = "projection", Path = "Build/SagaScript/projection_report.json" },
            new EvidenceArtifact { Kind = "sourceMap", Path = "Build/SagaScript/source_map.json" },
        ],
    };

    public static ViewCapabilityManifest CSharpSourceView() => new()
    {
        ViewId = "csharp-source-view",
        ViewKind = "CSharpSource",
        AllowedApiDomains = ["Gameplay", "Runtime", "Server", "Networking", "UI", "Diagnostics", "Assets", "Packaging"],
        AllowedApiLevels = ["High", "Low"],
        CanShowOpaqueNodes = true,
        MustDiscloseOpaqueNodes = true,
        CanEditSupportedNodes = false,
        CanEditUnsupportedNodes = false,
        CanApplyPatch = false,
        CanMutateSource = false,
        CanRegenerateSource = false,
        CanShowSourceLinks = true,
        CanShowDiagnostics = true,
        CanShowCollaborationMetadata = false,
        RequiredEvidenceArtifacts =
        [
            new EvidenceArtifact { Kind = "sourceMap", Path = "Build/SagaScript/source_map.json" },
        ],
    };

    public static ViewCapabilityManifest DiagnosticsView() => new()
    {
        ViewId = "diagnostics-view",
        ViewKind = "Diagnostics",
        AllowedApiDomains = ["Diagnostics"],
        AllowedApiLevels = ["High", "Low"],
        CanShowOpaqueNodes = false,
        MustDiscloseOpaqueNodes = false,
        CanEditSupportedNodes = false,
        CanEditUnsupportedNodes = false,
        CanApplyPatch = false,
        CanMutateSource = false,
        CanRegenerateSource = false,
        CanShowSourceLinks = false,
        CanShowDiagnostics = true,
        CanShowCollaborationMetadata = false,
        RequiredEvidenceArtifacts =
        [
            new EvidenceArtifact { Kind = "diagnosticsSummary", Path = "Build/Reports/diagnostics_summary.json", Required = false },
        ],
    };

    public static ViewCapabilityManifest TeamRoomView() => new()
    {
        ViewId = "team-room-view",
        ViewKind = "TeamRoom",
        AllowedApiDomains = ["Diagnostics", "Gameplay"],
        AllowedApiLevels = ["High", "Low"],
        CanShowOpaqueNodes = false,
        MustDiscloseOpaqueNodes = false,
        CanEditSupportedNodes = false,
        CanEditUnsupportedNodes = false,
        CanApplyPatch = false,
        CanMutateSource = false,
        CanRegenerateSource = false,
        CanShowSourceLinks = false,
        CanShowDiagnostics = true,
        CanShowCollaborationMetadata = true,
        RequiredEvidenceArtifacts =
        [
            new EvidenceArtifact { Kind = "collaborationMetadata", Path = ".saga/collaboration/workspace_transactions.jsonl", Required = false },
            new EvidenceArtifact { Kind = "presenceReport", Path = "Build/Collaboration/presence_report.json", Required = false },
        ],
    };

    public static bool TryGet(string idOrKind, out ViewCapabilityManifest manifest)
    {
        manifest = All.FirstOrDefault(profile =>
            string.Equals(profile.ViewId, idOrKind, StringComparison.OrdinalIgnoreCase) ||
            string.Equals(profile.ViewKind, idOrKind, StringComparison.OrdinalIgnoreCase) ||
            string.Equals(Normalize(profile.ViewId), Normalize(idOrKind), StringComparison.OrdinalIgnoreCase))!;
        return manifest is not null;
    }

    private static string Normalize(string text) => text.Replace("-", "").Replace("_", "");
}
