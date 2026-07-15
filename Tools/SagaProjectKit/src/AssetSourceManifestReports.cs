using System.Text.Json;
using System.Text.Json.Nodes;

namespace SagaProjectKit;

internal static class AssetSourceManifestReports
{
    public static AssetSourceManifestInventoryReport BuildAssetSourceManifestInventory(ManifestLoadResult load)
    {
        var diagnostics = new List<ProjectDiagnostic>(load.Diagnostics);
        var assetRoots = new List<AssetRootInventoryItem>();
        var manifests = new List<AssetSourceManifestItem>();
        var owners = new List<AssetOwnerInventoryItem>();
        var assetReferences = new List<AssetReferenceTruthItem>();
        var missingOwners = new List<MissingAssetOwnerItem>();
        var generatedArtifacts = new List<GeneratedAssetArtifactItem>();

        foreach (var assetRoot in AssetRoots(load))
        {
            var sourceManifests = assetRoot.Exists ? FindAssetSourceManifestFiles(assetRoot.AbsolutePath) : [];
            var classification = assetRoot.Exists && sourceManifests.Count == 0
                ? "PlaceholderAssetRoot"
                : assetRoot.Exists ? "SourceAssetRoot" : "MissingAssetRoot";
            var rootStatus = classification == "SourceAssetRoot" ? "Passed" : classification.Replace("AssetRoot", "");

            if (classification == "PlaceholderAssetRoot")
            {
                diagnostics.Add(Warning(
                    "Project.AssetSource.PlaceholderAssetRoot",
                    "AssetSource",
                    assetRoot.RelativePath,
                    "Asset root exists but has no assets.source.json or asset_manifest.json source metadata."));
            }

            assetRoots.Add(new AssetRootInventoryItem
            {
                AssetId = assetRoot.Id,
                RelativePath = assetRoot.RelativePath,
                Kind = assetRoot.Kind,
                Exists = assetRoot.Exists,
                Classification = classification,
                Status = rootStatus,
            });

            foreach (var manifestPath in sourceManifests)
            {
                AddAssetSourceManifest(load.ProjectRoot, manifestPath, true, manifests, owners, assetReferences, missingOwners, diagnostics);
            }
        }

        foreach (var manifestPath in FindGeneratedAssetManifestFiles(load.ProjectRoot, AssetRoots(load).Select(root => root.AbsolutePath)))
        {
            AddAssetSourceManifest(load.ProjectRoot, manifestPath, false, manifests, owners, assetReferences, missingOwners, diagnostics);
        }

        foreach (var scene in SceneFiles(load))
        {
            CollectSceneAssetEvidence(load.ProjectRoot, scene, assetReferences, generatedArtifacts, missingOwners, diagnostics);
        }

        var status = ReportStatus(diagnostics, warningMeansPartial: true);
        return new AssetSourceManifestInventoryReport
        {
            Command = "asset-source-manifest-inventory",
            Status = status,
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(diagnostics),
            AssetRoots = assetRoots.OrderBy(item => item.AssetId, StringComparer.Ordinal).ThenBy(item => item.RelativePath, StringComparer.Ordinal).ToList(),
            AssetSourceManifests = manifests.OrderBy(item => item.RelativePath, StringComparer.Ordinal).ToList(),
            AssetOwners = owners.OrderBy(item => item.AssetId, StringComparer.Ordinal).ThenBy(item => item.Path, StringComparer.Ordinal).ToList(),
            AssetReferences = assetReferences.OrderBy(item => item.SceneId, StringComparer.Ordinal).ThenBy(item => item.EntityId, StringComparer.Ordinal).ThenBy(item => item.AssetId, StringComparer.Ordinal).ToList(),
            MissingOwners = missingOwners.OrderBy(item => item.SceneId, StringComparer.Ordinal).ThenBy(item => item.EntityId, StringComparer.Ordinal).ThenBy(item => item.AssetId, StringComparer.Ordinal).ToList(),
            GeneratedArtifacts = generatedArtifacts.OrderBy(item => item.Path, StringComparer.Ordinal).ToList(),
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    public static AssetReferenceGateReport BuildAssetReferenceGate(
        ManifestLoadResult load,
        string sourceTruthInventoryPath,
        string assetManifestInventoryPath)
    {
        var diagnostics = new List<ProjectDiagnostic>(load.Diagnostics);
        var sourceTruth = LoadJsonObject(sourceTruthInventoryPath, diagnostics, "Project.AssetReference.SourceTruthInventoryMalformed");
        var assetInventory = LoadJsonObject(assetManifestInventoryPath, diagnostics, "Project.AssetReference.AssetManifestInventoryMalformed");
        var references = ReadAssetReferences(sourceTruth);
        var owners = ReadAssetOwners(assetInventory);
        var generatedManifestPaths = ReadGeneratedManifestPaths(assetInventory);
        var resolved = new List<ResolvedAssetReferenceItem>();
        var unresolved = new List<UnresolvedAssetReferenceItem>();
        var missingOwners = new List<MissingAssetOwnerItem>();
        var rejectedGeneratedOwners = new List<RejectedGeneratedOwnerItem>();
        var assetRootIds = AssetRoots(load).Select(root => root.Id).ToHashSet(StringComparer.Ordinal);

        foreach (var reference in references)
        {
            var owner = owners.FirstOrDefault(item =>
                Matches(reference.AssetId, item.AssetId) || Matches(reference.Path, item.Path));

            if (string.IsNullOrWhiteSpace(reference.Owner))
            {
                missingOwners.Add(MissingOwner(reference, "Asset reference does not declare an owner."));
                diagnostics.Add(Error(
                    "Project.AssetReference.OwnerMissing",
                    "AssetReference",
                    reference.Path,
                    "Asset reference must declare an owner."));
                continue;
            }

            if (IsGeneratedOwner(reference.Owner) ||
                owner is { } ownerItem && (IsGeneratedOwner(ownerItem.Owner) || generatedManifestPaths.Contains(ownerItem.SourceManifest)))
            {
                rejectedGeneratedOwners.Add(new RejectedGeneratedOwnerItem
                {
                    SceneId = reference.SceneId,
                    EntityId = reference.EntityId,
                    AssetId = reference.AssetId,
                    Owner = reference.Owner,
                    Reason = "Generated artifacts and generated/package manifests cannot own asset references.",
                });
                diagnostics.Add(Error(
                    "Project.AssetReference.GeneratedOwnerRejected",
                    "AssetReference",
                    reference.Path,
                    "Generated artifacts and generated/package manifests cannot own asset references."));
                continue;
            }

            var invalidPath = IsInvalidProjectRelativePath(reference.Path);
            if (invalidPath.Length > 0)
            {
                unresolved.Add(Unresolved(reference, invalidPath));
                diagnostics.Add(Error(
                    "Project.AssetReference.InvalidPath",
                    "AssetReference",
                    reference.Path,
                    invalidPath));
                continue;
            }

            if (ReferenceResolves(load.ProjectRoot, reference, assetRootIds, owners))
            {
                resolved.Add(new ResolvedAssetReferenceItem
                {
                    SceneId = reference.SceneId,
                    EntityId = reference.EntityId,
                    AssetId = reference.AssetId,
                    Owner = reference.Owner,
                    Path = reference.Path,
                    Resolution = owner is null ? "ProjectAssetRoot" : "AssetSourceManifest",
                });
            }
            else
            {
                unresolved.Add(Unresolved(reference, "Asset reference did not resolve to a declared asset root or asset source manifest owner."));
                diagnostics.Add(Error(
                    "Project.AssetReference.Unresolved",
                    "AssetReference",
                    reference.Path,
                    "Asset reference did not resolve to a declared asset root or asset source manifest owner."));
            }
        }

        var checks = new List<SourceTruthGateCheck>
        {
            Check("ReferencesResolved", unresolved.Count == 0 ? "Passed" : "Failed", "Scene/entity asset references resolve to project asset roots or source manifests."),
            Check("OwnersPresent", missingOwners.Count == 0 ? "Passed" : "Failed", "Scene/entity asset references declare owners."),
            Check("GeneratedOwnersRejected", rejectedGeneratedOwners.Count == 0 ? "Passed" : "Failed", "Generated artifacts are rejected as asset ownership source."),
            Check("ReportOnlyInvariants", "Passed", "Report-only invariants are preserved."),
        };

        return new AssetReferenceGateReport
        {
            Command = "asset-reference-gate",
            Status = checks.Any(item => item.Status == "Failed") || diagnostics.Any(item => item.Severity == "Error") ? "Failed" : "Passed",
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(diagnostics),
            ResolvedReferences = resolved.OrderBy(item => item.SceneId, StringComparer.Ordinal).ThenBy(item => item.EntityId, StringComparer.Ordinal).ThenBy(item => item.AssetId, StringComparer.Ordinal).ToList(),
            UnresolvedReferences = unresolved.OrderBy(item => item.SceneId, StringComparer.Ordinal).ThenBy(item => item.EntityId, StringComparer.Ordinal).ThenBy(item => item.AssetId, StringComparer.Ordinal).ToList(),
            MissingOwners = missingOwners.OrderBy(item => item.SceneId, StringComparer.Ordinal).ThenBy(item => item.EntityId, StringComparer.Ordinal).ThenBy(item => item.AssetId, StringComparer.Ordinal).ToList(),
            RejectedGeneratedOwners = rejectedGeneratedOwners.OrderBy(item => item.SceneId, StringComparer.Ordinal).ThenBy(item => item.EntityId, StringComparer.Ordinal).ThenBy(item => item.AssetId, StringComparer.Ordinal).ToList(),
            Checks = checks.OrderBy(item => item.CheckId, StringComparer.Ordinal).ToList(),
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    public static GeneratedArtifactFreshnessReport BuildGeneratedFreshnessGate(ManifestLoadResult load, string inventoryPath)
    {
        var diagnostics = new List<ProjectDiagnostic>(load.Diagnostics);
        var inventory = LoadJsonObject(inventoryPath, diagnostics, "Project.GeneratedFreshness.InventoryMalformed");
        var generated = new List<GeneratedFreshnessArtifactItem>();
        var sourceInputs = new List<GeneratedFreshnessSourceInputItem>();
        var freshness = new List<GeneratedFreshnessStatusItem>();

        foreach (var artifact in ReadGeneratedArtifacts(inventory))
        {
            var status = ClassifyFreshness(load.ProjectRoot, artifact, diagnostics, out var actualHash);
            generated.Add(new GeneratedFreshnessArtifactItem
            {
                SceneId = artifact.SceneId,
                ArtifactId = artifact.ArtifactId,
                Path = artifact.Path,
                ExpectedSourceHash = artifact.ExpectedSourceHash,
                ActualSourceHash = actualHash,
                Status = status,
            });
            sourceInputs.Add(new GeneratedFreshnessSourceInputItem
            {
                SceneId = artifact.SceneId,
                SourceHash = artifact.ExpectedSourceHash,
                RelativePath = ScenePathFor(inventory, artifact.SceneId),
            });
            freshness.Add(new GeneratedFreshnessStatusItem { ArtifactId = artifact.ArtifactId, Status = status });
        }

        if (inventory is not null && generated.Count == 0)
        {
            diagnostics.Add(Warning(
                "Project.GeneratedFreshness.NoGeneratedArtifacts",
                "GeneratedFreshness",
                inventoryPath,
                "No generated artifact evidence was found in the source truth inventory."));
        }

        var statusCountsAsPartial = generated.Any(item => item.Status is not "Fresh");
        var reportStatus = diagnostics.Any(item => item.Severity == "Error") || generated.Any(item => item.Status == "Invalid")
            ? "Failed"
            : statusCountsAsPartial || diagnostics.Any(item => item.Severity == "Warning") ? "PartiallyProven" : "Passed";

        return new GeneratedArtifactFreshnessReport
        {
            Command = "generated-freshness-gate",
            Status = reportStatus,
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(diagnostics),
            GeneratedArtifacts = generated.OrderBy(item => item.SceneId, StringComparer.Ordinal).ThenBy(item => item.ArtifactId, StringComparer.Ordinal).ToList(),
            SourceInputs = sourceInputs.OrderBy(item => item.SceneId, StringComparer.Ordinal).ThenBy(item => item.SourceHash, StringComparer.Ordinal).ToList(),
            FreshnessStatus = freshness.OrderBy(item => item.ArtifactId, StringComparer.Ordinal).ToList(),
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    public static SceneEntityValidationReport BuildSceneEntityValidation(ManifestLoadResult load)
    {
        var diagnostics = new List<ProjectDiagnostic>(load.Diagnostics);
        var scenes = new List<SceneSchemaValidationItem>();
        var entities = new List<EntitySchemaValidationItem>();
        var assetReferences = new List<AssetReferenceTruthItem>();
        var generatedArtifacts = new List<GeneratedArtifactTruthItem>();

        foreach (var scene in SceneFiles(load))
        {
            ValidateScene(load.ProjectRoot, scene, scenes, entities, assetReferences, generatedArtifacts, diagnostics);
        }

        var checks = new List<SourceTruthGateCheck>
        {
            Check("SchemaVersion", NoDiagnostic(diagnostics, "Project.SceneEntity.SchemaVersion"), "Scene and entity schema version 1 is declared."),
            Check("SceneIdsPresent", NoDiagnostic(diagnostics, "Project.SceneEntity.SceneIdMissing"), "Scenes declare stable scene ids."),
            Check("EntityIdsPresent", NoDiagnostic(diagnostics, "Project.SceneEntity.EntityIdMissing"), "Entities declare stable entity ids."),
            Check("DuplicateEntityIds", NoDiagnostic(diagnostics, "Project.SceneEntity.DuplicateEntityId"), "Entity ids are unique within each scene."),
            Check("AssetReferenceOwners", NoDiagnostic(diagnostics, "Project.SceneEntity.AssetReferenceOwnerMissing"), "Asset references declare owners."),
            Check("GeneratedArtifactsNonCanonical", NoDiagnostic(diagnostics, "Project.SceneEntity.GeneratedArtifactCanonical"), "Generated artifacts are not canonical source truth."),
            Check("ReportOnlyInvariants", "Passed", "Report-only invariants are preserved."),
        };

        return new SceneEntityValidationReport
        {
            Command = "scene-entity-validate",
            Status = diagnostics.Any(item => item.Severity == "Error") ? "Failed" : "Passed",
            Project = BuildProjectSummary(load),
            Diagnostics = SortDiagnostics(diagnostics),
            Scenes = scenes.OrderBy(item => item.SceneId, StringComparer.Ordinal).ThenBy(item => item.RelativePath, StringComparer.Ordinal).ToList(),
            Entities = entities.OrderBy(item => item.SceneId, StringComparer.Ordinal).ThenBy(item => item.EntityId, StringComparer.Ordinal).ToList(),
            AssetReferences = assetReferences.OrderBy(item => item.SceneId, StringComparer.Ordinal).ThenBy(item => item.EntityId, StringComparer.Ordinal).ThenBy(item => item.AssetId, StringComparer.Ordinal).ToList(),
            GeneratedArtifacts = generatedArtifacts.OrderBy(item => item.SceneId, StringComparer.Ordinal).ThenBy(item => item.ArtifactId, StringComparer.Ordinal).ToList(),
            SchemaChecks = checks.OrderBy(item => item.CheckId, StringComparer.Ordinal).ToList(),
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
    }

    private static void AddAssetSourceManifest(
        string projectRoot,
        string manifestPath,
        bool sourceControlled,
        List<AssetSourceManifestItem> manifests,
        List<AssetOwnerInventoryItem> owners,
        List<AssetReferenceTruthItem> assetReferences,
        List<MissingAssetOwnerItem> missingOwners,
        List<ProjectDiagnostic> diagnostics)
    {
        var relativePath = NormalizeRelative(Path.GetRelativePath(projectRoot, manifestPath));
        var manifestKind = Path.GetFileName(manifestPath) == "assets.source.json" ? "assets.source.json" : "asset_manifest.json";
        var classification = sourceControlled ? "SourceControlledAssetMetadata" : GeneratedClassification(relativePath);
        var canonical = sourceControlled;
        var manifestId = Path.GetFileNameWithoutExtension(manifestPath);

        var root = LoadJsonElement(manifestPath, diagnostics, "Project.AssetSource.ManifestMalformed");
        if (root.HasValue && root.Value.ValueKind == JsonValueKind.Object)
        {
            manifestId = FirstString(root.Value, "manifestId", "id", "assetManifestId", "packageId", "artifactId", fallback: manifestId);
            if (sourceControlled)
            {
                canonical = !root.Value.TryGetProperty("canonical", out var explicitCanonical) ||
                    explicitCanonical.ValueKind is not (JsonValueKind.False or JsonValueKind.True) ||
                    explicitCanonical.GetBoolean();
            }
            AddAssetOwnersFromManifest(root.Value, relativePath, classification, owners, missingOwners, diagnostics);
            AddAssetReferencesFromManifest(root.Value, relativePath, assetReferences, missingOwners, diagnostics);
        }

        manifests.Add(new AssetSourceManifestItem
        {
            ManifestId = manifestId,
            RelativePath = relativePath,
            ManifestKind = manifestKind,
            Classification = classification,
            Canonical = canonical,
            Status = sourceControlled ? "Accepted" : "NonCanonical",
        });

        if (!sourceControlled)
        {
            diagnostics.Add(Warning(
                "Project.AssetSource.GeneratedManifestNonCanonical",
                "AssetSource",
                relativePath,
                "Generated and package asset manifests are inventory evidence only and are not canonical asset ownership source."));
        }
    }

    private static void AddAssetOwnersFromManifest(
        JsonElement root,
        string sourceManifest,
        string classification,
        List<AssetOwnerInventoryItem> owners,
        List<MissingAssetOwnerItem> missingOwners,
        List<ProjectDiagnostic> diagnostics)
    {
        foreach (var item in EnumerateObjectArray(root, "assetOwners").Concat(EnumerateObjectArray(root, "assets")))
        {
            var assetId = FirstString(item, "assetId", "id", fallback: "");
            var owner = FirstString(item, "owner", fallback: "");
            var path = FirstString(item, "path", "relativePath", fallback: "");
            var status = string.IsNullOrWhiteSpace(owner) ? "MissingOwner" : "Passed";
            owners.Add(new AssetOwnerInventoryItem
            {
                AssetId = assetId,
                Owner = owner,
                Path = path,
                SourceManifest = sourceManifest,
                Classification = classification,
                Status = status,
            });
            if (status != "Passed")
            {
                missingOwners.Add(new MissingAssetOwnerItem { AssetId = assetId, Path = path, Reason = "Asset source manifest owner is missing." });
                diagnostics.Add(Error(
                    "Project.AssetSource.OwnerMissing",
                    "AssetSource",
                    sourceManifest,
                    "Asset source manifest entry must declare an owner."));
            }
        }
    }

    private static void AddAssetReferencesFromManifest(
        JsonElement root,
        string sourceManifest,
        List<AssetReferenceTruthItem> assetReferences,
        List<MissingAssetOwnerItem> missingOwners,
        List<ProjectDiagnostic> diagnostics)
    {
        foreach (var item in EnumerateObjectArray(root, "assetReferences"))
        {
            var assetId = FirstString(item, "assetId", "id", fallback: "");
            var owner = FirstString(item, "owner", fallback: "");
            var path = FirstString(item, "path", "relativePath", fallback: "");
            var status = string.IsNullOrWhiteSpace(owner) ? "MissingOwner" : "Passed";
            assetReferences.Add(new AssetReferenceTruthItem
            {
                AssetId = assetId,
                Owner = owner,
                Path = path,
                Classification = "AssetSourceManifestReference",
                Status = status,
            });
            if (status != "Passed")
            {
                missingOwners.Add(new MissingAssetOwnerItem { AssetId = assetId, Path = path, Reason = "Asset source manifest reference owner is missing." });
                diagnostics.Add(Error(
                    "Project.AssetSource.ReferenceOwnerMissing",
                    "AssetSource",
                    sourceManifest,
                    "Asset source manifest reference must declare an owner."));
            }
        }
    }

    private static void CollectSceneAssetEvidence(
        string projectRoot,
        ResolvedProjectPath scene,
        List<AssetReferenceTruthItem> assetReferences,
        List<GeneratedAssetArtifactItem> generatedArtifacts,
        List<MissingAssetOwnerItem> missingOwners,
        List<ProjectDiagnostic> diagnostics)
    {
        var root = LoadJsonElement(scene.AbsolutePath, diagnostics, "Project.AssetSource.SceneMalformed");
        if (!root.HasValue || root.Value.ValueKind != JsonValueKind.Object)
        {
            return;
        }

        var sceneId = FirstString(root.Value, "sceneId", fallback: scene.Id);
        foreach (var entity in EnumerateObjectArray(root.Value, "entities"))
        {
            var entityId = FirstString(entity, "entityId", fallback: "");
            foreach (var reference in EnumerateObjectArray(entity, "assetReferences"))
            {
                var assetId = FirstString(reference, "assetId", "id", fallback: "");
                var owner = FirstString(reference, "owner", fallback: "");
                var path = FirstString(reference, "path", "relativePath", fallback: "");
                var status = string.IsNullOrWhiteSpace(owner) ? "MissingOwner" : "Passed";
                assetReferences.Add(new AssetReferenceTruthItem
                {
                    SceneId = sceneId,
                    EntityId = entityId,
                    AssetId = assetId,
                    Owner = owner,
                    Path = path,
                    Classification = "SceneEntityAssetReference",
                    Status = status,
                });
                if (status != "Passed")
                {
                    missingOwners.Add(new MissingAssetOwnerItem
                    {
                        SceneId = sceneId,
                        EntityId = entityId,
                        AssetId = assetId,
                        Path = path,
                        Reason = "Scene/entity asset reference owner is missing.",
                    });
                    diagnostics.Add(Error(
                        "Project.AssetSource.SceneAssetReferenceOwnerMissing",
                        "AssetSource",
                        scene.RelativePath,
                        "Scene/entity asset reference must declare an owner."));
                }
            }
        }

        foreach (var artifact in EnumerateObjectArray(root.Value, "generatedArtifacts"))
        {
            var path = FirstString(artifact, "path", fallback: "");
            generatedArtifacts.Add(new GeneratedAssetArtifactItem
            {
                ArtifactId = FirstString(artifact, "artifactId", "id", fallback: ""),
                Path = path,
                Classification = "GeneratedArtifact",
                Canonical = ReadBool(artifact, "canonical"),
                Status = File.Exists(Path.Combine(projectRoot, path)) ? "Present" : "Missing",
            });
        }
    }

    private static void ValidateScene(
        string projectRoot,
        ResolvedProjectPath scene,
        List<SceneSchemaValidationItem> scenes,
        List<EntitySchemaValidationItem> entities,
        List<AssetReferenceTruthItem> assetReferences,
        List<GeneratedArtifactTruthItem> generatedArtifacts,
        List<ProjectDiagnostic> diagnostics)
    {
        var root = LoadJsonElement(scene.AbsolutePath, diagnostics, "Project.SceneEntity.SceneMalformed", warning: false);
        if (!root.HasValue || root.Value.ValueKind != JsonValueKind.Object)
        {
            scenes.Add(new SceneSchemaValidationItem { SceneId = scene.Id, RelativePath = scene.RelativePath, Status = "Malformed" });
            return;
        }

        var sceneId = FirstString(root.Value, "sceneId", fallback: "");
        var schemaVersion = ReadIntString(root.Value, "schemaVersion");
        var sceneStatus = "Passed";
        if (schemaVersion != "1")
        {
            sceneStatus = "Failed";
            diagnostics.Add(Error("Project.SceneEntity.SchemaVersion", "SceneEntity", scene.RelativePath, "Scene schemaVersion must be 1."));
        }
        if (string.IsNullOrWhiteSpace(sceneId))
        {
            sceneStatus = "Failed";
            sceneId = scene.Id;
            diagnostics.Add(Error("Project.SceneEntity.SceneIdMissing", "SceneEntity", scene.RelativePath, "Scene schema v1 requires sceneId."));
        }
        if (FirstString(root.Value, "sourceKind", fallback: "") != "SceneSourceTruth")
        {
            sceneStatus = "Failed";
            diagnostics.Add(Error("Project.SceneEntity.SceneSourceKindInvalid", "SceneEntity", scene.RelativePath, "Scene schema v1 requires sourceKind SceneSourceTruth."));
        }

        scenes.Add(new SceneSchemaValidationItem
        {
            SceneId = sceneId,
            RelativePath = scene.RelativePath,
            SchemaVersion = schemaVersion,
            Status = sceneStatus,
        });

        if (!root.Value.TryGetProperty("entities", out var entityArray) || entityArray.ValueKind != JsonValueKind.Array)
        {
            diagnostics.Add(Error("Project.SceneEntity.EntitiesMissing", "SceneEntity", scene.RelativePath, "Scene schema v1 requires an entities array."));
        }
        else
        {
            var seenEntityIds = new HashSet<string>(StringComparer.Ordinal);
            foreach (var entity in entityArray.EnumerateArray())
            {
                if (entity.ValueKind != JsonValueKind.Object)
                {
                    diagnostics.Add(Error("Project.SceneEntity.EntityMalformed", "SceneEntity", scene.RelativePath, "Entity schema v1 entries must be JSON objects."));
                    continue;
                }
                ValidateEntity(scene.RelativePath, sceneId, entity, seenEntityIds, entities, assetReferences, diagnostics);
            }
        }

        foreach (var artifact in EnumerateObjectArray(root.Value, "generatedArtifacts"))
        {
            var canonical = ReadBool(artifact, "canonical");
            var artifactId = FirstString(artifact, "artifactId", "id", fallback: "");
            if (canonical)
            {
                diagnostics.Add(Error("Project.SceneEntity.GeneratedArtifactCanonical", "SceneEntity", scene.RelativePath, "Generated artifacts cannot be canonical source truth."));
            }
            generatedArtifacts.Add(new GeneratedArtifactTruthItem
            {
                SceneId = sceneId,
                ArtifactId = artifactId,
                Path = FirstString(artifact, "path", fallback: ""),
                Canonical = canonical,
                ExpectedSourceHash = FirstString(artifact, "expectedSourceHash", fallback: ""),
                Status = canonical ? "InvalidCanonical" : "Passed",
            });
        }
    }

    private static void ValidateEntity(
        string scenePath,
        string sceneId,
        JsonElement entity,
        HashSet<string> seenEntityIds,
        List<EntitySchemaValidationItem> entities,
        List<AssetReferenceTruthItem> assetReferences,
        List<ProjectDiagnostic> diagnostics)
    {
        var entityId = FirstString(entity, "entityId", fallback: "");
        var status = "Passed";
        if (string.IsNullOrWhiteSpace(entityId))
        {
            status = "Failed";
            entityId = "missing-entity-id";
            diagnostics.Add(Error("Project.SceneEntity.EntityIdMissing", "SceneEntity", scenePath, "Entity schema v1 requires entityId."));
        }
        else if (!seenEntityIds.Add(entityId))
        {
            status = "Failed";
            diagnostics.Add(Error("Project.SceneEntity.DuplicateEntityId", "SceneEntity", scenePath, $"Duplicate entity id in scene: {entityId}."));
        }
        if (FirstString(entity, "sourceKind", fallback: "") != "EntitySourceTruth")
        {
            status = "Failed";
            diagnostics.Add(Error("Project.SceneEntity.EntitySourceKindInvalid", "SceneEntity", scenePath, "Entity schema v1 requires sourceKind EntitySourceTruth."));
        }

        entities.Add(new EntitySchemaValidationItem
        {
            SceneId = sceneId,
            EntityId = entityId,
            RelativePath = scenePath,
            SchemaVersion = "1",
            Status = status,
        });

        foreach (var reference in EnumerateObjectArray(entity, "assetReferences"))
        {
            var owner = FirstString(reference, "owner", fallback: "");
            var assetId = FirstString(reference, "assetId", "id", fallback: "");
            var path = FirstString(reference, "path", "relativePath", fallback: "");
            var referenceStatus = string.IsNullOrWhiteSpace(owner) ? "MissingOwner" : "Passed";
            if (referenceStatus != "Passed")
            {
                diagnostics.Add(Error("Project.SceneEntity.AssetReferenceOwnerMissing", "SceneEntity", scenePath, "Asset references in scene/entity schema v1 must declare owner."));
            }
            assetReferences.Add(new AssetReferenceTruthItem
            {
                SceneId = sceneId,
                EntityId = entityId,
                AssetId = assetId,
                Owner = owner,
                Path = path,
                Classification = "SceneEntityAssetReference",
                Status = referenceStatus,
            });
        }
    }

    private static string ClassifyFreshness(
        string projectRoot,
        GeneratedArtifactTruthItem artifact,
        List<ProjectDiagnostic> diagnostics,
        out string actualHash)
    {
        actualHash = "";
        if (artifact.Canonical || IsInvalidProjectRelativePath(artifact.Path).Length > 0)
        {
            diagnostics.Add(Error(
                "Project.GeneratedFreshness.Invalid",
                "GeneratedFreshness",
                artifact.Path,
                "Generated artifacts cannot be canonical and must use a project-relative path."));
            return "Invalid";
        }

        if (string.IsNullOrWhiteSpace(artifact.ExpectedSourceHash))
        {
            diagnostics.Add(Warning(
                "Project.GeneratedFreshness.MissingSource",
                "GeneratedFreshness",
                artifact.Path,
                "Generated artifact freshness cannot be evaluated without an expected source hash."));
            return "MissingSource";
        }

        var fullPath = Path.Combine(projectRoot, artifact.Path);
        if (string.IsNullOrWhiteSpace(artifact.Path) || !File.Exists(fullPath))
        {
            diagnostics.Add(Warning(
                "Project.GeneratedFreshness.MissingGenerated",
                "GeneratedFreshness",
                artifact.Path,
                "Generated artifact evidence is missing."));
            return "MissingGenerated";
        }

        actualHash = ReadGeneratedHash(fullPath, diagnostics);
        if (string.IsNullOrWhiteSpace(actualHash))
        {
            diagnostics.Add(Warning(
                "Project.GeneratedFreshness.UnknownFreshness",
                "GeneratedFreshness",
                artifact.Path,
                "Generated artifact does not declare sourceHash or inputHash."));
            return "UnknownFreshness";
        }

        if (actualHash == artifact.ExpectedSourceHash)
        {
            return "Fresh";
        }

        diagnostics.Add(Warning(
            "Project.GeneratedFreshness.Stale",
            "GeneratedFreshness",
            artifact.Path,
            "Generated artifact sourceHash/inputHash does not match expected scene source hash."));
        return "Stale";
    }

    private static List<AssetReferenceTruthItem> ReadAssetReferences(JsonObject? root)
    {
        if (root?["assetReferences"] is not JsonArray array)
        {
            return [];
        }

        return array
            .OfType<JsonObject>()
            .Select(item => new AssetReferenceTruthItem
            {
                SceneId = ReadString(item, "sceneId"),
                EntityId = ReadString(item, "entityId"),
                AssetId = ReadString(item, "assetId"),
                Owner = ReadString(item, "owner"),
                Path = ReadString(item, "path"),
                Classification = ReadString(item, "classification"),
                Status = ReadString(item, "status"),
            })
            .OrderBy(item => item.SceneId, StringComparer.Ordinal)
            .ThenBy(item => item.EntityId, StringComparer.Ordinal)
            .ThenBy(item => item.AssetId, StringComparer.Ordinal)
            .ToList();
    }

    private static List<AssetOwnerInventoryItem> ReadAssetOwners(JsonObject? root)
    {
        if (root?["assetOwners"] is not JsonArray array)
        {
            return [];
        }

        return array
            .OfType<JsonObject>()
            .Select(item => new AssetOwnerInventoryItem
            {
                AssetId = ReadString(item, "assetId"),
                Owner = ReadString(item, "owner"),
                Path = ReadString(item, "path"),
                SourceManifest = ReadString(item, "sourceManifest"),
                Classification = ReadString(item, "classification"),
                Status = ReadString(item, "status"),
            })
            .ToList();
    }

    private static HashSet<string> ReadGeneratedManifestPaths(JsonObject? root)
    {
        if (root?["assetSourceManifests"] is not JsonArray array)
        {
            return [];
        }

        return array
            .OfType<JsonObject>()
            .Where(item => !ReadBool(item, "canonical") || ReadString(item, "classification").Contains("Generated", StringComparison.Ordinal))
            .Select(item => ReadString(item, "relativePath"))
            .Where(path => path.Length > 0)
            .ToHashSet(StringComparer.Ordinal);
    }

    private static List<GeneratedArtifactTruthItem> ReadGeneratedArtifacts(JsonObject? root)
    {
        if (root?["generatedArtifacts"] is not JsonArray array)
        {
            return [];
        }

        return array
            .OfType<JsonObject>()
            .Select(item => new GeneratedArtifactTruthItem
            {
                SceneId = ReadString(item, "sceneId"),
                ArtifactId = ReadString(item, "artifactId"),
                Path = ReadString(item, "path"),
                Canonical = ReadBool(item, "canonical"),
                ExpectedSourceHash = ReadString(item, "expectedSourceHash"),
                ActualSourceHash = ReadString(item, "actualSourceHash"),
                Status = ReadString(item, "status"),
            })
            .ToList();
    }

    private static string ScenePathFor(JsonObject? inventory, string sceneId)
    {
        if (inventory?["scenes"] is not JsonArray array)
        {
            return "";
        }

        return array.OfType<JsonObject>()
            .Where(item => ReadString(item, "sceneId") == sceneId)
            .Select(item => ReadString(item, "relativePath"))
            .FirstOrDefault() ?? "";
    }

    private static bool ReferenceResolves(
        string projectRoot,
        AssetReferenceTruthItem reference,
        HashSet<string> assetRootIds,
        IReadOnlyList<AssetOwnerInventoryItem> owners)
    {
        if (!string.IsNullOrWhiteSpace(reference.AssetId) && assetRootIds.Contains(reference.AssetId))
        {
            return string.IsNullOrWhiteSpace(reference.Path) || File.Exists(Path.Combine(projectRoot, reference.Path)) || Directory.Exists(Path.Combine(projectRoot, reference.Path));
        }

        return owners.Any(item =>
            !IsGeneratedOwner(item.Owner) &&
            (Matches(reference.AssetId, item.AssetId) || Matches(reference.Path, item.Path)));
    }

    private static string ReadGeneratedHash(string path, List<ProjectDiagnostic> diagnostics)
    {
        var root = LoadJsonElement(path, diagnostics, "Project.GeneratedFreshness.GeneratedMalformed");
        return root.HasValue ? FirstString(root.Value, "sourceHash", "inputHash", fallback: "") : "";
    }

    private static List<ResolvedProjectPath> AssetRoots(ManifestLoadResult load) =>
        load.ResolvedPaths
            .Where(path => path.Category == "asset")
            .OrderBy(path => path.Id, StringComparer.Ordinal)
            .ThenBy(path => path.RelativePath, StringComparer.Ordinal)
            .ToList();

    private static List<ResolvedProjectPath> SceneFiles(ManifestLoadResult load) =>
        load.ResolvedPaths
            .Where(path => path.Category == "scene" && path.Exists)
            .OrderBy(path => path.Id, StringComparer.Ordinal)
            .ThenBy(path => path.RelativePath, StringComparer.Ordinal)
            .ToList();

    private static List<string> FindAssetSourceManifestFiles(string assetRoot)
    {
        if (!Directory.Exists(assetRoot))
        {
            return [];
        }

        return Directory.EnumerateFiles(assetRoot, "*.json", SearchOption.AllDirectories)
            .Where(path => Path.GetFileName(path) is "assets.source.json" or "asset_manifest.json")
            .OrderBy(NormalizeAbsolute, StringComparer.Ordinal)
            .ToList();
    }

    private static List<string> FindGeneratedAssetManifestFiles(string projectRoot, IEnumerable<string> assetRootPaths)
    {
        var assetRoots = assetRootPaths.Select(NormalizeAbsolute).ToList();
        var candidates = new[] { "Generated", "Build", "Packages" }
            .Select(path => Path.Combine(projectRoot, path))
            .Where(Directory.Exists)
            .SelectMany(path => Directory.EnumerateFiles(path, "*.json", SearchOption.AllDirectories))
            .Where(path => Path.GetFileName(path) is "asset_manifest.json" or "assets.source.json")
            .Where(path => !assetRoots.Any(root => NormalizeAbsolute(path).StartsWith(root + "/", StringComparison.Ordinal)))
            .OrderBy(NormalizeAbsolute, StringComparer.Ordinal)
            .ToList();
        return candidates;
    }

    private static IEnumerable<JsonElement> EnumerateObjectArray(JsonElement root, string key) =>
        root.ValueKind == JsonValueKind.Object &&
        root.TryGetProperty(key, out var array) &&
        array.ValueKind == JsonValueKind.Array
            ? array.EnumerateArray().Where(item => item.ValueKind == JsonValueKind.Object)
            : [];

    private static JsonElement? LoadJsonElement(string path, List<ProjectDiagnostic> diagnostics, string code, bool warning = true)
    {
        try
        {
            using var input = File.OpenRead(path);
            using var document = JsonDocument.Parse(input);
            return document.RootElement.Clone();
        }
        catch (Exception e) when (e is JsonException or IOException or UnauthorizedAccessException)
        {
            var diagnostic = warning
                ? Warning(code, CategoryForCode(code), path, $"JSON could not be read: {e.Message}")
                : Error(code, CategoryForCode(code), path, $"JSON could not be read: {e.Message}");
            diagnostics.Add(diagnostic);
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

    private static string FirstString(JsonElement root, params string[] keys) =>
        FirstString(root, keys, fallback: "");

    private static string FirstString(JsonElement root, string key, string key2, string key3, string key4, string key5, string fallback) =>
        FirstString(root, [key, key2, key3, key4, key5], fallback);

    private static string FirstString(JsonElement root, string key, string key2, string key3, string key4, string fallback) =>
        FirstString(root, [key, key2, key3, key4], fallback);

    private static string FirstString(JsonElement root, string key, string key2, string fallback) =>
        FirstString(root, [key, key2], fallback);

    private static string FirstString(JsonElement root, string key, string fallback) =>
        FirstString(root, [key], fallback);

    private static string FirstString(JsonElement root, string[] keys, string fallback)
    {
        if (root.ValueKind != JsonValueKind.Object)
        {
            return fallback;
        }

        foreach (var key in keys)
        {
            if (root.TryGetProperty(key, out var value) && value.ValueKind == JsonValueKind.String)
            {
                return value.GetString() ?? fallback;
            }
        }
        return fallback;
    }

    private static bool ReadBool(JsonElement root, string key) =>
        root.ValueKind == JsonValueKind.Object &&
        root.TryGetProperty(key, out var value) &&
        value.ValueKind is JsonValueKind.True or JsonValueKind.False &&
        value.GetBoolean();

    private static string ReadIntString(JsonElement root, string key)
    {
        if (root.ValueKind != JsonValueKind.Object || !root.TryGetProperty(key, out var value))
        {
            return "";
        }
        return value.ValueKind == JsonValueKind.Number && value.TryGetInt32(out var result) ? result.ToString() : "";
    }

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

    private static bool Matches(string left, string right) =>
        !string.IsNullOrWhiteSpace(left) &&
        !string.IsNullOrWhiteSpace(right) &&
        string.Equals(NormalizeRelative(left), NormalizeRelative(right), StringComparison.Ordinal);

    private static string IsInvalidProjectRelativePath(string value)
    {
        if (string.IsNullOrWhiteSpace(value))
        {
            return "";
        }
        if (Path.IsPathRooted(value) || value.StartsWith("/", StringComparison.Ordinal) || value.StartsWith("\\", StringComparison.Ordinal))
        {
            return "Asset paths must be project-relative.";
        }
        if (NormalizeRelative(value).Split('/', StringSplitOptions.RemoveEmptyEntries).Any(segment => segment == ".."))
        {
            return "Asset paths must not contain parent traversal.";
        }
        return "";
    }

    private static bool IsGeneratedOwner(string owner) =>
        owner.Contains("generated", StringComparison.OrdinalIgnoreCase) ||
        owner.Contains("package", StringComparison.OrdinalIgnoreCase);

    private static MissingAssetOwnerItem MissingOwner(AssetReferenceTruthItem reference, string reason) =>
        new()
        {
            SceneId = reference.SceneId,
            EntityId = reference.EntityId,
            AssetId = reference.AssetId,
            Path = reference.Path,
            Reason = reason,
        };

    private static UnresolvedAssetReferenceItem Unresolved(AssetReferenceTruthItem reference, string reason) =>
        new()
        {
            SceneId = reference.SceneId,
            EntityId = reference.EntityId,
            AssetId = reference.AssetId,
            Owner = reference.Owner,
            Path = reference.Path,
            Reason = reason,
        };

    private static string GeneratedClassification(string path) =>
        path.StartsWith("Build/", StringComparison.Ordinal) || path.StartsWith("Packages/", StringComparison.Ordinal)
            ? "PackageNonCanonical"
            : "GeneratedNonCanonical";

    private static string NoDiagnostic(IReadOnlyList<ProjectDiagnostic> diagnostics, string code) =>
        diagnostics.Any(item => item.Code == code) ? "Failed" : "Passed";

    private static string ReportStatus(IReadOnlyList<ProjectDiagnostic> diagnostics, bool warningMeansPartial)
    {
        if (diagnostics.Any(item => item.Severity == "Error"))
        {
            return "Failed";
        }
        return warningMeansPartial && diagnostics.Any(item => item.Severity == "Warning") ? "PartiallyProven" : "Passed";
    }

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
        if (code.Contains("SceneEntity", StringComparison.Ordinal))
        {
            return "SceneEntity";
        }
        if (code.Contains("GeneratedFreshness", StringComparison.Ordinal))
        {
            return "GeneratedFreshness";
        }
        if (code.Contains("AssetReference", StringComparison.Ordinal))
        {
            return "AssetReference";
        }
        return "AssetSource";
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

    private static string NormalizeAbsolute(string path) =>
        Path.GetFullPath(path).Replace('\\', '/');
}
