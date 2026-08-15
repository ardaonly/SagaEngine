// SPDX-License-Identifier: MPL-2.0

using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;
using SagaProjectKit.Core;

namespace SagaScript;

internal sealed record BehaviorMigrationAttachment
{
    public string ScenePath { get; init; } = "";
    public string SceneHash { get; init; } = "";
    public string EntityId { get; init; } = "";
    public string AttachmentId { get; init; } = "";
}

internal sealed record BehaviorMigrationChange
{
    public string SourcePath { get; init; } = "";
    public string SourceHash { get; init; } = "";
    public string SourceSymbolId { get; init; } = "";
    public string BehaviorModuleId { get; init; } = "";
    public IReadOnlyList<BehaviorMigrationAttachment> Attachments { get; init; } = [];
}

internal sealed record BehaviorMigrationPlan
{
    public int SchemaVersion { get; init; } = 1;
    public string Status { get; init; } = "Planned";
    public string ProjectPath { get; init; } = "";
    public string ProjectId { get; init; } = "";
    public string ProjectHash { get; init; } = "";
    public IReadOnlyList<BehaviorMigrationChange> Changes { get; init; } = [];
    public IReadOnlyList<WorkerDiagnostic> Diagnostics { get; init; } = [];
}

internal static class BehaviorIdentityMigration
{
    public static BehaviorMigrationPlan Preview(string projectPath)
    {
        var selection = ProjectSourceSelection.Load(projectPath);
        var diagnostics = new List<WorkerDiagnostic>();
        var changes = new List<BehaviorMigrationChange>();
        var symbols = new HashSet<string>(StringComparer.Ordinal);
        foreach (var sourcePath in selection.SourceFiles.Order(StringComparer.Ordinal))
        {
            var text = File.ReadAllText(sourcePath, Encoding.UTF8);
            var root = CSharpSyntaxTree.ParseText(text).GetCompilationUnitRoot();
            foreach (var type in root.DescendantNodes().OfType<ClassDeclarationSyntax>()
                         .Where(DerivesFromSagaBehavior))
            {
                if (type.AttributeLists.SelectMany(item => item.Attributes)
                    .Any(item => AttributeName(item) == "SagaBehaviorModule")) continue;
                var symbol = SourceSymbol(type);
                if (!symbols.Add(symbol))
                {
                    diagnostics.Add(Error("Script.Migration.SourceSymbolDuplicate", sourcePath,
                        $"Legacy C# source symbol '{symbol}' is ambiguous."));
                    continue;
                }
                var moduleId = $"behavior://project/{selection.Project.Manifest!.ProjectId.ToLowerInvariant()}/{Guid.NewGuid():N}";
                var attachments = FindAttachments(selection, symbol, diagnostics);
                changes.Add(new BehaviorMigrationChange
                {
                    SourcePath = Relative(selection.ProjectRoot, sourcePath),
                    SourceHash = CanonicalJson.HashFile(sourcePath),
                    SourceSymbolId = symbol,
                    BehaviorModuleId = moduleId,
                    Attachments = attachments,
                });
            }
        }
        var duplicateModule = changes.GroupBy(item => item.BehaviorModuleId, StringComparer.Ordinal)
            .FirstOrDefault(group => group.Count() > 1);
        if (duplicateModule is not null)
            diagnostics.Add(Error("Script.Migration.ModuleIdDuplicate", duplicateModule.Key,
                "Allocated behavior module ids must be unique."));
        return new BehaviorMigrationPlan
        {
            Status = diagnostics.Count == 0 ? "Planned" : "Failed",
            ProjectPath = Path.GetFullPath(projectPath),
            ProjectId = selection.Project.Manifest!.ProjectId,
            ProjectHash = selection.Project.ProjectHash,
            Changes = changes.OrderBy(item => item.SourceSymbolId, StringComparer.Ordinal).ToArray(),
            Diagnostics = diagnostics,
        };
    }

    public static object Apply(string planPath, string cancellationPath)
    {
        var plan = JsonSerializer.Deserialize<BehaviorMigrationPlan>(
            File.ReadAllBytes(planPath), CanonicalJson.SerializerOptions)
            ?? throw new InvalidOperationException("Behavior migration plan is invalid.");
        if (plan.SchemaVersion != 1 || plan.Status != "Planned" || plan.Diagnostics.Count > 0)
            throw new InvalidOperationException("Only a successful behavior migration plan can be applied.");
        if (Cancelled(cancellationPath)) return Report("Cancelled", plan, []);
        var project = new ProjectLoader().Load(plan.ProjectPath, checkReferences: true);
        if (project.Manifest?.ProjectId != plan.ProjectId)
            throw new InvalidOperationException("Migration plan project identity does not match.");
        if (IsApplied(plan, project.ProjectRoot)) return Report("AlreadyApplied", plan, []);
        if (project.ProjectHash != plan.ProjectHash)
            throw new InvalidOperationException("Project manifest changed after migration preview.");

        var writes = new Dictionary<string, byte[]>(StringComparer.Ordinal);
        foreach (var sourceGroup in plan.Changes.GroupBy(item => item.SourcePath, StringComparer.Ordinal))
        {
            var path = Full(project.ProjectRoot, sourceGroup.Key);
            if (CanonicalJson.HashFile(path) != sourceGroup.First().SourceHash)
                throw new InvalidOperationException($"Migration source changed: {sourceGroup.Key}");
            var text = File.ReadAllText(path, Encoding.UTF8);
            var insertions = new List<(int Offset, string Text)>();
            var root = CSharpSyntaxTree.ParseText(text).GetCompilationUnitRoot();
            foreach (var change in sourceGroup)
            {
                var type = root.DescendantNodes().OfType<ClassDeclarationSyntax>()
                    .SingleOrDefault(item => SourceSymbol(item) == change.SourceSymbolId &&
                        DerivesFromSagaBehavior(item))
                    ?? throw new InvalidOperationException(
                        $"Legacy behavior source symbol disappeared: {change.SourceSymbolId}");
                if (type.AttributeLists.SelectMany(item => item.Attributes)
                    .Any(item => AttributeName(item) == "SagaBehaviorModule"))
                    throw new InvalidOperationException(
                        $"Behavior acquired conflicting module metadata: {change.SourceSymbolId}");
                var lineStart = text.LastIndexOf('\n', Math.Max(0, type.SpanStart - 1)) + 1;
                var indent = text[lineStart..type.SpanStart].TakeWhile(char.IsWhiteSpace).ToArray();
                insertions.Add((lineStart,
                    new string(indent) + $"[SagaBehaviorModule(\"{change.BehaviorModuleId}\")]\n"));
            }
            foreach (var insertion in insertions.OrderByDescending(item => item.Offset))
                text = text.Insert(insertion.Offset, insertion.Text);
            writes[path] = Encoding.UTF8.GetBytes(text);
        }

        foreach (var sceneGroup in plan.Changes.SelectMany(change => change.Attachments.Select(
                     attachment => (Change: change, Attachment: attachment)))
                 .GroupBy(item => item.Attachment.ScenePath, StringComparer.Ordinal))
        {
            var path = Full(project.ProjectRoot, sceneGroup.Key);
            if (sceneGroup.Select(item => item.Attachment.SceneHash).Distinct(StringComparer.Ordinal)
                    .Single() is var expected && CanonicalJson.HashFile(path) != expected)
                throw new InvalidOperationException($"Migration scene changed: {sceneGroup.Key}");
            var scene = JsonNode.Parse(File.ReadAllBytes(path))?.AsObject()
                ?? throw new InvalidOperationException($"Migration scene is invalid: {sceneGroup.Key}");
            var bySymbol = sceneGroup.ToDictionary(item => item.Change.SourceSymbolId,
                item => item, StringComparer.Ordinal);
            foreach (var entityNode in scene["entities"]!.AsArray())
            {
                var entity = entityNode!.AsObject();
                var entityId = entity["entityId"]!.GetValue<string>();
                foreach (var componentNode in entity["components"]!.AsArray())
                {
                    var component = componentNode!.AsObject();
                    var type = component["type"]?.GetValue<string>() ?? "";
                    if (type == "ScriptBehavior")
                    {
                        var symbol = component["behaviorTypeId"]?.GetValue<string>() ?? "";
                        if (!bySymbol.TryGetValue(symbol, out var item) ||
                            item.Attachment.EntityId != entityId) continue;
                        var configuration = component["instanceConfiguration"]?.DeepClone() ?? new JsonObject();
                        var capabilities = component["requiredCapabilities"]?.DeepClone() ?? new JsonArray();
                        component.Clear();
                        component["attachmentId"] = item.Attachment.AttachmentId;
                        component["behaviorModuleId"] = item.Change.BehaviorModuleId;
                        component["enabled"] = true;
                        component["instanceConfiguration"] = configuration;
                        component["requiredCapabilities"] = capabilities;
                        component["type"] = "BehaviorAttachment";
                    }
                    else if (type is "ScriptActionBinding" or "ScriptEventBinding")
                    {
                        var symbol = component["behaviorTypeId"]?.GetValue<string>() ?? "";
                        if (!bySymbol.TryGetValue(symbol, out var item)) continue;
                        component.Remove("behaviorTypeId");
                        component["behaviorModuleId"] = item.Change.BehaviorModuleId;
                        component["targetAttachmentId"] = item.Attachment.AttachmentId;
                    }
                }
            }
            writes[path] = CanonicalJson.Serialize(scene);
        }
        if (Cancelled(cancellationPath)) return Report("Cancelled", plan, []);
        AtomicCommit(writes);
        return Report("Applied", plan, writes.Keys.Select(path => Relative(project.ProjectRoot, path)).ToArray());
    }

    private static BehaviorMigrationAttachment[] FindAttachments(ProjectSourceSelection selection,
        string sourceSymbol, List<WorkerDiagnostic> diagnostics)
    {
        var result = new List<BehaviorMigrationAttachment>();
        foreach (var scene in selection.Project.Manifest!.Scenes)
        {
            var path = Full(selection.ProjectRoot, scene.Path);
            var attachmentsBeforeScene = result.Count;
            var bindingReferences = 0;
            using var document = JsonDocument.Parse(File.ReadAllBytes(path));
            foreach (var entity in document.RootElement.GetProperty("entities").EnumerateArray())
                foreach (var component in entity.GetProperty("components").EnumerateArray())
                {
                    var type = component.GetProperty("type").GetString() ?? "";
                    var legacyIdentity = component.TryGetProperty("behaviorTypeId", out _);
                    var canonicalIdentity = component.TryGetProperty("behaviorModuleId", out _) ||
                        component.TryGetProperty("attachmentId", out _) ||
                        component.TryGetProperty("targetAttachmentId", out _);
                    if (legacyIdentity && canonicalIdentity)
                    {
                        diagnostics.Add(Error("Script.Migration.MixedIdentityShape", scene.Path,
                            "Legacy and canonical behavior identity fields cannot coexist."));
                        continue;
                    }
                    if (type == "ScriptBehavior" && legacyIdentity &&
                        component.GetProperty("behaviorTypeId").GetString() == sourceSymbol)
                        result.Add(new BehaviorMigrationAttachment
                        {
                            ScenePath = scene.Path.Replace('\\', '/'),
                            SceneHash = CanonicalJson.HashFile(path),
                            EntityId = entity.GetProperty("entityId").GetString() ?? "",
                            AttachmentId = $"attachment://{Guid.NewGuid():N}",
                        });
                    else if (type is "ScriptActionBinding" or "ScriptEventBinding" &&
                        legacyIdentity &&
                        component.GetProperty("behaviorTypeId").GetString() == sourceSymbol)
                        ++bindingReferences;
                }
            var attachmentCount = result.Count - attachmentsBeforeScene;
            if (bindingReferences > 0 && attachmentCount != 1)
                diagnostics.Add(Error("Script.Migration.BindingTargetAmbiguous", scene.Path,
                    $"Legacy bindings for '{sourceSymbol}' require exactly one attachment in the scene."));
        }
        if (result.GroupBy(item => item.ScenePath, StringComparer.Ordinal).Any(group => group.Count() > 1))
            diagnostics.Add(Error("Script.Migration.AttachmentAmbiguous", sourceSymbol,
                "A legacy module with multiple attachments in one scene requires explicit attachment targeting."));
        return result.ToArray();
    }

    private static bool IsApplied(BehaviorMigrationPlan plan, string root)
    {
        foreach (var change in plan.Changes)
        {
            var source = File.ReadAllText(Full(root, change.SourcePath), Encoding.UTF8);
            if (!source.Contains($"[SagaBehaviorModule(\"{change.BehaviorModuleId}\")]",
                    StringComparison.Ordinal)) return false;
            foreach (var attachment in change.Attachments)
            {
                var scene = File.ReadAllText(Full(root, attachment.ScenePath), Encoding.UTF8);
                if (!scene.Contains(change.BehaviorModuleId, StringComparison.Ordinal) ||
                    !scene.Contains(attachment.AttachmentId, StringComparison.Ordinal)) return false;
            }
        }
        return true;
    }

    private static void AtomicCommit(IReadOnlyDictionary<string, byte[]> writes)
    {
        var originals = writes.ToDictionary(item => item.Key, item => File.ReadAllBytes(item.Key),
            StringComparer.Ordinal);
        var temporary = new Dictionary<string, string>(StringComparer.Ordinal);
        try
        {
            foreach (var item in writes.OrderBy(item => item.Key, StringComparer.Ordinal))
            {
                var temp = item.Key + "." + Guid.NewGuid().ToString("N") + ".migration.tmp";
                File.WriteAllBytes(temp, item.Value);
                temporary[item.Key] = temp;
            }
            foreach (var item in temporary.OrderBy(item => item.Key, StringComparer.Ordinal))
                File.Move(item.Value, item.Key, overwrite: true);
        }
        catch
        {
            foreach (var item in originals) WorkerFileSupport.AtomicWrite(item.Key, item.Value);
            throw;
        }
        finally
        {
            foreach (var path in temporary.Values)
                if (File.Exists(path)) File.Delete(path);
        }
    }

    private static object Report(string status, BehaviorMigrationPlan plan,
        IReadOnlyList<string> changedFiles) => new
    {
        schemaVersion = 1, status, plan.ProjectId,
        changes = plan.Changes.Count, changedFiles = changedFiles.Order(StringComparer.Ordinal).ToArray(),
    };

    private static bool Cancelled(string path) => path.Length > 0 && File.Exists(path);
    private static bool DerivesFromSagaBehavior(ClassDeclarationSyntax type) =>
        type.BaseList?.Types.Any(item => item.Type.ToString().Split('.').Last() == "SagaBehavior") == true;
    private static string AttributeName(AttributeSyntax attribute) =>
        attribute.Name.ToString().Split('.').Last().Replace("Attribute", "", StringComparison.Ordinal);
    private static string SourceSymbol(ClassDeclarationSyntax type)
    {
        var namespaces = type.Ancestors().Where(item => item is BaseNamespaceDeclarationSyntax)
            .Cast<BaseNamespaceDeclarationSyntax>().Reverse().Select(item => item.Name.ToString());
        var containingTypes = type.Ancestors().OfType<TypeDeclarationSyntax>().Reverse()
            .Select(item => item.Identifier.ValueText);
        return string.Join('.', namespaces.Concat(containingTypes).Append(type.Identifier.ValueText));
    }
    private static string Full(string root, string relative)
    {
        if (Path.IsPathFullyQualified(relative))
            throw new InvalidOperationException("Migration paths must be project-relative.");
        var fullRoot = Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar) +
            Path.DirectorySeparatorChar;
        var full = Path.GetFullPath(Path.Combine(root, relative));
        if (!full.StartsWith(fullRoot, StringComparison.Ordinal))
            throw new InvalidOperationException("Migration path escapes the project root.");
        return full;
    }
    private static string Relative(string root, string path) =>
        Path.GetRelativePath(root, path).Replace('\\', '/');
    private static WorkerDiagnostic Error(string code, string path, string message) => new()
    { Code = code, Path = path.Replace('\\', '/'), Message = message };
}
