using System.Text.Json.Nodes;

namespace SagaAlphaGate;

internal static class Program
{
    private const int Passed = 0;
    private const int Failed = 1;
    private const int InvalidUsage = 2;
    private const int MissingInput = 3;
    private const int InternalError = 4;

    private static readonly string[] AllowedAlphaClaims =
    [
        "Small-team alpha planning may begin after accepted Technical Preview evidence.",
        "Alpha work is limited to small-team workflow proof for MultiplayerSandbox.",
        "Opening Block A may define evidence, acceptance, and budget contracts.",
    ];

    private static readonly string[] ClosureAlphaClaims =
    [
        "Small-Team Alpha foundation evidence may close Hedef 2 when required focused evidence is present.",
        "Local/offline collaboration review metadata is accepted only as model/report evidence.",
        "C# source-preserving authoring, backend-neutral editor workflow models, and package/launch/asset/performance gates are bounded foundation evidence.",
        "Deferred gameplay expansion, client preview, and editor UI remain explicit closure debt.",
    ];

    private static readonly string[] BlockedAlphaClaims =
    [
        "product beta",
        "release candidate",
        "production-ready MMO server",
        "production network readiness",
        "full visual scripting",
        "arbitrary C# to blocks",
        "editable graph",
        "patch apply",
        "source mutation",
        "full editor MVP",
        "full collaboration",
        "enterprise-ready collaboration",
        "cloud workspace",
        "full security model",
        "raw full CTest passed",
        "Hedef 3 started",
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
                "opening-check" => RunOpeningCheck(options),
                "acceptance-plan" => RunAcceptancePlan(options),
                "budget-report" => RunBudgetReport(options),
                "script-evidence" => RunScriptEvidence(options),
                "editor-workflow-evidence" => RunEditorWorkflowEvidence(options),
                "collaboration-evidence" => RunCollaborationEvidence(options),
                "gameplay-expansion-blockers" => RunGameplayExpansionBlockers(options),
                "accept-alpha" => RunAcceptAlpha(options),
                "evidence-matrix" => RunEvidenceMatrix(options),
                "close-alpha" => RunCloseAlpha(options),
                _ => InvalidUsage,
            };
        }
        catch (FileNotFoundException e)
        {
            Console.Error.WriteLine(e.Message);
            return MissingInput;
        }
        catch (Exception e)
        {
            Console.Error.WriteLine($"Internal sagaalphagate error: {e.Message}");
            return InternalError;
        }
    }

    private static int RunOpeningCheck(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.TechnicalPreviewRoot) ||
            string.IsNullOrWhiteSpace(options.OutPath))
        {
            Console.Error.WriteLine("opening-check requires --technical-preview-root and --out.");
            return InvalidUsage;
        }

        var root = Path.GetFullPath(options.TechnicalPreviewRoot);
        var evidence = TechnicalPreviewEvidence(root).ToArray();
        var diagnostics = evidence
            .Where(item => item.Required && item.Status != "Passed" && item.Status != "Accepted")
            .Select(item => Error(
                "Alpha.Opening.TechnicalPreviewEvidenceMissingOrBlocked",
                "Required Technical Preview closure evidence is missing or blocked.",
                item.Path))
            .ToList();
        var status = diagnostics.Count == 0 ? "Passed" : "Blocked";
        var report = new OpeningReport
        {
            Status = status,
            TechnicalPreviewRoot = root,
            Phase65Evidence = evidence,
            AllowedAlphaClaims = AllowedAlphaClaims,
            BlockedAlphaClaims = BlockedAlphaClaims,
            ResidualTechnicalPreviewDebt =
            [
                "Client preview remains bounded by roadmap evidence.",
                "Patch apply and C# source mutation remain blocked until a later source-trust phase explicitly opens them.",
                "Editor workflow implementation starts in later Hedef 2 blocks, not Phase 66.",
                "Enterprise policy, security, cloud workspace, and Hedef 3 work remain blocked.",
            ],
            Diagnostics = diagnostics,
        };
        ReportWriter.Write(options.OutPath, report);
        return status == "Passed" ? Passed : Failed;
    }

    private static int RunAcceptancePlan(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.OutPath))
        {
            Console.Error.WriteLine("acceptance-plan requires --out.");
            return InvalidUsage;
        }

        var report = new AcceptancePlanReport
        {
            ScenarioMatrix = AcceptanceScenario(),
            Diagnostics =
            [
                Info(
                    "Alpha.AcceptancePlan.InventoryOnly",
                    "Phase 67 inventories the Small-Team Alpha acceptance path; later phases implement missing workflow evidence."),
            ],
        };
        ReportWriter.Write(options.OutPath, report);
        return Passed;
    }

    private static int RunAcceptAlpha(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.EvidenceRoot) ||
            string.IsNullOrWhiteSpace(options.OutPath))
        {
            Console.Error.WriteLine("accept-alpha requires --evidence-root and --out.");
            return InvalidUsage;
        }

        var evidenceRoot = Path.GetFullPath(options.EvidenceRoot);
        var evidence = SmallTeamAlphaEvidence(evidenceRoot).ToArray();
        var categories = AlphaCategories(evidence);
        var diagnostics = EvidenceDiagnostics(evidence, "Alpha.Acceptance").ToList();
        if (evidence.Any(item => item.Status == "Deferred" && item.Category.StartsWith("Deferred", StringComparison.Ordinal)))
        {
            diagnostics.Add(Warn(
                "Alpha.Acceptance.DeferredScope",
                "Deferred gameplay, client preview, or editor UI remains non-passing closure debt.",
                "Phase 93"));
        }

        var status = GateStatus(evidence);
        var report = new SmallTeamAlphaAcceptanceReport
        {
            Status = status,
            EvidenceRoot = evidenceRoot,
            Categories = categories,
            Evidence = evidence,
            AllowedAlphaClaims = ClosureAlphaClaims,
            BlockedAlphaClaims = BlockedAlphaClaims,
            Diagnostics = diagnostics,
        };
        ReportWriter.Write(options.OutPath, report);
        return status == "Failed" || status == "Blocked" ? Failed : Passed;
    }

    private static int RunEvidenceMatrix(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.EvidenceRoot) ||
            string.IsNullOrWhiteSpace(options.OutPath))
        {
            Console.Error.WriteLine("evidence-matrix requires --evidence-root and --out.");
            return InvalidUsage;
        }

        var evidenceRoot = Path.GetFullPath(options.EvidenceRoot);
        var matrix = SmallTeamAlphaEvidence(evidenceRoot).ToArray();
        var diagnostics = EvidenceDiagnostics(matrix, "Alpha.EvidenceMatrix").ToArray();
        var status = MatrixStatus(matrix);
        var report = new SmallTeamAlphaEvidenceMatrixReport
        {
            Status = status,
            EvidenceRoot = evidenceRoot,
            Matrix = matrix,
            Diagnostics = diagnostics,
        };
        ReportWriter.Write(options.OutPath, report);
        return status == "Failed" || status == "Blocked" ? Failed : Passed;
    }

    private static int RunCloseAlpha(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.EvidenceRoot) ||
            string.IsNullOrWhiteSpace(options.OutPath))
        {
            Console.Error.WriteLine("close-alpha requires --evidence-root and --out.");
            return InvalidUsage;
        }

        var evidenceRoot = Path.GetFullPath(options.EvidenceRoot);
        var matrix = SmallTeamAlphaEvidence(evidenceRoot).ToArray();
        var diagnostics = EvidenceDiagnostics(matrix, "Alpha.Closure").ToList();
        var acceptanceStatus = ReportEvidenceStatus(Path.Combine(evidenceRoot, "small_team_alpha_acceptance_report.json"));
        var matrixStatus = ReportEvidenceStatus(Path.Combine(evidenceRoot, "small_team_alpha_evidence_matrix.json"));
        if (acceptanceStatus.Status != "Passed")
        {
            diagnostics.Add(Error(
                "Alpha.Closure.AcceptanceReportMissingOrBlocked",
                "Phase 95 closure requires a passing small_team_alpha_acceptance_report.json.",
                "small_team_alpha_acceptance_report.json"));
        }
        if (matrixStatus.Status is "MissingEvidence" or "Failed" or "Blocked")
        {
            diagnostics.Add(Error(
                "Alpha.Closure.EvidenceMatrixMissingOrBlocked",
                "Phase 95 closure requires a usable small_team_alpha_evidence_matrix.json.",
                "small_team_alpha_evidence_matrix.json"));
        }

        var blocking = diagnostics.Any(item => item.Severity == "Error");
        var decision = blocking
            ? "Small-Team Alpha rejected / blocked"
            : matrix.Any(item => item.Status == "MissingEvidence" || item.Status == "Deferred" || item.Status == "Blocked")
                ? "Small-Team Alpha partially proven"
                : "Small-Team Alpha accepted";
        var status = decision switch
        {
            "Small-Team Alpha accepted" => "Accepted",
            "Small-Team Alpha partially proven" => "PartiallyProven",
            _ => "RejectedOrBlocked",
        };
        var report = new SmallTeamAlphaClosureReport
        {
            Status = status,
            EvidenceRoot = evidenceRoot,
            Decision = decision,
            ImplementedMatrix = matrix.Where(item => item.Status == "Passed").ToArray(),
            EvidenceMatrix = matrix,
            KnownDebt =
            [
                "Runtime and Server gameplay expansion remains outside Hedef 2 closure evidence.",
                "Client preview remains blocked until a bounded ClientHost/runtime preview report seam exists.",
                "Editor UI and Qt implementation remain outside this backend-neutral closure.",
                "Scene/entity source truth and asset import/cook workflows remain deferred.",
                "Production networking, heavy stress, long soak, and raw full CTest are not claimed.",
                "Enterprise, cloud, account/auth, permission enforcement, and security models start only after Hedef 3 is explicitly opened.",
            ],
            BlockedClaims = BlockedAlphaClaims,
            Hedef3OpeningRecommendation = status == "RejectedOrBlocked"
                ? "Do not open Hedef 3 until Phase 95 required evidence is present and blockers are resolved or explicitly accepted as residual debt."
                : "Hedef 3 may be planned after this Phase 95 closure, but this report does not start Phase 96 or implement enterprise work.",
            Diagnostics = diagnostics,
        };
        ReportWriter.Write(options.OutPath, report);
        return status == "RejectedOrBlocked" ? Failed : Passed;
    }

    private static int RunBudgetReport(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.OutPath))
        {
            Console.Error.WriteLine("budget-report requires --out.");
            return InvalidUsage;
        }

        var evidenceRoot = string.IsNullOrWhiteSpace(options.EvidenceRoot)
            ? ""
            : Path.GetFullPath(options.EvidenceRoot);
        var measurements = LoadMeasurements(evidenceRoot);
        var budgets = BudgetDefinitions()
            .Select(budget =>
            {
                var measured = measurements.TryGetValue(budget.Id, out var value) ? value : (int?)null;
                var status = measured is null
                    ? "MeasurementMissing"
                    : measured.Value <= budget.BudgetMs ? "Passed" : "BudgetExceeded";
                return budget with
                {
                    MeasuredMs = measured,
                    Status = status,
                    EvidencePath = string.IsNullOrWhiteSpace(evidenceRoot) ? "" : "alpha_budget_measurements.json",
                };
            })
            .ToArray();
        var diagnostics = budgets.SelectMany(budget =>
            budget.Status switch
            {
                "MeasurementMissing" => new[] { Warn("Alpha.Budget.MeasurementMissing", "Budget measurement is missing.", budget.Id) },
                "BudgetExceeded" => new[] { Error("Alpha.Budget.Exceeded", "Budget measurement exceeds the v0 budget.", budget.Id) },
                _ => Array.Empty<Diagnostic>(),
            }).ToArray();
        var status = budgets.Any(budget => budget.Status == "BudgetExceeded")
            ? "Failed"
            : budgets.Any(budget => budget.Status == "MeasurementMissing") ? "PassedWithMissingMeasurements" : "Passed";
        var report = new BudgetReport
        {
            Status = status,
            Budgets = budgets,
            Diagnostics = diagnostics,
        };
        ReportWriter.Write(options.OutPath, report);
        return status == "Failed" ? Failed : Passed;
    }

    private static int RunScriptEvidence(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.ScriptValidationPath) ||
            string.IsNullOrWhiteSpace(options.OutPath))
        {
            Console.Error.WriteLine("script-evidence requires --script-validation and --out.");
            return InvalidUsage;
        }

        var path = Path.GetFullPath(options.ScriptValidationPath);
        var diagnostics = new List<Diagnostic>();
        var validationStatus = "Missing";
        var summary = new ScriptRuntimeProofSummary();
        if (!File.Exists(path))
        {
            diagnostics.Add(Error("Alpha.ScriptEvidence.ValidationMissing", "SagaScript validation report is missing.", path));
        }
        else
        {
            try
            {
                var json = JsonNode.Parse(File.ReadAllText(path)) as JsonObject
                    ?? throw new InvalidOperationException("script validation report must be a JSON object.");
                validationStatus = ReadString(json, "status");
                if (ReadString(json, "tool") != "sagascript" ||
                    ReadString(json, "command") != "validate-artifacts")
                {
                    diagnostics.Add(Error("Alpha.ScriptEvidence.InvalidValidationReport", "Expected a sagascript validate-artifacts report.", path));
                }
                var runtimeProofSummary = json["runtimeProofSummary"] as JsonObject;
                summary = new ScriptRuntimeProofSummary
                {
                    RuntimeBackedWithEvidence = ReadInt(runtimeProofSummary, "runtimeBackedWithEvidence"),
                    RuntimeBackedMissingEvidence = ReadInt(runtimeProofSummary, "runtimeBackedMissingEvidence"),
                    ProjectionOnly = ReadInt(runtimeProofSummary, "projectionOnly"),
                    Deferred = ReadInt(runtimeProofSummary, "deferred"),
                };
                if (validationStatus != "Passed")
                {
                    diagnostics.Add(Error("Alpha.ScriptEvidence.ValidationBlocked", "SagaScript validation report is not Passed.", path));
                }
                if (summary.RuntimeBackedMissingEvidence > 0)
                {
                    diagnostics.Add(Error("Alpha.ScriptEvidence.RuntimeProofMissing", "RuntimeBacked script nodes are missing explicit runtime proof.", path));
                }
            }
            catch (Exception e) when (e is IOException or InvalidOperationException or System.Text.Json.JsonException)
            {
                validationStatus = "Failed";
                diagnostics.Add(Error("Alpha.ScriptEvidence.InvalidJson", $"Unable to read SagaScript validation report: {e.Message}", path));
            }
        }

        var status = diagnostics.Any(diagnostic => diagnostic.Severity == "Error") ? "Blocked" : "Passed";
        var report = new ScriptEvidenceReport
        {
            Status = status,
            ScriptValidationPath = path,
            ScriptValidationStatus = validationStatus,
            RuntimeProofSummary = summary,
            BlockedClaims = BlockedAlphaClaims,
            Diagnostics = diagnostics,
        };
        ReportWriter.Write(options.OutPath, report);
        return status == "Passed" ? Passed : Failed;
    }

    private static int RunGameplayExpansionBlockers(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.OutPath))
        {
            Console.Error.WriteLine("gameplay-expansion-blockers requires --out.");
            return InvalidUsage;
        }

        var blockers = new[]
        {
            Blocker("pickup", "Pickup interaction", "Runtime gameplay", "Requires runtime gameplay source truth and executable gameplay behavior."),
            Blocker("door-trigger", "Door/trigger interaction", "Runtime gameplay", "Current gameplay nodes are projection/deferred metadata, not runtime proof."),
            Blocker("score-reward", "Score/reward loop", "Server gameplay", "Requires authoritative state changes and accepted gameplay source truth."),
            Blocker("spawn-reset", "Spawn/reset flow", "Server gameplay", "Requires bounded server gameplay behavior beyond current headless launch proof."),
            Blocker("two-client-local-preview", "Two-client/local preview", "ClientHost bounded preview seam", "Client preview is deferred without a bounded ClientHost/runtime preview report seam."),
            Blocker("diagnostics-markers", "Gameplay diagnostics markers", "Runtime and Server gameplay", "Diagnostics markers need real gameplay events before they can be validated."),
        };
        var report = new GameplayExpansionBlockerReport
        {
            Blockers = blockers,
            Diagnostics =
            [
                Warn(
                    "Alpha.GameplayExpansion.Deferred",
                    "Phase 86 is deferred; no gameplay systems, server authority, Runtime, ClientHost, scripts, scenes, or package behavior are modified.",
                    "Phase 86"),
            ],
        };
        ReportWriter.Write(options.OutPath, report);
        return Passed;
    }

    private static int RunEditorWorkflowEvidence(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.OutPath))
        {
            Console.Error.WriteLine("editor-workflow-evidence requires --out.");
            return InvalidUsage;
        }

        var report = new WorkflowEvidenceReport
        {
            Command = "editor-workflow-evidence",
            ReportKind = "EditorWorkflowModel",
            PhaseRange = "Phase 76-81",
            EvidenceScope = "Backend-neutral editor workflow model evidence only; no Editor UI, Qt UI, graph editing, or source mutation is claimed.",
            Documentation =
            [
                "docs/architecture/EDITOR.md",
                "docs/architecture/SAGA_EDITOR_SHELL_MINIMUM_WORKFLOW.md",
                "docs/architecture/EDITOR_UI_IMPLEMENTATION_STRATEGY.md",
            ],
            FocusedTests = ["EditorAuthoringSpineTests"],
            BlockedClaims =
            [
                "full editor MVP",
                "full visual scripting",
                "editable graph",
                "patch apply",
                "source mutation",
            ],
            Diagnostics =
            [
                Info(
                    "Alpha.EditorWorkflow.ModelEvidenceOnly",
                    "Editor workflow evidence is limited to backend-neutral authoring model coverage and focused tests.",
                    "EditorAuthoringSpineTests"),
            ],
        };
        ReportWriter.Write(options.OutPath, report);
        return Passed;
    }

    private static int RunCollaborationEvidence(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.OutRoot))
        {
            Console.Error.WriteLine("collaboration-evidence requires --out-root.");
            return InvalidUsage;
        }

        var outRoot = Path.GetFullPath(options.OutRoot);
        Directory.CreateDirectory(outRoot);
        var reports = new (string FileName, string Kind, string Scope)[]
        {
            ("workspace_session_report.json", "WorkspaceSession", "Local/offline workspace session model evidence for simulated small-team activity."),
            ("workspace_lock_report.json", "WorkspaceLock", "Local/offline persistent artifact lock model evidence."),
            ("artifact_comments_report.json", "ArtifactComments", "Local/offline artifact comment transaction model evidence."),
            ("review_report.json", "ReviewRequest", "Local/offline review request and decision metadata evidence."),
            ("team_room_status_report.json", "TeamRoomStatus", "Local/offline Team Room status model evidence; no cloud presence service is claimed."),
        };

        foreach (var item in reports)
        {
            var report = new WorkflowEvidenceReport
            {
                Command = "collaboration-evidence",
                ReportKind = item.Kind,
                PhaseRange = "Phase 87-92",
                EvidenceScope = item.Scope,
                Documentation =
                [
                    "docs/architecture/SAGA_COLLABORATIVE_WORKSPACE_ARCHITECTURE.md",
                    "docs/architecture/SAGA_REVIEW_COMMENT_TRANSACTION_MODEL.md",
                    "docs/internal/product-history/SMALL_TEAM_ALPHA_COLLABORATION_GUIDE.md",
                ],
                FocusedTests = ["CollaborationModelTests"],
                BlockedClaims =
                [
                    "full collaboration",
                    "enterprise-ready collaboration",
                    "cloud workspace",
                    "full security model",
                    "auth system implemented",
                    "permission system implemented",
                ],
                Diagnostics =
                [
                    Info(
                        "Alpha.Collaboration.LocalModelEvidenceOnly",
                        "Collaboration evidence is limited to local/offline model reports and focused tests.",
                        "CollaborationModelTests"),
                ],
            };
            ReportWriter.Write(Path.Combine(outRoot, item.FileName), report);
        }

        return Passed;
    }

    private static IEnumerable<EvidenceCheck> TechnicalPreviewEvidence(string root)
    {
        var checks = new (string Id, string Phase, string Title, string RelativePath)[]
        {
            ("phase60", "Phase 60", "Clean Onboarding Command", "quickstart_report.json"),
            ("phase61", "Phase 61", "Full MVP Acceptance Script", "Acceptance/mvp_acceptance_report.json"),
            ("phase62", "Phase 62", "Cross-Platform Build Evidence", "build_matrix_report.json"),
            ("phase63", "Phase 63", "Known Limitations / Non-Claims Freeze", "docguard_report.json"),
            ("phase64", "Phase 64", "SagaEngine 0.1 Technical Preview Package", "Package/SagaEngine-0.1-TechnicalPreview/technical_preview_package_report.json"),
            ("phase65", "Phase 65", "MVP Closure Audit", "technical_preview_closure_report.json"),
        };
        foreach (var check in checks)
        {
            var path = Path.Combine(root, check.RelativePath);
            yield return new EvidenceCheck
            {
                Id = check.Id,
                Phase = check.Phase,
                Title = check.Title,
                Path = check.RelativePath,
                Status = ReportStatus(path),
            };
        }
    }

    private static IReadOnlyList<AcceptanceScenarioStep> AcceptanceScenario()
    {
        var rows = new (string Id, string Description, string Classification, string Evidence, string Phase)[]
        {
            ("simulated-users", "Two or three simulated users are represented in local/offline workflow evidence.", "DeferredToLaterPhase", "workspace_session_report.json", "Phase 87"),
            ("open-project", "Open MultiplayerSandbox from shared project truth.", "Automated", "project_validation_report.json", "Phase 66"),
            ("simple-block-edit", "Edit a simple supported behavior through blocks.", "DeferredToLaterPhase", "patch/diff evidence", "Phase 72"),
            ("csharp-diff-review", "Inspect and review the same behavior through C# source/diff views.", "DeferredToLaterPhase", "diff_review_report.json", "Phase 79"),
            ("local-preview", "Run local preview from the accepted launch path.", "Automated", "launch_preview_report.json", "Phase 84"),
            ("diagnostics-summary", "Summarize diagnostics for the preview run.", "Automated", "diagnostics_summary.json", "Phase 78"),
            ("package-publish-check", "Package and publish-check the project.", "Automated", "publish_report.json", "Phase 83"),
            ("persistent-lock", "Use a persistent artifact lock.", "DeferredToLaterPhase", "workspace_lock_report.json", "Phase 88"),
            ("comment", "Add and resolve an artifact comment.", "DeferredToLaterPhase", "artifact_comments_report.json", "Phase 89"),
            ("review-request", "Request and review a change.", "DeferredToLaterPhase", "review_report.json", "Phase 90"),
            ("team-room-status", "Show Team Room activity and review queue status.", "Manual", "team_room_status_report.json", "Phase 92"),
            ("alpha-acceptance-report", "Produce the alpha acceptance report.", "DeferredToLaterPhase", "small_team_alpha_acceptance_report.json", "Phase 93"),
        };

        return rows.Select((row, index) => new AcceptanceScenarioStep
        {
            Sequence = index + 1,
            Id = row.Id,
            Description = row.Description,
            Classification = row.Classification,
            RequiredEvidence = row.Evidence,
            RoadmapPhase = row.Phase,
        }).ToArray();
    }

    private static IReadOnlyList<BudgetEntry> BudgetDefinitions()
    {
        return
        [
            new BudgetEntry { Id = "editor-open", Workflow = "Editor open budget", BudgetMs = 5000 },
            new BudgetEntry { Id = "project-validation", Workflow = "Project validation budget", BudgetMs = 1000 },
            new BudgetEntry { Id = "script-analysis", Workflow = "Script analysis budget", BudgetMs = 2500 },
            new BudgetEntry { Id = "preview-launch", Workflow = "Preview launch budget", BudgetMs = 8000 },
            new BudgetEntry { Id = "package-check", Workflow = "Package check budget", BudgetMs = 4000 },
            new BudgetEntry { Id = "diagnostics-summary", Workflow = "Diagnostics summary budget", BudgetMs = 1000 },
            new BudgetEntry { Id = "collaboration-update", Workflow = "Collaboration update budget", BudgetMs = 1000 },
        ];
    }

    private static IReadOnlyDictionary<string, int> LoadMeasurements(string evidenceRoot)
    {
        if (string.IsNullOrWhiteSpace(evidenceRoot))
        {
            return new Dictionary<string, int>(StringComparer.Ordinal);
        }

        var path = Path.Combine(evidenceRoot, "alpha_budget_measurements.json");
        if (!File.Exists(path))
        {
            return new Dictionary<string, int>(StringComparer.Ordinal);
        }

        var root = JsonNode.Parse(File.ReadAllText(path)) as JsonObject
            ?? throw new InvalidOperationException("alpha_budget_measurements.json must be a JSON object.");
        var measurements = root["measurements"] as JsonArray
            ?? throw new InvalidOperationException("alpha_budget_measurements.json must contain a measurements array.");
        return measurements
            .OfType<JsonObject>()
            .Where(item => item["id"] is not null && item["measuredMs"] is not null)
            .OrderBy(item => item["id"]!.GetValue<string>(), StringComparer.Ordinal)
            .ToDictionary(
                item => item["id"]!.GetValue<string>(),
                item => item["measuredMs"]!.GetValue<int>(),
                StringComparer.Ordinal);
    }

    private static string ReportStatus(string path)
    {
        if (!File.Exists(path))
        {
            return "Missing";
        }

        try
        {
            var json = JsonNode.Parse(File.ReadAllText(path)) as JsonObject;
            return json?["status"]?.GetValue<string>() is { Length: > 0 } status ? status : "Passed";
        }
        catch
        {
            return "Failed";
        }
    }

    private static IReadOnlyList<AlphaAcceptanceCategory> AlphaCategories(IReadOnlyList<AlphaEvidenceItem> evidence)
    {
        var order = new[]
        {
            "ProjectTruth",
            "ScriptAuthoring",
            "EditorWorkflowModel",
            "PackageProfileEvidence",
            "LaunchProfileEvidence",
            "AssetWorkflowEvidence",
            "PerformanceBudgetEvidence",
            "CollaborationReviewEvidence",
            "ClaimHonesty",
            "DeferredGameplayExpansion",
            "DeferredClientPreview",
            "DeferredEditorUI",
        };
        return order
            .Select(category =>
            {
                var items = evidence.Where(item => item.Category == category).ToArray();
                return new AlphaAcceptanceCategory
                {
                    Category = category,
                    Status = CategoryStatus(items),
                    EvidenceIds = items.Select(item => item.Id).ToArray(),
                };
            })
            .ToArray();
    }

    private static string CategoryStatus(IReadOnlyList<AlphaEvidenceItem> items)
    {
        if (items.Count == 0)
        {
            return "NotApplicable";
        }
        if (items.Any(item => item.Required && item.Status == "Failed"))
        {
            return "Failed";
        }
        if (items.Any(item => item.Required && item.Status == "Blocked"))
        {
            return "Blocked";
        }
        if (items.Any(item => item.Required && item.Status == "MissingEvidence"))
        {
            return "MissingEvidence";
        }
        if (items.All(item => item.Status == "Deferred"))
        {
            return "Deferred";
        }
        if (items.All(item => item.Status == "NotApplicable"))
        {
            return "NotApplicable";
        }
        return "Passed";
    }

    private static string GateStatus(IReadOnlyList<AlphaEvidenceItem> evidence)
    {
        if (evidence.Any(item => item.Required && item.Status == "Failed"))
        {
            return "Failed";
        }
        if (evidence.Any(item => item.Required && item.Status is "Blocked" or "MissingEvidence"))
        {
            return "Blocked";
        }
        return "Passed";
    }

    private static string MatrixStatus(IReadOnlyList<AlphaEvidenceItem> matrix)
    {
        if (matrix.Any(item => item.Required && item.Status == "Failed"))
        {
            return "Failed";
        }
        if (matrix.Any(item => item.Required && item.Status == "Blocked"))
        {
            return "Blocked";
        }
        if (matrix.Any(item => item.Required && item.Status == "MissingEvidence"))
        {
            return "MissingEvidence";
        }
        return matrix.Any(item => item.Status is "Deferred" or "NotApplicable")
            ? "PassedWithDeferredEvidence"
            : "Passed";
    }

    private static IEnumerable<Diagnostic> EvidenceDiagnostics(
        IReadOnlyList<AlphaEvidenceItem> evidence,
        string prefix)
    {
        foreach (var item in evidence.Where(item => item.Required && item.Status == "MissingEvidence"))
        {
            yield return Error($"{prefix}.MissingEvidence", "Required Small-Team Alpha evidence is missing.", item.Path);
        }
        foreach (var item in evidence.Where(item => item.Required && item.Status == "Failed"))
        {
            yield return Error($"{prefix}.FailedEvidence", "Required Small-Team Alpha evidence failed.", item.Path);
        }
        foreach (var item in evidence.Where(item => item.Required && item.Status == "Blocked"))
        {
            yield return Error($"{prefix}.BlockedEvidence", "Required Small-Team Alpha evidence is blocked.", item.Path);
        }
        foreach (var item in evidence.Where(item => item.Status == "Deferred"))
        {
            yield return Warn($"{prefix}.DeferredEvidence", item.Message, item.Path);
        }
    }

    private static IReadOnlyList<AlphaEvidenceItem> SmallTeamAlphaEvidence(string evidenceRoot)
    {
        return AlphaEvidenceDefinitions()
            .Select(definition =>
            {
                var status = definition.ExpectedStatus;
                var confidence = definition.Confidence;
                var message = definition.Message;
                if (definition.Classification != "Deferred")
                {
                    var evidenceStatus = ReportEvidenceStatus(Path.Combine(evidenceRoot, definition.Path));
                    status = evidenceStatus.Status;
                    confidence = evidenceStatus.Status == "MissingEvidence"
                        ? "MissingEvidence"
                        : definition.Confidence;
                    message = evidenceStatus.Message;
                }

                return new AlphaEvidenceItem
                {
                    Id = definition.Id,
                    Block = definition.Block,
                    PhaseRange = definition.PhaseRange,
                    Category = definition.Category,
                    Title = definition.Title,
                    Path = definition.Path,
                    Required = definition.Required,
                    Status = status,
                    Confidence = confidence,
                    Classification = definition.Classification,
                    Message = message,
                };
            })
            .OrderBy(item => item.PhaseRange, StringComparer.Ordinal)
            .ThenBy(item => item.Id, StringComparer.Ordinal)
            .ToArray();
    }

    private static IReadOnlyList<AlphaEvidenceDefinition> AlphaEvidenceDefinitions()
    {
        return
        [
            Def("block-a-opening", "Block A", "Phase 66-68", "ProjectTruth", "Small-Team Alpha opening checkpoint", "small_team_alpha_opening_report.json", true, "LocalFocusedEvidence"),
            Def("project-validation", "Block A", "Phase 66-68", "ProjectTruth", "MultiplayerSandbox project validation", "project_validation_report.json", true, "LocalFocusedEvidence"),
            Def("acceptance-plan", "Block A", "Phase 66-68", "ProjectTruth", "Phase 67 acceptance plan", "alpha_acceptance_plan_report.json", true, "ReportOnlyEvidence"),
            Def("script-projection", "Block B", "Phase 69-75", "ScriptAuthoring", "C# projection report", "projection_report.json", true, "LocalFocusedEvidence"),
            Def("script-patch-diff", "Block B", "Phase 69-75", "ScriptAuthoring", "Patch diff report", "patch_diff_report.json", true, "LocalFocusedEvidence"),
            Def("script-patch-review", "Block B", "Phase 69-75", "ScriptAuthoring", "Patch review report", "patch_review_report.json", true, "LocalFocusedEvidence"),
            Def("script-artifact-validation", "Block B", "Phase 69-75", "ScriptAuthoring", "Script artifact validation", "script_artifact_validation_report.json", true, "LocalFocusedEvidence"),
            Def("editor-workflow-model", "Block C", "Phase 76-81", "EditorWorkflowModel", "Backend-neutral editor workflow model evidence", "editor_workflow_model_report.json", true, "LocalFocusedEvidence"),
            Def("asset-workflow", "Block D", "Phase 82-86", "AssetWorkflowEvidence", "Asset workflow validation", "asset_workflow_report.json", true, "ReportOnlyEvidence"),
            Def("package-profile-matrix", "Block D", "Phase 82-86", "PackageProfileEvidence", "Package profile matrix", "package_profile_matrix_report.json", true, "ReportOnlyEvidence"),
            Def("launch-profile-matrix", "Block D", "Phase 82-86", "LaunchProfileEvidence", "Launch profile matrix", "launch_profile_matrix_report.json", true, "ReportOnlyEvidence"),
            Def("launch-preview", "Block D", "Phase 82-86", "LaunchProfileEvidence", "Server/headless launch preview", "launch_preview_report.json", true, "LocalFocusedEvidence"),
            Def("publish-check", "Block D", "Phase 82-86", "PackageProfileEvidence", "Package publish-check report", "publish_report.json", true, "LocalFocusedEvidence"),
            Def("diagnostics-summary", "Block D", "Phase 82-86", "ProjectTruth", "Diagnostics summary", "diagnostics_summary.json", true, "LocalFocusedEvidence"),
            Def("performance-budget", "Block D", "Phase 82-86", "PerformanceBudgetEvidence", "Performance budget report", "performance_budget_report.json", true, "ReportOnlyEvidence"),
            Def("collaboration-workspace", "Block E", "Phase 87-92", "CollaborationReviewEvidence", "Workspace session report", "workspace_session_report.json", true, "LocalFocusedEvidence"),
            Def("collaboration-lock", "Block E", "Phase 87-92", "CollaborationReviewEvidence", "Persistent lock report", "workspace_lock_report.json", true, "LocalFocusedEvidence"),
            Def("collaboration-comments", "Block E", "Phase 87-92", "CollaborationReviewEvidence", "Artifact comments report", "artifact_comments_report.json", true, "LocalFocusedEvidence"),
            Def("collaboration-review", "Block E", "Phase 87-92", "CollaborationReviewEvidence", "Review request report", "review_report.json", true, "LocalFocusedEvidence"),
            Def("team-room-status", "Block E", "Phase 87-92", "CollaborationReviewEvidence", "Team Room status report", "team_room_status_report.json", true, "LocalFocusedEvidence"),
            Def("docguard", "Block F", "Phase 93-95", "ClaimHonesty", "DocGuard honesty scan", "docguard_report.json", true, "LocalFocusedEvidence"),
            Def("deferred-gameplay-expansion", "Block D", "Phase 86", "DeferredGameplayExpansion", "MultiplayerSandbox gameplay expansion", "gameplay_expansion_blocker_report.json", false, "DeferredWithEvidence", "Deferred", "Deferred", "Runtime and Server gameplay expansion remains deferred."),
            Def("deferred-client-preview", "Block D", "Phase 84", "DeferredClientPreview", "One-client/two-client preview", "client_preview_blocker_report.json", false, "DeferredWithEvidence", "Deferred", "Deferred", "Client preview remains blocked without a bounded ClientHost/runtime seam."),
            Def("deferred-editor-ui", "Block C", "Phase 76-81", "DeferredEditorUI", "Editor UI and Qt implementation", "editor_ui_blocker_report.json", false, "DeferredWithEvidence", "Deferred", "Deferred", "Editor UI and Qt implementation remain outside backend-neutral workflow evidence."),
            Def("unverified-broad-health", "Block F", "Phase 93-95", "ClaimHonesty", "Raw full CTest and heavy stress", "raw_full_ctest_report.json", false, "UnverifiedBroadHealth", "NotApplicable", "NotApplicable", "Raw full CTest, heavy stress, long soak, and real transport stress are not claimed."),
        ];
    }

    private static (string Status, string Message) ReportEvidenceStatus(string path)
    {
        if (!File.Exists(path))
        {
            return ("MissingEvidence", "Evidence report is missing.");
        }

        try
        {
            var json = JsonNode.Parse(File.ReadAllText(path)) as JsonObject;
            var raw = json?["status"]?.GetValue<string>() ?? "Passed";
            return raw switch
            {
                "Passed" or "Accepted" or "PartiallyProven" or "PassedWithDeferredEvidence" => ("Passed", $"Evidence report status is {raw}."),
                "PassedWithMissingMeasurements" => ("Passed", "Evidence report passed with explicit missing measurements."),
                _ when raw.StartsWith("PassedWith", StringComparison.Ordinal) => ("Passed", $"Evidence report status is {raw}."),
                "Deferred" => ("Deferred", "Evidence report is explicitly deferred."),
                "MissingSourceOfTruth" => ("Deferred", "Evidence report is explicit missing source-of-truth debt."),
                "Blocked" => ("Blocked", $"Evidence report status is {raw}."),
                "Failed" or "RejectedOrBlocked" => ("Failed", $"Evidence report status is {raw}."),
                _ => (raw, $"Evidence report status is {raw}."),
            };
        }
        catch (Exception e) when (e is IOException or InvalidOperationException or System.Text.Json.JsonException)
        {
            return ("Failed", $"Unable to read evidence report: {e.Message}");
        }
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

    private static Diagnostic Error(string code, string message, string path = "") =>
        new() { Severity = "Error", Code = code, Message = message, Path = path };

    private static Diagnostic Warn(string code, string message, string path = "") =>
        new() { Severity = "Warning", Code = code, Message = message, Path = path };

    private static Diagnostic Info(string code, string message, string path = "") =>
        new() { Severity = "Info", Code = code, Message = message, Path = path };

    private static GameplayExpansionBlocker Blocker(
        string id,
        string scope,
        string requiredSeam,
        string reason) =>
        new()
        {
            Id = id,
            Scope = scope,
            RequiredSeam = requiredSeam,
            Reason = reason,
        };

    private static void PrintUsage()
    {
        Console.Error.WriteLine("SagaAlphaGate CLI");
        Console.Error.WriteLine("  sagaalphagate opening-check --technical-preview-root <dir> --out <report>");
        Console.Error.WriteLine("  sagaalphagate acceptance-plan --out <report>");
        Console.Error.WriteLine("  sagaalphagate budget-report --out <report> [--evidence-root <dir>]");
        Console.Error.WriteLine("  sagaalphagate script-evidence --script-validation <report> --out <report>");
        Console.Error.WriteLine("  sagaalphagate editor-workflow-evidence --out <report>");
        Console.Error.WriteLine("  sagaalphagate collaboration-evidence --out-root <dir>");
        Console.Error.WriteLine("  sagaalphagate gameplay-expansion-blockers --out <report>");
        Console.Error.WriteLine("  sagaalphagate accept-alpha --evidence-root <dir> --out <report>");
        Console.Error.WriteLine("  sagaalphagate evidence-matrix --evidence-root <dir> --out <report>");
        Console.Error.WriteLine("  sagaalphagate close-alpha --evidence-root <dir> --out <report>");
    }

    private sealed record CommandOptions(
        bool Ok,
        string Error,
        string TechnicalPreviewRoot,
        string EvidenceRoot,
        string ScriptValidationPath,
        string OutPath,
        string OutRoot)
    {
        public static CommandOptions Parse(string[] args)
        {
            string technicalPreviewRoot = "";
            string evidenceRoot = "";
            string scriptValidation = "";
            string output = "";
            string outRoot = "";

            for (var i = 0; i < args.Length; ++i)
            {
                switch (args[i])
                {
                    case "--technical-preview-root" when i + 1 < args.Length:
                        technicalPreviewRoot = args[++i];
                        break;
                    case "--evidence-root" when i + 1 < args.Length:
                        evidenceRoot = args[++i];
                        break;
                    case "--script-validation" when i + 1 < args.Length:
                        scriptValidation = args[++i];
                        break;
                    case "--out" when i + 1 < args.Length:
                        output = args[++i];
                        break;
                    case "--out-root" when i + 1 < args.Length:
                        outRoot = args[++i];
                        break;
                    default:
                        return Fail($"Unknown or incomplete argument: {args[i]}");
                }
            }

            return new CommandOptions(true, "", technicalPreviewRoot, evidenceRoot, scriptValidation, output, outRoot);
        }

        private static CommandOptions Fail(string error) => new(false, error, "", "", "", "", "");
    }

    private sealed record AlphaEvidenceDefinition(
        string Id,
        string Block,
        string PhaseRange,
        string Category,
        string Title,
        string Path,
        bool Required,
        string Confidence,
        string Classification,
        string ExpectedStatus,
        string Message);

    private static AlphaEvidenceDefinition Def(
        string id,
        string block,
        string phaseRange,
        string category,
        string title,
        string path,
        bool required,
        string confidence,
        string classification = "Evidence",
        string expectedStatus = "Passed",
        string message = "") =>
        new(id, block, phaseRange, category, title, path, required, confidence, classification, expectedStatus, message);
}
