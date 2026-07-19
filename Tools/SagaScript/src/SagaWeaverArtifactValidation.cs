// SPDX-License-Identifier: MPL-2.0
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;

namespace SagaScript;

internal static partial class SagaWeaverArtifacts
{
    public static ScriptArtifactValidationReport ValidateArtifacts(string artifactRoot)
    {
        var root = Path.GetFullPath(artifactRoot);
        var diagnostics = new List<SagaDiagnostic>();
        var checkedArtifacts = new List<CheckedScriptArtifact>();

        var nodeLibrary = CheckArtifact(root, "nodeLibrary", "node_library_report.json", required: false, diagnostics: diagnostics);
        var nodeMetadata = CheckArtifact(root, "nodeMetadata", "node_metadata.json", required: false, diagnostics: diagnostics);
        var runtimeBindings = CheckArtifact(root, "runtimeBindings", "runtime_bindings.json", required: false, diagnostics: diagnostics);
        var compatibilityProfile = CheckArtifact(root, "compatibilityProfile", "csharp_compatibility_profile_v2.json", required: true, diagnostics: diagnostics);

        checkedArtifacts.AddRange([nodeLibrary.Check, nodeMetadata.Check, runtimeBindings.Check, compatibilityProfile.Check]);
        var patchFiles = new (string File, string Command, bool MutatesSource)[]
        {
            ("patch_evaluation.json", "patch-evaluation", false),
            ("patch_apply_report.json", "patch-apply", true),
            ("patch_diff_report.json", "patch-diff", false),
            ("patch_review_report.json", "patch-review", false),
            ("patch_rollback_report.json", "patch-rollback", true),
        };
        foreach (var patch in patchFiles)
        {
            var checkedPatch = CheckArtifact(root, "patchReport", patch.File, required: false, diagnostics: diagnostics);
            checkedArtifacts.Add(checkedPatch.Check);
            if (checkedPatch.Json is null)
            {
                continue;
            }
            if (ReadString(checkedPatch.Json, "command") != patch.Command)
            {
                diagnostics.Add(Error("Script.Artifact.PatchCommandMismatch", $"{patch.File} must be a {patch.Command} report.", checkedPatch.Path));
            }
            if (ReadBool(checkedPatch.Json, "mutatesSource") != patch.MutatesSource)
            {
                diagnostics.Add(Error("Script.Artifact.PatchMutationMismatch", $"{patch.File} has an unexpected mutatesSource value.", checkedPatch.Path));
            }
        }

        if (nodeMetadata.Json is not null && runtimeBindings.Json is not null)
        {
            ValidateNodeCapabilityConsistency(nodeMetadata.Json, runtimeBindings.Json, diagnostics);
        }
        if (nodeMetadata.Json is not null && nodeLibrary.Json is null)
        {
            diagnostics.Add(Error("Script.Artifact.NodeLibraryMissing", "node_metadata.json is present without node_library_report.json evidence.", nodeLibrary.Path));
        }
        if (nodeLibrary.Json is not null && ReadString(nodeLibrary.Json, "status") != "Passed")
        {
            diagnostics.Add(Error("Script.Artifact.NodeLibraryFailed", "node_library_report.json must have status Passed.", nodeLibrary.Path));
        }
        if (compatibilityProfile.Json is not null && ReadString(compatibilityProfile.Json, "status") != "Passed")
        {
            diagnostics.Add(Error("Script.Artifact.CompatibilityProfileFailed", "csharp_compatibility_profile_v2.json must have status Passed.", compatibilityProfile.Path));
        }

        var runtimeProofSummary = SummarizeRuntimeProof(runtimeBindings.Json, diagnostics, runtimeBindings.Path);
        var sortedDiagnostics = SortDiagnostics(diagnostics).ToArray();
        var status = sortedDiagnostics.Any(SagaScriptAnalyzer.IsBlocking) ? "Failed" : "Passed";

        return new ScriptArtifactValidationReport
        {
            Status = status,
            ArtifactRoot = root,
            CheckedArtifacts = checkedArtifacts
                .OrderBy(item => item.Kind, StringComparer.Ordinal)
                .ThenBy(item => item.Path, StringComparer.Ordinal)
                .ToArray(),
            RuntimeProofSummary = runtimeProofSummary,
            Summary = new Dictionary<string, int>(StringComparer.Ordinal)
            {
                ["checkedArtifactCount"] = checkedArtifacts.Count,
                ["diagnosticCount"] = sortedDiagnostics.Length
            },
            Diagnostics = sortedDiagnostics
        };
    }
}
