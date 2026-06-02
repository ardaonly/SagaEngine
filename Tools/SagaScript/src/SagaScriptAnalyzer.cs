// SagaScriptAnalyzer.cs
// Roslyn-backed source metadata extraction and validation for SagaScript.

using System.Security.Cryptography;
using System.Text;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;

namespace SagaScript;

internal sealed class SagaScriptAnalyzer
{
    private static readonly HashSet<string> SupportedTypes = new(StringComparer.Ordinal)
    {
        "void",
        "bool",
        "int",
        "float",
        "double",
        "string",
        "ScriptId",
        "ArtifactId",
        "ArtifactRef",
        "BuildId",
        "BuildProfileId",
        "ParticipantId",
        "SessionId",
        "PackageId",
        "ScriptArtifactRef"
    };

    private static readonly HashSet<string> CapabilityPrefixes = new(StringComparer.Ordinal)
    {
        "Gameplay",
        "UI",
        "Client",
        "Server",
        "Shared",
        "Tool",
        "Editor",
        "Build",
        "NativeExtension",
        "EngineInternal",
        "Engine"
    };

    public AnalysisResult Analyze(IReadOnlyList<string> sourceFiles, bool requireManifestReady)
    {
        var bindings = new List<ScriptBinding>();
        var diagnostics = new List<SagaDiagnostic>();
        var scriptIds = new Dictionary<string, string>(StringComparer.Ordinal);
        var bindingIds = new HashSet<string>(StringComparer.Ordinal);
        var sourceHashes = new List<string>();

        foreach (var sourceFile in sourceFiles)
        {
            var sourceText = File.ReadAllText(sourceFile);
            var sourceHash = ComputeHash(sourceText);
            sourceHashes.Add($"{sourceFile}:{sourceHash}");
            AnalyzeFile(sourceFile, sourceText, sourceHash, requireManifestReady, bindings, diagnostics);
        }

        foreach (var binding in bindings)
        {
            var scriptOwner = $"{binding.SourceFile}::{binding.DeclaringType}";
            if (!scriptIds.TryAdd(binding.ScriptId, scriptOwner) &&
                scriptIds[binding.ScriptId] != scriptOwner)
            {
                diagnostics.Add(Error(
                    "Script.Identity.Duplicate",
                    $"Duplicate script id '{binding.ScriptId}' appears in '{scriptIds[binding.ScriptId]}' and '{scriptOwner}'.",
                    binding.ScriptId,
                    binding.SourceFile,
                    binding.SourceRange));
            }

            if (!bindingIds.Add(binding.BindingId))
            {
                diagnostics.Add(Error(
                    "Script.Binding.Duplicate",
                    $"Duplicate binding identity '{binding.BindingId}'.",
                    binding.ScriptId,
                    binding.SourceFile,
                    binding.SourceRange,
                    binding.BindingId));
            }
        }

        return new AnalysisResult
        {
            Inputs = sourceFiles,
            SourceHash = ComputeHash(string.Join('\n', sourceHashes.Order(StringComparer.Ordinal))),
            Bindings = bindings,
            Diagnostics = diagnostics,
            Summary = BuildSummary(diagnostics)
        };
    }

    private static void AnalyzeFile(
        string sourceFile,
        string sourceText,
        string sourceHash,
        bool requireManifestReady,
        List<ScriptBinding> bindings,
        List<SagaDiagnostic> diagnostics)
    {
        var tree = CSharpSyntaxTree.ParseText(sourceText, path: sourceFile);
        var root = tree.GetCompilationUnitRoot();
        var fileBindingCount = 0;
        var nonCallableMethodCount = 0;
        var fileLevelMetadataCount = 0;

        foreach (var typeDeclaration in root.DescendantNodes().OfType<TypeDeclarationSyntax>())
        {
            var typeAttributes = AttributeMap.From(typeDeclaration.AttributeLists);
            fileLevelMetadataCount += CountSagaAttributes(typeAttributes);
            var typeScriptId = typeAttributes.FirstString("SagaScriptId");
            var callableMethods = typeDeclaration.Members
                .OfType<MethodDeclarationSyntax>()
                .Where(method => AttributeMap.From(method.AttributeLists).Has("BlockCallable"))
                .ToArray();
            if (requireManifestReady &&
                callableMethods.Length > 0 &&
                !DerivesFromSagaScript(typeDeclaration))
            {
                diagnostics.Add(Error(
                    "Script.Class.MissingSagaScriptBase",
                    "Executable SagaScript classes must derive from SagaScript.",
                    typeScriptId,
                    sourceFile,
                    ToRange(tree, typeDeclaration.GetLocation())));
            }

            foreach (var method in typeDeclaration.Members.OfType<MethodDeclarationSyntax>())
            {
                var methodAttributes = AttributeMap.From(method.AttributeLists);
                fileLevelMetadataCount += CountSagaAttributes(methodAttributes);
                if (!methodAttributes.Has("BlockCallable"))
                {
                    nonCallableMethodCount++;
                    continue;
                }

                fileBindingCount++;
                var binding = BuildBinding(sourceFile, sourceHash, tree, typeDeclaration, typeScriptId, method, methodAttributes);
                bindings.Add(binding);
                ValidateBinding(binding, requireManifestReady, diagnostics);
            }
        }

        if (fileBindingCount == 0)
        {
            diagnostics.Add(Warning(
                "Script.Source.NoBlockCallableBindings",
                "Script source contains no [BlockCallable] bindings.",
                null,
                sourceFile,
                null));
        }

        if (nonCallableMethodCount > 0)
        {
            diagnostics.Add(Warning(
                "Script.Source.FreeformNotRoundTrippable",
                "Freeform C# method(s) were detected and cannot be guaranteed to round-trip back into blocks.",
                null,
                sourceFile,
                null));
        }

        if (fileBindingCount == 0 && fileLevelMetadataCount > 0)
        {
            diagnostics.Add(Warning(
                "Script.Metadata.Unreferenced",
                "SagaScript authority or capability metadata exists, but no callable binding references it.",
                null,
                sourceFile,
                null));
        }
    }

    private static ScriptBinding BuildBinding(
        string sourceFile,
        string sourceHash,
        SyntaxTree tree,
        TypeDeclarationSyntax typeDeclaration,
        string? typeScriptId,
        MethodDeclarationSyntax method,
        AttributeMap attributes)
    {
        var scriptId = attributes.FirstString("SagaScriptId") ?? typeScriptId ?? "";
        var declaringType = GetDeclaringType(typeDeclaration);
        var methodName = method.Identifier.ValueText;
        var authority = ResolveAuthority(attributes);
        var sideEffects = new List<string>();
        if (attributes.Has("WritesPersistentState"))
        {
            sideEffects.Add("WritePersistentState");
        }

        if (attributes.Has("Replicates"))
        {
            sideEffects.Add("WriteReplicatedState");
        }

        var capabilities = attributes.Strings("RequiresCapability").ToArray();
        var parameters = method.ParameterList.Parameters
            .Select(parameter => new ScriptParameter
            {
                Name = parameter.Identifier.ValueText,
                Type = NormalizeType(parameter.Type?.ToString() ?? ""),
                Supported = IsSupportedType(NormalizeType(parameter.Type?.ToString() ?? ""))
            })
            .ToArray();

        var returnType = NormalizeType(method.ReturnType.ToString());
        var bindingId = string.IsNullOrWhiteSpace(scriptId)
            ? $"binding://missing-script-id/{declaringType}.{methodName}"
            : $"{scriptId}#{declaringType}.{methodName}";

        return new ScriptBinding
        {
            BindingId = bindingId,
            ScriptId = scriptId,
            DeclaringType = declaringType,
            MethodName = methodName,
            BlockName = attributes.FirstString("BlockName"),
            BlockCategory = attributes.FirstString("BlockCategory"),
            Parameters = parameters,
            ReturnType = new ScriptReturn
            {
                Type = returnType,
                Supported = IsSupportedType(returnType)
            },
            Authority = authority,
            SideEffects = sideEffects,
            PredictionUnsafe = attributes.Has("PredictionUnsafe"),
            RequestedCapabilities = capabilities,
            SourceFile = sourceFile,
            SourceRange = ToRange(tree, method.GetLocation()),
            SourceHash = sourceHash,
            GeneratedOrigin = BuildGeneratedOrigin(attributes)
        };
    }

    private static bool DerivesFromSagaScript(TypeDeclarationSyntax typeDeclaration)
    {
        if (typeDeclaration.BaseList is null)
        {
            return false;
        }

        return typeDeclaration.BaseList.Types.Any(baseType =>
        {
            var name = baseType.Type.ToString();
            return name == "SagaScript" ||
                   name == "SagaEngine.Scripting.SagaScript" ||
                   name.EndsWith(".SagaScript", StringComparison.Ordinal);
        });
    }

    private static void ValidateBinding(ScriptBinding binding, bool requireManifestReady, List<SagaDiagnostic> diagnostics)
    {
        if (string.IsNullOrWhiteSpace(binding.Authority))
        {
            diagnostics.Add(Error(
                "Script.Binding.MissingAuthority",
                "[BlockCallable] method is missing authority metadata.",
                binding.ScriptId,
                binding.SourceFile,
                binding.SourceRange,
                binding.BindingId));
        }

        if (requireManifestReady && !IsValidScriptId(binding.ScriptId))
        {
            diagnostics.Add(Error(
                "Script.Identity.Invalid",
                "Manifest emission requires a valid [SagaScriptId(\"script://...\")].",
                binding.ScriptId,
                binding.SourceFile,
                binding.SourceRange,
                binding.BindingId));
        }

        if (binding.SideEffects.Contains("WritePersistentState", StringComparer.Ordinal) && binding.Authority != "ServerOnly")
        {
            diagnostics.Add(Error(
                "Script.Authority.PersistentWriteRequiresServer",
                "[WritesPersistentState] requires [ServerOnly] authority.",
                binding.ScriptId,
                binding.SourceFile,
                binding.SourceRange,
                binding.BindingId));
        }

        if (binding.SideEffects.Contains("WriteReplicatedState", StringComparer.Ordinal) && binding.Authority != "ServerOnly")
        {
            diagnostics.Add(Error(
                "Script.Authority.ReplicationRequiresServer",
                "[Replicates] requires [ServerOnly] authority.",
                binding.ScriptId,
                binding.SourceFile,
                binding.SourceRange,
                binding.BindingId));
        }

        if (binding.PredictionUnsafe && binding.Authority == "ClientOnly")
        {
            diagnostics.Add(Error(
                "Script.Authority.PredictionUnsafeClient",
                "[PredictionUnsafe] cannot be staged as client-presentable without explicit non-client authority.",
                binding.ScriptId,
                binding.SourceFile,
                binding.SourceRange,
                binding.BindingId));
        }

        foreach (var parameter in binding.Parameters.Where(parameter => !parameter.Supported))
        {
            diagnostics.Add(Error(
                "Script.Type.UnsupportedParameter",
                $"Unsupported parameter type '{parameter.Type}' for parameter '{parameter.Name}'.",
                binding.ScriptId,
                binding.SourceFile,
                binding.SourceRange,
                binding.BindingId));
        }

        if (!binding.ReturnType.Supported)
        {
            diagnostics.Add(Error(
                "Script.Type.UnsupportedReturn",
                $"Unsupported return type '{binding.ReturnType.Type}'.",
                binding.ScriptId,
                binding.SourceFile,
                binding.SourceRange,
                binding.BindingId));
        }

        foreach (var capability in binding.RequestedCapabilities)
        {
            if (!IsRecognizedCapability(capability))
            {
                diagnostics.Add(Error(
                    "Script.Capability.Unrecognized",
                    $"Capability '{capability}' is not recognized by the v1 capability model.",
                    binding.ScriptId,
                    binding.SourceFile,
                    binding.SourceRange,
                    binding.BindingId,
                    capability));
            }

            if (IsEngineInternal(capability) && !binding.ScriptId.StartsWith("script://engine/", StringComparison.Ordinal))
            {
                diagnostics.Add(new SagaDiagnostic
                {
                    Severity = SagaDiagnosticSeverity.Security.ToString(),
                    Code = "Script.Capability.EngineInternalDenied",
                    Message = "EngineInternal capability is denied for non-engine-owned scripts.",
                    ScriptId = binding.ScriptId,
                    SourceFile = binding.SourceFile,
                    SourceRange = binding.SourceRange,
                    BindingId = binding.BindingId,
                    Capability = capability
                });
            }
        }

        if (string.IsNullOrWhiteSpace(binding.BlockName))
        {
            diagnostics.Add(Warning(
                "Script.Binding.MissingBlockName",
                "Block-callable method is missing a friendly [BlockName].",
                binding.ScriptId,
                binding.SourceFile,
                binding.SourceRange,
                binding.BindingId));
        }

        if (string.IsNullOrWhiteSpace(binding.BlockCategory))
        {
            diagnostics.Add(Warning(
                "Script.Binding.MissingBlockCategory",
                "Block-callable method is missing a [BlockCategory].",
                binding.ScriptId,
                binding.SourceFile,
                binding.SourceRange,
                binding.BindingId));
        }

        ValidateGeneratedOrigin(binding, diagnostics);
    }

    private static void ValidateGeneratedOrigin(ScriptBinding binding, List<SagaDiagnostic> diagnostics)
    {
        var origin = binding.GeneratedOrigin;
        if (origin?.GeneratedPath is null || origin.SourceHash is null)
        {
            return;
        }

        var generatedPath = Path.IsPathRooted(origin.GeneratedPath)
            ? origin.GeneratedPath
            : Path.GetFullPath(Path.Combine(Path.GetDirectoryName(binding.SourceFile) ?? ".", origin.GeneratedPath));
        if (!File.Exists(generatedPath))
        {
            return;
        }

        var actualHash = ComputeHash(File.ReadAllText(generatedPath));
        if (!origin.SourceHash.Equals(actualHash, StringComparison.OrdinalIgnoreCase) &&
            !origin.SourceHash.Equals($"sha256:{actualHash}", StringComparison.OrdinalIgnoreCase))
        {
            diagnostics.Add(Error(
                "Script.GeneratedOrigin.HashMismatch",
                "Generated-code origin hash does not match the referenced generated file.",
                binding.ScriptId,
                binding.SourceFile,
                binding.SourceRange,
                binding.BindingId));
        }
    }

    private static GeneratedCodeOrigin? BuildGeneratedOrigin(AttributeMap attributes)
    {
        var values = attributes.Strings("GeneratedCodeOrigin").ToArray();
        if (values.Length == 0)
        {
            return null;
        }

        return new GeneratedCodeOrigin
        {
            SourceResourceId = values.ElementAtOrDefault(0),
            SourceHash = values.ElementAtOrDefault(1),
            GeneratedPath = values.ElementAtOrDefault(2)
        };
    }

    private static string ResolveAuthority(AttributeMap attributes)
    {
        var authorities = new List<string>();
        if (attributes.Has("ClientOnly"))
        {
            authorities.Add("ClientOnly");
        }

        if (attributes.Has("ServerOnly"))
        {
            authorities.Add("ServerOnly");
        }

        if (attributes.Has("SharedPure"))
        {
            authorities.Add("SharedPure");
        }

        return authorities.Count == 1 ? authorities[0] : "";
    }

    private static int CountSagaAttributes(AttributeMap attributes)
    {
        return attributes.Names.Count(name =>
            name is "ClientOnly" or "ServerOnly" or "SharedPure" or "WritesPersistentState"
                or "Replicates" or "PredictionUnsafe" or "RequiresCapability");
    }

    private static string GetDeclaringType(TypeDeclarationSyntax typeDeclaration)
    {
        var names = new Stack<string>();
        SyntaxNode? current = typeDeclaration;
        while (current is not null)
        {
            switch (current)
            {
                case TypeDeclarationSyntax type:
                    names.Push(type.Identifier.ValueText);
                    break;
                case NamespaceDeclarationSyntax ns:
                    names.Push(ns.Name.ToString());
                    break;
                case FileScopedNamespaceDeclarationSyntax fileNs:
                    names.Push(fileNs.Name.ToString());
                    break;
            }

            current = current.Parent;
        }

        return string.Join('.', names);
    }

    private static string NormalizeType(string typeName)
    {
        var trimmed = typeName.Trim();
        var lastDot = trimmed.LastIndexOf('.');
        return lastDot >= 0 ? trimmed[(lastDot + 1)..] : trimmed;
    }

    private static bool IsSupportedType(string typeName)
    {
        return SupportedTypes.Contains(typeName);
    }

    private static bool IsValidScriptId(string scriptId)
    {
        return scriptId.StartsWith("script://", StringComparison.Ordinal) && scriptId.Length > "script://".Length;
    }

    private static bool IsRecognizedCapability(string capability)
    {
        var parts = capability.Split('.', StringSplitOptions.RemoveEmptyEntries);
        return parts.Length >= 2 &&
               CapabilityPrefixes.Contains(parts[0]) &&
               parts.All(part => char.IsLetter(part[0]) && part.All(ch => char.IsLetterOrDigit(ch) || ch == '_'));
    }

    private static bool IsEngineInternal(string capability)
    {
        return capability.StartsWith("EngineInternal.", StringComparison.Ordinal) ||
               capability.StartsWith("Engine.Internal.", StringComparison.Ordinal);
    }

    private static SourceRange ToRange(SyntaxTree tree, Location location)
    {
        var span = tree.GetLineSpan(location.SourceSpan);
        return new SourceRange(
            span.StartLinePosition.Line + 1,
            span.StartLinePosition.Character + 1,
            span.EndLinePosition.Line + 1,
            span.EndLinePosition.Character + 1);
    }

    private static SagaDiagnostic Error(
        string code,
        string message,
        string? scriptId,
        string sourceFile,
        SourceRange? range,
        string? bindingId = null,
        string? capability = null)
    {
        return new SagaDiagnostic
        {
            Severity = SagaDiagnosticSeverity.Error.ToString(),
            Code = code,
            Message = message,
            ScriptId = scriptId,
            SourceFile = sourceFile,
            SourceRange = range,
            BindingId = bindingId,
            Capability = capability
        };
    }

    private static SagaDiagnostic Warning(
        string code,
        string message,
        string? scriptId,
        string sourceFile,
        SourceRange? range,
        string? bindingId = null)
    {
        return new SagaDiagnostic
        {
            Severity = SagaDiagnosticSeverity.Warning.ToString(),
            Code = code,
            Message = message,
            ScriptId = scriptId,
            SourceFile = sourceFile,
            SourceRange = range,
            BindingId = bindingId
        };
    }

    public static DiagnosticSummary BuildSummary(IReadOnlyList<SagaDiagnostic> diagnostics)
    {
        return new DiagnosticSummary
        {
            InfoCount = diagnostics.Count(diagnostic => diagnostic.Severity == SagaDiagnosticSeverity.Info.ToString()),
            WarningCount = diagnostics.Count(diagnostic => diagnostic.Severity == SagaDiagnosticSeverity.Warning.ToString()),
            ErrorCount = diagnostics.Count(diagnostic => diagnostic.Severity == SagaDiagnosticSeverity.Error.ToString()),
            SecurityCount = diagnostics.Count(diagnostic => diagnostic.Severity == SagaDiagnosticSeverity.Security.ToString()),
            HasBlockingDiagnostics = diagnostics.Any(IsBlocking)
        };
    }

    public static bool IsBlocking(SagaDiagnostic diagnostic)
    {
        return diagnostic.Severity is nameof(SagaDiagnosticSeverity.Error) or nameof(SagaDiagnosticSeverity.Security);
    }

    private static string ComputeHash(string text)
    {
        var bytes = SHA256.HashData(Encoding.UTF8.GetBytes(text));
        return Convert.ToHexString(bytes).ToLowerInvariant();
    }
}

internal sealed class AttributeMap
{
    private readonly Dictionary<string, List<AttributeSyntax>> _attributes;

    private AttributeMap(Dictionary<string, List<AttributeSyntax>> attributes)
    {
        _attributes = attributes;
    }

    public IReadOnlyCollection<string> Names => _attributes.Keys;

    public static AttributeMap From(SyntaxList<AttributeListSyntax> lists)
    {
        var attributes = new Dictionary<string, List<AttributeSyntax>>(StringComparer.Ordinal);
        foreach (var attribute in lists.SelectMany(list => list.Attributes))
        {
            var name = NormalizeName(attribute.Name.ToString());
            if (!attributes.TryGetValue(name, out var bucket))
            {
                bucket = new List<AttributeSyntax>();
                attributes[name] = bucket;
            }

            bucket.Add(attribute);
        }

        return new AttributeMap(attributes);
    }

    public bool Has(string name)
    {
        return _attributes.ContainsKey(name);
    }

    public string? FirstString(string name)
    {
        return Strings(name).FirstOrDefault();
    }

    public string? NamedString(string name, string argumentName)
    {
        if (!_attributes.TryGetValue(name, out var attributes))
        {
            return null;
        }

        foreach (var attribute in attributes)
        {
            if (attribute.ArgumentList is null)
            {
                continue;
            }

            foreach (var argument in attribute.ArgumentList.Arguments)
            {
                if (argument.NameEquals?.Name.Identifier.ValueText != argumentName)
                {
                    continue;
                }
                if (argument.Expression is LiteralExpressionSyntax literal &&
                    literal.Token.Value is string value)
                {
                    return value;
                }
            }
        }

        return null;
    }

    public string? NamedEnum(string name, string argumentName)
    {
        if (!_attributes.TryGetValue(name, out var attributes))
        {
            return null;
        }

        foreach (var attribute in attributes)
        {
            if (attribute.ArgumentList is null)
            {
                continue;
            }

            foreach (var argument in attribute.ArgumentList.Arguments)
            {
                if (argument.NameEquals?.Name.Identifier.ValueText != argumentName)
                {
                    continue;
                }

                var value = argument.Expression switch
                {
                    MemberAccessExpressionSyntax member => member.Name.Identifier.ValueText,
                    IdentifierNameSyntax identifier => identifier.Identifier.ValueText,
                    _ => argument.Expression.ToString().Split('.').LastOrDefault()
                };
                return string.IsNullOrWhiteSpace(value) ? null : value;
            }
        }

        return null;
    }

    public IEnumerable<string> Strings(string name)
    {
        if (!_attributes.TryGetValue(name, out var attributes))
        {
            yield break;
        }

        foreach (var attribute in attributes)
        {
            if (attribute.ArgumentList is null)
            {
                continue;
            }

            foreach (var argument in attribute.ArgumentList.Arguments)
            {
                if (argument.Expression is LiteralExpressionSyntax literal &&
                    literal.Token.Value is string value)
                {
                    yield return value;
                }
            }
        }
    }

    private static string NormalizeName(string name)
    {
        var lastDot = name.LastIndexOf('.');
        var normalized = lastDot >= 0 ? name[(lastDot + 1)..] : name;
        return normalized.EndsWith("Attribute", StringComparison.Ordinal)
            ? normalized[..^"Attribute".Length]
            : normalized;
    }
}
