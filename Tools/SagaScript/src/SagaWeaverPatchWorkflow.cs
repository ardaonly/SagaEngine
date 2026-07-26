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
    public static JsonObject BuildPatchEvaluation(
        IReadOnlyList<string> sourceFiles,
        string sourceMapPath,
        string patchRequestPath)
    {
        var diagnostics = new List<SagaDiagnostic>();
        var sourceMap = JsonNode.Parse(File.ReadAllText(sourceMapPath)) as JsonObject
            ?? throw new InvalidOperationException("source-map must be a JSON object.");
        var request = JsonNode.Parse(File.ReadAllText(patchRequestPath)) as JsonObject
            ?? throw new InvalidOperationException("patch request must be a JSON object.");

        var operation = ReadString(request, "operation");
        var nodeId = ReadString(request, "nodeId");
        var baseSourceHash = ReadString(request, "baseSourceHash");
        var replacement = ReadString(request, "replacement");

        if (operation != "ReplaceStringLiteral")
        {
            diagnostics.Add(Error("Script.Patch.UnsupportedOperation", $"Unsupported patch operation '{operation}'."));
        }

        var node = FindNode(sourceMap, nodeId);
        var status = "Passed";
        var sourceFile = "";
        var oldText = "";
        SourceSpan? span = null;
        if (node is null)
        {
            diagnostics.Add(Error("Script.Patch.NodeMissing", $"Patch target node was not found: {nodeId}."));
        }
        else
        {
            var kind = ReadString(node, "kind");
            if (kind != "StringLiteral")
            {
                diagnostics.Add(Error("Script.Patch.UnsupportedNode", "ReplaceStringLiteral requires a StringLiteral source-map node."));
            }
            if (ReadBool(node, "readOnly"))
            {
                diagnostics.Add(Error("Script.Patch.ReadOnlyNode", "Patch target is read-only or opaque."));
            }

            sourceFile = FindBehaviorSourceFile(sourceMap, nodeId);
            span = ReadSpan(node["sourceSpan"] as JsonObject);
            if (sourceFile.Length == 0 || span is null)
            {
                diagnostics.Add(Error("Script.Patch.SourceSpanMissing", "Patch target does not include a source span."));
            }
            else
            {
                var fullSource = sourceFiles
                    .Select(Path.GetFullPath)
                    .FirstOrDefault(path => string.Equals(path, sourceFile, StringComparison.Ordinal)) ?? sourceFile;
                var sourceText = File.ReadAllText(fullSource, Encoding.UTF8);
                var actualHash = ComputeHash(sourceText);
                if (!actualHash.Equals(baseSourceHash, StringComparison.OrdinalIgnoreCase))
                {
                    diagnostics.Add(Error("Script.Patch.SourceHashMismatch", "Patch base source hash does not match current source."));
                }
                if (span.StartByte >= 0 && span.EndByte <= Encoding.UTF8.GetByteCount(sourceText) && span.EndByte >= span.StartByte)
                {
                    oldText = SliceUtf8(sourceText, span.StartByte, span.EndByte);
                    if (!oldText.StartsWith("\"", StringComparison.Ordinal) || !oldText.EndsWith("\"", StringComparison.Ordinal))
                    {
                        diagnostics.Add(Error("Script.Patch.TargetNotStringLiteral", "Patch target span is not a string literal."));
                    }
                }
            }
        }

        var summary = SagaScriptAnalyzer.BuildSummary(SortDiagnostics(diagnostics).ToArray());
        if (summary.HasBlockingDiagnostics)
        {
            status = "Failed";
        }

        var payload = new JsonObject
        {
            ["schemaVersion"] = 1,
            ["tool"] = ToolInfo.Name,
            ["command"] = "patch-evaluation",
            ["status"] = status,
            ["operation"] = operation,
            ["nodeId"] = nodeId,
            ["sourceFile"] = sourceFile,
            ["baseSourceHash"] = baseSourceHash,
            ["mutatesSource"] = false,
            ["evaluation"] = status == "Passed" && span is not null
                ? new JsonObject
                {
                    ["startByte"] = span.StartByte,
                    ["endByte"] = span.EndByte,
                    ["oldText"] = oldText,
                    ["newText"] = "\"" + replacement + "\""
                }
                : null,
            ["summary"] = JsonSerializerNode.Summary(summary),
            ["diagnostics"] = JsonSerializerNode.Diagnostics(SortDiagnostics(diagnostics))
        };
        return payload;
    }

    public static JsonObject BuildPatchApply(
        IReadOnlyList<string> sourceFiles,
        IReadOnlyList<string> sourceInputs,
        string sourceMapPath,
        string patchRequestPath)
    {
        var diagnostics = new List<SagaDiagnostic>();
        var sourceMap = ReadJsonObjectFile(sourceMapPath, "source-map", diagnostics);
        var request = ReadJsonObjectFile(patchRequestPath, "patch request", diagnostics);

        var operation = ReadString(request, "operation");
        var nodeId = ReadString(request, "nodeId");
        var baseSourceHash = ReadString(request, "baseSourceHash");
        var expectedOldText = ReadString(request, "expectedOldText");
        var replacement = ReadString(request, "replacement");
        var operationId = BuildOperationId(operation, nodeId, baseSourceHash, expectedOldText, replacement);
        var newText = "\"" + replacement + "\"";
        var sourceFile = "";
        var backupPath = "";
        var tempPath = "";
        var rollbackStatus = "NotNeeded";
        var oldText = "";
        var newSourceHash = "";
        SourceSpan? span = null;

        if (request is not null)
        {
            foreach (var required in new[] { "operation", "nodeId", "baseSourceHash", "expectedOldText", "replacement" })
            {
                if (!HasStringField(request, required))
                {
                    diagnostics.Add(Error("Script.Patch.MissingField", $"patch-apply requires '{required}'."));
                }
            }
            if (request.TryGetPropertyValue("operations", out _))
            {
                diagnostics.Add(Error("Script.Patch.MultiTargetUnsupported", "patch-apply accepts exactly one target operation."));
            }
        }

        if (operation != "ReplaceStringLiteral")
        {
            diagnostics.Add(Error("Script.Patch.UnsupportedOperation", $"Unsupported patch operation '{operation}'."));
        }

        JsonObject? node = null;
        if (sourceMap is not null)
        {
            var matches = FindNodeMatches(sourceMap, nodeId).ToArray();
            if (matches.Length == 0)
            {
                diagnostics.Add(Error("Script.Patch.NodeMissing", $"Patch target node was not found: {nodeId}."));
            }
            else if (matches.Length > 1)
            {
                diagnostics.Add(Error("Script.Patch.MultiTargetUnsupported", $"Patch target node id is ambiguous: {nodeId}."));
            }
            else
            {
                node = matches[0].Node;
                sourceFile = matches[0].SourceFile;
            }
        }

        byte[] originalBytes = [];
        byte[] replacementBytes = Encoding.UTF8.GetBytes(newText);
        if (node is not null)
        {
            ValidatePatchNode(node, diagnostics);
            span = ReadSpan(node["sourceSpan"] as JsonObject);
            if (sourceFile.Length == 0 || span is null)
            {
                diagnostics.Add(Error("Script.Patch.SourceSpanMissing", "Patch target does not include a source span.", sourceFile));
            }
            else
            {
                var resolvedSource = ResolvePatchSourceFile(sourceFiles, sourceInputs, sourceFile);
                if (resolvedSource.Length == 0)
                {
                    diagnostics.Add(Error("Script.Patch.SourceFileOutsideInput", "Patch target source file is not part of the supplied --source inputs.", sourceFile));
                }
                else if (!File.Exists(resolvedSource))
                {
                    diagnostics.Add(Error("Script.Patch.SourceFileMissing", "Patch target source file does not exist.", sourceFile));
                }
                else
                {
                    sourceFile = resolvedSource;
                    originalBytes = File.ReadAllBytes(sourceFile);
                    var sourceText = File.ReadAllText(sourceFile, Encoding.UTF8);
                    var actualHash = ComputeHash(sourceText);
                    if (!actualHash.Equals(baseSourceHash, StringComparison.OrdinalIgnoreCase))
                    {
                        diagnostics.Add(Error("Script.Patch.SourceHashMismatch", "Patch base source hash does not match current source.", sourceFile));
                    }

                    if (!IsValidByteSpan(span, originalBytes.Length))
                    {
                        diagnostics.Add(Error("Script.Patch.InvalidSourceSpan", "Patch target source span has invalid UTF-8 byte bounds.", sourceFile));
                    }
                    else
                    {
                        try
                        {
                            oldText = DecodeUtf8(originalBytes, span.StartByte, span.EndByte - span.StartByte);
                            if (oldText != expectedOldText)
                            {
                                diagnostics.Add(Error("Script.Patch.ExpectedOldTextMismatch", "Patch expectedOldText does not match current source span.", sourceFile));
                            }
                            if (!IsQuotedStringLiteral(oldText))
                            {
                                diagnostics.Add(Error("Script.Patch.TargetNotStringLiteral", "Patch target span is not a string literal.", sourceFile));
                            }
                        }
                        catch (DecoderFallbackException)
                        {
                            diagnostics.Add(Error("Script.Patch.InvalidSourceSpan", "Patch target source span is not valid UTF-8.", sourceFile));
                        }
                    }
                }
            }
        }

        if (!IsQuotedStringLiteral(expectedOldText))
        {
            diagnostics.Add(Error("Script.Patch.ExpectedOldTextInvalid", "patch-apply expectedOldText must be a quoted string literal."));
        }
        if (!IsQuotedStringLiteral(newText))
        {
            diagnostics.Add(Error("Script.Patch.ReplacementInvalid", "patch-apply replacement must form a quoted string literal."));
        }

        var sortedDiagnostics = SortDiagnostics(diagnostics).ToArray();
        var summary = SagaScriptAnalyzer.BuildSummary(sortedDiagnostics);
        var status = summary.HasBlockingDiagnostics ? "Failed" : "Passed";

        if (status == "Passed" && span is not null)
        {
            backupPath = Path.Combine(
                Path.GetDirectoryName(sourceFile) ?? "",
                $"{Path.GetFileName(sourceFile)}.saga-backup.{operationId}");
            tempPath = Path.Combine(
                Path.GetDirectoryName(sourceFile) ?? "",
                $"{Path.GetFileName(sourceFile)}.saga-tmp.{operationId}");

            try
            {
                var patchedBytes = ReplaceBytes(originalBytes, span.StartByte, span.EndByte, replacementBytes);
                File.Copy(sourceFile, backupPath, overwrite: true);
                File.WriteAllBytes(tempPath, patchedBytes);
                File.Move(tempPath, sourceFile, overwrite: true);

                var afterBytes = File.ReadAllBytes(sourceFile);
                var forcePostApplyFailure = ReadBool(request, "testOnlyForcePostApplyVerificationFailure");
                if (forcePostApplyFailure || !VerifySingleSpanReplacement(originalBytes, afterBytes, span.StartByte, span.EndByte, replacementBytes))
                {
                    diagnostics.Add(Error("Script.Patch.PostApplyVerificationFailed", "Patch apply post-check failed; attempting rollback.", sourceFile));
                    rollbackStatus = RestoreBackup(backupPath, sourceFile) ? "Restored" : "Failed";
                    status = "Failed";
                }
                else
                {
                    newSourceHash = ComputeHash(File.ReadAllText(sourceFile, Encoding.UTF8));
                }
            }
            catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
            {
                diagnostics.Add(Error("Script.Patch.WriteFailed", $"Patch apply write failed: {ex.Message}", sourceFile));
                TryDeleteFile(tempPath);
                if (File.Exists(backupPath) && SourceBytesChanged(sourceFile, originalBytes))
                {
                    rollbackStatus = RestoreBackup(backupPath, sourceFile) ? "Restored" : "Failed";
                }
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
            ["command"] = "patch-apply",
            ["status"] = status,
            ["diagnostics"] = JsonSerializerNode.Diagnostics(sortedDiagnostics),
            ["summary"] = JsonSerializerNode.Summary(summary),
            ["operation"] = operation,
            ["operationId"] = operationId,
            ["nodeId"] = nodeId,
            ["sourceFile"] = sourceFile,
            ["baseSourceHash"] = baseSourceHash,
            ["newSourceHash"] = newSourceHash,
            ["sourceSpan"] = span is null ? null : SpanJson(span),
            ["oldText"] = oldText,
            ["newText"] = newText,
            ["backupPath"] = backupPath,
            ["tempPath"] = tempPath,
            ["rollbackStatus"] = rollbackStatus,
            ["mutatesSource"] = true,
            ["staleArtifacts"] = StaleArtifactArray(),
            ["artifactRegeneration"] = "NotPerformed"
        };
    }

    public static JsonObject BuildPatchDiff(
        IReadOnlyList<string> sourceFiles,
        IReadOnlyList<string> sourceInputs,
        string sourceMapPath,
        string patchRequestPath)
    {
        var diagnostics = new List<SagaDiagnostic>();
        var sourceMap = ReadJsonObjectFile(sourceMapPath, "source-map", diagnostics);
        var request = ReadJsonObjectFile(patchRequestPath, "patch request", diagnostics);

        var operation = ReadString(request, "operation");
        var nodeId = ReadString(request, "nodeId");
        var baseSourceHash = ReadString(request, "baseSourceHash");
        var expectedOldText = ReadString(request, "expectedOldText");
        var replacement = ReadString(request, "replacement");
        var operationId = BuildOperationId(operation, nodeId, baseSourceHash, expectedOldText, replacement);
        var newText = "\"" + replacement + "\"";
        var sourceFile = "";
        var oldText = "";
        var proposedSourceHash = "";
        SourceSpan? span = null;
        byte[] originalBytes = [];
        byte[] replacementBytes = Encoding.UTF8.GetBytes(newText);
        byte[] proposedBytes = [];
        var originalText = "";
        var proposedText = "";

        if (request is not null)
        {
            foreach (var required in new[] { "operation", "nodeId", "baseSourceHash", "expectedOldText", "replacement" })
            {
                if (!HasStringField(request, required))
                {
                    diagnostics.Add(Error("Script.Patch.MissingField", $"patch-diff requires '{required}'."));
                }
            }
            if (request.TryGetPropertyValue("operations", out _))
            {
                diagnostics.Add(Error("Script.Patch.MultiTargetUnsupported", "patch-diff accepts exactly one target operation."));
            }
        }

        if (operation != "ReplaceStringLiteral")
        {
            diagnostics.Add(Error("Script.Patch.UnsupportedOperation", $"Unsupported patch operation '{operation}'."));
        }

        JsonObject? node = null;
        if (sourceMap is not null)
        {
            var matches = FindNodeMatches(sourceMap, nodeId).ToArray();
            if (matches.Length == 0)
            {
                diagnostics.Add(Error("Script.Patch.NodeMissing", $"Patch target node was not found: {nodeId}."));
            }
            else if (matches.Length > 1)
            {
                diagnostics.Add(Error("Script.Patch.MultiTargetUnsupported", $"Patch target node id is ambiguous: {nodeId}."));
            }
            else
            {
                node = matches[0].Node;
                sourceFile = matches[0].SourceFile;
            }
        }

        if (node is not null)
        {
            ValidatePatchNode(node, diagnostics);
            span = ReadSpan(node["sourceSpan"] as JsonObject);
            if (sourceFile.Length == 0 || span is null)
            {
                diagnostics.Add(Error("Script.Patch.SourceSpanMissing", "Patch target does not include a source span.", sourceFile));
            }
            else
            {
                var resolvedSource = ResolvePatchSourceFile(sourceFiles, sourceInputs, sourceFile);
                if (resolvedSource.Length == 0)
                {
                    diagnostics.Add(Error("Script.Patch.SourceFileOutsideInput", "Patch target source file is not part of the supplied --source inputs.", sourceFile));
                }
                else if (!File.Exists(resolvedSource))
                {
                    diagnostics.Add(Error("Script.Patch.SourceFileMissing", "Patch target source file does not exist.", sourceFile));
                }
                else
                {
                    sourceFile = resolvedSource;
                    originalBytes = File.ReadAllBytes(sourceFile);
                    originalText = File.ReadAllText(sourceFile, Encoding.UTF8);
                    var actualHash = ComputeHash(originalText);
                    if (!actualHash.Equals(baseSourceHash, StringComparison.OrdinalIgnoreCase))
                    {
                        diagnostics.Add(Error("Script.Patch.SourceHashMismatch", "Patch base source hash does not match current source.", sourceFile));
                    }

                    if (!IsValidByteSpan(span, originalBytes.Length))
                    {
                        diagnostics.Add(Error("Script.Patch.InvalidSourceSpan", "Patch target source span has invalid UTF-8 byte bounds.", sourceFile));
                    }
                    else
                    {
                        try
                        {
                            oldText = DecodeUtf8(originalBytes, span.StartByte, span.EndByte - span.StartByte);
                            if (oldText != expectedOldText)
                            {
                                diagnostics.Add(Error("Script.Patch.ExpectedOldTextMismatch", "Patch expectedOldText does not match current source span.", sourceFile));
                            }
                            if (!IsQuotedStringLiteral(oldText))
                            {
                                diagnostics.Add(Error("Script.Patch.TargetNotStringLiteral", "Patch target span is not a string literal.", sourceFile));
                            }
                            proposedBytes = ReplaceBytes(originalBytes, span.StartByte, span.EndByte, replacementBytes);
                            proposedText = DecodeUtf8(proposedBytes, 0, proposedBytes.Length);
                            proposedSourceHash = ComputeHash(proposedText);
                        }
                        catch (DecoderFallbackException)
                        {
                            diagnostics.Add(Error("Script.Patch.InvalidSourceSpan", "Patch target source span is not valid UTF-8.", sourceFile));
                        }
                    }
                }
            }
        }

        if (!IsQuotedStringLiteral(expectedOldText))
        {
            diagnostics.Add(Error("Script.Patch.ExpectedOldTextInvalid", "patch-diff expectedOldText must be a quoted string literal."));
        }
        if (!IsQuotedStringLiteral(newText))
        {
            diagnostics.Add(Error("Script.Patch.ReplacementInvalid", "patch-diff replacement must form a quoted string literal."));
        }

        var sortedDiagnostics = SortDiagnostics(diagnostics).ToArray();
        var summary = SagaScriptAnalyzer.BuildSummary(sortedDiagnostics);
        var status = summary.HasBlockingDiagnostics ? "Failed" : "Passed";
        var newEndByte = span is null ? 0 : span.StartByte + replacementBytes.Length;

        return new JsonObject
        {
            ["schemaVersion"] = 1,
            ["tool"] = ToolInfo.Name,
            ["command"] = "patch-diff",
            ["status"] = status,
            ["summary"] = JsonSerializerNode.Summary(summary),
            ["diagnostics"] = JsonSerializerNode.Diagnostics(sortedDiagnostics),
            ["operation"] = operation,
            ["operationId"] = operationId,
            ["nodeId"] = nodeId,
            ["sourceFile"] = sourceFile,
            ["baseSourceHash"] = baseSourceHash,
            ["proposedSourceHash"] = status == "Passed" ? proposedSourceHash : "",
            ["sourceSpan"] = span is null ? null : SpanJson(span),
            ["oldText"] = oldText,
            ["newText"] = newText,
            ["byteDiff"] = span is null
                ? null
                : new JsonObject
                {
                    ["startByte"] = span.StartByte,
                    ["oldEndByte"] = span.EndByte,
                    ["newEndByte"] = newEndByte,
                    ["oldByteLength"] = span.EndByte - span.StartByte,
                    ["newByteLength"] = replacementBytes.Length
                },
            ["textDiff"] = status == "Passed"
                ? BuildUnifiedDiff(originalText, proposedText)
                : null,
            ["mutatesSource"] = false,
            ["artifactRegeneration"] = "NotPerformed"
        };
    }

    public static JsonObject BuildPatchReview(
        string diffReportPath,
        string decision,
        string reviewer)
    {
        var diagnostics = new List<SagaDiagnostic>();
        var diffReport = ReadJsonObjectFile(diffReportPath, "patch diff report", diagnostics);

        if (decision is not ("Approved" or "Rejected"))
        {
            diagnostics.Add(Error("Script.PatchReview.InvalidDecision", "patch-review decision must be Approved or Rejected."));
        }
        if (string.IsNullOrWhiteSpace(reviewer))
        {
            diagnostics.Add(Error("Script.PatchReview.MissingReviewer", "patch-review requires a reviewer."));
        }

        if (diffReport is not null)
        {
            if (ReadString(diffReport, "command") != "patch-diff")
            {
                diagnostics.Add(Error("Script.PatchReview.InvalidDiffReport", "patch-review requires a patch-diff report."));
            }
            if (ReadString(diffReport, "status") != "Passed")
            {
                diagnostics.Add(Error("Script.PatchReview.FailedDiffReport", "patch-review requires a passed patch-diff report."));
            }
            if (ReadBool(diffReport, "mutatesSource"))
            {
                diagnostics.Add(Error("Script.PatchReview.MutatingDiffReport", "patch-review requires a non-mutating diff report."));
            }
        }

        var sortedDiagnostics = SortDiagnostics(diagnostics).ToArray();
        var summary = SagaScriptAnalyzer.BuildSummary(sortedDiagnostics);
        var status = summary.HasBlockingDiagnostics ? "Failed" : "Passed";

        return new JsonObject
        {
            ["schemaVersion"] = 1,
            ["tool"] = ToolInfo.Name,
            ["command"] = "patch-review",
            ["status"] = status,
            ["summary"] = JsonSerializerNode.Summary(summary),
            ["diagnostics"] = JsonSerializerNode.Diagnostics(sortedDiagnostics),
            ["decision"] = decision,
            ["reviewer"] = reviewer,
            ["reviewedCommand"] = ReadString(diffReport, "command"),
            ["diffReport"] = Path.GetFullPath(diffReportPath),
            ["operation"] = ReadString(diffReport, "operation"),
            ["operationId"] = ReadString(diffReport, "operationId"),
            ["nodeId"] = ReadString(diffReport, "nodeId"),
            ["sourceFile"] = ReadString(diffReport, "sourceFile"),
            ["baseSourceHash"] = ReadString(diffReport, "baseSourceHash"),
            ["proposedSourceHash"] = ReadString(diffReport, "proposedSourceHash"),
            ["mutatesSource"] = false,
            ["appliesPatch"] = false,
            ["artifactRegeneration"] = "NotPerformed"
        };
    }

    public static JsonObject BuildPatchRollback(string applyReportPath)
    {
        var diagnostics = new List<SagaDiagnostic>();
        var applyReport = ReadJsonObjectFile(applyReportPath, "patch apply report", diagnostics);
        var operation = ReadString(applyReport, "operation");
        var operationId = ReadString(applyReport, "operationId");
        var nodeId = ReadString(applyReport, "nodeId");
        var sourceFile = ReadString(applyReport, "sourceFile");
        var backupPath = ReadString(applyReport, "backupPath");
        var baseSourceHash = ReadString(applyReport, "baseSourceHash");
        var newSourceHash = ReadString(applyReport, "newSourceHash");
        var preRollbackSourceHash = "";
        var postRollbackSourceHash = "";
        var rollbackStatus = "NotPerformed";
        var restoresExactPreviousBytes = false;

        if (applyReport is not null)
        {
            if (ReadString(applyReport, "command") != "patch-apply")
            {
                diagnostics.Add(Error("Script.PatchRollback.InvalidApplyReport", "patch-rollback requires a patch-apply report."));
            }
            if (ReadString(applyReport, "status") != "Passed")
            {
                diagnostics.Add(Error("Script.PatchRollback.FailedApplyReport", "patch-rollback requires a passed patch-apply report."));
            }
            if (!ReadBool(applyReport, "mutatesSource"))
            {
                diagnostics.Add(Error("Script.PatchRollback.NonMutatingApplyReport", "patch-rollback requires a mutating patch-apply report."));
            }
            foreach (var required in new[] { "sourceFile", "backupPath", "baseSourceHash", "newSourceHash" })
            {
                if (!HasStringField(applyReport, required))
                {
                    diagnostics.Add(Error("Script.PatchRollback.MissingField", $"patch-rollback requires apply report field '{required}'."));
                }
            }
        }

        byte[] currentBytes = [];
        byte[] backupBytes = [];
        if (!string.IsNullOrWhiteSpace(sourceFile))
        {
            sourceFile = Path.GetFullPath(sourceFile);
            if (!File.Exists(sourceFile))
            {
                diagnostics.Add(Error("Script.PatchRollback.SourceFileMissing", "Patch rollback source file does not exist.", sourceFile));
            }
            else
            {
                currentBytes = File.ReadAllBytes(sourceFile);
                try
                {
                    preRollbackSourceHash = ComputeHash(DecodeUtf8(currentBytes, 0, currentBytes.Length));
                    if (!preRollbackSourceHash.Equals(newSourceHash, StringComparison.OrdinalIgnoreCase))
                    {
                        diagnostics.Add(Error("Script.PatchRollback.SourceHashMismatch", "Current source hash does not match patch_apply_report.newSourceHash.", sourceFile));
                    }
                }
                catch (DecoderFallbackException)
                {
                    diagnostics.Add(Error("Script.PatchRollback.InvalidSourceEncoding", "Current source is not valid UTF-8.", sourceFile));
                }
            }
        }

        if (!string.IsNullOrWhiteSpace(backupPath))
        {
            backupPath = Path.GetFullPath(backupPath);
            if (!File.Exists(backupPath))
            {
                diagnostics.Add(Error("Script.PatchRollback.BackupMissing", "Patch rollback backup file does not exist.", backupPath));
            }
            else
            {
                backupBytes = File.ReadAllBytes(backupPath);
                try
                {
                    var backupHash = ComputeHash(DecodeUtf8(backupBytes, 0, backupBytes.Length));
                    if (!backupHash.Equals(baseSourceHash, StringComparison.OrdinalIgnoreCase))
                    {
                        diagnostics.Add(Error("Script.PatchRollback.BackupHashMismatch", "Backup hash does not match patch_apply_report.baseSourceHash.", backupPath));
                    }
                }
                catch (DecoderFallbackException)
                {
                    diagnostics.Add(Error("Script.PatchRollback.InvalidBackupEncoding", "Patch rollback backup is not valid UTF-8.", backupPath));
                }
            }
        }

        var sortedDiagnostics = SortDiagnostics(diagnostics).ToArray();
        var summary = SagaScriptAnalyzer.BuildSummary(sortedDiagnostics);
        var status = summary.HasBlockingDiagnostics ? "Failed" : "Passed";

        if (status == "Passed")
        {
            var tempPath = Path.Combine(
                Path.GetDirectoryName(sourceFile) ?? "",
                $"{Path.GetFileName(sourceFile)}.saga-rollback-tmp.{operationId}");
            var safeguardPath = Path.Combine(
                Path.GetDirectoryName(sourceFile) ?? "",
                $"{Path.GetFileName(sourceFile)}.saga-rollback-safeguard.{operationId}");

            try
            {
                ReplaceFileWithBytes(sourceFile, tempPath, backupBytes);
                postRollbackSourceHash = ComputeHash(File.ReadAllText(sourceFile, Encoding.UTF8));
                var forcePostRollbackFailure = ReadBool(applyReport, "testOnlyForceRollbackVerificationFailure");
                if (forcePostRollbackFailure ||
                    !postRollbackSourceHash.Equals(baseSourceHash, StringComparison.OrdinalIgnoreCase) ||
                    !File.ReadAllBytes(sourceFile).SequenceEqual(backupBytes))
                {
                    diagnostics.Add(Error("Script.PatchRollback.PostRollbackVerificationFailed", "Patch rollback post-check failed; restoring pre-rollback source.", sourceFile));
                    ReplaceFileWithBytes(sourceFile, safeguardPath, currentBytes);
                    rollbackStatus = "Failed";
                    status = "Failed";
                }
                else
                {
                    rollbackStatus = "Restored";
                    restoresExactPreviousBytes = true;
                }
            }
            catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
            {
                diagnostics.Add(Error("Script.PatchRollback.WriteFailed", $"Patch rollback write failed: {ex.Message}", sourceFile));
                TryDeleteFile(tempPath);
                try
                {
                    if (File.Exists(sourceFile) && !File.ReadAllBytes(sourceFile).SequenceEqual(currentBytes))
                    {
                        ReplaceFileWithBytes(sourceFile, safeguardPath, currentBytes);
                    }
                }
                catch (Exception restoreEx) when (restoreEx is IOException or UnauthorizedAccessException)
                {
                    diagnostics.Add(Error("Script.PatchRollback.RestoreCurrentFailed", $"Patch rollback could not restore current source: {restoreEx.Message}", sourceFile));
                }
                rollbackStatus = "Failed";
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
            ["command"] = "patch-rollback",
            ["status"] = status,
            ["summary"] = JsonSerializerNode.Summary(summary),
            ["diagnostics"] = JsonSerializerNode.Diagnostics(sortedDiagnostics),
            ["applyReport"] = Path.GetFullPath(applyReportPath),
            ["operation"] = operation,
            ["operationId"] = operationId,
            ["nodeId"] = nodeId,
            ["sourceFile"] = sourceFile,
            ["backupPath"] = backupPath,
            ["baseSourceHash"] = baseSourceHash,
            ["preRollbackSourceHash"] = preRollbackSourceHash,
            ["postRollbackSourceHash"] = postRollbackSourceHash,
            ["rollbackStatus"] = rollbackStatus,
            ["mutatesSource"] = true,
            ["restoresExactPreviousBytes"] = restoresExactPreviousBytes,
            ["artifactRegeneration"] = "NotPerformed",
            ["staleArtifacts"] = StaleArtifactArray()
        };
    }
}
