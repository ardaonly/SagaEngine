using System.Text.Json;
using System.Text.Json.Nodes;

namespace SagaProjectKit;

internal static class Phase141Reports
{
    private static readonly HashSet<string> KnownComponentTypes = new(StringComparer.Ordinal)
    {
        "BehaviorReference",
        "Transform",
    };

    private static readonly List<string> EditorProhibitedActions =
    [
        "SceneEditing",
        "EntityPlacement",
        "EditorUiMutation",
        "QtUiMutation",
        "CSharpSourceMutation",
    ];

    public static ComponentMetadataOwnershipReport BuildComponentMetadataOwnership(
        ManifestLoadResult load,
        string sceneValidationPath,
        string freshnessPath)
    {
        var diagnostics = new List<ProjectDiagnostic>(load.Diagnostics);
        _ = LoadJsonObject(sceneValidationPath, diagnostics, "Project.ComponentOwnership.SceneValidationMalformed");
        var freshness = LoadJsonObject(freshnessPath, diagnostics, "Project.ComponentOwnership.FreshnessMalformed");
        var components = new List<ComponentMetadataOwnershipItem>();
        var generated = new List<GeneratedEvidenceOwnershipItem>();

        foreach (var scene in SceneFiles(load))
        {
            var root = LoadJsonElement(scene.AbsolutePath, diagnostics, "Project.ComponentOwnership.SceneMalformed");
            if (!root.HasValue || root.Value.ValueKind != JsonValueKind.Object)
            {
                continue;
            }

            var sceneId = FirstString(root.Value, "sceneId", scene.Id);
            foreach (var entity in EnumerateObjectArray(root.Value, "entities"))
            {
                var entityId = FirstString(entity, "entityId", "");
                foreach (var component in EnumerateObjectArray(entity, "components"))
                {
                    var componentType = FirstString(component, "type", "");
                    var status = KnownComponentTypes.Contains(componentType) ? "Accepted" : "UnknownComponentType";
                    if (status != "Accepted")
                    {
                        diagnostics.Add(Warning(
                            "Project.ComponentOwnership.UnknownComponentType",
                            "ComponentOwnership",
                            scene.RelativePath,
                            $"Unknown component metadata type: {componentType}."));
                    }

                    components.Add(new ComponentMetadataOwnershipItem
                    {
                        SceneId = sceneId,
                        EntityId = entityId,
                        ComponentType = componentType,
                        RelativePath = scene.RelativePath,
                        Classification = "SceneEntitySourceTruth",
                        Status = status,
                    });
                }
            }

            foreach (var artifact in EnumerateObjectArray(root.Value, "generatedArtifacts"))
            {
                var canonical = ReadBool(artifact, "canonical");
                if (canonical)
                {
                    diagnostics.Add(Error(
                        "Project.ComponentOwnership.GeneratedCanonicalRejected",
                        "ComponentOwnership",
                        scene.RelativePath,
                        "Generated runtime/script bindings and projections cannot own canonical component metadata."));
                }

                generated.Add(new GeneratedEvidenceOwnershipItem
                {
                    SceneId = sceneId,
                    ArtifactId = FirstString(artifact, "artifactId", ""),
                    Path = FirstString(artifact, "path", ""),
                    ArtifactKind = FirstString(artifact, "artifactKind", "GeneratedProjection"),
                    Classification = "EvidenceOnly",
                    Canonical = canonical,
                    Status = canonical ? "RejectedCanonicalClaim" : "EvidenceOnly",
                });
            }
        }

        var freshnessDebt = ReadFreshness(freshness)
            .Where(item => item.Status is not "Fresh")
            .ToList();
        foreach (var debt in freshnessDebt)
        {
            diagnostics.Add(Warning(
                "Project.ComponentOwnership.StaleGeneratedEvidence",
                "ComponentOwnership",
                debt.ArtifactId,
                $"Generated component-facing evidence is not fresh: {debt.Status}."));
        }

        var checks = new List<SourceTruthGateCheck>
        {
            Check("KnownComponentTypesAccepted", components.Count > 0 && components.All(item => item.Status == "Accepted") ? "Passed" : "Failed", "Transform and BehaviorReference metadata are accepted from scene/entity source truth."),
            Check("GeneratedEvidenceOnly", generated.All(item => !item.Canonical && item.Classification == "EvidenceOnly") ? "Passed" : "Failed", "Generated bindings and projections are evidence only."),
            Check("ReportOnlyInvariants", "Passed", "Report-only invariants are preserved."),
        };

        return new ComponentMetadataOwnershipReport
        {
            Command = "component-metadata-ownership",
            Status = diagnostics.Any(item => item.Severity == "Error") || components.Any(item => item.Status != "Accepted")
                ? "Failed"
                : freshnessDebt.Count > 0 || diagnostics.Any(item => item.Severity == "Warning") ? "PartiallyProven" : "Passed",
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(diagnostics),
            Components = components.OrderBy(item => item.SceneId, StringComparer.Ordinal).ThenBy(item => item.EntityId, StringComparer.Ordinal).ThenBy(item => item.ComponentType, StringComparer.Ordinal).ToList(),
            GeneratedEvidence = generated.OrderBy(item => item.SceneId, StringComparer.Ordinal).ThenBy(item => item.ArtifactId, StringComparer.Ordinal).ToList(),
            FreshnessDebt = freshnessDebt.OrderBy(item => item.ArtifactId, StringComparer.Ordinal).ToList(),
            Checks = checks.OrderBy(item => item.CheckId, StringComparer.Ordinal).ToList(),
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    public static EditorInspectionModelReport BuildEditorInspectionModel(
        ManifestLoadResult load,
        string sceneValidationPath,
        string freshnessPath)
    {
        var diagnostics = new List<ProjectDiagnostic>(load.Diagnostics);
        _ = LoadJsonObject(sceneValidationPath, diagnostics, "Project.EditorInspection.SceneValidationMalformed");
        var freshness = LoadJsonObject(freshnessPath, diagnostics, "Project.EditorInspection.FreshnessMalformed");
        var scenes = new List<EditorInspectionSceneItem>();
        var entities = new List<EditorInspectionEntityItem>();
        var components = new List<ComponentMetadataOwnershipItem>();
        var assetReferences = new List<AssetReferenceTruthItem>();

        foreach (var scene in SceneFiles(load))
        {
            var root = LoadJsonElement(scene.AbsolutePath, diagnostics, "Project.EditorInspection.SceneMalformed");
            if (!root.HasValue || root.Value.ValueKind != JsonValueKind.Object)
            {
                continue;
            }

            var sceneId = FirstString(root.Value, "sceneId", scene.Id);
            var sceneSourceKind = FirstString(root.Value, "sourceKind", "");
            scenes.Add(new EditorInspectionSceneItem
            {
                SceneId = sceneId,
                RelativePath = scene.RelativePath,
                SourceKind = sceneSourceKind,
                Status = sceneSourceKind == "SceneSourceTruth" ? "Readable" : "InvalidSourceKind",
            });

            foreach (var entity in EnumerateObjectArray(root.Value, "entities"))
            {
                var entityId = FirstString(entity, "entityId", "");
                var sourceKind = FirstString(entity, "sourceKind", "");
                entities.Add(new EditorInspectionEntityItem
                {
                    SceneId = sceneId,
                    EntityId = entityId,
                    RelativePath = scene.RelativePath,
                    SourceKind = sourceKind,
                    Status = sourceKind == "EntitySourceTruth" ? "Readable" : "InvalidSourceKind",
                });
                foreach (var component in EnumerateObjectArray(entity, "components"))
                {
                    components.Add(new ComponentMetadataOwnershipItem
                    {
                        SceneId = sceneId,
                        EntityId = entityId,
                        ComponentType = FirstString(component, "type", ""),
                        RelativePath = scene.RelativePath,
                        Classification = "SceneEntitySourceTruth",
                        Status = "Readable",
                    });
                }
                foreach (var reference in EnumerateObjectArray(entity, "assetReferences"))
                {
                    assetReferences.Add(new AssetReferenceTruthItem
                    {
                        SceneId = sceneId,
                        EntityId = entityId,
                        AssetId = FirstString(reference, "assetId", ""),
                        Owner = FirstString(reference, "owner", ""),
                        Path = FirstString(reference, "path", ""),
                        Classification = "SceneEntityAssetReference",
                        Status = "Readable",
                    });
                }
            }
        }

        return new EditorInspectionModelReport
        {
            Command = "editor-inspection-model",
            Status = diagnostics.Any(item => item.Severity == "Error") ? "Failed" :
                ReadFreshness(freshness).Any(item => item.Status is not "Fresh") ? "PartiallyProven" : "Passed",
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(diagnostics),
            EditorMayMutate = false,
            ProhibitedActions = EditorProhibitedActions,
            Scenes = scenes.OrderBy(item => item.SceneId, StringComparer.Ordinal).ToList(),
            Entities = entities.OrderBy(item => item.SceneId, StringComparer.Ordinal).ThenBy(item => item.EntityId, StringComparer.Ordinal).ToList(),
            Components = components.OrderBy(item => item.SceneId, StringComparer.Ordinal).ThenBy(item => item.EntityId, StringComparer.Ordinal).ThenBy(item => item.ComponentType, StringComparer.Ordinal).ToList(),
            AssetReferences = assetReferences.OrderBy(item => item.SceneId, StringComparer.Ordinal).ThenBy(item => item.EntityId, StringComparer.Ordinal).ThenBy(item => item.AssetId, StringComparer.Ordinal).ToList(),
            FreshnessStatus = ReadFreshness(freshness),
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    public static EditorSourceTruthReadReport BuildEditorSourceTruthRead(ManifestLoadResult load, string inspectionModelPath)
    {
        var diagnostics = new List<ProjectDiagnostic>(load.Diagnostics);
        var inspection = LoadJsonObject(inspectionModelPath, diagnostics, "Project.EditorRead.InspectionModelMalformed");
        var readSources = new List<EditorSourceTruthReadItem>();
        var rejectedGenerated = new List<GeneratedEvidenceOwnershipItem>();

        foreach (var scene in ReadInspectionScenes(inspection))
        {
            var fullPath = Path.GetFullPath(Path.Combine(load.ProjectRoot, scene.RelativePath));
            var exists = File.Exists(fullPath);
            if (!exists)
            {
                diagnostics.Add(Error(
                    "Project.EditorRead.SceneSourceMissing",
                    "EditorRead",
                    scene.RelativePath,
                    "Editor source truth read requires the declared scene source file."));
                readSources.Add(new EditorSourceTruthReadItem
                {
                    SceneId = scene.SceneId,
                    RelativePath = scene.RelativePath,
                    Exists = false,
                    Status = "Missing",
                });
                continue;
            }

            readSources.Add(new EditorSourceTruthReadItem
            {
                SceneId = scene.SceneId,
                RelativePath = scene.RelativePath,
                Exists = true,
                Status = "ReadOnlyAccepted",
            });
            var root = LoadJsonElement(fullPath, diagnostics, "Project.EditorRead.SceneMalformed");
            if (root.HasValue)
            {
                foreach (var artifact in EnumerateObjectArray(root.Value, "generatedArtifacts").Where(item => ReadBool(item, "canonical")))
                {
                    rejectedGenerated.Add(new GeneratedEvidenceOwnershipItem
                    {
                        SceneId = scene.SceneId,
                        ArtifactId = FirstString(artifact, "artifactId", ""),
                        Path = FirstString(artifact, "path", ""),
                        ArtifactKind = FirstString(artifact, "artifactKind", "GeneratedProjection"),
                        Classification = "EvidenceOnly",
                        Canonical = true,
                        Status = "RejectedCanonicalClaim",
                    });
                    diagnostics.Add(Error(
                        "Project.EditorRead.GeneratedCanonicalRejected",
                        "EditorRead",
                        scene.RelativePath,
                        "Generated artifacts cannot claim canonical editor source truth."));
                }
            }
        }

        if (inspection is null)
        {
            diagnostics.Add(Error("Project.EditorRead.InspectionModelMissing", "EditorRead", inspectionModelPath, "Editor source truth read requires an inspection model report."));
        }

        return new EditorSourceTruthReadReport
        {
            Command = "editor-source-truth-read",
            Status = diagnostics.Any(item => item.Severity == "Error") ? "Failed" : "Passed",
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(diagnostics),
            EditorMayMutate = false,
            ReadSources = readSources.OrderBy(item => item.SceneId, StringComparer.Ordinal).ThenBy(item => item.RelativePath, StringComparer.Ordinal).ToList(),
            RejectedGeneratedCanonicalClaims = rejectedGenerated.OrderBy(item => item.SceneId, StringComparer.Ordinal).ThenBy(item => item.ArtifactId, StringComparer.Ordinal).ToList(),
            ProhibitedActions = EditorProhibitedActions,
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    public static RuntimeReadModelAuditReport BuildRuntimeReadModelAudit(
        ManifestLoadResult load,
        string sceneValidationPath,
        string freshnessPath)
    {
        var diagnostics = new List<ProjectDiagnostic>(load.Diagnostics);
        _ = LoadJsonObject(sceneValidationPath, diagnostics, "Project.RuntimeReadModel.SceneValidationMalformed");
        var freshness = LoadJsonObject(freshnessPath, diagnostics, "Project.RuntimeReadModel.FreshnessMalformed");
        var accepted = new List<RuntimeReadInputItem>();
        var generated = new List<RuntimeReadInputItem>();
        var forbidden = new List<RuntimeForbiddenSourceTruthItem>();

        foreach (var scene in SceneFiles(load))
        {
            accepted.Add(new RuntimeReadInputItem
            {
                InputId = scene.Id,
                Kind = "SceneEntitySourceTruth",
                Path = scene.RelativePath,
                Classification = "FutureReadableSourceTruth",
                Status = "AcceptedForPlanning",
            });

            var root = LoadJsonElement(scene.AbsolutePath, diagnostics, "Project.RuntimeReadModel.SceneMalformed");
            if (!root.HasValue)
            {
                continue;
            }
            foreach (var artifact in EnumerateObjectArray(root.Value, "generatedArtifacts"))
            {
                var id = FirstString(artifact, "artifactId", "");
                var path = FirstString(artifact, "path", "");
                generated.Add(new RuntimeReadInputItem
                {
                    InputId = id,
                    Kind = FirstString(artifact, "artifactKind", "GeneratedProjection"),
                    Path = path,
                    Classification = "NonCanonicalRuntimeFacingProjection",
                    Status = "EvidenceOnly",
                });
                if (ReadBool(artifact, "canonical"))
                {
                    forbidden.Add(new RuntimeForbiddenSourceTruthItem
                    {
                        InputId = id,
                        Kind = "GeneratedProjection",
                        Path = path,
                        Reason = "Generated projections cannot be runtime source truth.",
                    });
                }
            }
        }

        foreach (var item in ReadFreshness(freshness).Where(item => item.Status is not "Fresh"))
        {
            forbidden.Add(new RuntimeForbiddenSourceTruthItem
            {
                InputId = item.ArtifactId,
                Kind = "StaleGeneratedEvidence",
                Path = item.ArtifactId,
                Reason = "Stale generated files are forbidden runtime source truth.",
            });
        }

        forbidden.Add(new RuntimeForbiddenSourceTruthItem { InputId = "editor-diagnostics", Kind = "EditorOnlyDiagnostics", Path = sceneValidationPath, Reason = "Editor-only diagnostics are not runtime source truth." });
        forbidden.Add(new RuntimeForbiddenSourceTruthItem { InputId = "package-summary", Kind = "PackageOnlySummary", Path = "", Reason = "Package-only summaries are not runtime source truth." });

        var checks = new List<SourceTruthGateCheck>
        {
            Check("RuntimeInputsFutureReadableOnly", accepted.Count > 0 ? "Passed" : "Failed", "Runtime audit records future-readable source inputs only."),
            Check("GeneratedProjectionNonCanonical", generated.All(item => item.Classification == "NonCanonicalRuntimeFacingProjection") ? "Passed" : "Failed", "Generated projections are non-canonical runtime-facing evidence."),
            Check("ForbiddenRuntimeSourceTruthRecorded", forbidden.Count > 0 ? "Passed" : "Failed", "Forbidden runtime source truth classes are explicit."),
            Check("ReportOnlyInvariants", "Passed", "Report-only invariants are preserved."),
        };

        return new RuntimeReadModelAuditReport
        {
            Command = "runtime-read-model-audit",
            Status = diagnostics.Any(item => item.Severity == "Error") ? "Failed" :
                forbidden.Any(item => item.Kind == "StaleGeneratedEvidence") ? "PartiallyProven" : "Passed",
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(diagnostics),
            AcceptedInputs = accepted.OrderBy(item => item.InputId, StringComparer.Ordinal).ToList(),
            NonCanonicalGeneratedProjections = generated.OrderBy(item => item.InputId, StringComparer.Ordinal).ToList(),
            ForbiddenRuntimeSourceTruth = forbidden.OrderBy(item => item.Kind, StringComparer.Ordinal).ThenBy(item => item.InputId, StringComparer.Ordinal).ToList(),
            FreshnessStatus = ReadFreshness(freshness),
            Checks = checks.OrderBy(item => item.CheckId, StringComparer.Ordinal).ToList(),
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    public static RuntimeReadinessGateReport BuildRuntimeReadinessGate(
        ManifestLoadResult load,
        string sceneValidationPath,
        string freshnessPath,
        string runtimeReadModelPath)
    {
        var diagnostics = new List<ProjectDiagnostic>(load.Diagnostics);
        var sceneValidation = LoadJsonObject(sceneValidationPath, diagnostics, "Project.RuntimeReadiness.SceneValidationMalformed");
        var freshness = LoadJsonObject(freshnessPath, diagnostics, "Project.RuntimeReadiness.FreshnessMalformed");
        var runtime = LoadJsonObject(runtimeReadModelPath, diagnostics, "Project.RuntimeReadiness.RuntimeReadModelMalformed");
        var missing = sceneValidation is null || freshness is null || runtime is null;
        var failed = new[] { sceneValidation, freshness, runtime }.Any(report => ReadString(report, "status") == "Failed");
        var partial = new[] { sceneValidation, freshness, runtime }.Any(report => ReadString(report, "status") == "PartiallyProven") ||
            ReadFreshness(freshness).Any(item => item.Status is not "Fresh");

        var readiness = missing ? "MissingEvidence" : failed ? "Blocked" : partial ? "PartiallyProven" : "ReadyForPlanning";
        var checks = new List<SourceTruthGateCheck>
        {
            Check("SceneValidationAccepted", sceneValidation is null ? "MissingEvidence" : ReadString(sceneValidation, "status"), "Scene/entity validation evidence is present."),
            Check("FreshnessAccepted", freshness is null ? "MissingEvidence" : ReadString(freshness, "status"), "Generated freshness evidence is present."),
            Check("RuntimeReadModelAccepted", runtime is null ? "MissingEvidence" : ReadString(runtime, "status"), "Runtime read model audit evidence is present."),
            Check("PlanningOnlyRuntimeNotImplemented", "Passed", "ReadyForPlanning means planning allowed only; Runtime not implemented."),
            Check("ReportOnlyInvariants", "Passed", "Report-only invariants are preserved."),
        };

        return new RuntimeReadinessGateReport
        {
            Command = "runtime-readiness-gate",
            Status = readiness is "Blocked" or "MissingEvidence" ? "Failed" : readiness,
            ReadinessStatus = readiness,
            PlanningScope = "Planning allowed only; Runtime not implemented.",
            RuntimeImplementationClaim = "Runtime not implemented.",
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(diagnostics),
            Checks = checks.OrderBy(item => item.CheckId, StringComparer.Ordinal).ToList(),
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    public static SourceTruthScenarioReport BuildSourceTruthScenario(ManifestLoadResult load, string evidenceRoot)
    {
        var diagnostics = new List<ProjectDiagnostic>(load.Diagnostics);
        var fullRoot = Path.GetFullPath(Path.Combine(load.ProjectRoot, evidenceRoot));
        if (Path.IsPathRooted(evidenceRoot))
        {
            fullRoot = Path.GetFullPath(evidenceRoot);
        }

        var required = new (string Id, string Command, string File)[]
        {
            ("scene-validation", "scene-entity-validate", "scene_entity_validation_report.json"),
            ("asset-reference-gate", "asset-reference-gate", "asset_reference_gate_report.json"),
            ("freshness", "generated-freshness-gate", "generated_artifact_freshness_report.json"),
            ("component-ownership", "component-metadata-ownership", "component_metadata_ownership_report.json"),
            ("editor-inspection", "editor-inspection-model", "editor_inspection_model_report.json"),
            ("editor-read", "editor-source-truth-read", "editor_source_truth_read_report.json"),
            ("runtime-audit", "runtime-read-model-audit", "runtime_read_model_audit_report.json"),
            ("runtime-readiness", "runtime-readiness-gate", "runtime_readiness_gate_report.json"),
            ("package-alignment", "source-truth-alignment", "package_source_truth_alignment_report.json"),
            ("launch-alignment", "source-truth-alignment", "launch_source_truth_alignment_report.json"),
        };
        var evidence = new List<ScenarioEvidenceItem>();
        foreach (var item in required)
        {
            var path = Path.Combine(fullRoot, item.File);
            var report = LoadJsonObject(path, diagnostics, $"Project.SourceTruthScenario.{item.Id}.Malformed");
            var status = report is null ? "Missing" : ReadString(report, "status");
            evidence.Add(new ScenarioEvidenceItem
            {
                EvidenceId = item.Id,
                Command = item.Command,
                Path = NormalizeRelative(Path.GetRelativePath(load.ProjectRoot, path)),
                Status = string.IsNullOrWhiteSpace(status) ? "Missing" : status,
            });
        }

        var missingRequired = evidence.Where(item => item.Status == "Missing").ToList();
        foreach (var item in missingRequired)
        {
            diagnostics.Add(Error(
                "Project.SourceTruthScenario.RequiredEvidenceMissing",
                "SourceTruthScenario",
                item.Path,
                $"Required source-truth scenario evidence is missing: {item.EvidenceId}."));
        }

        var hasBlocking = evidence.Any(item =>
            (item.EvidenceId is "scene-validation" or "asset-reference-gate" or "component-ownership") &&
            (item.Status is "Failed" or "Missing"));
        var explicitDeferrals = new List<string>();
        if (evidence.Any(item => item.EvidenceId == "freshness" && item.Status == "PartiallyProven"))
        {
            explicitDeferrals.Add("Stale generated evidence is explicit.");
        }
        if (evidence.Any(item => item.EvidenceId == "launch-alignment" && item.Status == "PartiallyProven"))
        {
            explicitDeferrals.Add("Client Preview deferral is explicit.");
        }

        var outcome = hasBlocking ? "Blocked" :
            missingRequired.Count > 0 ? "MissingEvidence" :
            explicitDeferrals.Count > 0 || evidence.Any(item => item.Status == "PartiallyProven") ? "PartiallyProven" :
            "ReadyForPlanning";

        return new SourceTruthScenarioReport
        {
            Command = "source-truth-scenario",
            Status = outcome is "Blocked" or "MissingEvidence" ? "Failed" : outcome,
            Outcome = outcome,
            EvidenceRoot = NormalizeRelative(evidenceRoot),
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(diagnostics),
            RequiredEvidence = evidence.OrderBy(item => item.EvidenceId, StringComparer.Ordinal).ToList(),
            ExplicitDeferrals = explicitDeferrals.OrderBy(item => item, StringComparer.Ordinal).ToList(),
            NonClaims =
            [
                "No Runtime gameplay implementation is claimed.",
                "No Server gameplay implementation is claimed.",
                "No Client Preview implementation is claimed.",
                "No package staging or launch execution is performed by this scenario report.",
            ],
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    private static List<EditorSourceTruthReadItem> ReadInspectionScenes(JsonObject? root)
    {
        if (root?["scenes"] is not JsonArray scenes)
        {
            return [];
        }

        return scenes.OfType<JsonObject>()
            .Select(scene => new EditorSourceTruthReadItem
            {
                SceneId = ReadString(scene, "sceneId"),
                RelativePath = ReadString(scene, "relativePath"),
            })
            .ToList();
    }

    private static List<GeneratedFreshnessStatusItem> ReadFreshness(JsonObject? root)
    {
        if (root?["freshnessStatus"] is not JsonArray array)
        {
            return [];
        }

        return array.OfType<JsonObject>()
            .Select(item => new GeneratedFreshnessStatusItem
            {
                ArtifactId = ReadString(item, "artifactId"),
                Status = ReadString(item, "status"),
            })
            .OrderBy(item => item.ArtifactId, StringComparer.Ordinal)
            .ToList();
    }

    private static List<ResolvedProjectPath> SceneFiles(ManifestLoadResult load) =>
        load.ResolvedPaths
            .Where(path => path.Category == "scene" && path.Exists)
            .OrderBy(path => path.Id, StringComparer.Ordinal)
            .ThenBy(path => path.RelativePath, StringComparer.Ordinal)
            .ToList();

    private static IEnumerable<JsonElement> EnumerateObjectArray(JsonElement root, string key) =>
        root.ValueKind == JsonValueKind.Object &&
        root.TryGetProperty(key, out var array) &&
        array.ValueKind == JsonValueKind.Array
            ? array.EnumerateArray().Where(item => item.ValueKind == JsonValueKind.Object)
            : [];

    private static JsonElement? LoadJsonElement(string path, List<ProjectDiagnostic> diagnostics, string code)
    {
        try
        {
            using var input = File.OpenRead(path);
            using var document = JsonDocument.Parse(input);
            return document.RootElement.Clone();
        }
        catch (Exception e) when (e is JsonException or IOException or UnauthorizedAccessException)
        {
            diagnostics.Add(Error(code, CategoryForCode(code), path, $"JSON could not be read: {e.Message}"));
            return null;
        }
    }

    private static JsonObject? LoadJsonObject(string path, List<ProjectDiagnostic> diagnostics, string code)
    {
        try
        {
            var node = JsonNode.Parse(File.ReadAllText(path)) as JsonObject;
            if (node is null)
            {
                diagnostics.Add(Error(code, CategoryForCode(code), path, "Report JSON root must be an object."));
            }
            return node;
        }
        catch (Exception e) when (e is JsonException or IOException or UnauthorizedAccessException)
        {
            diagnostics.Add(Error(code, CategoryForCode(code), path, $"Report JSON could not be read: {e.Message}"));
            return null;
        }
    }

    private static string FirstString(JsonElement root, string key, string fallback) =>
        root.ValueKind == JsonValueKind.Object &&
        root.TryGetProperty(key, out var value) &&
        value.ValueKind == JsonValueKind.String
            ? value.GetString() ?? fallback
            : fallback;

    private static bool ReadBool(JsonElement root, string key) =>
        root.ValueKind == JsonValueKind.Object &&
        root.TryGetProperty(key, out var value) &&
        value.ValueKind is JsonValueKind.True or JsonValueKind.False &&
        value.GetBoolean();

    private static string ReadString(JsonObject? obj, string key) =>
        obj is not null &&
        obj.TryGetPropertyValue(key, out var node) &&
        node is JsonValue value &&
        value.TryGetValue<string>(out var text)
            ? text
            : "";

    private static SourceTruthGateCheck Check(string checkId, string status, string summary) =>
        new() { CheckId = checkId, Status = status, Summary = summary };

    private static ProjectSummary BuildProjectSummary(ManifestLoadResult load) =>
        new()
        {
            ProjectId = load.Descriptor?.ProjectId ?? "",
            DisplayName = load.Descriptor?.DisplayName ?? "",
            ProjectRoot = load.ProjectRoot,
            ManifestPath = load.ManifestPath,
            SchemaVersion = load.Descriptor?.SchemaVersion ?? 0,
        };

    private static ProjectDiagnostic Error(string code, string category, string path, string message) =>
        new() { Severity = "Error", Code = code, Category = category, Path = NormalizeRelative(path), Message = message };

    private static ProjectDiagnostic Warning(string code, string category, string path, string message) =>
        new() { Severity = "Warning", Code = code, Category = category, Path = NormalizeRelative(path), Message = message };

    private static string CategoryForCode(string code)
    {
        if (code.Contains("Editor", StringComparison.Ordinal))
        {
            return "EditorReadModel";
        }
        if (code.Contains("Runtime", StringComparison.Ordinal))
        {
            return "RuntimeReadModel";
        }
        if (code.Contains("Scenario", StringComparison.Ordinal))
        {
            return "SourceTruthScenario";
        }
        return "ComponentOwnership";
    }

    private static List<ProjectDiagnostic> SortDiagnostics(IEnumerable<ProjectDiagnostic> diagnostics) =>
        diagnostics
            .OrderByDescending(item => item.Severity == "Error")
            .ThenByDescending(item => item.Severity == "Warning")
            .ThenBy(item => item.Code, StringComparer.Ordinal)
            .ThenBy(item => item.Path, StringComparer.Ordinal)
            .ThenBy(item => item.Message, StringComparer.Ordinal)
            .ToList();

    private static string NormalizeRelative(string path) =>
        path.Replace('\\', '/').Trim();
}
