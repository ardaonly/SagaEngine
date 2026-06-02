using System.Text.Json;
using System.Text.Json.Nodes;

namespace SagaWorkspaceHub;

internal static class Program
{
    private const int Passed = 0;
    private const int Failed = 1;
    private const int InvalidUsage = 2;
    private const int InternalError = 4;

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
                "summarize" => RunSummarize(options),
                "policy-adapter" => RunPolicyAdapter(options),
                "slice-view" => RunSliceView(options),
                "audit-log" => RunAuditLog(options),
                "review-approval" => RunReviewApproval(options),
                "restricted-export" => RunRestrictedExport(options),
                "presence-redaction" => RunPresenceRedaction(options),
                "policy-actions" => RunPolicyActions(options),
                "team-room" => RunTeamRoom(options),
                "governance-panel" => RunGovernancePanel(options),
                _ => UnknownCommand(command),
            };
        }
        catch (Exception e)
        {
            Console.Error.WriteLine($"Internal sagaworkspacehub error: {e.Message}");
            return InternalError;
        }
    }

    private static int RunSummarize(CommandOptions options)
    {
        if (MissingRequired(options.OutPath, "--out") ||
            MissingRequired(options.ProjectPath, "--project") ||
            MissingRequired(options.SliceResolutionPath, "--slice-resolution") ||
            MissingRequired(options.PolicyReportPath, "--policy-report") ||
            MissingRequired(options.ViewCompatibilityPath, "--view-compatibility"))
        {
            return InvalidUsage;
        }

        var diagnostics = BaseDiagnostics();
        var project = LoadProject(options.ProjectPath, diagnostics);
        var slice = LoadSliceResolution(options.SliceResolutionPath, diagnostics, out _, out var sliceProject);
        var policy = BuildPolicySummary(options.PolicyReportPath, diagnostics);
        var view = LoadViewCompatibility(options.ViewCompatibilityPath, diagnostics);
        project = MergeProject(project, sliceProject);
        if (string.IsNullOrWhiteSpace(view.ReportPath))
        {
            diagnostics.Add(Error("WorkspaceHub.ViewCompatibilityMissing", "View compatibility evidence is missing or unreadable.", options.ViewCompatibilityPath));
        }

        var failed = diagnostics.Any(item => item.Severity == "Error");
        var report = new WorkspaceSummaryReport
        {
            Status = failed ? "Failed" : "Passed",
            Project = project,
            Slice = slice,
            Policy = policy,
            Collaboration = new CollaborationSummary(),
            Reports = new ReportInputs
            {
                Project = FullPath(options.ProjectPath),
                SliceResolution = FullPath(options.SliceResolutionPath),
                PolicyReport = FullPath(options.PolicyReportPath),
                ViewCompatibility = FullPath(options.ViewCompatibilityPath),
            },
            MutatesSource = false,
            Enforcement = "ReportOnly",
            Diagnostics = SortDiagnostics(diagnostics),
        };
        ReportWriter.Write(options.OutPath, report);
        return failed ? Failed : Passed;
    }

    private static int RunPolicyAdapter(CommandOptions options)
    {
        if (MissingRequired(options.OutPath, "--out") ||
            MissingRequired(options.PolicyReportPath, "--policy-report"))
        {
            return InvalidUsage;
        }

        var diagnostics = BaseDiagnostics();
        var policy = BuildPolicySummary(options.PolicyReportPath, diagnostics);
        var report = new PolicyAdapterReport
        {
            Status = AdapterStatus(policy.AdapterDecision),
            Policy = policy,
            MutatesSource = false,
            Enforcement = "ReportOnly",
            Diagnostics = SortDiagnostics(diagnostics),
        };
        ReportWriter.Write(options.OutPath, report);
        return policy.AdapterDecision == "AllowedByReport" ? Passed : Failed;
    }

    private static int RunSliceView(CommandOptions options)
    {
        if (MissingRequired(options.OutPath, "--out") ||
            MissingRequired(options.SliceResolutionPath, "--slice-resolution") ||
            MissingRequired(options.ViewCompatibilityPath, "--view-compatibility") ||
            MissingRequired(options.PolicyReportPath, "--policy-report"))
        {
            return InvalidUsage;
        }

        var diagnostics = BaseDiagnostics();
        var slice = LoadSliceResolution(options.SliceResolutionPath, diagnostics, out var resources, out var project);
        var policy = BuildPolicySummary(options.PolicyReportPath, diagnostics);
        var view = LoadViewCompatibility(options.ViewCompatibilityPath, diagnostics);
        var visible = resources
            .Where(resource => !IsRestricted(resource))
            .OrderBy(resource => resource.Kind, StringComparer.Ordinal)
            .ThenBy(resource => resource.Id, StringComparer.Ordinal)
            .ThenBy(resource => resource.RelativePath, StringComparer.Ordinal)
            .ToArray();
        var restricted = resources
            .Where(IsRestricted)
            .Select(resource => resource with
            {
                RelativePath = resource.Visibility == "Hidden" ? "" : resource.RelativePath,
                Placeholder = string.IsNullOrWhiteSpace(resource.Placeholder)
                    ? PlaceholderFor(resource)
                    : resource.Placeholder,
            })
            .OrderBy(resource => resource.Kind, StringComparer.Ordinal)
            .ThenBy(resource => resource.Id, StringComparer.Ordinal)
            .ThenBy(resource => resource.Visibility, StringComparer.Ordinal)
            .ToArray();

        var counts = new ResourceCounts
        {
            Visible = visible.Length,
            Restricted = restricted.Length,
            SummaryOnly = resources.Count(resource => resource.Visibility == "SummaryOnly"),
            OpaqueRestricted = resources.Count(resource => resource.Visibility == "OpaqueRestricted"),
            Hidden = resources.Count(resource => resource.Visibility == "Hidden"),
        };
        if (restricted.Length > 0)
        {
            diagnostics.Add(Info("WorkspaceHub.Slice.RestrictedResourcesDisclosed", "Restricted resource counts are disclosed without source text.", options.SliceResolutionPath));
        }

        var failed = diagnostics.Any(item => item.Severity == "Error") ||
                     policy.AdapterDecision is "DeniedByReport" or "MissingPolicyEvidence" or "UnknownPolicyState";
        var status = policy.AdapterDecision switch
        {
            "AllowedByReport" when !failed => "Passed",
            "RequiresReviewByReport" => "ReviewRequiredByReport",
            "DeniedByReport" => "BlockedByPolicyReport",
            "MissingPolicyEvidence" => "MissingPolicyEvidence",
            "UnknownPolicyState" => "UnknownPolicyState",
            _ => failed ? "Failed" : "Passed",
        };

        var report = new SliceScopedViewReport
        {
            Status = status,
            Project = project,
            Slice = slice,
            Policy = policy,
            View = view,
            Resources = new SliceViewResources
            {
                VisibleResources = visible,
                RestrictedPlaceholders = restricted,
            },
            Counts = counts,
            Reports = new ReportInputs
            {
                SliceResolution = FullPath(options.SliceResolutionPath),
                PolicyReport = FullPath(options.PolicyReportPath),
                ViewCompatibility = FullPath(options.ViewCompatibilityPath),
            },
            MutatesSource = false,
            Enforcement = "ReportOnly",
            Diagnostics = SortDiagnostics(diagnostics),
        };
        ReportWriter.Write(options.OutPath, report);
        return status == "Passed" ? Passed : Failed;
    }

    private static int RunAuditLog(CommandOptions options)
    {
        if (MissingRequired(options.OutPath, "--out") ||
            MissingRequired(options.EventsPath, "--events"))
        {
            return InvalidUsage;
        }

        var diagnostics = BaseDiagnostics();
        var events = LoadAuditEvents(options.EventsPath, diagnostics);
        var hashStatus = HashChainStatus(events, diagnostics, options.EventsPath);
        var summary = new AuditSummary
        {
            EventCount = events.Count,
            CountsByEventType = events
                .GroupBy(item => item.EventType, StringComparer.Ordinal)
                .OrderBy(group => group.Key, StringComparer.Ordinal)
                .ToDictionary(group => group.Key, group => group.Count(), StringComparer.Ordinal),
            HashChainStatus = hashStatus,
        };
        var failed = diagnostics.Any(item => item.Severity == "Error");
        var report = new AuditLogReport
        {
            Status = failed ? "Failed" : "Passed",
            Events = events,
            Summary = summary,
            MutatesSource = false,
            Enforcement = "ReportOnly",
            Diagnostics = SortDiagnostics(diagnostics),
        };
        ReportWriter.Write(options.OutPath, report);
        return failed ? Failed : Passed;
    }

    private static int RunReviewApproval(CommandOptions options)
    {
        if (MissingRequired(options.OutPath, "--out") ||
            MissingRequired(options.ReviewReportPath, "--review-report") ||
            MissingRequired(options.PolicyReportPath, "--policy-report"))
        {
            return InvalidUsage;
        }

        var diagnostics = BaseDiagnostics();
        var review = LoadReviewMetadata(options.ReviewReportPath, diagnostics, out var reviewStatus);
        var policy = BuildPolicySummary(options.PolicyReportPath, diagnostics);
        var audit = LoadAuditEvidenceSummary(options.AuditReportPath, diagnostics);
        var decision = ReviewDecision(review, reviewStatus, policy, audit, !string.IsNullOrWhiteSpace(options.AuditReportPath), diagnostics);
        var report = new ReviewApprovalReport
        {
            Status = decision == "ApprovedMetadataOnly" ? "Passed" : "Blocked",
            Review = review,
            Policy = policy,
            Decision = decision,
            Audit = audit,
            Reports = new ReportInputs
            {
                ReviewReport = FullPath(options.ReviewReportPath),
                PolicyReport = FullPath(options.PolicyReportPath),
                AuditReport = FullPath(options.AuditReportPath),
            },
            MutatesSource = false,
            Enforcement = "ReportOnly",
            Diagnostics = SortDiagnostics(diagnostics),
        };
        ReportWriter.Write(options.OutPath, report);
        return report.Status == "Passed" ? Passed : Failed;
    }

    private static int RunRestrictedExport(CommandOptions options)
    {
        if (MissingRequired(options.OutPath, "--out") ||
            MissingRequired(options.SliceResolutionPath, "--slice-resolution") ||
            MissingRequired(options.PolicyReportPath, "--policy-report") ||
            MissingRequired(options.RequestPath, "--request"))
        {
            return InvalidUsage;
        }

        var diagnostics = BaseDiagnostics();
        LoadSliceResolution(options.SliceResolutionPath, diagnostics, out var resources, out _);
        var policy = BuildPolicySummary(options.PolicyReportPath, diagnostics);
        var request = LoadExportRequest(options.RequestPath, diagnostics);
        var matches = MatchRequestedResources(resources, request);
        if (matches.Count == 0)
        {
            diagnostics.Add(Error("WorkspaceHub.RestrictedExport.ArtifactMissing", "Export request did not match a resolved resource.", options.RequestPath));
        }

        var allowed = new List<RestrictedExportItem>();
        var blocked = new List<RestrictedExportItem>();
        var restricted = new List<RestrictedExportItem>();
        foreach (var resource in matches)
        {
            var item = ExportItem(resource);
            var restrictedResource = IsRestricted(resource);
            if (restrictedResource)
            {
                restricted.Add(item);
            }

            var allowedByPolicy = policy.AdapterDecision == "AllowedByReport";
            var allow = allowedByPolicy && resource.Visibility is "FullSource" or "SourceLinkOnly" or "SummaryOnly";
            if (allow)
            {
                allowed.Add(item with
                {
                    Disposition = resource.Visibility switch
                    {
                        "FullSource" => "FullSourceMetadataOnly",
                        "SourceLinkOnly" => "SourceLinkReferenceOnly",
                        "SummaryOnly" => "SummaryMetadataOnly",
                        _ => "MetadataOnly",
                    },
                    Placeholder = resource.Visibility == "SummaryOnly" ? PlaceholderFor(resource) : "",
                });
            }
            else
            {
                blocked.Add(item with
                {
                    Disposition = allowedByPolicy ? "BlockedByVisibility" : "BlockedByPolicy",
                    RelativePath = resource.Visibility == "Hidden" ? "" : resource.RelativePath,
                    Placeholder = string.IsNullOrWhiteSpace(resource.Placeholder) ? PlaceholderFor(resource) : resource.Placeholder,
                });
            }
        }

        var hasErrors = diagnostics.Any(item => item.Severity == "Error");
        var status = !hasErrors && blocked.Count == 0 && policy.AdapterDecision == "AllowedByReport"
            ? "Passed"
            : "Blocked";
        var report = new RestrictedExportReport
        {
            Status = status,
            Request = request,
            Policy = policy,
            AllowedExports = allowed
                .OrderBy(item => item.Kind, StringComparer.Ordinal)
                .ThenBy(item => item.Id, StringComparer.Ordinal)
                .ThenBy(item => item.RelativePath, StringComparer.Ordinal)
                .ToArray(),
            BlockedExports = blocked
                .OrderBy(item => item.Kind, StringComparer.Ordinal)
                .ThenBy(item => item.Id, StringComparer.Ordinal)
                .ThenBy(item => item.RelativePath, StringComparer.Ordinal)
                .ToArray(),
            RestrictedArtifacts = restricted
                .OrderBy(item => item.Kind, StringComparer.Ordinal)
                .ThenBy(item => item.Id, StringComparer.Ordinal)
                .ThenBy(item => item.RelativePath, StringComparer.Ordinal)
                .ToArray(),
            Counts = new RestrictedExportCounts
            {
                AllowedExports = allowed.Count,
                BlockedExports = blocked.Count,
                RestrictedArtifacts = restricted.Count,
            },
            Reports = new ReportInputs
            {
                SliceResolution = FullPath(options.SliceResolutionPath),
                PolicyReport = FullPath(options.PolicyReportPath),
                Request = FullPath(options.RequestPath),
            },
            MutatesSource = false,
            Enforcement = "ReportOnly",
            Diagnostics = SortDiagnostics(diagnostics),
        };
        ReportWriter.Write(options.OutPath, report);
        return status == "Passed" ? Passed : Failed;
    }

    private static int RunPresenceRedaction(CommandOptions options)
    {
        if (MissingRequired(options.OutPath, "--out") ||
            MissingRequired(options.PresenceReportPath, "--presence-report") ||
            MissingRequired(options.SliceResolutionPath, "--slice-resolution") ||
            MissingRequired(options.PolicyReportPath, "--policy-report"))
        {
            return InvalidUsage;
        }

        var diagnostics = BaseDiagnostics();
        LoadSliceResolution(options.SliceResolutionPath, diagnostics, out var resources, out _);
        var policy = BuildPolicySummary(options.PolicyReportPath, diagnostics);
        var presence = LoadJsonRoot(options.PresenceReportPath, diagnostics, "WorkspaceHub.PresenceReport");
        var actors = ReadPresenceActors(presence);
        var presenceResources = ReadPresenceResources(presence);
        var visibleActors = new List<PresenceActorView>();
        var redactedActors = new List<PresenceActorView>();
        var visibleResources = new List<PresenceResourceView>();
        var redactedResources = new List<PresenceResourceView>();

        foreach (var actor in actors)
        {
            var restricted = IsPresenceRefRestricted(actor.ResourceRef, actor.ResourceRef, resources) ||
                actor.Placeholder == "RestrictedActor";
            if (restricted)
            {
                redactedActors.Add(actor with
                {
                    ActorId = "",
                    DisplayName = "",
                    ResourceRef = "",
                    Placeholder = "RedactedActor",
                });
            }
            else
            {
                visibleActors.Add(actor);
            }
        }

        foreach (var resource in presenceResources)
        {
            var sliceResource = FindResource(resources, resource.ResourceId, resource.RelativePath);
            var restricted = sliceResource is not null && IsRestricted(sliceResource);
            if (restricted)
            {
                redactedResources.Add(resource with
                {
                    RelativePath = sliceResource?.Visibility == "Hidden" ? "" : resource.RelativePath,
                    Visibility = sliceResource?.Visibility ?? resource.Visibility,
                    Placeholder = PlaceholderFor(sliceResource ?? new ResourceView { Visibility = resource.Visibility }),
                });
            }
            else
            {
                visibleResources.Add(resource);
            }
        }

        var failed = diagnostics.Any(item => item.Severity == "Error");
        var report = new PresenceRedactionReport
        {
            Status = failed ? "Failed" : "Passed",
            Policy = policy,
            VisibleActors = SortActors(visibleActors),
            RedactedActors = SortActors(redactedActors),
            VisibleResources = SortPresenceResources(visibleResources),
            RedactedResources = SortPresenceResources(redactedResources),
            Counts = new PresenceRedactionCounts
            {
                VisibleActors = visibleActors.Count,
                RedactedActors = redactedActors.Count,
                VisibleResources = visibleResources.Count,
                RedactedResources = redactedResources.Count,
            },
            Reports = new ReportInputs
            {
                PresenceReport = FullPath(options.PresenceReportPath),
                SliceResolution = FullPath(options.SliceResolutionPath),
                PolicyReport = FullPath(options.PolicyReportPath),
            },
            MutatesSource = false,
            Enforcement = "ReportOnly",
            Diagnostics = SortDiagnostics(diagnostics),
        };
        ReportWriter.Write(options.OutPath, report);
        return failed ? Failed : Passed;
    }

    private static int RunPolicyActions(CommandOptions options)
    {
        if (MissingRequired(options.OutPath, "--out") ||
            MissingRequired(options.PolicyReportPath, "--policy-report"))
        {
            return InvalidUsage;
        }

        var diagnostics = BaseDiagnostics();
        var policy = BuildPolicySummary(options.PolicyReportPath, diagnostics);
        var review = LoadJsonRoot(options.ReviewApprovalReportPath, diagnostics, "WorkspaceHub.ReviewApprovalReport", optional: true);
        var export = LoadJsonRoot(options.RestrictedExportReportPath, diagnostics, "WorkspaceHub.RestrictedExportReport", optional: true);
        var actions = new[]
        {
            DisabledAction("ApplyPatch", "Editor action disabled; SagaScript remains the owner for patch apply behavior.", "SagaScript"),
            DisabledAction("RollbackPatch", "Editor action disabled; SagaScript remains the owner for patch rollback behavior.", "SagaScript"),
            PolicyDrivenAction("ChangePackageProfile", policy),
            ExportAction(policy, export),
            ReviewAction(policy, review),
            PolicyDrivenAction("OverrideLock", policy),
            BlockedAction("DeleteBehaviorSource", "Delete behavior source is report-only and is not executed by WorkspaceHub."),
            BlockedAction("DeleteScene", "Delete scene is report-only and is not executed by WorkspaceHub."),
        }.OrderBy(item => item.ActionId, StringComparer.Ordinal).ToArray();

        var failed = diagnostics.Any(item => item.Severity == "Error");
        var report = new PolicyActionsReport
        {
            Status = failed ? "Failed" : "Passed",
            Policy = policy,
            Actions = actions,
            Reports = new ReportInputs
            {
                PolicyReport = FullPath(options.PolicyReportPath),
                ReviewReport = FullPath(options.ReviewApprovalReportPath),
                Request = FullPath(options.RestrictedExportReportPath),
            },
            MutatesSource = false,
            Enforcement = "ReportOnly",
            Diagnostics = SortDiagnostics(diagnostics),
        };
        ReportWriter.Write(options.OutPath, report);
        return failed ? Failed : Passed;
    }

    private static int RunTeamRoom(CommandOptions options)
    {
        if (MissingRequired(options.OutPath, "--out") ||
            MissingRequired(options.TeamRoomStatusPath, "--team-room-status") ||
            MissingRequired(options.PresenceRedactionPath, "--presence-redaction") ||
            MissingRequired(options.SliceResolutionPath, "--slice-resolution"))
        {
            return InvalidUsage;
        }

        var diagnostics = BaseDiagnostics();
        LoadSliceResolution(options.SliceResolutionPath, diagnostics, out var resources, out _);
        var policy = string.IsNullOrWhiteSpace(options.PolicyReportPath)
            ? new PolicySummary()
            : BuildPolicySummary(options.PolicyReportPath, diagnostics);
        var statusRoot = LoadJsonRoot(options.TeamRoomStatusPath, diagnostics, "WorkspaceHub.TeamRoomStatus");
        var redactionRoot = LoadJsonRoot(options.PresenceRedactionPath, diagnostics, "WorkspaceHub.PresenceRedaction");
        var visibleActors = ReadActorsFromRedaction(redactionRoot, "visibleActors");
        var visibleResources = ReadResourcesFromRedaction(redactionRoot, "visibleResources");
        var countsElement = redactionRoot.ValueKind == JsonValueKind.Object &&
            redactionRoot.TryGetProperty("counts", out var foundCounts)
                ? foundCounts
                : default;
        var reviews = ReadTeamItems(statusRoot, "reviews");
        var comments = ReadTeamItems(statusRoot, "comments");
        var visibleReviews = reviews.Where(item => !IsPresenceRefRestricted("", item.TargetRef, resources)).ToArray();
        var restrictedReviews = reviews.Count - visibleReviews.Length;
        var visibleComments = comments.Where(item => !IsPresenceRefRestricted("", item.TargetRef, resources)).ToArray();
        var restrictedComments = comments.Count - visibleComments.Length;

        var failed = diagnostics.Any(item => item.Severity == "Error");
        var report = new TeamRoomReport
        {
            Status = failed ? "Failed" : "Passed",
            Policy = policy,
            VisibleActors = SortActors(visibleActors),
            VisibleResources = SortPresenceResources(visibleResources),
            VisibleReviews = SortTeamItems(visibleReviews),
            VisibleComments = SortTeamItems(visibleComments),
            Counts = new TeamRoomCounts
            {
                VisibleActors = ReadInt(countsElement, "visibleActors"),
                RedactedActors = ReadInt(countsElement, "redactedActors"),
                VisibleResources = ReadInt(countsElement, "visibleResources"),
                RedactedResources = ReadInt(countsElement, "redactedResources"),
                VisibleReviews = visibleReviews.Length,
                RestrictedReviews = restrictedReviews,
                VisibleComments = visibleComments.Length,
                RestrictedComments = restrictedComments,
                HiddenResources = resources.Count(item => item.Visibility == "Hidden"),
            },
            Reports = new ReportInputs
            {
                TeamRoomStatus = FullPath(options.TeamRoomStatusPath),
                PresenceRedaction = FullPath(options.PresenceRedactionPath),
                SliceResolution = FullPath(options.SliceResolutionPath),
                PolicyReport = FullPath(options.PolicyReportPath),
            },
            MutatesSource = false,
            Enforcement = "ReportOnly",
            Diagnostics = SortDiagnostics(diagnostics),
        };
        ReportWriter.Write(options.OutPath, report);
        return failed ? Failed : Passed;
    }

    private static int RunGovernancePanel(CommandOptions options)
    {
        if (MissingRequired(options.OutPath, "--out") ||
            MissingRequired(options.EvidenceRoot, "--evidence-root"))
        {
            return InvalidUsage;
        }

        var diagnostics = BaseDiagnostics();
        var evidence = LoadEvidenceDocuments(options.EvidenceRoot, diagnostics);
        var policyDecisions = Summaries(evidence, item => ReadString(item.Root, "tool") == "sagapolicy", "policyDecision").ToArray();
        var dangerous = policyDecisions
            .Where(item => item.Decision is "Deny" or "RequiresReview")
            .OrderBy(item => item.Path, StringComparer.Ordinal)
            .ToArray();
        var reviews = Summaries(evidence, item => ReadString(item.Root, "command") == "review-approval", "reviewApproval").ToArray();
        var publish = PublishGateSummaries(evidence).ToArray();
        var audits = Summaries(evidence, item => ReadString(item.Root, "command") == "audit-log", "auditLog").ToArray();
        var exports = Summaries(evidence, item => ReadString(item.Root, "command") == "restricted-export", "restrictedExport").ToArray();
        var redactions = Summaries(evidence, item => ReadString(item.Root, "command") == "presence-redaction", "presenceRedaction").ToArray();
        var teamRooms = Summaries(evidence, item => ReadString(item.Root, "command") == "team-room", "teamRoom").ToArray();
        var residualDebt = ResidualDebtSummaries(evidence).ToArray();
        var missingEvidence = MissingEvidenceSummaries(evidence).ToArray();
        var failed = diagnostics.Any(item => item.Severity == "Error");
        var report = new GovernancePanelReport
        {
            Status = failed ? "Failed" : "Passed",
            PolicyDecisions = policyDecisions,
            DangerousOperationQueue = dangerous,
            ReviewApprovals = reviews,
            PublishGates = publish,
            AuditLogSummaries = audits,
            RestrictedExports = exports,
            RedactionReports = redactions,
            TeamRoomSummaries = teamRooms,
            ResidualDebt = residualDebt,
            MissingEvidence = missingEvidence,
            Counts = new GovernancePanelCounts
            {
                PolicyDecisions = policyDecisions.Length,
                DangerousOperations = dangerous.Length,
                ReviewApprovals = reviews.Length,
                PublishGates = publish.Length,
                AuditLogs = audits.Length,
                RestrictedExports = exports.Length,
                RedactionReports = redactions.Length,
                TeamRoomReports = teamRooms.Length,
                MissingEvidence = missingEvidence.Length,
                ResidualDebt = residualDebt.Length,
            },
            MutatesSource = false,
            Enforcement = "ReportOnly",
            Diagnostics = SortDiagnostics(diagnostics),
        };
        ReportWriter.Write(options.OutPath, report);
        return failed ? Failed : Passed;
    }

    private static List<AuditEventRecord> LoadAuditEvents(string path, List<Diagnostic> diagnostics)
    {
        var events = new List<AuditEventRecord>();
        if (!File.Exists(path))
        {
            diagnostics.Add(Error("WorkspaceHub.AuditLog.EventsMissing", "Audit event JSONL input is missing.", path));
            return events;
        }

        var lineNumber = 0;
        foreach (var line in File.ReadLines(path))
        {
            lineNumber++;
            if (string.IsNullOrWhiteSpace(line))
            {
                continue;
            }

            try
            {
                using var document = JsonDocument.Parse(line);
                var root = document.RootElement;
                var eventDiagnostics = ReadDiagnostics(root);
                events.Add(new AuditEventRecord
                {
                    EventId = ReadString(root, "eventId"),
                    EventType = ReadString(root, "eventType"),
                    ActorRef = ReadString(root, "actorRef"),
                    TargetRef = ReadString(root, "targetRef"),
                    Sequence = ReadInt(root, "sequence"),
                    Timestamp = ReadString(root, "timestamp"),
                    PreviousRecordHash = ReadString(root, "previousRecordHash"),
                    RecordHash = ReadString(root, "recordHash"),
                    EvidenceRefs = ReadStringArray(root, "evidenceRefs"),
                    Diagnostics = eventDiagnostics,
                });
            }
            catch (JsonException e)
            {
                diagnostics.Add(Error(
                    "WorkspaceHub.AuditLog.EventMalformed",
                    $"Audit event JSONL line {lineNumber} is invalid: {e.Message}",
                    path));
            }
        }

        return events
            .OrderBy(item => item.Sequence)
            .ThenBy(item => item.EventId, StringComparer.Ordinal)
            .ThenBy(item => item.EventType, StringComparer.Ordinal)
            .ToList();
    }

    private static string HashChainStatus(IReadOnlyList<AuditEventRecord> events, List<Diagnostic> diagnostics, string path)
    {
        if (events.Count == 0 ||
            events.All(item => string.IsNullOrWhiteSpace(item.RecordHash) && string.IsNullOrWhiteSpace(item.PreviousRecordHash)))
        {
            return "NotSupplied";
        }

        var incomplete = events.Any(item => string.IsNullOrWhiteSpace(item.RecordHash));
        for (var i = 1; i < events.Count; ++i)
        {
            var previousHash = events[i - 1].RecordHash;
            var claimedPrevious = events[i].PreviousRecordHash;
            if (!string.IsNullOrWhiteSpace(previousHash) &&
                !string.IsNullOrWhiteSpace(claimedPrevious) &&
                !string.Equals(previousHash, claimedPrevious, StringComparison.Ordinal))
            {
                diagnostics.Add(Error(
                    "WorkspaceHub.AuditLog.HashChainMismatch",
                    "Audit event previousRecordHash does not match the prior local recordHash.",
                    path));
                return "Mismatch";
            }
        }

        return incomplete ? "Incomplete" : "Consistent";
    }

    private static ReviewMetadata LoadReviewMetadata(string path, List<Diagnostic> diagnostics, out string reviewStatus)
    {
        reviewStatus = "";
        if (!File.Exists(path))
        {
            diagnostics.Add(Error("WorkspaceHub.ReviewReportMissing", "Review report is missing.", path));
            return new ReviewMetadata();
        }

        try
        {
            using var document = JsonDocument.Parse(File.ReadAllText(path));
            var root = document.RootElement;
            reviewStatus = ReadString(root, "status");
            var target = root.TryGetProperty("target", out var targetElement) && targetElement.ValueKind == JsonValueKind.Object
                ? targetElement
                : default;
            var reports = root.TryGetProperty("reports", out var reportsElement) && reportsElement.ValueKind == JsonValueKind.Object
                ? reportsElement
                : default;
            var sourceReports = ReadStringArray(root, "sourceReportPaths").ToList();
            sourceReports.AddRange(ReadStringArray(reports, "sourceReportPaths"));
            return new ReviewMetadata
            {
                ReviewId = FirstNonEmpty(ReadString(root, "reviewId"), ReadString(root, "id")),
                Requester = ReadString(root, "requester"),
                Reviewers = ReadStringArray(root, "reviewers"),
                TargetRef = FirstNonEmpty(ReadString(root, "targetRef"), ReadString(target, "targetRef"), ReadString(target, "ref")),
                ExpectedHash = FirstNonEmpty(ReadString(root, "expectedHash"), ReadString(target, "expectedHash")),
                ActualHash = FirstNonEmpty(ReadString(root, "actualHash"), ReadString(target, "actualHash")),
                SourceReportPaths = sourceReports
                    .Where(item => !string.IsNullOrWhiteSpace(item))
                    .Distinct(StringComparer.Ordinal)
                    .OrderBy(item => item, StringComparer.Ordinal)
                    .ToArray(),
            };
        }
        catch (JsonException e)
        {
            diagnostics.Add(Error("WorkspaceHub.ReviewReportMalformed", $"Review report JSON is invalid: {e.Message}", path));
            return new ReviewMetadata();
        }
    }

    private static AuditEvidenceSummary LoadAuditEvidenceSummary(string path, List<Diagnostic> diagnostics)
    {
        if (string.IsNullOrWhiteSpace(path))
        {
            return new AuditEvidenceSummary();
        }
        if (!File.Exists(path))
        {
            diagnostics.Add(Error("WorkspaceHub.AuditReportMissing", "Audit report is missing.", path));
            return new AuditEvidenceSummary { ReportPath = FullPath(path), Status = "Missing" };
        }

        try
        {
            using var document = JsonDocument.Parse(File.ReadAllText(path));
            var root = document.RootElement;
            var summary = root.TryGetProperty("summary", out var summaryElement) ? summaryElement : default;
            return new AuditEvidenceSummary
            {
                ReportPath = FullPath(path),
                Status = ReadString(root, "status"),
                EventCount = ReadInt(summary, "eventCount"),
                CountsByEventType = ReadCounts(summary, "countsByEventType"),
            };
        }
        catch (JsonException e)
        {
            diagnostics.Add(Error("WorkspaceHub.AuditReportMalformed", $"Audit report JSON is invalid: {e.Message}", path));
            return new AuditEvidenceSummary { ReportPath = FullPath(path), Status = "Malformed" };
        }
    }

    private static string ReviewDecision(
        ReviewMetadata review,
        string reviewStatus,
        PolicySummary policy,
        AuditEvidenceSummary audit,
        bool auditWasSupplied,
        List<Diagnostic> diagnostics)
    {
        if (diagnostics.Any(item => item.Code is "WorkspaceHub.ReviewReportMissing" or "WorkspaceHub.ReviewReportMalformed"))
        {
            return "MissingEvidence";
        }
        if (auditWasSupplied && audit.Status is "Missing" or "Malformed")
        {
            return "MissingEvidence";
        }
        if (reviewStatus == "Superseded")
        {
            return "Superseded";
        }
        if (!string.IsNullOrWhiteSpace(review.ExpectedHash) &&
            !string.IsNullOrWhiteSpace(review.ActualHash) &&
            !string.Equals(review.ExpectedHash, review.ActualHash, StringComparison.Ordinal))
        {
            diagnostics.Add(Error("WorkspaceHub.ReviewApproval.StaleTargetHash", "Review target hash evidence is stale.", ""));
            return "Stale";
        }

        return policy.AdapterDecision switch
        {
            "AllowedByReport" => "ApprovedMetadataOnly",
            "DeniedByReport" => "BlockedByPolicy",
            "RequiresReviewByReport" => "RequiresReview",
            "MissingPolicyEvidence" => "MissingEvidence",
            _ => "UnknownPolicyState",
        };
    }

    private static ExportRequestMetadata LoadExportRequest(string path, List<Diagnostic> diagnostics)
    {
        if (!File.Exists(path))
        {
            diagnostics.Add(Error("WorkspaceHub.RestrictedExport.RequestMissing", "Restricted export request is missing.", path));
            return new ExportRequestMetadata();
        }

        try
        {
            using var document = JsonDocument.Parse(File.ReadAllText(path));
            var root = document.RootElement;
            return new ExportRequestMetadata
            {
                ArtifactId = FirstNonEmpty(ReadString(root, "artifactId"), ReadString(root, "id")),
                Path = FirstNonEmpty(ReadString(root, "path"), ReadString(root, "relativePath")),
                Visibility = ReadString(root, "visibility"),
                Role = ReadString(root, "role"),
                OutputTarget = FirstNonEmpty(ReadString(root, "outputTarget"), ReadString(root, "out")),
            };
        }
        catch (JsonException e)
        {
            diagnostics.Add(Error("WorkspaceHub.RestrictedExport.RequestMalformed", $"Restricted export request JSON is invalid: {e.Message}", path));
            return new ExportRequestMetadata();
        }
    }

    private static List<ResourceView> MatchRequestedResources(IEnumerable<ResourceView> resources, ExportRequestMetadata request)
    {
        var matches = resources.Where(resource =>
            (!string.IsNullOrWhiteSpace(request.ArtifactId) && string.Equals(resource.Id, request.ArtifactId, StringComparison.Ordinal)) ||
            (!string.IsNullOrWhiteSpace(request.Path) && string.Equals(resource.RelativePath, request.Path, StringComparison.Ordinal))).ToList();

        if (!string.IsNullOrWhiteSpace(request.Visibility))
        {
            matches = matches
                .Where(resource => string.Equals(resource.Visibility, request.Visibility, StringComparison.Ordinal))
                .ToList();
        }

        return matches
            .OrderBy(resource => resource.Kind, StringComparer.Ordinal)
            .ThenBy(resource => resource.Id, StringComparer.Ordinal)
            .ThenBy(resource => resource.RelativePath, StringComparer.Ordinal)
            .ToList();
    }

    private static RestrictedExportItem ExportItem(ResourceView resource) =>
        new()
        {
            Kind = resource.Kind,
            Id = resource.Id,
            RelativePath = resource.RelativePath,
            Visibility = resource.Visibility,
            Placeholder = resource.Placeholder,
        };

    private static JsonElement LoadJsonRoot(string path, List<Diagnostic> diagnostics, string codePrefix, bool optional = false)
    {
        if (string.IsNullOrWhiteSpace(path))
        {
            if (!optional)
            {
                diagnostics.Add(Error($"{codePrefix}Missing", "Required JSON report path is missing.", path));
            }
            return default;
        }
        if (!File.Exists(path))
        {
            if (!optional)
            {
                diagnostics.Add(Error($"{codePrefix}Missing", "Required JSON report is missing.", path));
            }
            return default;
        }

        try
        {
            using var document = JsonDocument.Parse(File.ReadAllText(path));
            return document.RootElement.Clone();
        }
        catch (JsonException e)
        {
            diagnostics.Add(Error($"{codePrefix}Malformed", $"Required JSON report is invalid: {e.Message}", path));
            return default;
        }
    }

    private static IReadOnlyList<PresenceActorView> ReadPresenceActors(JsonElement root)
    {
        if (root.ValueKind != JsonValueKind.Object ||
            !root.TryGetProperty("actors", out var actors) ||
            actors.ValueKind != JsonValueKind.Array)
        {
            return Array.Empty<PresenceActorView>();
        }

        return actors.EnumerateArray()
            .Select(actor => new PresenceActorView
            {
                ActorId = FirstNonEmpty(ReadString(actor, "actorId"), ReadString(actor, "id")),
                DisplayName = ReadString(actor, "displayName"),
                Role = ReadString(actor, "role"),
                ResourceRef = FirstNonEmpty(ReadString(actor, "resourceRef"), ReadString(actor, "resourceId"), ReadString(actor, "targetRef")),
                Placeholder = ReadString(actor, "placeholder"),
            })
            .ToArray();
    }

    private static IReadOnlyList<PresenceResourceView> ReadPresenceResources(JsonElement root)
    {
        if (root.ValueKind != JsonValueKind.Object ||
            !root.TryGetProperty("resources", out var resources) ||
            resources.ValueKind != JsonValueKind.Array)
        {
            return Array.Empty<PresenceResourceView>();
        }

        return resources.EnumerateArray()
            .Select(resource => new PresenceResourceView
            {
                ResourceId = FirstNonEmpty(ReadString(resource, "resourceId"), ReadString(resource, "id")),
                RelativePath = FirstNonEmpty(ReadString(resource, "relativePath"), ReadString(resource, "path")),
                Visibility = ReadString(resource, "visibility"),
                Placeholder = ReadString(resource, "placeholder"),
            })
            .ToArray();
    }

    private static IReadOnlyList<PresenceActorView> ReadActorsFromRedaction(JsonElement root, string name)
    {
        if (root.ValueKind != JsonValueKind.Object ||
            !root.TryGetProperty(name, out var actors) ||
            actors.ValueKind != JsonValueKind.Array)
        {
            return Array.Empty<PresenceActorView>();
        }

        return actors.EnumerateArray()
            .Select(actor => new PresenceActorView
            {
                ActorId = ReadString(actor, "actorId"),
                DisplayName = ReadString(actor, "displayName"),
                Role = ReadString(actor, "role"),
                ResourceRef = ReadString(actor, "resourceRef"),
                Placeholder = ReadString(actor, "placeholder"),
            })
            .ToArray();
    }

    private static IReadOnlyList<PresenceResourceView> ReadResourcesFromRedaction(JsonElement root, string name)
    {
        if (root.ValueKind != JsonValueKind.Object ||
            !root.TryGetProperty(name, out var resources) ||
            resources.ValueKind != JsonValueKind.Array)
        {
            return Array.Empty<PresenceResourceView>();
        }

        return resources.EnumerateArray()
            .Select(resource => new PresenceResourceView
            {
                ResourceId = ReadString(resource, "resourceId"),
                RelativePath = ReadString(resource, "relativePath"),
                Visibility = ReadString(resource, "visibility"),
                Placeholder = ReadString(resource, "placeholder"),
            })
            .ToArray();
    }

    private static IReadOnlyList<TeamRoomItem> ReadTeamItems(JsonElement root, string name)
    {
        if (root.ValueKind != JsonValueKind.Object ||
            !root.TryGetProperty(name, out var items) ||
            items.ValueKind != JsonValueKind.Array)
        {
            return Array.Empty<TeamRoomItem>();
        }

        return items.EnumerateArray()
            .Select(item => new TeamRoomItem
            {
                Id = FirstNonEmpty(ReadString(item, "id"), ReadString(item, "reviewId"), ReadString(item, "commentId")),
                AuthorRef = ReadString(item, "authorRef"),
                TargetRef = FirstNonEmpty(ReadString(item, "targetRef"), ReadString(item, "resourceRef"), ReadString(item, "relativePath")),
                Summary = ReadString(item, "summary"),
            })
            .ToArray();
    }

    private static ResourceView? FindResource(IEnumerable<ResourceView> resources, string id, string path) =>
        resources.FirstOrDefault(resource =>
            (!string.IsNullOrWhiteSpace(id) && string.Equals(resource.Id, id, StringComparison.Ordinal)) ||
            (!string.IsNullOrWhiteSpace(path) && string.Equals(resource.RelativePath, path, StringComparison.Ordinal)));

    private static bool IsPresenceRefRestricted(string id, string pathOrRef, IEnumerable<ResourceView> resources)
    {
        var resource = FindResource(resources, id, pathOrRef);
        if (resource is not null)
        {
            return IsRestricted(resource);
        }

        return resources.Any(item =>
            IsRestricted(item) &&
            (string.Equals(item.Id, pathOrRef, StringComparison.Ordinal) ||
             string.Equals(item.RelativePath, pathOrRef, StringComparison.Ordinal)));
    }

    private static IReadOnlyList<PresenceActorView> SortActors(IEnumerable<PresenceActorView> actors) =>
        actors
            .OrderBy(item => item.Role, StringComparer.Ordinal)
            .ThenBy(item => item.ActorId, StringComparer.Ordinal)
            .ThenBy(item => item.DisplayName, StringComparer.Ordinal)
            .ThenBy(item => item.Placeholder, StringComparer.Ordinal)
            .ToArray();

    private static IReadOnlyList<PresenceResourceView> SortPresenceResources(IEnumerable<PresenceResourceView> resources) =>
        resources
            .OrderBy(item => item.Visibility, StringComparer.Ordinal)
            .ThenBy(item => item.ResourceId, StringComparer.Ordinal)
            .ThenBy(item => item.RelativePath, StringComparer.Ordinal)
            .ToArray();

    private static IReadOnlyList<TeamRoomItem> SortTeamItems(IEnumerable<TeamRoomItem> items) =>
        items
            .OrderBy(item => item.TargetRef, StringComparer.Ordinal)
            .ThenBy(item => item.Id, StringComparer.Ordinal)
            .ToArray();

    private static PolicyActionDescriptor DisabledAction(string actionId, string reason, string owner) =>
        new()
        {
            ActionId = actionId,
            Status = "Disabled",
            Reason = reason,
            SourceMutationOwner = owner,
        };

    private static PolicyActionDescriptor BlockedAction(string actionId, string reason) =>
        new()
        {
            ActionId = actionId,
            Status = "BlockedByPolicy",
            Reason = reason,
            SourceMutationOwner = "None",
        };

    private static PolicyActionDescriptor PolicyDrivenAction(string actionId, PolicySummary policy) =>
        new()
        {
            ActionId = actionId,
            Status = ActionStatus(policy),
            Reason = $"Policy adapter decision: {policy.AdapterDecision}.",
            SourceMutationOwner = "None",
        };

    private static PolicyActionDescriptor ExportAction(PolicySummary policy, JsonElement export)
    {
        if (export.ValueKind == JsonValueKind.Object)
        {
            var counts = export.TryGetProperty("counts", out var countsElement) ? countsElement : default;
            if (ReadString(export, "status") != "Passed" || ReadInt(counts, "blockedExports") > 0)
            {
                return BlockedAction("ExportArtifact", "Restricted export evidence blocks this descriptor.");
            }
        }

        return PolicyDrivenAction("ExportArtifact", policy);
    }

    private static PolicyActionDescriptor ReviewAction(PolicySummary policy, JsonElement review)
    {
        if (review.ValueKind == JsonValueKind.Object)
        {
            var decision = ReadString(review, "decision");
            return new PolicyActionDescriptor
            {
                ActionId = "ApproveReview",
                Status = decision == "ApprovedMetadataOnly" ? "Enabled" : decision == "RequiresReview" ? "RequiresReview" : "BlockedByPolicy",
                Reason = $"Review approval evidence decision: {decision}. Metadata only; no patch apply or rollback.",
                SourceMutationOwner = "None",
            };
        }

        return PolicyDrivenAction("ApproveReview", policy);
    }

    private static string ActionStatus(PolicySummary policy) =>
        policy.AdapterDecision switch
        {
            "AllowedByReport" => "Enabled",
            "RequiresReviewByReport" => "RequiresReview",
            "DeniedByReport" => "BlockedByPolicy",
            _ => "Disabled",
        };

    private static IReadOnlyList<EvidenceDocument> LoadEvidenceDocuments(string rootPath, List<Diagnostic> diagnostics)
    {
        var root = Path.GetFullPath(rootPath);
        if (!Directory.Exists(root))
        {
            diagnostics.Add(Error("WorkspaceHub.GovernancePanel.EvidenceRootMissing", "Evidence root is missing.", rootPath));
            return Array.Empty<EvidenceDocument>();
        }

        var documents = new List<EvidenceDocument>();
        foreach (var path in Directory.EnumerateFiles(root, "*.json", SearchOption.AllDirectories).OrderBy(path => path, StringComparer.Ordinal))
        {
            try
            {
                var node = JsonNode.Parse(File.ReadAllText(path)) as JsonObject;
                if (node is not null && ReadString(node, "command") != "governance-panel")
                {
                    documents.Add(new EvidenceDocument(FullPath(path), node));
                }
            }
            catch (JsonException e)
            {
                diagnostics.Add(Error("WorkspaceHub.GovernancePanel.EvidenceMalformed", $"Evidence JSON is invalid: {e.Message}", path));
            }
        }

        return documents;
    }

    private static IEnumerable<EvidenceSummary> Summaries(
        IEnumerable<EvidenceDocument> evidence,
        Func<EvidenceDocument, bool> predicate,
        string kind) =>
        evidence
            .Where(predicate)
            .Select(item => new EvidenceSummary
            {
                Kind = kind,
                Path = item.Path,
                Status = ReadString(item.Root, "status"),
                Decision = FirstNonEmpty(ReadString(item.Root, "decision"), ReadString(item.Root, "command")),
                Summary = SummaryText(item.Root),
            })
            .OrderBy(item => item.Path, StringComparer.Ordinal);

    private static IEnumerable<EvidenceSummary> PublishGateSummaries(IEnumerable<EvidenceDocument> evidence)
    {
        foreach (var item in evidence
            .Where(item => ReadString(item.Root, "tool") == "sagapack" && ReadString(item.Root, "command") == "publish-check")
            .OrderBy(item => item.Path, StringComparer.Ordinal))
        {
            if (item.Root["gates"] is not JsonArray gates)
            {
                yield return new EvidenceSummary
                {
                    Kind = "publishGate",
                    Path = item.Path,
                    Status = ReadString(item.Root, "status"),
                    Decision = "NoGates",
                    Summary = "Publish-check report has no gate array.",
                };
                continue;
            }

            foreach (var gate in gates.OfType<JsonObject>().OrderBy(gate => ReadString(gate, "name"), StringComparer.Ordinal))
            {
                yield return new EvidenceSummary
                {
                    Kind = "publishGate",
                    Path = item.Path,
                    Status = ReadString(gate, "status"),
                    Decision = ReadString(gate, "name"),
                    Summary = ReadString(gate, "message"),
                };
            }
        }
    }

    private static IEnumerable<EvidenceSummary> ResidualDebtSummaries(IEnumerable<EvidenceDocument> evidence)
    {
        foreach (var item in evidence.OrderBy(item => item.Path, StringComparer.Ordinal))
        {
            if (item.Root["residualDebt"] is not JsonArray debt)
            {
                continue;
            }

            foreach (var entry in debt)
            {
                yield return new EvidenceSummary
                {
                    Kind = "residualDebt",
                    Path = item.Path,
                    Status = "Open",
                    Decision = "",
                    Summary = entry is JsonValue value && value.TryGetValue<string>(out var text)
                        ? text
                        : entry?.ToJsonString(ReportWriter.Options) ?? "",
                };
            }
        }
    }

    private static IEnumerable<EvidenceSummary> MissingEvidenceSummaries(IEnumerable<EvidenceDocument> evidence)
    {
        foreach (var item in evidence.OrderBy(item => item.Path, StringComparer.Ordinal))
        {
            if (ReadString(item.Root, "status") == "MissingEvidence")
            {
                yield return new EvidenceSummary
                {
                    Kind = "missingEvidence",
                    Path = item.Path,
                    Status = "MissingEvidence",
                    Decision = FirstNonEmpty(ReadString(item.Root, "decision"), ReadString(item.Root, "command")),
                    Summary = "Report status indicates missing evidence.",
                };
            }

            if (item.Root["checks"] is JsonArray checks)
            {
                foreach (var check in checks.OfType<JsonObject>().Where(check => ReadString(check, "status") == "MissingEvidence"))
                {
                    yield return new EvidenceSummary
                    {
                        Kind = "missingEvidence",
                        Path = item.Path,
                        Status = "MissingEvidence",
                        Decision = ReadString(check, "checkId"),
                        Summary = ReadString(check, "summary"),
                    };
                }
            }
        }
    }

    private static string SummaryText(JsonObject root)
    {
        if (root["summary"] is JsonObject summary)
        {
            return summary.ToJsonString(ReportWriter.Options);
        }
        if (root["counts"] is JsonObject counts)
        {
            return counts.ToJsonString(ReportWriter.Options);
        }

        return "";
    }

    private static string ReadString(JsonObject? obj, string key) =>
        obj is not null &&
        obj.TryGetPropertyValue(key, out var node) &&
        node is JsonValue value &&
        value.TryGetValue<string>(out var text)
            ? text
            : "";

    private static ProjectSummary LoadProject(string projectPath, List<Diagnostic> diagnostics)
    {
        if (!File.Exists(projectPath))
        {
            diagnostics.Add(Error("WorkspaceHub.ProjectMissing", "Project manifest is missing.", projectPath));
            return new ProjectSummary { ManifestPath = FullPath(projectPath) };
        }

        try
        {
            using var document = JsonDocument.Parse(File.ReadAllText(projectPath));
            var root = document.RootElement;
            return new ProjectSummary
            {
                ProjectId = ReadString(root, "projectId"),
                DisplayName = ReadString(root, "displayName"),
                ProjectRoot = Normalize(Path.GetDirectoryName(Path.GetFullPath(projectPath)) ?? ""),
                ManifestPath = FullPath(projectPath),
                SchemaVersion = ReadInt(root, "schemaVersion"),
            };
        }
        catch (JsonException e)
        {
            diagnostics.Add(Error("WorkspaceHub.ProjectMalformed", $"Project manifest JSON is invalid: {e.Message}", projectPath));
            return new ProjectSummary { ManifestPath = FullPath(projectPath) };
        }
    }

    private static SliceSummary LoadSliceResolution(
        string path,
        List<Diagnostic> diagnostics,
        out List<ResourceView> resources,
        out ProjectSummary project)
    {
        resources = [];
        project = new ProjectSummary();
        if (!File.Exists(path))
        {
            diagnostics.Add(Error("WorkspaceHub.SliceResolutionMissing", "Slice resolution report is missing.", path));
            return new SliceSummary();
        }

        try
        {
            using var document = JsonDocument.Parse(File.ReadAllText(path));
            var root = document.RootElement;
            project = ReadProject(root);
            var slice = root.TryGetProperty("slice", out var sliceElement)
                ? new SliceSummary
                {
                    SliceId = ReadString(sliceElement, "sliceId"),
                    DisplayName = ReadString(sliceElement, "displayName"),
                    IntendedRole = ReadString(sliceElement, "intendedRole"),
                    SlicePath = ReadString(sliceElement, "slicePath"),
                    SchemaVersion = ReadInt(sliceElement, "schemaVersion"),
                }
                : new SliceSummary();

            if (!root.TryGetProperty("resourceVisibility", out var visibility) ||
                visibility.ValueKind != JsonValueKind.Array)
            {
                diagnostics.Add(Error("WorkspaceHub.SliceResourceVisibilityMissing", "Slice resolution report must include resourceVisibility.", path));
                return slice;
            }

            resources = visibility.EnumerateArray()
                .Select(ReadResource)
                .OrderBy(resource => resource.Kind, StringComparer.Ordinal)
                .ThenBy(resource => resource.Id, StringComparer.Ordinal)
                .ThenBy(resource => resource.RelativePath, StringComparer.Ordinal)
                .ToList();
            return slice;
        }
        catch (JsonException e)
        {
            diagnostics.Add(Error("WorkspaceHub.SliceResolutionMalformed", $"Slice resolution report JSON is invalid: {e.Message}", path));
            return new SliceSummary();
        }
    }

    private static PolicySummary BuildPolicySummary(string path, List<Diagnostic> diagnostics)
    {
        if (!File.Exists(path))
        {
            diagnostics.Add(Error("WorkspaceHub.PolicyReportMissing", "Policy report is missing.", path));
            return new PolicySummary
            {
                ReportPath = FullPath(path),
                AdapterDecision = "MissingPolicyEvidence",
                OperationDisposition = "ReportOnlyMissingEvidence",
                OperationRejectedBeforeMutation = true,
            };
        }

        try
        {
            using var document = JsonDocument.Parse(File.ReadAllText(path));
            var root = document.RootElement;
            var decision = ReadString(root, "decision");
            if (string.IsNullOrWhiteSpace(decision))
            {
                diagnostics.Add(Error("WorkspaceHub.PolicyDecisionMissing", "Policy report does not include a decision.", path));
                return MissingPolicy(path);
            }

            var adapterDecision = MapPolicyDecision(decision);
            if (adapterDecision == "MissingPolicyEvidence")
            {
                diagnostics.Add(Error("WorkspaceHub.PolicyEvidenceMissing", "Policy report indicates missing evidence.", path));
            }
            else if (adapterDecision == "UnknownPolicyState")
            {
                diagnostics.Add(Error("WorkspaceHub.PolicyDecisionUnknown", $"Policy report decision is not recognized by WorkspaceHub: {decision}.", path));
            }

            var scope = root.TryGetProperty("scope", out var scopeElement) ? scopeElement : default;
            return new PolicySummary
            {
                ReportPath = FullPath(path),
                SourceDecision = decision,
                AdapterDecision = adapterDecision,
                OperationDisposition = Disposition(adapterDecision),
                OperationRejectedBeforeMutation = adapterDecision is not "AllowedByReport",
                Action = ReadString(root, "action"),
                Resource = ReadString(root, "resource"),
                Role = ReadString(root, "role"),
                Subject = ReadString(root, "subject"),
                ScopeProjectId = scope.ValueKind == JsonValueKind.Object ? ReadString(scope, "projectId") : "",
                ScopeWorkspaceId = scope.ValueKind == JsonValueKind.Object ? ReadString(scope, "workspaceId") : "",
            };
        }
        catch (JsonException e)
        {
            diagnostics.Add(Error("WorkspaceHub.PolicyReportMalformed", $"Policy report JSON is invalid: {e.Message}", path));
            return new PolicySummary
            {
                ReportPath = FullPath(path),
                AdapterDecision = "UnknownPolicyState",
                OperationDisposition = "ReportOnlyUnknownPolicyState",
                OperationRejectedBeforeMutation = true,
            };
        }
    }

    private static ViewSummary LoadViewCompatibility(string path, List<Diagnostic> diagnostics)
    {
        if (!File.Exists(path))
        {
            diagnostics.Add(Error("WorkspaceHub.ViewCompatibilityReportMissing", "View compatibility report is missing.", path));
            return new ViewSummary();
        }

        try
        {
            using var document = JsonDocument.Parse(File.ReadAllText(path));
            var root = document.RootElement;
            return new ViewSummary
            {
                ReportPath = FullPath(path),
                View = ReadString(root, "view"),
                Decision = ReadString(root, "decision"),
                Status = ReadString(root, "status"),
            };
        }
        catch (JsonException e)
        {
            diagnostics.Add(Error("WorkspaceHub.ViewCompatibilityMalformed", $"View compatibility report JSON is invalid: {e.Message}", path));
            return new ViewSummary { ReportPath = FullPath(path) };
        }
    }

    private static ProjectSummary ReadProject(JsonElement root)
    {
        if (!root.TryGetProperty("project", out var project) || project.ValueKind != JsonValueKind.Object)
        {
            return new ProjectSummary();
        }

        return new ProjectSummary
        {
            ProjectId = ReadString(project, "projectId"),
            DisplayName = ReadString(project, "displayName"),
            ProjectRoot = ReadString(project, "projectRoot"),
            ManifestPath = ReadString(project, "manifestPath"),
            SchemaVersion = ReadInt(project, "schemaVersion"),
        };
    }

    private static ResourceView ReadResource(JsonElement element) =>
        new()
        {
            Kind = ReadString(element, "kind"),
            Id = ReadString(element, "id"),
            RelativePath = ReadString(element, "relativePath"),
            Status = ReadString(element, "status"),
            Visibility = ReadString(element, "visibility"),
            Placeholder = ReadString(element, "placeholder"),
        };

    private static bool IsRestricted(ResourceView resource) =>
        resource.Visibility is "SummaryOnly" or "OpaqueRestricted" or "Hidden" ||
        resource.Status is "Restricted" or "Excluded";

    private static string PlaceholderFor(ResourceView resource) =>
        resource.Visibility switch
        {
            "SummaryOnly" => "SummaryOnlyResource",
            "OpaqueRestricted" => "OpaqueRestrictedResource",
            "Hidden" => "HiddenResource",
            _ => "RestrictedResource",
        };

    private static ProjectSummary MergeProject(ProjectSummary project, ProjectSummary fallback) =>
        string.IsNullOrWhiteSpace(project.ProjectId) &&
        string.IsNullOrWhiteSpace(project.DisplayName) &&
        string.IsNullOrWhiteSpace(project.ProjectRoot)
            ? fallback
            : project;

    private static PolicySummary MissingPolicy(string path) =>
        new()
        {
            ReportPath = FullPath(path),
            AdapterDecision = "MissingPolicyEvidence",
            OperationDisposition = "ReportOnlyMissingEvidence",
            OperationRejectedBeforeMutation = true,
        };

    private static string MapPolicyDecision(string decision) =>
        decision switch
        {
            "Allow" => "AllowedByReport",
            "Warn" => "AllowedByReport",
            "Deny" => "DeniedByReport",
            "RequiresReview" => "RequiresReviewByReport",
            "MissingEvidence" => "MissingPolicyEvidence",
            _ => "UnknownPolicyState",
        };

    private static string AdapterStatus(string adapterDecision) =>
        adapterDecision switch
        {
            "AllowedByReport" => "Passed",
            "DeniedByReport" => "Denied",
            "RequiresReviewByReport" => "ReviewRequired",
            "MissingPolicyEvidence" => "MissingEvidence",
            _ => "UnknownPolicyState",
        };

    private static string Disposition(string adapterDecision) =>
        adapterDecision switch
        {
            "AllowedByReport" => "ReportOnlyAllowed",
            "DeniedByReport" => "ReportOnlyRejected",
            "RequiresReviewByReport" => "ReportOnlyRequiresReview",
            "MissingPolicyEvidence" => "ReportOnlyMissingEvidence",
            _ => "ReportOnlyUnknownPolicyState",
        };

    private static List<Diagnostic> BaseDiagnostics() =>
    [
        Info(
            "WorkspaceHub.ReportOnlyLocal",
            "SagaWorkspaceHub reads local reports, exposes no network surface, and does not mutate source.",
            "")
    ];

    private static IReadOnlyList<Diagnostic> SortDiagnostics(IEnumerable<Diagnostic> diagnostics) =>
        diagnostics
            .OrderByDescending(item => item.Severity == "Error")
            .ThenByDescending(item => item.Severity == "Warning")
            .ThenBy(item => item.Code, StringComparer.Ordinal)
            .ThenBy(item => item.Path, StringComparer.Ordinal)
            .ThenBy(item => item.Message, StringComparer.Ordinal)
            .ToArray();

    private static string ReadString(JsonElement element, string name) =>
        element.ValueKind == JsonValueKind.Object &&
        element.TryGetProperty(name, out var value) &&
        value.ValueKind == JsonValueKind.String
            ? value.GetString() ?? ""
            : "";

    private static int ReadInt(JsonElement element, string name) =>
        element.ValueKind == JsonValueKind.Object &&
        element.TryGetProperty(name, out var value) &&
        value.ValueKind == JsonValueKind.Number &&
        value.TryGetInt32(out var result)
            ? result
            : 0;

    private static IReadOnlyList<string> ReadStringArray(JsonElement element, string name)
    {
        if (element.ValueKind != JsonValueKind.Object ||
            !element.TryGetProperty(name, out var value) ||
            value.ValueKind != JsonValueKind.Array)
        {
            return Array.Empty<string>();
        }

        return value.EnumerateArray()
            .Where(item => item.ValueKind == JsonValueKind.String)
            .Select(item => item.GetString() ?? "")
            .Where(item => !string.IsNullOrWhiteSpace(item))
            .OrderBy(item => item, StringComparer.Ordinal)
            .ToArray();
    }

    private static IReadOnlyList<Diagnostic> ReadDiagnostics(JsonElement element)
    {
        if (element.ValueKind != JsonValueKind.Object ||
            !element.TryGetProperty("diagnostics", out var value) ||
            value.ValueKind != JsonValueKind.Array)
        {
            return Array.Empty<Diagnostic>();
        }

        return value.EnumerateArray()
            .Select(item => new Diagnostic
            {
                Severity = ReadString(item, "severity"),
                Code = ReadString(item, "code"),
                Message = ReadString(item, "message"),
                Path = ReadString(item, "path"),
            })
            .OrderBy(item => item.Code, StringComparer.Ordinal)
            .ThenBy(item => item.Path, StringComparer.Ordinal)
            .ThenBy(item => item.Message, StringComparer.Ordinal)
            .ToArray();
    }

    private static Dictionary<string, int> ReadCounts(JsonElement element, string name)
    {
        if (element.ValueKind != JsonValueKind.Object ||
            !element.TryGetProperty(name, out var value) ||
            value.ValueKind != JsonValueKind.Object)
        {
            return new Dictionary<string, int>(StringComparer.Ordinal);
        }

        return value.EnumerateObject()
            .Where(property => property.Value.ValueKind == JsonValueKind.Number && property.Value.TryGetInt32(out _))
            .OrderBy(property => property.Name, StringComparer.Ordinal)
            .ToDictionary(property => property.Name, property => property.Value.GetInt32(), StringComparer.Ordinal);
    }

    private static string FirstNonEmpty(params string[] values) =>
        values.FirstOrDefault(value => !string.IsNullOrWhiteSpace(value)) ?? "";

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

    private static string FullPath(string path) =>
        string.IsNullOrWhiteSpace(path) ? "" : Normalize(Path.GetFullPath(path));

    private static string Normalize(string path) => path.Replace('\\', '/');

    private static int UnknownCommand(string command)
    {
        Console.Error.WriteLine($"Unknown command: {command}");
        PrintUsage();
        return InvalidUsage;
    }

    private static void PrintUsage()
    {
        Console.Error.WriteLine("SagaWorkspaceHub CLI");
        Console.Error.WriteLine("  sagaworkspacehub summarize --project <.sagaproj> --slice-resolution <report> --policy-report <report> --view-compatibility <report> --out <workspacehub_summary_report.json>");
        Console.Error.WriteLine("  sagaworkspacehub policy-adapter --policy-report <policy_evaluation_report.json> --out <workspacehub_policy_adapter_report.json>");
        Console.Error.WriteLine("  sagaworkspacehub slice-view --slice-resolution <restricted_project_resolution_report.json> --view-compatibility <view_slice_compatibility_report.json> --policy-report <policy_evaluation_report.json> --out <workspacehub_slice_scoped_view_report.json>");
        Console.Error.WriteLine("  sagaworkspacehub audit-log --events <audit_events.jsonl> --out <audit_report.json>");
        Console.Error.WriteLine("  sagaworkspacehub review-approval --review-report <review_report.json> --policy-report <policy_evaluation_report.json> [--audit-report <audit_report.json>] --out <review_approval_report.json>");
        Console.Error.WriteLine("  sagaworkspacehub restricted-export --slice-resolution <restricted_project_resolution_report.json> --policy-report <policy_evaluation_report.json> --request <export_request.json> --out <restricted_export_report.json>");
        Console.Error.WriteLine("  sagaworkspacehub presence-redaction --presence-report <presence_report.json> --slice-resolution <restricted_project_resolution_report.json> --policy-report <policy_evaluation_report.json> --out <presence_redaction_report.json>");
        Console.Error.WriteLine("  sagaworkspacehub policy-actions --policy-report <policy_evaluation_report.json> [--review-approval-report <review_approval_report.json>] [--restricted-export-report <restricted_export_report.json>] --out <policy_aware_editor_actions_report.json>");
        Console.Error.WriteLine("  sagaworkspacehub team-room --team-room-status <team_room_status.json> --presence-redaction <presence_redaction_report.json> --slice-resolution <restricted_project_resolution_report.json> [--policy-report <policy_evaluation_report.json>] --out <project_slice_aware_team_room_report.json>");
        Console.Error.WriteLine("  sagaworkspacehub governance-panel --evidence-root <dir> --out <governance_panel_report.json>");
    }

    private sealed class CommandOptions
    {
        public bool Ok { get; init; }
        public string Error { get; init; } = "";
        public string ProjectPath { get; private init; } = "";
        public string SliceResolutionPath { get; private init; } = "";
        public string PolicyReportPath { get; private init; } = "";
        public string ViewCompatibilityPath { get; private init; } = "";
        public string EventsPath { get; private init; } = "";
        public string ReviewReportPath { get; private init; } = "";
        public string AuditReportPath { get; private init; } = "";
        public string RequestPath { get; private init; } = "";
        public string PresenceReportPath { get; private init; } = "";
        public string ReviewApprovalReportPath { get; private init; } = "";
        public string RestrictedExportReportPath { get; private init; } = "";
        public string TeamRoomStatusPath { get; private init; } = "";
        public string PresenceRedactionPath { get; private init; } = "";
        public string EvidenceRoot { get; private init; } = "";
        public string OutPath { get; private init; } = "";

        public static CommandOptions Parse(string[] args)
        {
            var options = new MutableOptions();
            for (var i = 0; i < args.Length; ++i)
            {
                if (i + 1 >= args.Length)
                {
                    return Fail($"{args[i]} requires a value.");
                }

                switch (args[i])
                {
                    case "--project":
                        options.ProjectPath = args[++i];
                        break;
                    case "--slice-resolution":
                        options.SliceResolutionPath = args[++i];
                        break;
                    case "--policy-report":
                        options.PolicyReportPath = args[++i];
                        break;
                    case "--view-compatibility":
                        options.ViewCompatibilityPath = args[++i];
                        break;
                    case "--events":
                        options.EventsPath = args[++i];
                        break;
                    case "--review-report":
                        options.ReviewReportPath = args[++i];
                        break;
                    case "--audit-report":
                        options.AuditReportPath = args[++i];
                        break;
                    case "--request":
                        options.RequestPath = args[++i];
                        break;
                    case "--presence-report":
                        options.PresenceReportPath = args[++i];
                        break;
                    case "--review-approval-report":
                        options.ReviewApprovalReportPath = args[++i];
                        break;
                    case "--restricted-export-report":
                        options.RestrictedExportReportPath = args[++i];
                        break;
                    case "--team-room-status":
                        options.TeamRoomStatusPath = args[++i];
                        break;
                    case "--presence-redaction":
                        options.PresenceRedactionPath = args[++i];
                        break;
                    case "--evidence-root":
                        options.EvidenceRoot = args[++i];
                        break;
                    case "--out":
                        options.OutPath = args[++i];
                        break;
                    default:
                        return Fail($"Unknown argument: {args[i]}");
                }
            }

            return new CommandOptions
            {
                Ok = true,
                ProjectPath = options.ProjectPath,
                SliceResolutionPath = options.SliceResolutionPath,
                PolicyReportPath = options.PolicyReportPath,
                ViewCompatibilityPath = options.ViewCompatibilityPath,
                EventsPath = options.EventsPath,
                ReviewReportPath = options.ReviewReportPath,
                AuditReportPath = options.AuditReportPath,
                RequestPath = options.RequestPath,
                PresenceReportPath = options.PresenceReportPath,
                ReviewApprovalReportPath = options.ReviewApprovalReportPath,
                RestrictedExportReportPath = options.RestrictedExportReportPath,
                TeamRoomStatusPath = options.TeamRoomStatusPath,
                PresenceRedactionPath = options.PresenceRedactionPath,
                EvidenceRoot = options.EvidenceRoot,
                OutPath = options.OutPath,
            };
        }

        private static CommandOptions Fail(string error) => new() { Ok = false, Error = error };

        private sealed class MutableOptions
        {
            public string ProjectPath { get; set; } = "";
            public string SliceResolutionPath { get; set; } = "";
            public string PolicyReportPath { get; set; } = "";
            public string ViewCompatibilityPath { get; set; } = "";
            public string EventsPath { get; set; } = "";
            public string ReviewReportPath { get; set; } = "";
            public string AuditReportPath { get; set; } = "";
            public string RequestPath { get; set; } = "";
            public string PresenceReportPath { get; set; } = "";
            public string ReviewApprovalReportPath { get; set; } = "";
            public string RestrictedExportReportPath { get; set; } = "";
            public string TeamRoomStatusPath { get; set; } = "";
            public string PresenceRedactionPath { get; set; } = "";
            public string EvidenceRoot { get; set; } = "";
            public string OutPath { get; set; } = "";
        }
    }

    private sealed record EvidenceDocument(string Path, JsonObject Root);
}
