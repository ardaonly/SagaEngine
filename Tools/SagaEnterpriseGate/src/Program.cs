using System.Text.Json;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace SagaEnterpriseGate;

internal static class Program
{
    private const int Passed = 0;
    private const int Failed = 1;
    private const int InvalidUsage = 2;
    private const int InternalError = 4;
    private static readonly RegexOptions RegexOptions =
        RegexOptions.IgnoreCase | RegexOptions.CultureInvariant | RegexOptions.Compiled;
    private static readonly TimeSpan RegexTimeout = TimeSpan.FromMilliseconds(100);

    private static readonly IReadOnlyList<string> OwnReportNames =
    [
        "enterprise_evidence_report.json",
        "enterprise_acceptance_report.json",
        "enterprise_closure_report.json",
    ];

    private static readonly IReadOnlyList<string> ResidualDebtDefaults =
    [
        "Hedef 2 deferred Editor UI / Qt UI remains explicit.",
        "Hedef 2 deferred ClientHost preview remains explicit.",
        "Hedef 2 deferred gameplay expansion remains explicit.",
        "Asset workflow source-of-truth debt remains explicit.",
        "Raw full CTest remains unclaimed/missing.",
    ];

    private static readonly IReadOnlyList<ForbiddenClaimRule> ForbiddenClaimRules =
    [
        new("EnterpriseReadiness", @"\benterprise[- ]ready\b"),
        new("ProductionReadiness", @"\bproduction[- ]ready\b"),
        new("ReleaseCandidate", @"\brelease\s+candidate\b"),
        new("ProductBeta", @"\bproduct\s+beta\b|\bbeta\s+product\b"),
        new("SecureCloud", @"\bsecure\s+cloud\s+platform\b"),
        new("CertificationReadiness", @"\bSOC2\b|\bISO\s+27001\b|\bcompliance[- ]ready\b"),
        new("FullSecurityModel", @"\bfull\s+security\s+model\b"),
        new("RealPermissionEnforcement", @"\breal\s+permission\s+enforcement\b|\bpermission\s+system\s+implemented\b"),
        new("CloudWorkspace", @"\bcloud\s+workspace\b"),
        new("FullCollaboration", @"\bfull\s+collaboration\b"),
        new("ProductionAuditSecurityCompliance", @"\blegal\s+audit\s+compliance\b|\bproduction\s+audit\b|\bzero[- ]trust\b"),
    ];

    private static readonly IReadOnlyList<string> ClosureBlockedClaimIds =
    [
        "EnterpriseReadiness",
        "ProductionReadiness",
        "ReleaseCandidate",
        "ProductBeta",
        "SecureCloud",
        "CertificationReadiness",
        "FullSecurityModel",
        "RealPermissionEnforcement",
        "CloudWorkspace",
        "FullCollaboration",
        "ProductionAuditSecurityCompliance",
    ];

    public static int Main(string[] args)
    {
        try
        {
            if (args.Length == 0 || args[0] is "-h" or "--help" or "help")
            {
                PrintUsage();
                return args.Length == 0 ? InvalidUsage : Passed;
            }

            var command = args[0];
            var options = CommandOptions.Parse(args.Skip(1).ToArray());
            if (!options.Ok)
            {
                Console.Error.WriteLine(options.Error);
                PrintUsage();
                return InvalidUsage;
            }

            return command switch
            {
                "policy-regression" => RunPolicyRegression(options),
                "evidence-gate" => RunEvidenceGate(options),
                "acceptance-scenario" => RunAcceptanceScenario(options),
                "closure-check" => RunClosureCheck(options),
                _ => UnknownCommand(command),
            };
        }
        catch (Exception e)
        {
            Console.Error.WriteLine($"Internal sagaenterprisegate error: {e.Message}");
            return InternalError;
        }
    }

    private static int RunPolicyRegression(CommandOptions options)
    {
        if (MissingRequired(options.EvidenceRoot, "--evidence-root") ||
            MissingRequired(options.OutPath, "--out"))
        {
            return InvalidUsage;
        }

        var diagnostics = BaseDiagnostics();
        var root = Path.GetFullPath(options.EvidenceRoot);
        var evidence = LoadEvidence(root, diagnostics);
        var checks = new[]
        {
            CheckPolicyDecision(evidence, "PolicyAllow", "Allow", "Policy allow evidence is present."),
            CheckPolicyDecision(evidence, "PolicyDeny", "Deny", "Policy deny evidence is present."),
            CheckPolicyDecision(evidence, "DangerousOperationRequiresReview", "RequiresReview", "Dangerous operation remains review-gated."),
            CheckRestrictedSlice(evidence),
            CheckHiddenSource(evidence),
            CheckPublishBlockedByPolicy(evidence),
            CheckReviewBlockedByStaleApproval(evidence),
            CheckRestrictedExportBlocksHidden(evidence),
            CheckWorkspaceHubReportOnly(evidence),
        }.OrderBy(item => item.CheckId, StringComparer.Ordinal).ToArray();

        var summary = new RegressionSummary
        {
            Passed = checks.Count(item => item.Status == "Passed"),
            Failed = checks.Count(item => item.Status == "Failed"),
            MissingEvidence = checks.Count(item => item.Status == "MissingEvidence"),
            Total = checks.Length,
        };
        var status = summary.Failed == 0 && summary.MissingEvidence == 0 ? "Passed" : "Failed";
        if (status != "Passed")
        {
            diagnostics.Add(Error("EnterpriseGate.PolicyRegression.Failed", "One or more local policy regression checks failed or are missing evidence.", root));
        }

        var report = new PolicyRegressionReport
        {
            Status = status,
            Checks = checks,
            Summary = summary,
            MutatesSource = false,
            Enforcement = "ReportOnly",
            Diagnostics = SortDiagnostics(diagnostics),
        };
        ReportWriter.Write(options.OutPath, report);
        return status == "Passed" ? Passed : Failed;
    }

    private static int RunEvidenceGate(CommandOptions options)
    {
        if (MissingRequired(options.EvidenceRoot, "--evidence-root") ||
            MissingRequired(options.OutPath, "--out"))
        {
            return InvalidUsage;
        }

        var diagnostics = BaseDiagnostics();
        var root = Path.GetFullPath(options.EvidenceRoot);
        var evidence = LoadEvidenceSet(root, diagnostics, "evidence-gate");
        var categories = BuildEvidenceGateCategories(evidence).OrderBy(item => item.Category, StringComparer.Ordinal).ToArray();
        var residualDebt = ExtractResidualDebt(evidence);
        AddForbiddenClaimDiagnostics(evidence, diagnostics);

        var summary = Summarize(categories);
        var hasClaimBlock = diagnostics.Any(item => item.Severity == "Error" && item.Code == "EnterpriseGate.Claim.PositiveClaim");
        var status = DetermineGateStatus(summary, residualDebt.Count, hasClaimBlock);
        if (status is "MissingEvidence" or "Blocked")
        {
            diagnostics.Add(Error("EnterpriseGate.EvidenceGate.Incomplete", "Required local/report-only evidence is missing or blocked.", root));
        }

        var report = new EvidenceGateReport
        {
            Status = status,
            Categories = categories,
            Summary = summary,
            ResidualDebt = residualDebt,
            Diagnostics = SortDiagnostics(diagnostics),
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
        ReportWriter.Write(options.OutPath, report);
        return status is "Passed" or "PartiallyProven" ? Passed : Failed;
    }

    private static int RunAcceptanceScenario(CommandOptions options)
    {
        if (MissingRequired(options.EvidenceRoot, "--evidence-root") ||
            MissingRequired(options.OutPath, "--out"))
        {
            return InvalidUsage;
        }

        var diagnostics = BaseDiagnostics();
        var root = Path.GetFullPath(options.EvidenceRoot);
        var evidence = LoadEvidenceSet(root, diagnostics, "acceptance-scenario");
        var steps = BuildAcceptanceSteps(evidence).OrderBy(item => item.StepId, StringComparer.Ordinal).ToArray();
        var residualDebt = ExtractResidualDebt(evidence);
        AddForbiddenClaimDiagnostics(evidence, diagnostics);

        var summary = new AcceptanceSummary
        {
            Passed = steps.Count(item => item.Status == "Passed"),
            PartiallyProven = steps.Count(item => item.Status == "PartiallyProven"),
            MissingEvidence = steps.Count(item => item.Status == "MissingEvidence"),
            Blocked = steps.Count(item => item.Status == "Blocked"),
            Total = steps.Length,
        };
        var hasClaimBlock = diagnostics.Any(item => item.Severity == "Error" && item.Code == "EnterpriseGate.Claim.PositiveClaim");
        var status = DetermineAcceptanceStatus(summary, residualDebt.Count, hasClaimBlock);
        if (status is "MissingEvidence" or "Blocked")
        {
            diagnostics.Add(Error("EnterpriseGate.Acceptance.Incomplete", "The bounded local governance scenario is missing required evidence or is blocked.", root));
        }

        var report = new AcceptanceScenarioReport
        {
            Status = status,
            Steps = steps,
            Summary = summary,
            ResidualDebt = residualDebt,
            Diagnostics = SortDiagnostics(diagnostics),
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
        ReportWriter.Write(options.OutPath, report);
        return status is "Passed" or "PartiallyProven" ? Passed : Failed;
    }

    private static int RunClosureCheck(CommandOptions options)
    {
        if (MissingRequired(options.EvidenceRoot, "--evidence-root") ||
            MissingRequired(options.OutPath, "--out"))
        {
            return InvalidUsage;
        }

        var diagnostics = BaseDiagnostics();
        var root = Path.GetFullPath(options.EvidenceRoot);
        var evidence = LoadEvidenceSet(root, diagnostics, "closure-check");
        var matrix = BuildClosureEvidenceMatrix(evidence).OrderBy(item => item.Category, StringComparer.Ordinal).ToArray();
        var residualDebt = ExtractResidualDebt(evidence);
        AddForbiddenClaimDiagnostics(evidence, diagnostics);

        var summary = Summarize(matrix);
        var hasClaimBlock = diagnostics.Any(item => item.Severity == "Error" && item.Code == "EnterpriseGate.Claim.PositiveClaim");
        var outcome = DetermineClosureOutcome(summary, matrix, residualDebt.Count, hasClaimBlock);
        if (outcome == "Blocked")
        {
            diagnostics.Add(Error("EnterpriseGate.Closure.Blocked", "Closure is blocked by missing evidence, blocked evidence, or forbidden positive claim wording.", root));
        }

        var report = new ClosureCheckReport
        {
            Outcome = outcome,
            EvidenceMatrix = matrix,
            Summary = summary,
            ResidualDebt = residualDebt,
            BlockedClaims = ClosureBlockedClaimIds,
            Diagnostics = SortDiagnostics(diagnostics),
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
        ReportWriter.Write(options.OutPath, report);
        return outcome is "FoundationEstablished" or "PartiallyProven" ? Passed : Failed;
    }

    private static EvidenceSet LoadEvidenceSet(string root, List<Diagnostic> diagnostics, string command)
    {
        var documents = LoadEvidence(root, diagnostics, command);
        return new EvidenceSet(root, documents);
    }

    private static IReadOnlyList<EvidenceDocument> LoadEvidence(string root, List<Diagnostic> diagnostics, string command = "")
    {
        if (!Directory.Exists(root))
        {
            diagnostics.Add(Error("EnterpriseGate.EvidenceRootMissing", "Evidence root is missing.", root));
            return Array.Empty<EvidenceDocument>();
        }

        var documents = new List<EvidenceDocument>();
        foreach (var path in Directory.EnumerateFiles(root, "*.json", SearchOption.AllDirectories).OrderBy(path => path, StringComparer.Ordinal))
        {
            var fileName = Path.GetFileName(path);
            if (OwnReportNames.Contains(fileName, StringComparer.OrdinalIgnoreCase))
            {
                if ((command == "acceptance-scenario" && fileName == "enterprise_evidence_report.json") ||
                    (command == "closure-check" && fileName is "enterprise_evidence_report.json" or "enterprise_acceptance_report.json"))
                {
                    // Later closure commands intentionally consume earlier gate outputs.
                }
                else
                {
                    continue;
                }
            }

            try
            {
                var node = JsonNode.Parse(File.ReadAllText(path)) as JsonObject;
                if (node is not null)
                {
                    documents.Add(new EvidenceDocument(Normalize(Path.GetFullPath(path)), node));
                }
            }
            catch (JsonException e)
            {
                diagnostics.Add(Error("EnterpriseGate.EvidenceMalformed", $"Evidence JSON is invalid: {e.Message}", path));
            }
        }

        return documents;
    }

    private static IReadOnlyList<EvidenceCategory> BuildEvidenceGateCategories(EvidenceSet evidence)
    {
        return
        [
            CategoryFromDoc(evidence, "OpeningCheckpoint", "docs/architecture/ENTERPRISE_EVOLVABLE_OPENING_CHECKPOINT.md", "Phase 96 opening checkpoint is present."),
            CategoryFromDoc(evidence, "ThreatModel", "docs/architecture/SAGA_ENTERPRISE_THREAT_MODEL_V0.md", "Phase 97 threat model is present."),
            CategoryFromDoc(evidence, "ClaimGate", "docs/dev/SAGA_ENTERPRISE_CLAIM_LEVELS.md", "Phase 98 claim level document is present."),
            CategoryFromDocument(evidence, "PolicyEvaluation", item => ReadString(item.Root, "tool") == "sagapolicy" && ReadString(item.Root, "command") == "evaluate", "Policy evaluation evidence is present."),
            CategoryFromDocument(evidence, "ProjectSlice", item => ReadString(item.Root, "tool") == "sagaproject" && ReadString(item.Root, "command") is "resolve-slice" or "restricted-resolve", "Project slice or restricted resolution evidence is present."),
            CategoryFromDocument(evidence, "SourceVisibility", item => item.Root["resourceVisibility"] is JsonArray, "Source visibility classification evidence is present."),
            CategoryFromDocument(evidence, "WorkspaceHub", item => ReadString(item.Root, "tool") == "sagaworkspacehub", "WorkspaceHub local/report-only evidence is present."),
            CategoryFromDocument(evidence, "AuditLog", item => ReadString(item.Root, "tool") == "sagaworkspacehub" && ReadString(item.Root, "command") == "audit-log", "Audit log evidence is present."),
            CategoryFromDocument(evidence, "ReviewApproval", item => ReadString(item.Root, "tool") == "sagaworkspacehub" && ReadString(item.Root, "command") == "review-approval", "Review approval evidence is present."),
            CategoryFromDocument(evidence, "PublishPolicy", item => ReadString(item.Root, "tool") == "sagapack" && ReadString(item.Root, "command") == "publish-check", "Publish policy gate evidence is present."),
            CategoryFromDocument(evidence, "RestrictedExport", item => ReadString(item.Root, "tool") == "sagaworkspacehub" && ReadString(item.Root, "command") == "restricted-export", "Restricted export evidence is present."),
            CategoryFromDocument(evidence, "PolicyRegression", item => ReadString(item.Root, "tool") == ToolInfo.Name && ReadString(item.Root, "command") == "policy-regression", "Policy regression evidence is present."),
            CategoryFromDocument(evidence, "PresenceRedaction", item => ReadString(item.Root, "tool") == "sagaworkspacehub" && ReadString(item.Root, "command") == "presence-redaction", "Presence redaction evidence is present."),
            CategoryFromDocument(evidence, "PolicyAwareActions", item => ReadString(item.Root, "tool") == "sagaworkspacehub" && ReadString(item.Root, "command") == "policy-actions", "Policy-aware action evidence is present."),
            CategoryFromDocument(evidence, "SliceAwareTeamRoom", item => ReadString(item.Root, "tool") == "sagaworkspacehub" && ReadString(item.Root, "command") == "team-room", "Slice-aware team room evidence is present."),
            CategoryFromDocument(evidence, "GovernancePanel", item => ReadString(item.Root, "tool") == "sagaworkspacehub" && ReadString(item.Root, "command") == "governance-panel", "Governance panel evidence is present."),
            CategoryFromDoc(evidence, "Limitations", "docs/architecture/ENTERPRISE_SECURITY_LIMITATIONS.md", "Security and governance limitations freeze is present."),
            CategoryFromResidualDebt(evidence),
        ];
    }

    private static IReadOnlyList<AcceptanceStep> BuildAcceptanceSteps(EvidenceSet evidence)
    {
        return
        [
            StepFromDocument(evidence, "Hedef2ClosureDebt", item => ReadString(item.Root, "command") == "small-team-alpha-closure" || ReadString(item.Root, "status") == "PartiallyProven" || HasResidualDebt(item.Root), "Hedef 2 closure or residual debt evidence is present."),
            StepFromDocs(evidence, "Phase96To98Docs", ["docs/architecture/ENTERPRISE_EVOLVABLE_OPENING_CHECKPOINT.md", "docs/architecture/SAGA_ENTERPRISE_THREAT_MODEL_V0.md", "docs/dev/SAGA_ENTERPRISE_CLAIM_LEVELS.md"], "Phase 96-98 docs are present."),
            StepFromDocument(evidence, "PolicyEvaluation", item => ReadString(item.Root, "tool") == "sagapolicy" && ReadString(item.Root, "command") == "evaluate", "Policy evaluation evidence is present."),
            StepFromDocument(evidence, "SliceRestrictedResolution", item => ReadString(item.Root, "tool") == "sagaproject" && ReadString(item.Root, "command") is "resolve-slice" or "restricted-resolve", "Slice/restricted resolution evidence is present."),
            StepFromDocument(evidence, "WorkspaceHubEvidence", item => ReadString(item.Root, "tool") == "sagaworkspacehub", "WorkspaceHub evidence is present."),
            StepFromDocuments(evidence, "AuditReviewPublishExportEvidence", [
                item => ReadString(item.Root, "tool") == "sagaworkspacehub" && ReadString(item.Root, "command") == "audit-log",
                item => ReadString(item.Root, "tool") == "sagaworkspacehub" && ReadString(item.Root, "command") == "review-approval",
                item => ReadString(item.Root, "tool") == "sagapack" && ReadString(item.Root, "command") == "publish-check",
                item => ReadString(item.Root, "tool") == "sagaworkspacehub" && ReadString(item.Root, "command") == "restricted-export",
            ], "Audit, review, publish, and export evidence is present."),
            StepFromDocument(evidence, "PolicyRegression", item => ReadString(item.Root, "tool") == ToolInfo.Name && ReadString(item.Root, "command") == "policy-regression", "Policy regression evidence is present."),
            StepFromDocuments(evidence, "RedactionActionsTeamRoomGovernanceEvidence", [
                item => ReadString(item.Root, "tool") == "sagaworkspacehub" && ReadString(item.Root, "command") == "presence-redaction",
                item => ReadString(item.Root, "tool") == "sagaworkspacehub" && ReadString(item.Root, "command") == "policy-actions",
                item => ReadString(item.Root, "tool") == "sagaworkspacehub" && ReadString(item.Root, "command") == "team-room",
                item => ReadString(item.Root, "tool") == "sagaworkspacehub" && ReadString(item.Root, "command") == "governance-panel",
            ], "Redaction, action, team-room, and governance panel evidence is present."),
            StepFromDocs(evidence, "LimitationsFreeze", ["docs/architecture/ENTERPRISE_SECURITY_LIMITATIONS.md"], "Limitations freeze doc is present."),
            StepFromDocument(evidence, "EvidenceGateReport", item => ReadString(item.Root, "tool") == ToolInfo.Name && ReadString(item.Root, "command") == "evidence-gate", "Evidence gate report is present."),
            StepForbiddenClaims(evidence),
        ];
    }

    private static IReadOnlyList<EvidenceCategory> BuildClosureEvidenceMatrix(EvidenceSet evidence)
    {
        return
        [
            CategoryFromDocument(evidence, "EnterpriseEvidenceReport", item => ReadString(item.Root, "tool") == ToolInfo.Name && ReadString(item.Root, "command") == "evidence-gate", "Enterprise evidence gate report is present."),
            CategoryFromDocument(evidence, "EnterpriseAcceptanceReport", item => ReadString(item.Root, "tool") == ToolInfo.Name && ReadString(item.Root, "command") == "acceptance-scenario", "Enterprise acceptance scenario report is present."),
            CategoryFromDocument(evidence, "PolicyCheckReport", item => ReadString(item.Root, "tool") == "sagapolicy" && ReadString(item.Root, "command") == "evaluate", "Policy check report is present."),
            CategoryFromDocument(evidence, "SliceResolutionReport", item => ReadString(item.Root, "tool") == "sagaproject" && ReadString(item.Root, "command") is "resolve-slice" or "restricted-resolve", "Slice resolution report is present."),
            CategoryFromDocument(evidence, "WorkspaceSessionReport", item => ReadString(item.Root, "tool") == "sagaworkspacehub" && ReadString(item.Root, "command") == "summarize", "Workspace session report is present."),
            CategoryFromDocument(evidence, "AuditReport", item => ReadString(item.Root, "tool") == "sagaworkspacehub" && ReadString(item.Root, "command") == "audit-log", "Audit report is present."),
            CategoryFromDocument(evidence, "ReviewApprovalReport", item => ReadString(item.Root, "tool") == "sagaworkspacehub" && ReadString(item.Root, "command") == "review-approval", "Review approval report is present."),
            CategoryFromDocument(evidence, "PublishReport", item => ReadString(item.Root, "tool") == "sagapack" && ReadString(item.Root, "command") == "publish-check", "Publish report is present."),
            CategoryFromDocument(evidence, "DocGuardReport", item => ReadString(item.Root, "tool") == "sagadocguard" && ReadString(item.Root, "command") == "scan", "DocGuard report is present."),
            CategoryFromDoc(evidence, "SecurityLimitationsDoc", "docs/architecture/ENTERPRISE_SECURITY_LIMITATIONS.md", "Security limitations doc is present."),
            CategoryFromDoc(evidence, "ClosureCheckpointDoc", "docs/architecture/ENTERPRISE_EVOLVABLE_FOUNDATION_CLOSURE_CHECKPOINT.md", "Closure checkpoint doc is present."),
        ];
    }

    private static EvidenceCategory CategoryFromDoc(EvidenceSet evidence, string category, string relativePath, string summary)
    {
        var fullPath = ResolveRequiredDocumentPath(evidence.Root, relativePath);
        return File.Exists(fullPath)
            ? new EvidenceCategory { Category = category, Status = "Passed", EvidencePath = Normalize(Path.GetFullPath(fullPath)), Summary = summary }
            : new EvidenceCategory { Category = category, Status = "MissingEvidence", Summary = $"Missing required document: {relativePath}." };
    }

    private static EvidenceCategory CategoryFromDocument(EvidenceSet evidence, string category, Func<EvidenceDocument, bool> predicate, string summary)
    {
        var match = evidence.Documents.Where(predicate).OrderBy(item => item.Path, StringComparer.Ordinal).FirstOrDefault();
        return match is null
            ? new EvidenceCategory { Category = category, Status = "MissingEvidence", Summary = $"Missing required evidence category: {category}." }
            : new EvidenceCategory { Category = category, Status = ClassifyDocumentStatus(match.Root), EvidencePath = match.Path, Summary = summary };
    }

    private static EvidenceCategory CategoryFromResidualDebt(EvidenceSet evidence)
    {
        var debt = ExtractResidualDebt(evidence);
        return debt.Count == 0
            ? new EvidenceCategory { Category = "ResidualDebt", Status = "NotApplicable", Summary = "No residual debt array was found in local JSON evidence." }
            : new EvidenceCategory { Category = "ResidualDebt", Status = "PartiallyProven", Summary = "Residual debt is preserved in the report." };
    }

    private static AcceptanceStep StepFromDocs(EvidenceSet evidence, string stepId, IReadOnlyList<string> paths, string summary)
    {
        var resolved = paths.Select(path => new { Relative = path, FullPath = ResolveRequiredDocumentPath(evidence.Root, path) }).ToArray();
        var missing = resolved.Where(path => !File.Exists(path.FullPath)).ToArray();
        return missing.Length == 0
            ? new AcceptanceStep { StepId = stepId, Status = "Passed", EvidencePath = Normalize(Path.GetFullPath(resolved[0].FullPath)), Summary = summary }
            : new AcceptanceStep { StepId = stepId, Status = "MissingEvidence", Summary = $"Missing required document: {missing[0].Relative}." };
    }

    private static AcceptanceStep StepFromDocument(EvidenceSet evidence, string stepId, Func<EvidenceDocument, bool> predicate, string summary)
    {
        var match = evidence.Documents.Where(predicate).OrderBy(item => item.Path, StringComparer.Ordinal).FirstOrDefault();
        return match is null
            ? new AcceptanceStep { StepId = stepId, Status = "MissingEvidence", Summary = $"Missing required scenario evidence: {stepId}." }
            : new AcceptanceStep { StepId = stepId, Status = ClassifyStepStatus(match.Root), EvidencePath = match.Path, Summary = summary };
    }

    private static AcceptanceStep StepFromDocuments(EvidenceSet evidence, string stepId, IReadOnlyList<Func<EvidenceDocument, bool>> predicates, string summary)
    {
        var matches = new List<EvidenceDocument>();
        foreach (var predicate in predicates)
        {
            var match = evidence.Documents.Where(predicate).OrderBy(item => item.Path, StringComparer.Ordinal).FirstOrDefault();
            if (match is null)
            {
                return new AcceptanceStep { StepId = stepId, Status = "MissingEvidence", Summary = $"Missing required scenario evidence: {stepId}." };
            }
            matches.Add(match);
        }

        var statuses = matches.Select(item => ClassifyStepStatus(item.Root)).ToArray();
        var status = statuses.Any(item => item == "Blocked") ? "Blocked" :
            statuses.Any(item => item == "MissingEvidence") ? "MissingEvidence" :
            statuses.Any(item => item == "PartiallyProven") ? "PartiallyProven" :
            "Passed";
        return new AcceptanceStep { StepId = stepId, Status = status, EvidencePath = matches.OrderBy(item => item.Path, StringComparer.Ordinal).First().Path, Summary = summary };
    }

    private static AcceptanceStep StepForbiddenClaims(EvidenceSet evidence)
    {
        var hasForbidden = evidence.Documents.Any(HasForbiddenPositiveClaim);
        return hasForbidden
            ? new AcceptanceStep { StepId = "ForbiddenPositiveClaimAbsence", Status = "Blocked", Summary = "Forbidden positive claim wording was found in local JSON evidence." }
            : new AcceptanceStep { StepId = "ForbiddenPositiveClaimAbsence", Status = "Passed", Summary = "No forbidden positive claim wording was found in local JSON evidence." };
    }

    private static string ClassifyDocumentStatus(JsonObject root)
    {
        var status = ReadString(root, "status");
        var decision = ReadString(root, "decision");
        if (status is "Passed" or "Ready" or "Accepted" or "FoundationEstablished" || decision is "Allow" or "ApprovedMetadataOnly")
        {
            return "Passed";
        }
        if (status is "PartiallyProven" or "PassedWithDeferredEvidence" || HasResidualDebt(root))
        {
            return "PartiallyProven";
        }
        if (status == "Deferred")
        {
            return "Deferred";
        }
        if (status == "MissingEvidence" || decision == "MissingEvidence")
        {
            return "MissingEvidence";
        }
        if (status is "Blocked" or "Failed" or "Denied" || decision is "Deny" or "BlockedByPolicy" or "Stale")
        {
            return "Blocked";
        }

        return string.IsNullOrWhiteSpace(status) ? "Passed" : "PartiallyProven";
    }

    private static string ClassifyStepStatus(JsonObject root)
    {
        var status = ClassifyDocumentStatus(root);
        return status == "Deferred" ? "PartiallyProven" : status;
    }

    private static GateSummary Summarize(IReadOnlyList<EvidenceCategory> categories) =>
        new()
        {
            Passed = categories.Count(item => item.Status == "Passed"),
            PartiallyProven = categories.Count(item => item.Status == "PartiallyProven"),
            Deferred = categories.Count(item => item.Status == "Deferred"),
            MissingEvidence = categories.Count(item => item.Status == "MissingEvidence"),
            Blocked = categories.Count(item => item.Status == "Blocked"),
            NotApplicable = categories.Count(item => item.Status == "NotApplicable"),
            Total = categories.Count,
        };

    private static string DetermineGateStatus(GateSummary summary, int residualDebtCount, bool hasClaimBlock)
    {
        if (hasClaimBlock || summary.Blocked > 0)
        {
            return "Blocked";
        }
        if (summary.MissingEvidence > 0)
        {
            return "MissingEvidence";
        }
        if (summary.PartiallyProven > 0 || summary.Deferred > 0 || residualDebtCount > 0)
        {
            return "PartiallyProven";
        }
        return "Passed";
    }

    private static string DetermineAcceptanceStatus(AcceptanceSummary summary, int residualDebtCount, bool hasClaimBlock)
    {
        if (hasClaimBlock || summary.Blocked > 0)
        {
            return "Blocked";
        }
        if (summary.MissingEvidence > 0)
        {
            return "MissingEvidence";
        }
        if (summary.PartiallyProven > 0 || residualDebtCount > 0)
        {
            return "PartiallyProven";
        }
        return "Passed";
    }

    private static string DetermineClosureOutcome(GateSummary summary, IReadOnlyList<EvidenceCategory> matrix, int residualDebtCount, bool hasClaimBlock)
    {
        if (hasClaimBlock || summary.Blocked > 0 || summary.MissingEvidence > 0)
        {
            return "Blocked";
        }
        if (summary.PartiallyProven > 0 || summary.Deferred > 0 || residualDebtCount > 0 ||
            matrix.Any(item => (item.Category is "EnterpriseEvidenceReport" or "EnterpriseAcceptanceReport") && item.Status == "PartiallyProven"))
        {
            return "PartiallyProven";
        }
        return "FoundationEstablished";
    }

    private static IReadOnlyList<string> ExtractResidualDebt(EvidenceSet evidence)
    {
        var debt = new SortedSet<string>(StringComparer.Ordinal);
        foreach (var item in ResidualDebtDefaults)
        {
            debt.Add(item);
        }

        foreach (var document in evidence.Documents)
        {
            AddDebtFromNode(document.Root["residualDebt"], debt);
            AddDebtFromNode(document.Root["knownDebt"], debt);
            AddDebtFromNode(document.Root["deferredEvidence"], debt);
        }

        return debt.ToArray();
    }

    private static void AddDebtFromNode(JsonNode? node, ISet<string> debt)
    {
        if (node is JsonArray array)
        {
            foreach (var item in array)
            {
                if (item is JsonValue value && value.TryGetValue<string>(out var text) && !string.IsNullOrWhiteSpace(text))
                {
                    debt.Add(text);
                }
                else if (item is JsonObject obj)
                {
                    var summary = ReadString(obj, "summary");
                    if (!string.IsNullOrWhiteSpace(summary))
                    {
                        debt.Add(summary);
                    }
                }
            }
        }
    }

    private static bool HasResidualDebt(JsonObject root) =>
        root["residualDebt"] is JsonArray { Count: > 0 } ||
        root["knownDebt"] is JsonArray { Count: > 0 } ||
        root["deferredEvidence"] is JsonArray { Count: > 0 };

    private static void AddForbiddenClaimDiagnostics(EvidenceSet evidence, List<Diagnostic> diagnostics)
    {
        foreach (var document in evidence.Documents)
        {
            foreach (var ruleId in FindForbiddenPositiveClaimIds(document))
            {
                diagnostics.Add(Error("EnterpriseGate.Claim.PositiveClaim", $"Forbidden positive claim marker found: {ruleId}.", document.Path));
            }
        }
    }

    private static bool HasForbiddenPositiveClaim(EvidenceDocument document) =>
        FindForbiddenPositiveClaimIds(document).Count > 0;

    private static IReadOnlyList<string> FindForbiddenPositiveClaimIds(EvidenceDocument document)
    {
        if (ReadString(document.Root, "tool") == "sagadocguard")
        {
            return document.Root["forbiddenMatches"] is JsonArray { Count: > 0 }
                ? ["DocGuardForbiddenMatches"]
                : Array.Empty<string>();
        }

        var ids = new SortedSet<string>(StringComparer.Ordinal);
        FindForbiddenPositiveClaimIds(document.Root, "", ids);
        return ids.ToArray();
    }

    private static void FindForbiddenPositiveClaimIds(JsonNode? node, string key, ISet<string> ids)
    {
        if (node is null || IsNonClaimKey(key))
        {
            return;
        }

        if (node is JsonObject obj)
        {
            foreach (var property in obj)
            {
                FindForbiddenPositiveClaimIds(property.Value, property.Key, ids);
            }
            return;
        }

        if (node is JsonArray array)
        {
            foreach (var item in array)
            {
                FindForbiddenPositiveClaimIds(item, key, ids);
            }
            return;
        }

        if (node is JsonValue value && value.TryGetValue<string>(out var text) && !IsNonClaimText(text))
        {
            foreach (var rule in ForbiddenClaimRules)
            {
                if (Regex.IsMatch(text, rule.Pattern, RegexOptions, RegexTimeout))
                {
                    ids.Add(rule.Id);
                }
            }
        }
    }

    private static bool IsNonClaimKey(string key) =>
        key.Contains("blocked", StringComparison.OrdinalIgnoreCase) ||
        key.Contains("forbidden", StringComparison.OrdinalIgnoreCase) ||
        key.Contains("nonclaim", StringComparison.OrdinalIgnoreCase) ||
        key.Contains("nonClaim", StringComparison.OrdinalIgnoreCase) ||
        key.Equals("blockedClaims", StringComparison.OrdinalIgnoreCase) ||
        key.Equals("forbiddenClaims", StringComparison.OrdinalIgnoreCase) ||
        key.Equals("nonClaims", StringComparison.OrdinalIgnoreCase) ||
        key.Equals("reviewedNonClaimMatches", StringComparison.OrdinalIgnoreCase) ||
        key.Equals("missingRequiredNonClaims", StringComparison.OrdinalIgnoreCase) ||
        key.Equals("residualDebt", StringComparison.OrdinalIgnoreCase) ||
        key.Equals("knownDebt", StringComparison.OrdinalIgnoreCase) ||
        key.Equals("deferredEvidence", StringComparison.OrdinalIgnoreCase);

    private static bool IsNonClaimText(string text) =>
        Regex.IsMatch(
            text,
            @"\b(no|not|non[- ]claim|blocked|forbidden|deferred|out\s+of\s+scope|unclaimed|missing|without\s+claiming|does\s+not|do\s+not)\b",
            RegexOptions,
            RegexTimeout);

    private static RegressionCheck CheckPolicyDecision(IReadOnlyList<EvidenceDocument> evidence, string checkId, string decision, string summary)
    {
        EvidenceDocument? match = evidence.FirstOrDefault(item =>
            ReadString(item.Root, "tool") == "sagapolicy" &&
            ReadString(item.Root, "command") == "evaluate" &&
            ReadString(item.Root, "decision") == decision);
        return match is null
            ? Missing(checkId, $"Missing policy decision evidence: {decision}.")
            : PassedCheck(checkId, match.Path, summary);
    }

    private static RegressionCheck CheckRestrictedSlice(IReadOnlyList<EvidenceDocument> evidence)
    {
        EvidenceDocument? match = evidence.FirstOrDefault(item =>
            ReadString(item.Root, "command") == "restricted-resolve" &&
            item.Root["resourceVisibility"] is JsonArray);
        return match is null
            ? Missing("RestrictedSliceExists", "Missing restricted slice resolution evidence.")
            : PassedCheck("RestrictedSliceExists", match.Path, "Restricted slice resolution evidence is present.");
    }

    private static RegressionCheck CheckHiddenSource(IReadOnlyList<EvidenceDocument> evidence)
    {
        EvidenceDocument? match = evidence.FirstOrDefault(item =>
            (item.Root["resourceVisibility"] as JsonArray)?.Any(resource => ReadString(resource as JsonObject, "visibility") == "Hidden") == true);
        return match is null
            ? Missing("HiddenSourceClassificationExists", "Missing hidden source classification evidence.")
            : PassedCheck("HiddenSourceClassificationExists", match.Path, "Hidden source classification evidence is present.");
    }

    private static RegressionCheck CheckPublishBlockedByPolicy(IReadOnlyList<EvidenceDocument> evidence)
    {
        EvidenceDocument? match = evidence.FirstOrDefault(item =>
            ReadString(item.Root, "tool") == "sagapack" &&
            ReadString(item.Root, "command") == "publish-check" &&
            ReadString(item.Root, "status") == "Blocked" &&
            GateBlocked(item.Root, "PolicyReportAccepted"));
        return match is null
            ? Missing("PublishBlockedByPolicy", "Missing publish-check policy block evidence.")
            : PassedCheck("PublishBlockedByPolicy", match.Path, "Publish-check policy block evidence is present.");
    }

    private static RegressionCheck CheckReviewBlockedByStaleApproval(IReadOnlyList<EvidenceDocument> evidence)
    {
        EvidenceDocument? match = evidence.FirstOrDefault(item =>
            ReadString(item.Root, "tool") == "sagaworkspacehub" &&
            ReadString(item.Root, "command") == "review-approval" &&
            ReadString(item.Root, "decision") == "Stale");
        return match is null
            ? Missing("ReviewBlockedByStaleApproval", "Missing stale review approval block evidence.")
            : PassedCheck("ReviewBlockedByStaleApproval", match.Path, "Stale review approval block evidence is present.");
    }

    private static RegressionCheck CheckRestrictedExportBlocksHidden(IReadOnlyList<EvidenceDocument> evidence)
    {
        EvidenceDocument? match = evidence.FirstOrDefault(item =>
            ReadString(item.Root, "tool") == "sagaworkspacehub" &&
            ReadString(item.Root, "command") == "restricted-export" &&
            ReadString(item.Root, "status") == "Blocked" &&
            HasHiddenBlockedExport(item.Root));
        return match is null
            ? Missing("RestrictedExportBlocksHiddenArtifacts", "Missing hidden artifact restricted export block evidence.")
            : PassedCheck("RestrictedExportBlocksHiddenArtifacts", match.Path, "Hidden artifact restricted export block evidence is present.");
    }

    private static RegressionCheck CheckWorkspaceHubReportOnly(IReadOnlyList<EvidenceDocument> evidence)
    {
        var reports = evidence
            .Where(item => ReadString(item.Root, "tool") == "sagaworkspacehub")
            .OrderBy(item => item.Path, StringComparer.Ordinal)
            .ToArray();
        if (reports.Length == 0)
        {
            return Missing("WorkspaceHubReportsReportOnly", "Missing WorkspaceHub report evidence.");
        }

        var allReportOnly = reports.All(item =>
            ReadBool(item.Root, "mutatesSource") == false &&
            ReadString(item.Root, "enforcement") == "ReportOnly" &&
            (!item.Root.ContainsKey("localOnly") || ReadBool(item.Root, "localOnly") == true) &&
            (!item.Root.ContainsKey("networkExposure") || ReadString(item.Root, "networkExposure") == "None"));
        return allReportOnly
            ? PassedCheck("WorkspaceHubReportsReportOnly", reports[0].Path, "WorkspaceHub report-only invariants are preserved.")
            : new RegressionCheck
            {
                CheckId = "WorkspaceHubReportsReportOnly",
                Status = "Failed",
                EvidencePath = reports[0].Path,
                Summary = "A WorkspaceHub report does not preserve local/report-only invariants.",
            };
    }

    private static bool GateBlocked(JsonObject root, string name) =>
        (root["gates"] as JsonArray)?.Any(gate =>
            ReadString(gate as JsonObject, "name") == name &&
            ReadString(gate as JsonObject, "status") == "Blocked") == true;

    private static bool HasHiddenBlockedExport(JsonObject root) =>
        (root["blockedExports"] as JsonArray)?.Any(item => ReadString(item as JsonObject, "visibility") == "Hidden") == true ||
        (root["restrictedArtifacts"] as JsonArray)?.Any(item => ReadString(item as JsonObject, "visibility") == "Hidden") == true;

    private static RegressionCheck PassedCheck(string checkId, string path, string summary) =>
        new()
        {
            CheckId = checkId,
            Status = "Passed",
            EvidencePath = path,
            Summary = summary,
        };

    private static RegressionCheck Missing(string checkId, string summary) =>
        new()
        {
            CheckId = checkId,
            Status = "MissingEvidence",
            Summary = summary,
        };

    private static string ReadString(JsonObject? obj, string key) =>
        obj is not null &&
        obj.TryGetPropertyValue(key, out var node) &&
        node is JsonValue value &&
        value.TryGetValue<string>(out var text)
            ? text
            : "";

    private static bool ReadBool(JsonObject? obj, string key) =>
        obj is not null &&
        obj.TryGetPropertyValue(key, out var node) &&
        node is JsonValue value &&
        value.TryGetValue<bool>(out var result) &&
        result;

    private static List<Diagnostic> BaseDiagnostics() =>
    [
        Info("EnterpriseGate.ReportOnlyLocal", "SagaEnterpriseGate reads local evidence only, exposes no network surface, and does not mutate source.", "")
    ];

    private static IReadOnlyList<Diagnostic> SortDiagnostics(IEnumerable<Diagnostic> diagnostics) =>
        diagnostics
            .OrderByDescending(item => item.Severity == "Error")
            .ThenByDescending(item => item.Severity == "Warning")
            .ThenBy(item => item.Code, StringComparer.Ordinal)
            .ThenBy(item => item.Path, StringComparer.Ordinal)
            .ThenBy(item => item.Message, StringComparer.Ordinal)
            .ToArray();

    private static Diagnostic Error(string code, string message, string path) =>
        new() { Severity = "Error", Code = code, Message = message, Path = path };

    private static Diagnostic Info(string code, string message, string path) =>
        new() { Severity = "Info", Code = code, Message = message, Path = path };

    private static bool MissingRequired(string value, string name)
    {
        if (!string.IsNullOrWhiteSpace(value))
        {
            return false;
        }

        Console.Error.WriteLine($"{name} is required.");
        return true;
    }

    private static string Normalize(string path) => path.Replace('\\', '/');

    private static string ResolveRequiredDocumentPath(string evidenceRoot, string relativePath)
    {
        var evidencePath = Path.Combine(evidenceRoot, relativePath);
        if (File.Exists(evidencePath))
        {
            return evidencePath;
        }

        var repositoryRoot = FindRepositoryRoot(Directory.GetCurrentDirectory());
        if (repositoryRoot is null || !IsPathWithin(Path.GetFullPath(evidenceRoot), repositoryRoot))
        {
            return evidencePath;
        }

        var repositoryPath = Path.Combine(repositoryRoot, relativePath);
        return File.Exists(repositoryPath) ? repositoryPath : evidencePath;
    }

    private static string? FindRepositoryRoot(string start)
    {
        var current = new DirectoryInfo(Path.GetFullPath(start));
        while (current is not null)
        {
            if (Directory.Exists(Path.Combine(current.FullName, ".git")) ||
                (Directory.Exists(Path.Combine(current.FullName, "docs")) && Directory.Exists(Path.Combine(current.FullName, "Tools"))))
            {
                return current.FullName;
            }
            current = current.Parent;
        }

        return null;
    }

    private static bool IsPathWithin(string path, string root)
    {
        var relative = Path.GetRelativePath(Path.GetFullPath(root), Path.GetFullPath(path));
        return relative == "." || (!relative.StartsWith("..", StringComparison.Ordinal) && !Path.IsPathRooted(relative));
    }

    private static int UnknownCommand(string command)
    {
        Console.Error.WriteLine($"Unknown command: {command}");
        PrintUsage();
        return InvalidUsage;
    }

    private static void PrintUsage()
    {
        Console.Error.WriteLine("SagaEnterpriseGate CLI");
        Console.Error.WriteLine("  sagaenterprisegate policy-regression --evidence-root <dir> --out <policy_regression_report.json>");
        Console.Error.WriteLine("  sagaenterprisegate evidence-gate --evidence-root <dir> --out <enterprise_evidence_report.json>");
        Console.Error.WriteLine("  sagaenterprisegate acceptance-scenario --evidence-root <dir> --out <enterprise_acceptance_report.json>");
        Console.Error.WriteLine("  sagaenterprisegate closure-check --evidence-root <dir> --out <enterprise_closure_report.json>");
    }

    private sealed record CommandOptions(bool Ok, string Error, string EvidenceRoot, string OutPath)
    {
        public static CommandOptions Parse(string[] args)
        {
            string evidenceRoot = "";
            string output = "";
            for (var i = 0; i < args.Length; ++i)
            {
                switch (args[i])
                {
                    case "--evidence-root" when i + 1 < args.Length:
                        evidenceRoot = args[++i];
                        break;
                    case "--out" when i + 1 < args.Length:
                        output = args[++i];
                        break;
                    default:
                        return Fail($"Unknown or incomplete argument: {args[i]}");
                }
            }

            return new CommandOptions(true, "", evidenceRoot, output);
        }

        private static CommandOptions Fail(string error) => new(false, error, "", "");
    }

    private sealed record EvidenceDocument(string Path, JsonObject Root);
    private sealed record EvidenceSet(string Root, IReadOnlyList<EvidenceDocument> Documents);
    private sealed record ForbiddenClaimRule(string Id, string Pattern);
}
