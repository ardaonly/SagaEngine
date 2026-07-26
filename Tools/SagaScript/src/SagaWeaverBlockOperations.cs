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
    public static JsonObject BuildBlockEditEvaluation(string projectionPath, string operationPath)
    {
        var diagnostics = new List<SagaDiagnostic>();
        var projection = ReadBlockOperationJsonObjectFile(projectionPath, "visual blocks projection", diagnostics);
        var operation = ReadBlockOperationJsonObjectFile(operationPath, "block operation", diagnostics);

        var operationKind = ReadString(operation, "operationKind");
        var targetBlockId = ReadString(operation, "targetBlockId");
        var requestedValue = ReadString(operation, "requestedValue");
        var suppliedOperationId = ReadString(operation, "operationId");
        var operationId = string.IsNullOrWhiteSpace(suppliedOperationId)
            ? BuildBlockOperationId(operationKind, targetBlockId, requestedValue)
            : suppliedOperationId;

        if (operation is not null)
        {
            foreach (var required in new[] { "operationKind", "targetBlockId", "requestedValue" })
            {
                if (!HasStringField(operation, required))
                {
                    diagnostics.Add(BlockOperationError("Script.BlockOperation.MissingField", $"plan-block-edit requires '{required}'."));
                }
            }
        }

        if (!string.IsNullOrWhiteSpace(operationKind))
        {
            if (ForbiddenBlockOperationKinds.Contains(operationKind))
            {
                diagnostics.Add(BlockOperationError("Script.BlockOperation.ForbiddenOperation", $"Block operation '{operationKind}' is forbidden by the block-operation contract."));
            }
            else if (!BlockOperationKinds.Contains(operationKind))
            {
                diagnostics.Add(BlockOperationError("Script.BlockOperation.UnsupportedOperation", $"Block operation '{operationKind}' is not part of the block-operation contract."));
            }
            else if (operationKind != "StringLiteralEdit")
            {
                diagnostics.Add(BlockOperationError("Script.BlockOperation.NotImplementedForEvaluation", $"Block operation '{operationKind}' is contract-known but not evaluationable."));
            }
        }

        JsonObject? targetBlock = null;
        JsonObject? targetOpaqueRegion = null;
        if (projection is not null && !string.IsNullOrWhiteSpace(targetBlockId))
        {
            targetBlock = FindVisualBlock(projection, targetBlockId);
            targetOpaqueRegion = FindVisualOpaqueRegion(projection, targetBlockId);
            if (targetBlock is null && targetOpaqueRegion is null)
            {
                diagnostics.Add(BlockOperationError("Script.BlockOperation.TargetMissing", $"Target block was not found: {targetBlockId}."));
            }
        }

        var target = targetBlock ?? targetOpaqueRegion;
        var sourceFile = ReadString(target, "sourceFile");
        var classification = ReadString(target, "classification");
        var blockKind = ReadString(target, "blockKind");
        var sourceHash = ReadString(target, "sourceHash");
        var span = ReadSpan(target?["sourceSpan"] as JsonObject);
        JsonObject? patchEvaluation = null;

        if (target is not null)
        {
            if (targetOpaqueRegion is not null || classification == "Opaque" || blockKind == "OpaqueSourceRegionBlock")
            {
                diagnostics.Add(BlockOperationError("Script.BlockOperation.OpaqueRegionReadOnly", "Opaque source regions are not editable by block operations.", sourceFile));
            }
            if (classification is "Unsupported" or "Invalid" || blockKind == "UnsupportedDiagnosticBlock")
            {
                diagnostics.Add(BlockOperationError("Script.BlockOperation.UnsupportedDiagnosticReadOnly", "Unsupported diagnostic blocks are not editable by block operations.", sourceFile));
            }
            if (classification != "EditableByPatch")
            {
                diagnostics.Add(BlockOperationError("Script.BlockOperation.NotPatchCapable", $"Target classification '{classification}' is not patch-capable.", sourceFile));
            }
            if (span is null)
            {
                diagnostics.Add(BlockOperationError("Script.BlockOperation.SourceSpanMissing", "Target block does not include a source span.", sourceFile));
            }
            if (string.IsNullOrWhiteSpace(sourceHash))
            {
                diagnostics.Add(BlockOperationError("Script.BlockOperation.SourceHashMissing", "Target block does not include a source hash.", sourceFile));
            }
        }

        var replacementText = "";
        if (operationKind == "StringLiteralEdit")
        {
            replacementText = JsonSerializer.Serialize(requestedValue);
            if (!IsQuotedStringLiteral(replacementText))
            {
                diagnostics.Add(BlockOperationError("Script.BlockOperation.InvalidReplacement", "Requested value cannot be represented as a C# string literal.", sourceFile));
            }
        }

        var sortedDiagnostics = SortDiagnostics(diagnostics).ToArray();
        var summary = SagaScriptAnalyzer.BuildSummary(sortedDiagnostics);
        var status = summary.HasBlockingDiagnostics ? "Failed" : "Passed";
        if (status == "Passed" && span is not null)
        {
            patchEvaluation = new JsonObject
            {
                ["mutatesSource"] = false,
                ["replacementKind"] = "MinimalSpanReplacement",
                ["startByte"] = span.StartByte,
                ["endByte"] = span.EndByte,
                ["replacementText"] = replacementText
            };
        }

        return new JsonObject
        {
            ["schemaVersion"] = 1,
            ["tool"] = ToolInfo.Name,
            ["command"] = "plan-block-edit",
            ["sourceFile"] = sourceFile,
            ["operationId"] = operationId,
            ["operationKind"] = operationKind,
            ["targetBlockId"] = targetBlockId,
            ["targetSourceHash"] = sourceHash,
            ["targetSourceSpan"] = span is null ? null : SpanJson(span),
            ["requestedValue"] = requestedValue,
            ["status"] = status,
            ["patchEvaluation"] = patchEvaluation,
            ["diagnostics"] = JsonSerializerNode.Diagnostics(sortedDiagnostics),
            ["summary"] = JsonSerializerNode.Summary(summary),
            ["sourcePreservation"] = "SourceNotMutated",
            ["mutatesSource"] = false,
            ["nonClaims"] = BlockOperationNonClaims()
        };
    }

    public static JsonObject BuildBlockEditApply(
        string evaluationPath,
        string sourceRoot,
        string outputDirectory)
    {
        var diagnostics = new List<SagaDiagnostic>();
        var evaluation = ReadBlockOperationJsonObjectFile(evaluationPath, "block patch evaluation", diagnostics);

        var sourceFile = ReadString(evaluation, "sourceFile");
        var operationId = ReadString(evaluation, "operationId");
        var operationKind = ReadString(evaluation, "operationKind");
        var targetBlockId = ReadString(evaluation, "targetBlockId");
        var targetSourceHash = ReadString(evaluation, "targetSourceHash");
        var span = ReadSpan(evaluation?["targetSourceSpan"] as JsonObject);
        var patchEvaluation = evaluation?["patchEvaluation"] as JsonObject;
        var replacementText = ReadString(patchEvaluation, "replacementText");
        var resolvedSourceFile = "";
        var patchedSourceFile = "";
        var originalHash = "";
        var patchedHash = "";
        var changedSpanOnly = false;

        if (evaluation is not null)
        {
            foreach (var required in new[] { "command", "sourceFile", "operationId", "operationKind", "targetBlockId", "targetSourceHash" })
            {
                if (!HasStringField(evaluation, required))
                {
                    diagnostics.Add(BlockOperationError("Script.BlockApply.MissingField", $"apply-block-edit requires evaluation field '{required}'."));
                }
            }
            if (ReadString(evaluation, "command") != "plan-block-edit")
            {
                diagnostics.Add(BlockOperationError("Script.BlockApply.InvalidEvaluation", "apply-block-edit requires a plan-block-edit evaluation report.", sourceFile));
            }
            if (ReadString(evaluation, "status") != "Passed")
            {
                diagnostics.Add(BlockOperationError("Script.BlockApply.EvaluationFailed", "apply-block-edit requires a passed block edit evaluation.", sourceFile));
            }
            if (operationKind != "StringLiteralEdit")
            {
                diagnostics.Add(BlockOperationError("Script.BlockApply.UnsupportedOperation", $"apply-block-edit only supports StringLiteralEdit, not '{operationKind}'.", sourceFile));
            }
            if (!HasBoolField(evaluation, "mutatesSource"))
            {
                diagnostics.Add(BlockOperationError("Script.BlockApply.MissingField", "apply-block-edit requires evaluation field 'mutatesSource'.", sourceFile));
            }
            else if (ReadBool(evaluation, "mutatesSource"))
            {
                diagnostics.Add(BlockOperationError("Script.BlockApply.MutatingEvaluation", "apply-block-edit requires a non-mutating evaluation report.", sourceFile));
            }
            if (span is null)
            {
                diagnostics.Add(BlockOperationError("Script.BlockApply.SourceSpanMissing", "apply-block-edit requires targetSourceSpan.", sourceFile));
            }
        }

        if (patchEvaluation is null)
        {
            diagnostics.Add(BlockOperationError("Script.BlockApply.PatchEvaluationMissing", "apply-block-edit requires patchEvaluation metadata.", sourceFile));
        }
        else
        {
            foreach (var required in new[] { "startByte", "endByte" })
            {
                if (!HasIntField(patchEvaluation, required))
                {
                    diagnostics.Add(BlockOperationError("Script.BlockApply.MissingField", $"apply-block-edit requires patchEvaluation field '{required}'.", sourceFile));
                }
            }
            if (!HasStringField(patchEvaluation, "replacementKind"))
            {
                diagnostics.Add(BlockOperationError("Script.BlockApply.MissingField", "apply-block-edit requires patchEvaluation field 'replacementKind'.", sourceFile));
            }
            if (!HasStringField(patchEvaluation, "replacementText"))
            {
                diagnostics.Add(BlockOperationError("Script.BlockApply.MissingField", "apply-block-edit requires patchEvaluation field 'replacementText'.", sourceFile));
            }
            if (!HasBoolField(patchEvaluation, "mutatesSource"))
            {
                diagnostics.Add(BlockOperationError("Script.BlockApply.MissingField", "apply-block-edit requires patchEvaluation field 'mutatesSource'.", sourceFile));
            }
            if (ReadString(patchEvaluation, "replacementKind") != "MinimalSpanReplacement")
            {
                diagnostics.Add(BlockOperationError("Script.BlockApply.UnsupportedReplacement", "apply-block-edit only supports MinimalSpanReplacement.", sourceFile));
            }
            if (ReadBool(patchEvaluation, "mutatesSource"))
            {
                diagnostics.Add(BlockOperationError("Script.BlockApply.MutatingEvaluation", "apply-block-edit requires patchEvaluation.mutatesSource to be false.", sourceFile));
            }
            if (!IsQuotedStringLiteral(replacementText))
            {
                diagnostics.Add(BlockOperationError("Script.BlockApply.InvalidReplacement", "Replacement text must be a valid quoted C# string literal.", sourceFile));
            }
            if (span is not null &&
                (ReadInt(patchEvaluation, "startByte") != span.StartByte ||
                 ReadInt(patchEvaluation, "endByte") != span.EndByte))
            {
                diagnostics.Add(BlockOperationError("Script.BlockApply.SourceSpanMismatch", "patchEvaluation byte bounds must match targetSourceSpan.", sourceFile));
            }
        }

        if (!File.Exists(sourceRoot) && !Directory.Exists(sourceRoot))
        {
            diagnostics.Add(BlockOperationError("Script.BlockApply.SourceRootMissing", $"Source root does not exist: {sourceRoot}."));
        }
        else if (!string.IsNullOrWhiteSpace(sourceFile))
        {
            resolvedSourceFile = ResolveBlockApplySourceFile(sourceRoot, sourceFile);
            if (string.IsNullOrWhiteSpace(resolvedSourceFile))
            {
                diagnostics.Add(BlockOperationError("Script.BlockApply.SourceFileOutsideRoot", "Evaluation source file is outside --source-root.", sourceFile));
            }
            else if (!File.Exists(resolvedSourceFile))
            {
                diagnostics.Add(BlockOperationError("Script.BlockApply.SourceFileMissing", "Evaluation source file does not exist.", resolvedSourceFile));
            }
        }

        byte[] originalBytes = [];
        if (!string.IsNullOrWhiteSpace(resolvedSourceFile) && File.Exists(resolvedSourceFile))
        {
            try
            {
                originalBytes = File.ReadAllBytes(resolvedSourceFile);
                var sourceText = File.ReadAllText(resolvedSourceFile, Encoding.UTF8);
                originalHash = ComputeHash(sourceText);
                if (!string.IsNullOrWhiteSpace(targetSourceHash) &&
                    !originalHash.Equals(targetSourceHash, StringComparison.OrdinalIgnoreCase))
                {
                    diagnostics.Add(BlockOperationError("Script.BlockApply.SourceHashMismatch", "Current source hash does not match block patch evaluation targetSourceHash.", resolvedSourceFile));
                }
                if (span is not null)
                {
                    if (!IsValidByteSpan(span, originalBytes.Length))
                    {
                        diagnostics.Add(BlockOperationError("Script.BlockApply.InvalidSourceSpan", "Target source span has invalid UTF-8 byte bounds.", resolvedSourceFile));
                    }
                    else
                    {
                        var oldText = DecodeUtf8(originalBytes, span.StartByte, span.EndByte - span.StartByte);
                        if (!IsQuotedStringLiteral(oldText))
                        {
                            diagnostics.Add(BlockOperationError("Script.BlockApply.TargetNotStringLiteral", "Target source span is not a string literal.", resolvedSourceFile));
                        }
                    }
                }
            }
            catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or DecoderFallbackException)
            {
                diagnostics.Add(BlockOperationError("Script.BlockApply.SourceReadFailed", $"Unable to read source file: {ex.Message}", resolvedSourceFile));
            }
        }

        var sortedDiagnostics = SortDiagnostics(diagnostics).ToArray();
        var summary = SagaScriptAnalyzer.BuildSummary(sortedDiagnostics);
        var status = summary.HasBlockingDiagnostics ? "Failed" : "Passed";

        if (status == "Passed" && span is not null)
        {
            try
            {
                var replacementBytes = Encoding.UTF8.GetBytes(replacementText);
                var patchedBytes = ReplaceBytes(originalBytes, span.StartByte, span.EndByte, replacementBytes);
                changedSpanOnly = VerifySingleSpanReplacement(originalBytes, patchedBytes, span.StartByte, span.EndByte, replacementBytes);
                if (!changedSpanOnly)
                {
                    diagnostics.Add(BlockOperationError("Script.BlockApply.ChangedSpanVerificationFailed", "Patched copy changed bytes outside the requested span.", resolvedSourceFile));
                    status = "Failed";
                }
                else
                {
                    patchedSourceFile = BuildBlockApplyOutputPath(sourceRoot, resolvedSourceFile, outputDirectory);
                    Directory.CreateDirectory(Path.GetDirectoryName(patchedSourceFile) ?? outputDirectory);
                    File.WriteAllBytes(patchedSourceFile, patchedBytes);
                    patchedHash = ComputeHash(DecodeUtf8(patchedBytes, 0, patchedBytes.Length));
                }
            }
            catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or DecoderFallbackException)
            {
                diagnostics.Add(BlockOperationError("Script.BlockApply.WriteFailed", $"Unable to write patched copy: {ex.Message}", patchedSourceFile));
                status = "Failed";
            }

            sortedDiagnostics = SortDiagnostics(diagnostics).ToArray();
            summary = SagaScriptAnalyzer.BuildSummary(sortedDiagnostics);
            if (summary.HasBlockingDiagnostics)
            {
                status = "Failed";
            }
        }

        return new JsonObject
        {
            ["schemaVersion"] = 1,
            ["tool"] = ToolInfo.Name,
            ["command"] = "apply-block-edit",
            ["operationId"] = operationId,
            ["operationKind"] = operationKind,
            ["targetBlockId"] = targetBlockId,
            ["sourceFile"] = resolvedSourceFile.Length == 0 ? sourceFile : resolvedSourceFile,
            ["patchedSourceFile"] = patchedSourceFile,
            ["targetSourceHash"] = targetSourceHash,
            ["targetSourceSpan"] = span is null ? null : SpanJson(span),
            ["originalHash"] = originalHash,
            ["patchedHash"] = patchedHash,
            ["changedSpanOnly"] = changedSpanOnly,
            ["status"] = status,
            ["diagnostics"] = JsonSerializerNode.Diagnostics(sortedDiagnostics),
            ["summary"] = JsonSerializerNode.Summary(summary),
            ["sourcePreservation"] = status == "Passed" ? "CopiedSourceWithSingleSpanReplacement" : "SourceNotMutated",
            ["mutatesSource"] = false,
            ["nonClaims"] = BlockApplyNonClaims()
        };
    }
}
