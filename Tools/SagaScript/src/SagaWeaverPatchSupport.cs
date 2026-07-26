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
    private static JsonObject? ReadBlockOperationJsonObjectFile(
        string path,
        string label,
        List<SagaDiagnostic> diagnostics)
    {
        try
        {
            if (!File.Exists(path))
            {
                diagnostics.Add(BlockOperationError("Script.BlockOperation.InputMissing", $"Required {label} file does not exist: {path}."));
                return null;
            }

            return JsonNode.Parse(File.ReadAllText(path, Encoding.UTF8)) as JsonObject
                ?? throw new InvalidOperationException($"{label} must be a JSON object.");
        }
        catch (Exception ex) when (ex is IOException or InvalidOperationException or System.Text.Json.JsonException)
        {
            diagnostics.Add(BlockOperationError("Script.BlockOperation.InputInvalid", $"Unable to read {label}: {ex.Message}"));
            return null;
        }
    }

    private static JsonObject? ReadJsonObjectFile(string path, string label, List<SagaDiagnostic> diagnostics)
    {
        try
        {
            if (!File.Exists(path))
            {
                diagnostics.Add(Error("Script.Patch.InputMissing", $"Required {label} file does not exist: {path}."));
                return null;
            }

            return JsonNode.Parse(File.ReadAllText(path, Encoding.UTF8)) as JsonObject
                ?? throw new InvalidOperationException($"{label} must be a JSON object.");
        }
        catch (Exception ex) when (ex is IOException or InvalidOperationException or System.Text.Json.JsonException)
        {
            diagnostics.Add(Error("Script.Patch.InputInvalid", $"Unable to read {label}: {ex.Message}"));
            return null;
        }
    }

    private static string BuildBlockOperationId(string operationKind, string targetBlockId, string requestedValue)
    {
        var material = string.Join('\n', operationKind, targetBlockId, requestedValue);
        return ComputeHash(material)[..16];
    }

    private static JsonObject? FindVisualBlock(JsonObject projection, string blockId)
    {
        var blocks = projection["blocks"] as JsonArray;
        if (blocks is null)
        {
            return null;
        }

        return blocks
            .OfType<JsonObject>()
            .FirstOrDefault(block => ReadString(block, "blockId") == blockId);
    }

    private static JsonObject? FindVisualOpaqueRegion(JsonObject projection, string blockId)
    {
        var regions = projection["opaqueRegions"] as JsonArray;
        if (regions is null)
        {
            return null;
        }

        return regions
            .OfType<JsonObject>()
            .FirstOrDefault(region => ReadString(region, "blockId") == blockId);
    }

    private static JsonArray BlockOperationNonClaims()
    {
        var nonClaims = new JsonArray();
        nonClaims.Add("EvaluationOnly");
        nonClaims.Add("NoSourceMutation");
        nonClaims.Add("NoPatchApply");
        nonClaims.Add("NoVisualBlocksEditor");
        nonClaims.Add("NoBlockEditingUi");
        nonClaims.Add("NoArbitraryCSharpToBlocks");
        nonClaims.Add("NoFullVisualScripting");
        nonClaims.Add("NoRuntimeGraphInterpretation");
        return nonClaims;
    }

    private static JsonArray BlockApplyNonClaims()
    {
        var nonClaims = new JsonArray();
        nonClaims.Add("NoFullVisualScripting");
        nonClaims.Add("NoArbitraryCSharpToBlocks");
        nonClaims.Add("NoEditorUi");
        nonClaims.Add("NoBlockGraphRuntime");
        nonClaims.Add("NoMethodClassRegeneration");
        nonClaims.Add("NoInPlaceMutationByDefault");
        return nonClaims;
    }

    private static bool HasStringField(JsonObject obj, string key)
    {
        return obj.TryGetPropertyValue(key, out var node) &&
            node is JsonValue value &&
            value.TryGetValue<string>(out _);
    }

    private static bool HasIntField(JsonObject obj, string key)
    {
        return obj.TryGetPropertyValue(key, out var node) &&
            node is JsonValue value &&
            value.TryGetValue<int>(out _);
    }

    private static bool HasBoolField(JsonObject obj, string key)
    {
        return obj.TryGetPropertyValue(key, out var node) &&
            node is JsonValue value &&
            value.TryGetValue<bool>(out _);
    }

    private static string BuildOperationId(
        string operation,
        string nodeId,
        string baseSourceHash,
        string expectedOldText,
        string replacement)
    {
        var material = string.Join('\n', operation, nodeId, baseSourceHash, expectedOldText, replacement);
        return ComputeHash(material)[..16];
    }

    private static IEnumerable<(JsonObject Node, string SourceFile)> FindNodeMatches(JsonObject sourceMap, string nodeId)
    {
        var behaviors = sourceMap["behaviors"] as JsonArray;
        if (behaviors is null)
        {
            yield break;
        }

        foreach (var behavior in behaviors.OfType<JsonObject>())
        {
            var sourceFile = ReadString(behavior, "sourceFile");
            var nodes = behavior["nodes"] as JsonArray;
            if (nodes is null)
            {
                continue;
            }

            foreach (var node in nodes.OfType<JsonObject>())
            {
                if (ReadString(node, "nodeId") == nodeId)
                {
                    yield return (node, sourceFile);
                }
            }
        }
    }

    private static void ValidatePatchNode(JsonObject node, List<SagaDiagnostic> diagnostics)
    {
        if (ReadString(node, "kind") != "StringLiteral")
        {
            diagnostics.Add(Error("Script.Patch.UnsupportedNode", "ReplaceStringLiteral requires a StringLiteral source-map node."));
        }
        if (ReadBool(node, "readOnly"))
        {
            diagnostics.Add(Error("Script.Patch.ReadOnlyNode", "Patch target is read-only."));
        }
        if (ReadString(node, "capability") == "Deferred")
        {
            diagnostics.Add(Error("Script.Patch.DeferredNode", "Deferred patch targets are not editable."));
        }

        var projectionCompatibility = ReadString(node, "projectionCompatibility");
        if (projectionCompatibility != "EditableEvaluationOnly")
        {
            diagnostics.Add(Error("Script.Patch.UnsupportedProjectionCompatibility", $"Patch target projection compatibility is '{projectionCompatibility}'."));
        }
        if (!string.IsNullOrWhiteSpace(ReadString(node, "opaqueReason")))
        {
            diagnostics.Add(Error("Script.Patch.OpaqueNode", "Opaque patch targets are not editable."));
        }
    }

    private static string ResolvePatchSourceFile(
        IReadOnlyList<string> sourceFiles,
        IReadOnlyList<string> sourceInputs,
        string sourceFile)
    {
        var fullSource = Path.GetFullPath(sourceFile);
        var discovered = sourceFiles
            .Select(Path.GetFullPath)
            .FirstOrDefault(path => string.Equals(path, fullSource, StringComparison.Ordinal));
        if (!string.IsNullOrWhiteSpace(discovered))
        {
            return discovered;
        }

        return IsUnderAnySourceInput(fullSource, sourceInputs) ? fullSource : "";
    }

    private static bool IsUnderAnySourceInput(string fullSource, IReadOnlyList<string> sourceInputs)
    {
        foreach (var input in sourceInputs)
        {
            var fullInput = Path.GetFullPath(input);
            if (File.Exists(fullInput))
            {
                if (string.Equals(fullInput, fullSource, StringComparison.Ordinal))
                {
                    return true;
                }
                continue;
            }

            var directory = Directory.Exists(fullInput)
                ? fullInput
                : fullInput;
            var normalized = Path.TrimEndingDirectorySeparator(directory) + Path.DirectorySeparatorChar;
            if (fullSource.StartsWith(normalized, StringComparison.Ordinal))
            {
                return true;
            }
        }

        return false;
    }

    private static string ResolveBlockApplySourceFile(string sourceRoot, string sourceFile)
    {
        var fullRoot = Path.GetFullPath(sourceRoot);
        var fullSource = Path.IsPathRooted(sourceFile)
            ? Path.GetFullPath(sourceFile)
            : Path.GetFullPath(Path.Combine(fullRoot, sourceFile));

        if (File.Exists(fullRoot))
        {
            return string.Equals(fullRoot, fullSource, StringComparison.Ordinal)
                ? fullSource
                : "";
        }

        return IsUnderDirectory(fullSource, fullRoot) ? fullSource : "";
    }

    private static string BuildBlockApplyOutputPath(
        string sourceRoot,
        string resolvedSourceFile,
        string outputDirectory)
    {
        var fullRoot = Path.GetFullPath(sourceRoot);
        var fullSource = Path.GetFullPath(resolvedSourceFile);
        var relativePath = File.Exists(fullRoot)
            ? Path.GetFileName(fullSource)
            : Path.GetRelativePath(fullRoot, fullSource);
        if (relativePath.StartsWith("..", StringComparison.Ordinal) ||
            Path.IsPathRooted(relativePath))
        {
            relativePath = Path.GetFileName(fullSource);
        }

        return Path.GetFullPath(Path.Combine(outputDirectory, "patched-source", relativePath));
    }

    private static bool IsUnderDirectory(string fullSource, string directory)
    {
        var fullDirectory = Path.TrimEndingDirectorySeparator(Path.GetFullPath(directory));
        var normalizedDirectory = fullDirectory + Path.DirectorySeparatorChar;
        return fullSource.StartsWith(normalizedDirectory, StringComparison.Ordinal);
    }

    private static bool IsValidByteSpan(SourceSpan span, int byteLength)
    {
        return span.StartByte >= 0 &&
            span.EndByte >= span.StartByte &&
            span.EndByte <= byteLength;
    }

    private static string DecodeUtf8(byte[] bytes, int start, int count)
    {
        return new UTF8Encoding(encoderShouldEmitUTF8Identifier: false, throwOnInvalidBytes: true)
            .GetString(bytes, start, count);
    }

    private static bool IsQuotedStringLiteral(string text)
    {
        if (!text.StartsWith("\"", StringComparison.Ordinal) ||
            !text.EndsWith("\"", StringComparison.Ordinal))
        {
            return false;
        }

        var tree = CSharpSyntaxTree.ParseText("class C { void M() { _ = " + text + "; } }");
        if (tree.GetDiagnostics().Any(diagnostic => diagnostic.Severity == DiagnosticSeverity.Error))
        {
            return false;
        }

        var literal = tree.GetCompilationUnitRoot()
            .DescendantNodes()
            .OfType<LiteralExpressionSyntax>()
            .SingleOrDefault(node => node.IsKind(SyntaxKind.StringLiteralExpression));
        return literal is not null && literal.ToString() == text;
    }

    private static byte[] ReplaceBytes(byte[] originalBytes, int startByte, int endByte, byte[] replacementBytes)
    {
        var prefixLength = startByte;
        var suffixLength = originalBytes.Length - endByte;
        var result = new byte[prefixLength + replacementBytes.Length + suffixLength];
        Buffer.BlockCopy(originalBytes, 0, result, 0, prefixLength);
        Buffer.BlockCopy(replacementBytes, 0, result, prefixLength, replacementBytes.Length);
        Buffer.BlockCopy(originalBytes, endByte, result, prefixLength + replacementBytes.Length, suffixLength);
        return result;
    }

    private static bool VerifySingleSpanReplacement(
        byte[] originalBytes,
        byte[] afterBytes,
        int startByte,
        int endByte,
        byte[] replacementBytes)
    {
        var expectedLength = originalBytes.Length - (endByte - startByte) + replacementBytes.Length;
        if (afterBytes.Length != expectedLength)
        {
            return false;
        }

        return originalBytes.AsSpan(0, startByte).SequenceEqual(afterBytes.AsSpan(0, startByte)) &&
            replacementBytes.AsSpan().SequenceEqual(afterBytes.AsSpan(startByte, replacementBytes.Length)) &&
            originalBytes.AsSpan(endByte).SequenceEqual(afterBytes.AsSpan(startByte + replacementBytes.Length));
    }

    private static bool RestoreBackup(string backupPath, string sourceFile)
    {
        try
        {
            if (!File.Exists(backupPath))
            {
                return false;
            }

            File.Copy(backupPath, sourceFile, overwrite: true);
            return true;
        }
        catch (IOException)
        {
            return false;
        }
        catch (UnauthorizedAccessException)
        {
            return false;
        }
    }

    private static bool SourceBytesChanged(string sourceFile, byte[] originalBytes)
    {
        try
        {
            return File.Exists(sourceFile) && !File.ReadAllBytes(sourceFile).SequenceEqual(originalBytes);
        }
        catch (IOException)
        {
            return true;
        }
        catch (UnauthorizedAccessException)
        {
            return true;
        }
    }

    private static void TryDeleteFile(string path)
    {
        try
        {
            if (!string.IsNullOrWhiteSpace(path) && File.Exists(path))
            {
                File.Delete(path);
            }
        }
        catch (IOException)
        {
        }
        catch (UnauthorizedAccessException)
        {
        }
    }

    private static JsonObject SpanJson(SourceSpan span)
    {
        return new JsonObject
        {
            ["startLine"] = span.StartLine,
            ["startColumn"] = span.StartColumn,
            ["endLine"] = span.EndLine,
            ["endColumn"] = span.EndColumn,
            ["startByte"] = span.StartByte,
            ["endByte"] = span.EndByte
        };
    }

    private static JsonArray StaleArtifactArray()
    {
        var array = new JsonArray();
        array.Add("source_map.json");
        array.Add("projection_report.json");
        array.Add("node_metadata.json");
        array.Add("runtime_bindings.json");
        return array;
    }

    private static JsonObject BuildUnifiedDiff(string oldText, string newText)
    {
        var oldLines = NormalizeDiffLines(oldText);
        var newLines = NormalizeDiffLines(newText);
        var prefix = 0;
        while (prefix < oldLines.Length &&
               prefix < newLines.Length &&
               string.Equals(oldLines[prefix], newLines[prefix], StringComparison.Ordinal))
        {
            prefix++;
        }

        var suffix = 0;
        while (suffix < oldLines.Length - prefix &&
               suffix < newLines.Length - prefix &&
               string.Equals(oldLines[oldLines.Length - 1 - suffix], newLines[newLines.Length - 1 - suffix], StringComparison.Ordinal))
        {
            suffix++;
        }

        var hunkOldStart = Math.Max(0, prefix - 3);
        var hunkNewStart = Math.Max(0, prefix - 3);
        var oldChangeEnd = oldLines.Length - suffix;
        var newChangeEnd = newLines.Length - suffix;
        var hunkOldEnd = Math.Min(oldLines.Length, oldChangeEnd + 3);
        var hunkNewEnd = Math.Min(newLines.Length, newChangeEnd + 3);
        var lines = new JsonArray();

        for (var i = hunkOldStart; i < prefix; i++)
        {
            lines.Add(" " + oldLines[i]);
        }
        for (var i = prefix; i < oldChangeEnd; i++)
        {
            lines.Add("-" + oldLines[i]);
        }
        for (var i = prefix; i < newChangeEnd; i++)
        {
            lines.Add("+" + newLines[i]);
        }
        for (var i = oldChangeEnd; i < hunkOldEnd; i++)
        {
            lines.Add(" " + oldLines[i]);
        }

        var hunks = new JsonArray
        {
            new JsonObject
            {
                ["oldStartLine"] = hunkOldStart + 1,
                ["oldLineCount"] = hunkOldEnd - hunkOldStart,
                ["newStartLine"] = hunkNewStart + 1,
                ["newLineCount"] = hunkNewEnd - hunkNewStart,
                ["lines"] = lines
            }
        };

        return new JsonObject
        {
            ["format"] = "unified",
            ["oldLabel"] = "source@base",
            ["newLabel"] = "source@proposed",
            ["hunks"] = hunks
        };
    }

    private static string[] NormalizeDiffLines(string text)
    {
        return text.Replace("\r\n", "\n", StringComparison.Ordinal)
            .Replace('\r', '\n')
            .Split('\n');
    }

    private static void ReplaceFileWithBytes(string sourceFile, string tempPath, byte[] bytes)
    {
        File.WriteAllBytes(tempPath, bytes);
        File.Move(tempPath, sourceFile, overwrite: true);
    }

    private static JsonObject? FindNode(JsonObject sourceMap, string nodeId)
    {
        var behaviors = sourceMap["behaviors"] as JsonArray;
        if (behaviors is null)
        {
            return null;
        }
        foreach (var behavior in behaviors.OfType<JsonObject>())
        {
            var nodes = behavior["nodes"] as JsonArray;
            if (nodes is null)
            {
                continue;
            }
            foreach (var node in nodes.OfType<JsonObject>())
            {
                if (ReadString(node, "nodeId") == nodeId)
                {
                    return node;
                }
            }
        }
        return null;
    }

    private static string FindBehaviorSourceFile(JsonObject sourceMap, string nodeId)
    {
        var behaviors = sourceMap["behaviors"] as JsonArray;
        if (behaviors is null)
        {
            return "";
        }
        foreach (var behavior in behaviors.OfType<JsonObject>())
        {
            var nodes = behavior["nodes"] as JsonArray;
            if (nodes is null)
            {
                continue;
            }
            if (nodes.OfType<JsonObject>().Any(node => ReadString(node, "nodeId") == nodeId))
            {
                return ReadString(behavior, "sourceFile");
            }
        }
        return "";
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

    private static bool ReadBool(JsonObject? obj, string key)
    {
        return obj is not null &&
            obj.TryGetPropertyValue(key, out var node) &&
            node is JsonValue value &&
            value.TryGetValue<bool>(out var result) &&
            result;
    }

    private static SourceSpan? ReadSpan(JsonObject? obj)
    {
        if (obj is null)
        {
            return null;
        }
        return new SourceSpan(
            ReadInt(obj, "startLine"),
            ReadInt(obj, "startColumn"),
            ReadInt(obj, "endLine"),
            ReadInt(obj, "endColumn"),
            ReadInt(obj, "startByte"),
            ReadInt(obj, "endByte"));
    }

    private static int ReadInt(JsonObject obj, string key)
    {
        return obj.TryGetPropertyValue(key, out var node) &&
            node is JsonValue value &&
            value.TryGetValue<int>(out var result)
                ? result
                : 0;
    }

    private static string SliceUtf8(string sourceText, int startByte, int endByte)
    {
        var bytes = Encoding.UTF8.GetBytes(sourceText);
        return Encoding.UTF8.GetString(bytes, startByte, endByte - startByte);
    }
}
