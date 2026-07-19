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
    private static (CheckedScriptArtifact Check, JsonObject? Json, string Path) CheckArtifact(
        string root,
        string kind,
        string fileName,
        bool required,
        List<SagaDiagnostic> diagnostics)
    {
        var path = Path.Combine(root, fileName);
        if (!File.Exists(path))
        {
            if (required)
            {
                diagnostics.Add(Error("Script.Artifact.Missing", $"Required artifact is missing: {fileName}.", path));
            }
            return (new CheckedScriptArtifact
            {
                Kind = kind,
                Path = path,
                Status = required ? "Missing" : "NotApplicable"
            }, null, path);
        }

        try
        {
            var json = JsonNode.Parse(File.ReadAllText(path, Encoding.UTF8)) as JsonObject;
            if (json is null)
            {
                diagnostics.Add(Error("Script.Artifact.InvalidJson", $"{fileName} must be a JSON object.", path));
                return (new CheckedScriptArtifact { Kind = kind, Path = path, Status = "Failed" }, null, path);
            }
            ValidateEnvelope(json, fileName, path, diagnostics);
            ValidateSourceHashes(json, fileName, path, diagnostics);
            var status = ReadString(json, "status") == "Failed" ? "Failed" : "Passed";
            return (new CheckedScriptArtifact { Kind = kind, Path = path, Status = status }, json, path);
        }
        catch (Exception ex) when (ex is IOException or System.Text.Json.JsonException)
        {
            diagnostics.Add(Error("Script.Artifact.InvalidJson", $"Unable to read {fileName}: {ex.Message}", path));
            return (new CheckedScriptArtifact { Kind = kind, Path = path, Status = "Failed" }, null, path);
        }
    }

    private static void ValidateEnvelope(JsonObject json, string fileName, string path, List<SagaDiagnostic> diagnostics)
    {
        foreach (var required in new[] { "schemaVersion", "tool", "command", "status" })
        {
            if (!json.ContainsKey(required))
            {
                diagnostics.Add(Error("Script.Artifact.MissingEnvelopeField", $"{fileName} is missing envelope field '{required}'.", path));
            }
        }
    }

    private static void ValidateSourceHashes(JsonObject json, string fileName, string path, List<SagaDiagnostic> diagnostics)
    {
        if (json.ContainsKey("sourceFile") && string.IsNullOrWhiteSpace(ReadString(json, "sourceHash")) &&
            string.IsNullOrWhiteSpace(ReadString(json, "baseSourceHash")))
        {
            diagnostics.Add(Error("Script.Artifact.SourceHashMissing", $"{fileName} exposes a source file without a source hash.", path));
        }
        if (json["sourceFiles"] is JsonArray sourceFiles)
        {
            foreach (var sourceFile in sourceFiles.OfType<JsonObject>())
            {
                if (string.IsNullOrWhiteSpace(ReadString(sourceFile, "sourceHash")))
                {
                    diagnostics.Add(Error("Script.Artifact.SourceHashMissing", $"{fileName} sourceFiles entry is missing sourceHash.", path));
                }
            }
        }
    }

    private static void ValidateNodeCapabilityConsistency(JsonObject nodeMetadata, JsonObject runtimeBindings, List<SagaDiagnostic> diagnostics)
    {
        var metadata = (nodeMetadata["nodes"] as JsonArray)?.OfType<JsonObject>()
            .Where(node => !string.IsNullOrWhiteSpace(ReadString(node, "nodeId")))
            .ToDictionary(node => ReadString(node, "nodeId"), node => ReadString(node, "capability"), StringComparer.Ordinal)
            ?? new Dictionary<string, string>(StringComparer.Ordinal);
        var bindings = runtimeBindings["bindings"] as JsonArray;
        if (bindings is null)
        {
            return;
        }
        foreach (var binding in bindings.OfType<JsonObject>())
        {
            var nodes = binding["nodes"] as JsonArray;
            if (nodes is null)
            {
                continue;
            }
            foreach (var node in nodes.OfType<JsonObject>())
            {
                var nodeId = ReadString(node, "nodeId");
                if (metadata.TryGetValue(nodeId, out var expectedCapability) &&
                    expectedCapability != ReadString(node, "capability"))
                {
                    diagnostics.Add(Error("Script.Artifact.NodeCapabilityMismatch", $"Runtime binding node '{nodeId}' capability does not match node metadata."));
                }
            }
        }
    }

    private static RuntimeProofSummary SummarizeRuntimeProof(JsonObject? runtimeBindings, List<SagaDiagnostic> diagnostics, string path)
    {
        var runtimeBackedWithEvidence = 0;
        var runtimeBackedMissingEvidence = 0;
        var projectionOnly = 0;
        var deferred = 0;
        var bindings = runtimeBindings?["bindings"] as JsonArray;
        if (bindings is not null)
        {
            foreach (var binding in bindings.OfType<JsonObject>())
            {
                var nodes = binding["nodes"] as JsonArray;
                if (nodes is null)
                {
                    continue;
                }
                foreach (var node in nodes.OfType<JsonObject>())
                {
                    switch (ReadString(node, "runtimeProof"))
                    {
                        case "RuntimeBackedWithEvidence":
                            runtimeBackedWithEvidence++;
                            break;
                        case "RuntimeBackedWithEvidenceMissing":
                            runtimeBackedMissingEvidence++;
                            diagnostics.Add(Error("Script.Artifact.RuntimeProofMissing", "RuntimeBacked node is missing explicit runtime evidence.", path));
                            break;
                        case "Deferred":
                            deferred++;
                            break;
                        default:
                            projectionOnly++;
                            break;
                    }
                }
            }
        }
        return new RuntimeProofSummary
        {
            RuntimeBackedWithEvidence = runtimeBackedWithEvidence,
            RuntimeBackedMissingEvidence = runtimeBackedMissingEvidence,
            ProjectionOnly = projectionOnly,
            Deferred = deferred
        };
    }
}
