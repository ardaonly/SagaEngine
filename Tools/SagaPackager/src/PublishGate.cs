using System.Text.Json.Nodes;

namespace SagaPackager;

internal static class PublishGate
{
    public static PublishReport Check(
        string projectPath,
        string profileId,
        string packageReportPath,
        string diagnosticsSummaryPath,
        string scriptValidationPath = "",
        string policyReportPath = "",
        string reviewApprovalReportPath = "",
        string auditReportPath = "",
        string restrictedExportReportPath = "")
    {
        var resolved = new ProjectResolver().Resolve(projectPath, profileId);
        var diagnostics = resolved.Diagnostics.ToList();
        var gates = new List<GateResult>();
        var evidence = new List<CheckedInput>();

        AddEvidence(evidence, "packageReport", packageReportPath);
        AddEvidence(evidence, "diagnosticsSummary", diagnosticsSummaryPath);
        if (!string.IsNullOrWhiteSpace(scriptValidationPath))
        {
            AddEvidence(evidence, "scriptValidation", scriptValidationPath);
        }
        if (!string.IsNullOrWhiteSpace(policyReportPath))
        {
            AddEvidence(evidence, "policyReport", policyReportPath);
        }
        if (!string.IsNullOrWhiteSpace(reviewApprovalReportPath))
        {
            AddEvidence(evidence, "reviewApprovalReport", reviewApprovalReportPath);
        }
        if (!string.IsNullOrWhiteSpace(auditReportPath))
        {
            AddEvidence(evidence, "auditReport", auditReportPath);
        }
        if (!string.IsNullOrWhiteSpace(restrictedExportReportPath))
        {
            AddEvidence(evidence, "restrictedExportReport", restrictedExportReportPath);
        }

        AddGate(gates, "ProjectValid", ProjectResolver.HasErrors(resolved.Diagnostics) ? "Blocked" : "Passed", resolved.ManifestPath, "Project/profile validation gate.");
        AddGate(gates, "PackageProfileValid", resolved.Profile is null ? "Blocked" : "Passed", resolved.PackageProfilesPath, "Package profile gate.");

        JsonObject? packageReport = LoadJson(packageReportPath, diagnostics, "Publish.PackageReport");
        JsonObject? diagnosticsSummary = LoadJson(diagnosticsSummaryPath, diagnostics, "Publish.DiagnosticsSummary");
        JsonObject? scriptValidation = string.IsNullOrWhiteSpace(scriptValidationPath)
            ? null
            : LoadJson(scriptValidationPath, diagnostics, "Publish.ScriptValidation");
        JsonObject? policyReport = string.IsNullOrWhiteSpace(policyReportPath)
            ? null
            : LoadJson(policyReportPath, diagnostics, "Publish.PolicyReport");
        JsonObject? reviewApprovalReport = string.IsNullOrWhiteSpace(reviewApprovalReportPath)
            ? null
            : LoadJson(reviewApprovalReportPath, diagnostics, "Publish.ReviewApprovalReport");
        JsonObject? auditReport = string.IsNullOrWhiteSpace(auditReportPath)
            ? null
            : LoadJson(auditReportPath, diagnostics, "Publish.AuditReport");
        JsonObject? restrictedExportReport = string.IsNullOrWhiteSpace(restrictedExportReportPath)
            ? null
            : LoadJson(restrictedExportReportPath, diagnostics, "Publish.RestrictedExportReport");

        var stagePassed = packageReport is not null &&
            ReadString(packageReport, "tool") == "sagapack" &&
            ReadString(packageReport, "command") == "stage" &&
            ReadString(packageReport, "status") == "Passed";
        AddGate(gates, "PackageStagePassed", stagePassed ? "Passed" : "Blocked", packageReportPath, "Stage report must be a passed sagapack stage report.");

        var packageManifestPath = ReadString(packageReport, "packageManifestPath");
        AddGate(gates, "PackageManifestExists", File.Exists(packageManifestPath) ? "Passed" : "Blocked", packageManifestPath, "Staged package manifest must exist.");

        var criticalCount = ReadInt(diagnosticsSummary?["summary"] as JsonObject, "criticalDiagnosticCount");
        var diagnosticsAccepted = diagnosticsSummary is not null && criticalCount == 0 && ReadString(diagnosticsSummary, "status") != "Failed";
        AddGate(gates, "DiagnosticsSummaryAccepted", diagnosticsAccepted ? "Passed" : "Blocked", diagnosticsSummaryPath, "Diagnostics summary must not contain critical diagnostics.");

        AddGate(gates, "LaunchProfileExists", string.IsNullOrWhiteSpace(resolved.Profile?.LaunchProfileId) ? "Blocked" : "Passed", resolved.Profile?.LaunchProfileId ?? "", "Package profile must reference a launch profile.");
        var scriptMetadataPath = ResolveStagedProfilePath(packageManifestPath, resolved.Profile?.ScriptMetadataPath ?? "");
        AddGate(
            gates,
            "ScriptMetadataAccepted",
            File.Exists(scriptMetadataPath) ? "Passed" : "NotApplicable",
            scriptMetadataPath,
            "Script metadata is recognized when present but is not required for the server/headless package profile.");

        if (!string.IsNullOrWhiteSpace(scriptValidationPath))
        {
            var scriptValidationAccepted = scriptValidation is not null &&
                ReadString(scriptValidation, "tool") == "sagascript" &&
                ReadString(scriptValidation, "command") == "validate-artifacts" &&
                ReadString(scriptValidation, "status") == "Passed";
            AddGate(
                gates,
                "ScriptArtifactValidationAccepted",
                scriptValidationAccepted ? "Passed" : "Blocked",
                scriptValidationPath,
                "Script artifact validation must be a passed sagascript validate-artifacts report.");
        }

        if (!string.IsNullOrWhiteSpace(policyReportPath))
        {
            var decision = ReadString(policyReport, "decision");
            var policyAccepted = policyReport is not null &&
                decision is "Allow" or "Warn" &&
                ReadString(policyReport, "status") != "Failed";
            AddGate(
                gates,
                "PolicyReportAccepted",
                policyAccepted ? "Passed" : "Blocked",
                policyReportPath,
                "Policy report must be supplied and allow publish-check metadata evaluation.");
        }

        if (!string.IsNullOrWhiteSpace(reviewApprovalReportPath))
        {
            var reviewAccepted = reviewApprovalReport is not null &&
                ReadString(reviewApprovalReport, "tool") == "sagaworkspacehub" &&
                ReadString(reviewApprovalReport, "command") == "review-approval" &&
                ReadString(reviewApprovalReport, "status") == "Passed" &&
                ReadString(reviewApprovalReport, "decision") == "ApprovedMetadataOnly";
            AddGate(
                gates,
                "ReviewApprovalAccepted",
                reviewAccepted ? "Passed" : "Blocked",
                reviewApprovalReportPath,
                "Review approval report must be approved metadata-only evidence.");
        }

        if (!string.IsNullOrWhiteSpace(auditReportPath))
        {
            var auditAccepted = auditReport is not null &&
                ReadString(auditReport, "tool") == "sagaworkspacehub" &&
                ReadString(auditReport, "command") == "audit-log" &&
                ReadString(auditReport, "status") == "Passed";
            AddGate(
                gates,
                "AuditEvidenceAccepted",
                auditAccepted ? "Passed" : "Blocked",
                auditReportPath,
                "Audit report must be passed local audit evidence.");
        }

        if (!string.IsNullOrWhiteSpace(restrictedExportReportPath))
        {
            var counts = restrictedExportReport?["counts"] as JsonObject;
            var restrictedExportAccepted = restrictedExportReport is not null &&
                ReadString(restrictedExportReport, "tool") == "sagaworkspacehub" &&
                ReadString(restrictedExportReport, "command") == "restricted-export" &&
                ReadString(restrictedExportReport, "status") == "Passed" &&
                ReadInt(counts, "blockedExports") == 0;
            AddGate(
                gates,
                "RestrictedExportAccepted",
                restrictedExportAccepted ? "Passed" : "Blocked",
                restrictedExportReportPath,
                "Restricted export report must have no blocked exports.");
        }

        if (gates.Any(g => g.Status == "Blocked" || g.Status == "Failed"))
        {
            diagnostics.Add(ProjectResolver.Error(
                "Publish.Gate.Blocked",
                "Publish",
                packageReportPath,
                "One or more publish-check gates are blocked."));
        }

        diagnostics = ProjectResolver.Sort(diagnostics);
        return new PublishReport
        {
            Status = gates.Any(g => g.Status == "Failed") ? "Failed" :
                gates.Any(g => g.Status == "Blocked") ? "Blocked" : "Passed",
            Project = ProjectResolver.ProjectSummary(resolved),
            PackageProfile = ProjectResolver.ProfileSummary(resolved.Profile),
            RequiredEvidence = evidence.OrderBy(e => e.Kind, StringComparer.Ordinal).ToList(),
            Gates = gates.OrderBy(g => g.Name, StringComparer.Ordinal).ToList(),
            Diagnostics = diagnostics,
        };
    }

    private static JsonObject? LoadJson(
        string path,
        List<PackageDiagnostic> diagnostics,
        string codePrefix)
    {
        if (string.IsNullOrWhiteSpace(path) || !File.Exists(path))
        {
            diagnostics.Add(ProjectResolver.Error(
                $"{codePrefix}.Missing",
                "Publish",
                path,
                "Required publish-check evidence is missing."));
            return null;
        }
        try
        {
            return JsonNode.Parse(File.ReadAllText(path)) as JsonObject;
        }
        catch (Exception e)
        {
            diagnostics.Add(ProjectResolver.Error(
                $"{codePrefix}.InvalidJson",
                "Publish",
                path,
                $"Required publish-check evidence JSON is invalid: {e.Message}"));
            return null;
        }
    }

    private static void AddEvidence(List<CheckedInput> evidence, string kind, string path)
    {
        var fullPath = string.IsNullOrWhiteSpace(path) ? "" : Path.GetFullPath(path);
        var exists = fullPath.Length > 0 && File.Exists(fullPath);
        evidence.Add(new CheckedInput
        {
            Kind = kind,
            Path = ProjectResolver.Normalize(fullPath),
            Exists = exists,
            Status = exists ? "Present" : "Missing",
        });
    }

    private static void AddGate(
        List<GateResult> gates,
        string name,
        string status,
        string evidencePath,
        string message)
    {
        gates.Add(new GateResult
        {
            Name = name,
            Status = status,
            EvidencePath = ProjectResolver.Normalize(evidencePath),
            Message = message,
        });
    }

    private static string ResolveStagedProfilePath(string packageManifestPath, string profileRelativePath)
    {
        if (string.IsNullOrWhiteSpace(packageManifestPath) || string.IsNullOrWhiteSpace(profileRelativePath))
        {
            return profileRelativePath;
        }

        var root = Path.GetDirectoryName(packageManifestPath);
        return string.IsNullOrWhiteSpace(root)
            ? profileRelativePath
            : Path.GetFullPath(Path.Combine(root, profileRelativePath));
    }

    private static string ReadString(JsonObject? obj, string key)
    {
        return obj is not null &&
            obj.TryGetPropertyValue(key, out var node) &&
            node is JsonValue value &&
            value.TryGetValue<string>(out var text)
                ? text
                : "";
    }

    private static int ReadInt(JsonObject? obj, string key)
    {
        return obj is not null &&
            obj.TryGetPropertyValue(key, out var node) &&
            node is JsonValue value &&
            value.TryGetValue<int>(out var number)
                ? number
                : 0;
    }
}
