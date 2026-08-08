// SPDX-License-Identifier: MPL-2.0
// Long-lived, output-free SagaScript language service. Standard output is protocol-only.

using System.Collections.Concurrent;
using System.Globalization;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.Classification;
using Microsoft.CodeAnalysis.Completion;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;
using Microsoft.CodeAnalysis.FindSymbols;
using Microsoft.CodeAnalysis.Host.Mef;
using Microsoft.CodeAnalysis.QuickInfo;
using Microsoft.CodeAnalysis.Recommendations;
using Microsoft.CodeAnalysis.Text;

namespace SagaScript;
internal static class LanguageService
{
    internal const int SchemaVersion = 1;
    internal const int ProtocolVersion = 1;
    internal const int MaximumMessageBytes = 4 * 1024 * 1024;

    public static async Task<int> RunAsync(TextReader input, TextWriter output, TextWriter log)
    {
        var server = new Server(output, log);
        while (!server.ShuttingDown)
        {
            var line = await input.ReadLineAsync();
            if (line is null) break;
            if (Encoding.UTF8.GetByteCount(line) > MaximumMessageBytes)
            {
                await server.FatalAsync("LanguageService.MessageTooLarge", "Protocol message exceeds the 4 MiB limit.");
                return 2;
            }
            await server.AcceptAsync(line);
        }
        await server.DrainAsync();
        await server.DisposeAsync();
        return server.Fatal ? 2 : 0;
    }
    private sealed class Server : IAsyncDisposable
    {
        private readonly TextWriter output;
        private readonly TextWriter log;
        private readonly SemaphoreSlim outputLock = new(1, 1);
        private readonly Dictionary<string, ProjectState> projects = new(StringComparer.Ordinal);
        private readonly Dictionary<string, ProjectScheduler> schedulers = new(StringComparer.Ordinal);
        private readonly ConcurrentDictionary<long, CancellationTokenSource> requests = new();
        private long lastRequestId;
        private bool initialized;
        public bool ShuttingDown { get; private set; }
        public bool Fatal { get; private set; }

        public Server(TextWriter output, TextWriter log)
        {
            this.output = output;
            this.log = log;
        }
        public async Task AcceptAsync(string line)
        {
            Envelope? envelope;
            try
            {
                envelope = JsonSerializer.Deserialize<Envelope>(line, JsonOptions);
                ValidateEnvelope(envelope);
            }
            catch (Exception ex)
            {
                await FatalAsync("LanguageService.MalformedEnvelope", ex.Message);
                return;
            }

            if (envelope!.RequestId <= Interlocked.Read(ref lastRequestId))
            {
                await ErrorAsync(envelope, "LanguageService.StaleRequest", "Request IDs must increase monotonically.");
                return;
            }
            Interlocked.Exchange(ref lastRequestId, envelope.RequestId);
            if (envelope.Method == "cancel")
            {
                var id = envelope.Payload?["requestId"]?.GetValue<long>() ?? -1;
                if (requests.TryGetValue(id, out var pending)) pending.Cancel();
                await RespondAsync(envelope, new JsonObject { ["cancelledRequestId"] = id });
                return;
            }
            var cancellation = new CancellationTokenSource();
            if (!requests.TryAdd(envelope.RequestId, cancellation))
            {
                cancellation.Dispose();
                await ErrorAsync(envelope, "LanguageService.DuplicateRequest", "Request IDs must be monotonically unique.");
                return;
            }
            if (envelope.Method is "initialize" or "heartbeat" or "project/open")
            {
                await ExecuteRequestAsync(envelope, cancellation);
                return;
            }
            if (envelope.Method == "shutdown")
            {
                await DrainAsync();
                await ExecuteRequestAsync(envelope, cancellation);
                return;
            }
            if (!schedulers.TryGetValue(envelope.ProjectSessionId, out var scheduler))
            {
                await ExecuteRequestAsync(envelope, cancellation);
                return;
            }
            if (envelope.Method == "project/reconfigure")
                scheduler.EnqueueReconfiguration(envelope, cancellation);
            else
                scheduler.Enqueue(envelope, cancellation);
        }
        private async Task ExecuteRequestAsync(Envelope envelope,
            CancellationTokenSource cancellation)
        {
            try
            {
                cancellation.Token.ThrowIfCancellationRequested();
                await HandleAsync(envelope, cancellation.Token);
            }
            catch (OperationCanceledException)
            {
                await ErrorAsync(envelope, "LanguageService.Cancelled", "Request was cancelled.");
            }
            catch (ProtocolException ex)
            {
                await ErrorAsync(envelope, ex.Code, ex.Message);
            }
            catch (Exception ex)
            {
                await log.WriteLineAsync($"language-service: {ex}");
                await ErrorAsync(envelope, "LanguageService.InternalError", ex.Message);
            }
            finally
            {
                requests.TryRemove(envelope.RequestId, out _);
                cancellation.Dispose();
            }
        }
        private async Task HandleAsync(Envelope request, CancellationToken cancellation)
        {
            if (request.Method == "initialize")
            {
                Require(!initialized, "LanguageService.InvalidState", "initialize may only be sent once.");
                initialized = true;
                await RespondAsync(request, new JsonObject
                {
                    ["server"] = "SagaScript",
                    ["positionEncoding"] = "utf-16",
                    ["fullTextSynchronization"] = true,
                    ["capabilities"] = new JsonArray("diagnostics", "classifications", "completion", "hover", "definition", "references", "symbols", "preflight", "controlled-edit/plan", "controlled-projection")
                });
                return;
            }
            Require(initialized, "LanguageService.NotInitialized", "initialize is required first.");
            if (request.Method is "heartbeat" or "shutdown")
            {
                await RespondAsync(request, new JsonObject { ["alive"] = true });
                if (request.Method == "shutdown") ShuttingDown = true;
                return;
            }
            if (request.Method == "project/open")
            {
                Require(!projects.ContainsKey(request.ProjectSessionId), "LanguageService.InvalidState", "Project session is already open.");
                var openedProject = ProjectState.Create(request);
                projects.Add(request.ProjectSessionId, openedProject);
                schedulers.Add(request.ProjectSessionId,
                    new ProjectScheduler(this, openedProject));
                await RespondAsync(request, new JsonObject { ["opened"] = true });
                return;
            }
            var project = GetProject(request);
            if (request.Method == "project/reconfigure")
            {
                var acceptedIdentity = project.Reconfigure(request);
                await RespondAsync(request, new JsonObject
                {
                    ["reconfigured"] = true,
                    ["acceptedProjectSnapshotIdentity"] = acceptedIdentity
                });
                return;
            }
            if (request.Method == "project/close")
            {
                project.Close();
                await RespondAsync(request, new JsonObject { ["closed"] = true });
                return;
            }
            if (request.Method is "document/open" or "document/change" or "document/save")
            {
                project.PutDocument(request);
                await RespondAsync(request, new JsonObject { ["acceptedVersion"] = request.DocumentVersion });
                schedulers[request.ProjectSessionId].ScheduleAnalysis(request);
                return;
            }
            if (request.Method == "document/close")
            {
                project.CloseDocument(request);
                await RespondAsync(request, new JsonObject { ["closed"] = true });
                return;
            }

            var document = project.GetDocument(request);
            var position = request.Payload?["position"] is JsonObject positionNode
                ? PositionToOffset(document, positionNode) : 0;
            JsonNode payload = request.Method switch
            {
                "diagnostics" => await DiagnosticsAsync(document, project.Visibility, cancellation),
                "classifications" => await ClassificationsAsync(document, cancellation),
                "completion" => await CompletionAsync(document, position, project.Visibility, cancellation),
                "hover" => await HoverAsync(document, position, project.Visibility, cancellation),
                "definition" => await DefinitionAsync(document, position, cancellation),
                "references" => await ReferencesAsync(document, position, cancellation),
                "symbols" => await SymbolsAsync(document, cancellation),
                "preflight" => await PreflightAsync(document, request, project.Visibility, cancellation),
                "controlled-edit/plan" => await ControlledEditPlanAsync(document, request, project.Visibility, cancellation),
                "controlled-projection" => await ControlledProjectionAsync(document, request, cancellation),
                _ => throw new ProtocolException("LanguageService.UnknownMethod", $"Unknown method '{request.Method}'.")
            };
            project.EnsureCurrent(request);
            await RespondAsync(request, payload);
        }
        private ProjectState GetProject(Envelope request)
        {
            Require(projects.TryGetValue(request.ProjectSessionId, out var project), "LanguageService.ProjectClosed", "Project session is not open.");
            project!.EnsureSnapshot(request);
            return project;
        }
        public async Task DrainAsync()
        {
            foreach (var scheduler in schedulers.Values)
                await scheduler.CompleteAsync();
        }
        private async Task PublishScheduledAnalysisAsync(Envelope request,
            ProjectState project, CancellationToken cancellation)
        {
            try
            {
                await PublishAnalysisAsync(request, project, cancellation);
            }
            catch (OperationCanceledException)
            {
            }
            catch (ProtocolException exception) when (exception.Code is
                "LanguageService.StaleDocument" or
                "LanguageService.StaleProject" or
                "LanguageService.DocumentClosed" or
                "LanguageService.ProjectClosed")
            {
            }
            catch (Exception exception)
            {
                await log.WriteLineAsync($"language-service background: {exception}");
            }
        }
        private async Task PublishAnalysisAsync(Envelope request, ProjectState project, CancellationToken cancellation)
        {
            var document = project.GetDocument(request);
            var payload = new JsonObject
            {
                ["diagnostics"] = await DiagnosticsAsync(document, project.Visibility, cancellation),
                ["classifications"] = await ClassificationsAsync(document, cancellation)
            };
            project.EnsureCurrent(request);
            await NotifyAsync(request, "analysis/publish", payload);
        }
        private static async Task<JsonNode> DiagnosticsAsync(Document document,
            PackageVisibilityCatalog visibility, CancellationToken cancellation)
        {
            var model = await document.GetSemanticModelAsync(cancellation);
            Require(model is not null, "LanguageService.SemanticUnavailable", "Semantic model is unavailable.");
            var array = new JsonArray();
            foreach (var diagnostic in model!.GetDiagnostics(cancellationToken: cancellation))
            {
                if (!diagnostic.Location.IsInSource) continue;
                var span = diagnostic.Location.GetLineSpan().Span;
                array.Add(new JsonObject
                {
                    ["id"] = diagnostic.Id,
                    ["severity"] = diagnostic.Severity.ToString(),
                    ["message"] = diagnostic.GetMessage(),
                    ["range"] = Range(span)
                });
            }
            foreach (var usage in await visibility.FindDisabledUsagesAsync(
                         document, cancellation))
            {
                array.Add(PackageDiagnostic(usage));
            }
            return array;
        }
        private static JsonObject PackageDiagnostic(
            PackageVisibilityCatalog.DisabledUsage usage) => new()
        {
            ["id"] = "Script.PackageVisibility.DisabledModule",
            ["severity"] = "Error",
            ["message"] = $"Package-owned API '{usage.Owner.ApiId}' requires disabled module '{usage.Owner.PackageId}/{usage.Owner.Module}'.",
            ["range"] = Range(usage.Location.GetLineSpan().Span)
        };

        private static async Task<JsonNode> ClassificationsAsync(Document document, CancellationToken cancellation)
        {
            var text = await document.GetTextAsync(cancellation);
            var spans = await Classifier.GetClassifiedSpansAsync(document, new TextSpan(0, text.Length), cancellation);
            var array = new JsonArray();
            foreach (var span in spans)
                array.Add(new JsonObject { ["kind"] = span.ClassificationType, ["range"] = Range(text.Lines.GetLinePositionSpan(span.TextSpan)) });
            return array;
        }

        private static async Task<JsonNode> CompletionAsync(Document document,
            int position, PackageVisibilityCatalog visibility,
            CancellationToken cancellation)
        {
            var service = CompletionService.GetService(document);
            var list = service is null ? null : await service.GetCompletionsAsync(document, position, cancellationToken: cancellation);
            var recommended = (await Recommender.GetRecommendedSymbolsAtPositionAsync(
                document, position, cancellationToken: cancellation)).ToArray();
            var array = new JsonArray();
            if (list is not null)
                foreach (var item in list.ItemsList.Take(200))
                {
                    var matching = recommended.Where(symbol =>
                        symbol.Name == item.DisplayText ||
                        symbol.Name == item.FilterText).ToArray();
                    if (matching.Length > 0 && matching.All(visibility.IsDisabled))
                        continue;
                    array.Add(new JsonObject { ["label"] = item.DisplayText, ["filterText"] = item.FilterText, ["sortText"] = item.SortText, ["tags"] = new JsonArray(item.Tags.Select(tag => (JsonNode?)tag).ToArray()) });
                }
            return array;
        }

        private static async Task<JsonNode> HoverAsync(Document document,
            int position, PackageVisibilityCatalog visibility,
            CancellationToken cancellation)
        {
            var symbol = await SymbolFinder.FindSymbolAtPositionAsync(
                document, position, cancellationToken: cancellation);
            if (visibility.TryDisabledOwner(symbol, out var owner))
            {
                var diagnostics = new JsonArray();
                var root = await document.GetSyntaxRootAsync(cancellation);
                if (root is not null)
                    diagnostics.Add(PackageDiagnostic(new(
                        root.FindToken(position).GetLocation(), owner)));
                return new JsonObject { ["text"] = "", ["diagnostics"] = diagnostics };
            }
            var service = QuickInfoService.GetService(document);
            if (service is null) return new JsonObject();
            var info = await service.GetQuickInfoAsync(document, position, cancellation);
            if (info is null) return new JsonObject();
            return new JsonObject { ["text"] = string.Join("\n", info.Sections.Select(section => string.Concat(section.TaggedParts.Select(part => part.Text)))), ["spanStart"] = info.Span.Start, ["spanLength"] = info.Span.Length };
        }

        private static async Task<JsonNode> DefinitionAsync(Document document, int position, CancellationToken cancellation)
        {
            var symbol = await SymbolFinder.FindSymbolAtPositionAsync(document, position, cancellationToken: cancellation);
            var array = new JsonArray();
            if (symbol is not null)
                foreach (var location in symbol.Locations.Where(value => value.IsInSource)) array.Add(Location(location));
            return array;
        }

        private static async Task<JsonNode> ReferencesAsync(Document document, int position, CancellationToken cancellation)
        {
            var symbol = await SymbolFinder.FindSymbolAtPositionAsync(document, position, cancellationToken: cancellation);
            var array = new JsonArray();
            if (symbol is not null)
                foreach (var reference in await SymbolFinder.FindReferencesAsync(symbol, document.Project.Solution, cancellation))
                    foreach (var location in reference.Locations) array.Add(Location(location.Location));
            return array;
        }

        private static async Task<JsonNode> SymbolsAsync(Document document, CancellationToken cancellation)
        {
            var root = await document.GetSyntaxRootAsync(cancellation);
            var model = await document.GetSemanticModelAsync(cancellation);
            var array = new JsonArray();
            if (root is null || model is null) return array;
            foreach (var node in root.DescendantNodes().Where(node => node is Microsoft.CodeAnalysis.CSharp.Syntax.BaseTypeDeclarationSyntax or Microsoft.CodeAnalysis.CSharp.Syntax.MethodDeclarationSyntax or Microsoft.CodeAnalysis.CSharp.Syntax.PropertyDeclarationSyntax))
            {
                var symbol = model.GetDeclaredSymbol(node, cancellation);
                if (symbol is null) continue;
                array.Add(new JsonObject { ["name"] = symbol.Name, ["kind"] = symbol.Kind.ToString(), ["range"] = Range(node.GetLocation().GetLineSpan().Span) });
            }
            return array;
        }

        private static async Task<JsonNode> PreflightAsync(Document document,
            Envelope request, PackageVisibilityCatalog visibility,
            CancellationToken cancellation)
        {
            var candidate = request.Payload?["candidateText"]?.GetValue<string>() ?? throw new ProtocolException("LanguageService.InvalidPayload", "preflight requires candidateText.");
            var candidateDocument = document.WithText(SourceText.From(candidate, Encoding.UTF8));
            var diagnostics = await DiagnosticsAsync(candidateDocument, visibility, cancellation);
            var blocked = diagnostics.AsArray().Any(node => node?["severity"]?.GetValue<string>() == "Error");
            return new JsonObject { ["accepted"] = !blocked, ["diagnostics"] = diagnostics, ["candidateLength"] = Encoding.UTF8.GetByteCount(candidate) };
        }

        private static async Task<JsonNode> ControlledEditPlanAsync(
            Document document, Envelope request,
            PackageVisibilityCatalog visibility, CancellationToken cancellation)
        {
            const string scoreField = "field://ArenaRush.ArenaRushGameMode/EnergyCoreScore/initializer";
            var fieldId = request.Payload?["fieldId"]?.GetValue<string>() ??
                throw new ProtocolException("LanguageService.InvalidPayload", "controlled-edit/plan requires fieldId.");
            var replacement = fieldId == scoreField
                ? NormalizeInteger(request.Payload?["value"])
                : NormalizeDouble(request.Payload?["value"]);
            var resolved = await ResolveControlledFieldAsync(document, fieldId, cancellation);
            var fieldName = resolved.Field.Name;
            var initializer = resolved.Initializer;

            var text = await document.GetTextAsync(cancellation);
            var source = text.ToString();
            var oldValue = source.Substring(initializer.Span.Start, initializer.Span.Length);
            var candidate = source.Remove(initializer.Span.Start, initializer.Span.Length)
                .Insert(initializer.Span.Start, replacement);
            var candidateDocument = document.WithText(SourceText.From(candidate, Encoding.UTF8));
            var baselineDiagnostics = await DiagnosticsAsync(document, visibility, cancellation);
            var diagnostics = await DiagnosticsAsync(candidateDocument, visibility, cancellation);
            var baselineErrors = baselineDiagnostics.AsArray()
                .Where(node => node?["severity"]?.GetValue<string>() == "Error")
                .Select(DiagnosticIdentity).ToHashSet(StringComparer.Ordinal);
            var blocked = diagnostics.AsArray().Any(node =>
                node?["severity"]?.GetValue<string>() == "Error" &&
                !baselineErrors.Contains(DiagnosticIdentity(node)));
            var packageVisibilityPassed = !diagnostics.AsArray().Any(node =>
                node?["id"]?.GetValue<string>() ==
                    "Script.PackageVisibility.DisabledModule");
            blocked = blocked || !packageVisibilityPassed;
            var startByte = Encoding.UTF8.GetByteCount(source.AsSpan(0, initializer.Span.Start));
            var oldBytes = Encoding.UTF8.GetBytes(oldValue);
            return new JsonObject
            {
                ["accepted"] = !blocked,
                ["fieldId"] = fieldId,
                ["containingSymbol"] = resolved.ContainingType.ToDisplayString(),
                ["documentId"] = request.DocumentId,
                ["documentVersion"] = request.DocumentVersion,
                ["projectSnapshotIdentity"] = request.ProjectSnapshotIdentity,
                ["startByte"] = startByte,
                ["endByte"] = startByte + oldBytes.Length,
                ["oldChecksum"] = Hash(oldBytes),
                ["oldValue"] = oldValue,
                ["replacement"] = replacement,
                ["candidateHash"] = Hash(Encoding.UTF8.GetBytes(candidate)),
                ["candidateText"] = candidate,
                ["minimalDiff"] = $"- {fieldName} = {oldValue}\n+ {fieldName} = {replacement}",
                ["diagnostics"] = diagnostics,
                ["parsePassed"] = !blocked,
                ["semanticPassed"] = !blocked,
                ["packageVisibilityPassed"] = packageVisibilityPassed
            };
        }

        private static string DiagnosticIdentity(JsonNode? node) =>
            (node?["id"]?.GetValue<string>() ?? "") + "\n" +
            (node?["message"]?.GetValue<string>() ?? "") + "\n" +
            (node?["range"]?.ToJsonString() ?? "");

        private static async Task<JsonNode> ControlledProjectionAsync(
            Document document, Envelope request, CancellationToken cancellation)
        {
            const string scoreField = "field://ArenaRush.ArenaRushGameMode/EnergyCoreScore/initializer";
            const string respawnField = "field://ArenaRush.ArenaRushGameMode/EnergyCoreRespawnSeconds/initializer";
            var sourceText = request.Payload?["sourceText"]?.GetValue<string>() ??
                throw new ProtocolException("LanguageService.InvalidPayload",
                    "controlled-projection requires exact sourceText.");
            var sourceHash = request.Payload?["sourceHash"]?.GetValue<string>() ?? "";
            Require(sourceHash == Hash(Encoding.UTF8.GetBytes(sourceText)),
                "LanguageService.CandidateIdentityChanged",
                "Controlled projection source hash does not match sourceText.");
            document = document.WithText(SourceText.From(sourceText, Encoding.UTF8));
            var text = await document.GetTextAsync(cancellation);
            var source = text.ToString();
            Require(source == sourceText,
                "LanguageService.CandidateIdentityChanged",
                "Controlled projection source identity changed.");
            var fields = new JsonArray();
            foreach (var definition in new[]
            {
                (Id: scoreField, ValueType: "Int32", Minimum: "1"),
                (Id: respawnField, ValueType: "Double", Minimum: "0.0")
            })
            {
                try
                {
                    var resolved = await ResolveControlledFieldAsync(document, definition.Id, cancellation);
                    var initializer = resolved.Initializer;
                    var oldValue = source.Substring(initializer.Span.Start, initializer.Span.Length);
                    var startByte = Encoding.UTF8.GetByteCount(source.AsSpan(0, initializer.Span.Start));
                    var bytes = Encoding.UTF8.GetBytes(oldValue);
                    fields.Add(new JsonObject
                    {
                        ["fieldId"] = definition.Id,
                        ["valueType"] = definition.ValueType,
                        ["minimum"] = definition.Minimum,
                        ["containingSymbol"] = resolved.ContainingType.ToDisplayString(),
                        ["initializerRole"] = "ConstantInitializerLiteral",
                        ["startByte"] = startByte,
                        ["endByte"] = startByte + bytes.Length,
                        ["checksum"] = Hash(bytes),
                        ["value"] = oldValue
                    });
                }
                catch (ProtocolException)
                {
                    // A changed source shape is represented as an opaque projection.
                }
            }
            return new JsonObject
            {
                ["documentId"] = request.DocumentId,
                ["documentVersion"] = request.DocumentVersion,
                ["projectSnapshotIdentity"] = request.ProjectSnapshotIdentity,
                ["sourceHash"] = sourceHash,
                ["editableFields"] = fields,
                ["opaque"] = fields.Count != 2,
                ["opaqueReason"] = fields.Count == 2 ? "" :
                    "Supported constant initializer shape is unavailable."
            };
        }

        private static async Task<ControlledField> ResolveControlledFieldAsync(
            Document document, string fieldId, CancellationToken cancellation)
        {
            const string scoreField = "field://ArenaRush.ArenaRushGameMode/EnergyCoreScore/initializer";
            const string respawnField = "field://ArenaRush.ArenaRushGameMode/EnergyCoreRespawnSeconds/initializer";
            var fieldName = fieldId switch
            {
                scoreField => "EnergyCoreScore",
                respawnField => "EnergyCoreRespawnSeconds",
                _ => throw new ProtocolException("LanguageService.OpaqueField", "Field is not a supported controlled edit surface.")
            };
            var compilation = await document.Project.GetCompilationAsync(cancellation);
            var type = compilation?.GetTypeByMetadataName("ArenaRush.ArenaRushGameMode") ??
                throw new ProtocolException("LanguageService.FieldShapeChanged", "Containing type is unavailable.");
            var field = type.GetMembers(fieldName).OfType<IFieldSymbol>().SingleOrDefault();
            Require(field is { IsConst: true }, "LanguageService.FieldShapeChanged", "Controlled field must remain a unique constant.");
            var syntax = await field!.DeclaringSyntaxReferences.Single().GetSyntaxAsync(cancellation);
            Require(syntax is VariableDeclaratorSyntax { Initializer: not null },
                "LanguageService.FieldShapeChanged", "Controlled field initializer is unavailable.");
            var initializer = ((VariableDeclaratorSyntax)syntax).Initializer!.Value;
            Require(initializer is LiteralExpressionSyntax,
                "LanguageService.FieldShapeChanged", "Controlled field initializer must remain a literal.");
            Require(fieldId != scoreField || field.Type.SpecialType == SpecialType.System_Int32,
                "LanguageService.FieldShapeChanged", "EnergyCoreScore must remain an int constant.");
            Require(fieldId != respawnField || field.Type.SpecialType == SpecialType.System_Double,
                "LanguageService.FieldShapeChanged", "EnergyCoreRespawnSeconds must remain a double constant.");
            return new ControlledField(type, field, initializer);
        }

        private sealed record ControlledField(
            INamedTypeSymbol ContainingType, IFieldSymbol Field, ExpressionSyntax Initializer);

        private static string NormalizeInteger(JsonNode? value)
        {
            var parsed = value is JsonValue json && json.TryGetValue<int>(out var number)
                ? number
                : int.TryParse(value?.ToString(), NumberStyles.Integer,
                    CultureInfo.InvariantCulture, out number) ? number : 0;
            Require(parsed >= 1, "LanguageService.InvalidControlledValue", "Integer value must be at least 1.");
            return parsed.ToString(CultureInfo.InvariantCulture);
        }

        private static string NormalizeDouble(JsonNode? value)
        {
            var parsed = value is JsonValue json && json.TryGetValue<double>(out var number)
                ? number
                : double.TryParse(value?.ToString(), NumberStyles.Float,
                    CultureInfo.InvariantCulture, out number) ? number : double.NaN;
            Require(double.IsFinite(parsed) && parsed >= 0,
                "LanguageService.InvalidControlledValue", "Double value must be finite and non-negative.");
            var normalized = parsed.ToString("R", CultureInfo.InvariantCulture);
            if (!normalized.Contains('.') && !normalized.Contains('E') && !normalized.Contains('e'))
                normalized += ".0";
            return normalized;
        }

        private static string Hash(ReadOnlySpan<byte> bytes) =>
            "sha256:" + Convert.ToHexString(SHA256.HashData(bytes)).ToLowerInvariant();

        public async Task FatalAsync(string code, string message)
        {
            Fatal = true;
            ShuttingDown = true;
            await WriteAsync(new JsonObject { ["schemaVersion"] = SchemaVersion, ["protocolVersion"] = ProtocolVersion, ["messageType"] = "fatal", ["requestId"] = 0, ["projectSessionId"] = "", ["method"] = "protocol/fatal", ["projectSnapshotIdentity"] = "", ["payload"] = new JsonObject { ["code"] = code, ["message"] = message } });
        }

        private Task RespondAsync(Envelope request, JsonNode payload) => WriteAsync(Response(request, "response", payload));
        private Task ErrorAsync(Envelope request, string code, string message) => WriteAsync(Response(request, "error", new JsonObject { ["code"] = code, ["message"] = message }));
        private Task NotifyAsync(Envelope request, string method, JsonNode payload)
        {
            var node = Response(request, "notification", payload);
            node["method"] = method;
            return WriteAsync(node);
        }

        private static JsonObject Response(Envelope request, string type, JsonNode payload) => new()
        {
            ["schemaVersion"] = SchemaVersion, ["protocolVersion"] = ProtocolVersion, ["messageType"] = type,
            ["requestId"] = request.RequestId, ["projectSessionId"] = request.ProjectSessionId,
            ["method"] = request.Method, ["projectSnapshotIdentity"] = request.ProjectSnapshotIdentity,
            ["documentId"] = request.DocumentId, ["documentVersion"] = request.DocumentVersion, ["payload"] = payload
        };

        private async Task WriteAsync(JsonObject node)
        {
            await outputLock.WaitAsync();
            try { await output.WriteLineAsync(node.ToJsonString(JsonOptions)); await output.FlushAsync(); }
            finally { outputLock.Release(); }
        }

        public ValueTask DisposeAsync()
        {
            foreach (var request in requests.Values) request.Cancel();
            foreach (var scheduler in schedulers.Values) scheduler.Dispose();
            schedulers.Clear();
            foreach (var project in projects.Values) project.Dispose();
            projects.Clear();
            outputLock.Dispose();
            return ValueTask.CompletedTask;
        }

        private sealed class ProjectScheduler : IDisposable
        {
            private static readonly TimeSpan AnalysisDebounce =
                TimeSpan.FromMilliseconds(150);
            private readonly Server server;
            private readonly ProjectState project;
            private readonly object gate = new();
            private readonly LinkedList<(Envelope Request,
                CancellationTokenSource Cancellation)> foreground = new();
            private readonly Dictionary<string, ScheduledAnalysis> background =
                new(StringComparer.Ordinal);
            private readonly SemaphoreSlim signal = new(0);
            private readonly CancellationTokenSource lifetime = new();
            private readonly Task worker;
            private CancellationTokenSource? runningBackground;
            private CancellationTokenSource? runningForeground;
            private Envelope? runningRequest;
            private bool completing;

            public ProjectScheduler(Server server, ProjectState project)
            {
                this.server = server;
                this.project = project;
                worker = Task.Run(RunAsync);
            }

            public void Enqueue(Envelope request,
                CancellationTokenSource cancellation)
            {
                lock (gate)
                {
                    foreground.AddLast((request, cancellation));
                    runningBackground?.Cancel();
                }
                signal.Release();
            }

            public void EnqueueReconfiguration(Envelope request,
                CancellationTokenSource cancellation)
            {
                lock (gate)
                {
                    if (runningRequest is not null &&
                        !IsDocumentMutation(runningRequest.Method))
                        runningForeground?.Cancel();
                    runningBackground?.Cancel();
                    foreach (var value in foreground)
                        if (!IsDocumentMutation(value.Request.Method))
                            value.Cancellation.Cancel();
                    foreach (var value in background.Values)
                        value.Cancellation.Cancel();
                    background.Clear();
                    foreground.AddLast((request, cancellation));
                }
                signal.Release();
            }

            public void ScheduleAnalysis(Envelope request)
            {
                if (string.IsNullOrEmpty(request.DocumentId)) return;
                lock (gate)
                {
                    if (background.Remove(request.DocumentId!, out var older))
                        older.Cancellation.Cancel();
                    var cancellation = new CancellationTokenSource();
                    background.Add(request.DocumentId!, new ScheduledAnalysis(
                        request, DateTime.UtcNow + AnalysisDebounce, cancellation));
                    runningBackground?.Cancel();
                }
                signal.Release();
            }

            public async Task CompleteAsync()
            {
                lock (gate) completing = true;
                signal.Release();
                await worker;
            }

            private async Task RunAsync()
            {
                while (!lifetime.IsCancellationRequested)
                {
                    (Envelope Request, CancellationTokenSource Cancellation)? next = null;
                    ScheduledAnalysis? analysis = null;
                    TimeSpan delay = Timeout.InfiniteTimeSpan;
                    lock (gate)
                    {
                        if (foreground.Count > 0)
                        {
                            next = foreground.First!.Value;
                            foreground.RemoveFirst();
                            runningForeground = next.Value.Cancellation;
                            runningRequest = next.Value.Request;
                        }
                        else if (background.Count > 0)
                        {
                            var candidate = background.Values
                                .OrderBy(value => value.Due).First();
                            delay = candidate.Due - DateTime.UtcNow;
                            if (delay <= TimeSpan.Zero)
                            {
                                analysis = candidate;
                                background.Remove(candidate.Request.DocumentId!);
                                runningBackground = candidate.Cancellation;
                            }
                        }
                        else if (completing)
                            return;
                    }
                    if (next is not null)
                    {
                        await server.ExecuteRequestAsync(next.Value.Request,
                            next.Value.Cancellation);
                        lock (gate)
                        {
                            if (ReferenceEquals(runningForeground,
                                next.Value.Cancellation))
                            {
                                runningForeground = null;
                                runningRequest = null;
                            }
                        }
                        continue;
                    }
                    if (analysis is not null)
                    {
                        await server.PublishScheduledAnalysisAsync(
                            analysis.Request, project,
                            analysis.Cancellation.Token);
                        lock (gate)
                        {
                            if (ReferenceEquals(runningBackground,
                                    analysis.Cancellation))
                                runningBackground = null;
                        }
                        analysis.Cancellation.Dispose();
                        continue;
                    }
                    try
                    {
                        if (delay == Timeout.InfiniteTimeSpan)
                            await signal.WaitAsync(lifetime.Token);
                        else
                            await LanguageServiceSchedulerWait.WaitAsync(
                                signal, lifetime.Token, delay);
                    }
                    catch (OperationCanceledException)
                    {
                        return;
                    }
                }
            }

            public void Dispose()
            {
                lifetime.Cancel();
                lock (gate)
                {
                    runningBackground?.Cancel();
                    runningForeground?.Cancel();
                    foreach (var value in foreground)
                        value.Cancellation.Cancel();
                    foreach (var value in background.Values)
                        value.Cancellation.Cancel();
                }
                signal.Dispose();
                lifetime.Dispose();
            }

            private sealed record ScheduledAnalysis(Envelope Request,
                DateTime Due, CancellationTokenSource Cancellation);

            private static bool IsDocumentMutation(string method) => method is
                "document/open" or "document/change" or "document/save" or
                "document/close";
        }
    }

    private sealed class ProjectState : IDisposable
    {
        private readonly AdhocWorkspace workspace;
        private readonly string root;
        private readonly string packageProjectId;
        private ProjectId projectId;
        private readonly Dictionary<string, DocumentState> documents = new(StringComparer.Ordinal);
        private bool closed;
        public string SnapshotIdentity { get; private set; }
        public PackageVisibilityCatalog Visibility { get; private set; }

        private ProjectState(AdhocWorkspace workspace, ProjectId projectId,
            string root, string packageProjectId, string identity,
            PackageVisibilityCatalog visibility)
        { this.workspace = workspace; this.projectId = projectId; this.root = root;
          this.packageProjectId = packageProjectId;
          SnapshotIdentity = identity; Visibility = visibility; }

        public static ProjectState Create(Envelope request)
        {
            var payload = request.Payload ?? throw new ProtocolException("LanguageService.InvalidPayload", "project/open requires payload.");
            var root = Canonical(payload["canonicalRoot"]?.GetValue<string>() ?? "");
            Require(Directory.Exists(root), "LanguageService.InvalidRoot", "canonicalRoot must exist.");
            LanguageServiceProjectSnapshotValidator.Validate(payload, root);
            var workspace = new AdhocWorkspace(MefHostServices.DefaultHost);
            var id = ProjectId.CreateNewId(request.ProjectSessionId);
            var parse = new CSharpParseOptions(LanguageVersion.LatestMajor);
            var compilation = new CSharpCompilationOptions(
                OutputKind.DynamicallyLinkedLibrary, allowUnsafe: false);
            var references = LanguageServiceReferenceFactory.Create(payload, root);
            var visibility = PackageVisibilityCatalog.LoadLanguageService(payload, root);
            var info = ProjectInfo.Create(id, VersionStamp.Create(), request.ProjectSessionId, request.ProjectSessionId, LanguageNames.CSharp, parseOptions: parse, compilationOptions: compilation, metadataReferences: references);
            Require(workspace.AddProject(info) is not null, "LanguageService.WorkspaceFailure", "Unable to create project.");
            const string implicitUsings =
                "global using System;\n" +
                "global using System.Collections.Generic;\n" +
                "global using System.Linq;\n" +
                "global using System.Threading;\n" +
                "global using System.Threading.Tasks;\n";
            Require(workspace.AddDocument(id, "SagaImplicitUsings.g.cs",
                SourceText.From(implicitUsings, Encoding.UTF8)) is not null,
                "LanguageService.WorkspaceFailure", "Unable to add implicit usings.");
            return new ProjectState(workspace, id, root,
                payload["projectId"]?.GetValue<string>() ?? "",
                request.ProjectSnapshotIdentity, visibility);
        }

        public string Reconfigure(Envelope request)
        {
            EnsureSnapshot(request);
            var payload = request.Payload ?? throw new ProtocolException(
                "LanguageService.InvalidPayload", "project/reconfigure requires payload.");
            var candidateIdentity =
                payload["candidateProjectSnapshotIdentity"]?.GetValue<string>() ?? "";
            Require(!string.IsNullOrWhiteSpace(candidateIdentity) &&
                candidateIdentity != SnapshotIdentity,
                "LanguageService.InvalidProjectIdentity",
                "A distinct candidate project snapshot identity is required.");
            Require((payload["projectId"]?.GetValue<string>() ?? "") ==
                packageProjectId,
                "LanguageService.InvalidProjectIdentity",
                "Candidate project identity does not match the open project.");
            Require(Canonical(payload["canonicalRoot"]?.GetValue<string>() ?? "") == root,
                "LanguageService.InvalidRoot",
                "Candidate canonical root does not match the open project.");
            LanguageServiceProjectSnapshotValidator.Validate(payload, root);
            var references = LanguageServiceReferenceFactory.Create(payload, root)
                .ToArray();
            var visibility = PackageVisibilityCatalog.LoadLanguageService(payload, root);
            var solution = workspace.CurrentSolution.WithProjectMetadataReferences(
                projectId, references)
                .WithProjectParseOptions(projectId,
                    new CSharpParseOptions(LanguageVersion.LatestMajor))
                .WithProjectCompilationOptions(projectId,
                    new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary,
                        allowUnsafe: false));
            Require(solution.GetProject(projectId)?.GetCompilationAsync()
                .GetAwaiter().GetResult() is not null,
                "LanguageService.CompilationUnavailable",
                "Unable to construct the candidate compilation.");
            Require(workspace.TryApplyChanges(solution), "LanguageService.WorkspaceFailure",
                "Unable to reconfigure project references.");
            SnapshotIdentity = candidateIdentity;
            Visibility = visibility;
            foreach (var document in documents.Values) document.SnapshotIdentity = SnapshotIdentity;
            return candidateIdentity;
        }

        public void PutDocument(Envelope request)
        {
            Require(!string.IsNullOrWhiteSpace(request.DocumentId) && request.DocumentVersion.HasValue, "LanguageService.InvalidDocument", "Document identity and version are required.");
            var path = Canonical(request.Payload?["path"]?.GetValue<string>() ?? "");
            Require(IsContained(root, path), "LanguageService.PathEscape", "Document path escapes the canonical project root.");
            var text = request.Payload?["text"]?.GetValue<string>() ?? throw new ProtocolException("LanguageService.InvalidPayload", "Full document text is required.");
            if (documents.TryGetValue(request.DocumentId!, out var existing))
            {
                Require(request.DocumentVersion > existing.Version, "LanguageService.StaleDocument", "Document version is stale.");
                var solution = workspace.CurrentSolution.WithDocumentText(existing.Id, SourceText.From(text, Encoding.UTF8), PreservationMode.PreserveIdentity);
                Require(workspace.TryApplyChanges(solution), "LanguageService.WorkspaceFailure", "Unable to replace document text.");
                existing.Version = request.DocumentVersion!.Value;
                existing.SnapshotIdentity = request.ProjectSnapshotIdentity;
            }
            else
            {
                Require(request.Method == "document/open", "LanguageService.DocumentClosed", "document/open is required first.");
                var id = DocumentId.CreateNewId(projectId, request.DocumentId);
                var info = DocumentInfo.Create(id, Path.GetFileName(path), filePath: path, loader: TextLoader.From(TextAndVersion.Create(SourceText.From(text, Encoding.UTF8), VersionStamp.Create(), path)));
                Require(workspace.AddDocument(info) is not null, "LanguageService.WorkspaceFailure", "Unable to add document.");
                documents.Add(request.DocumentId!, new DocumentState(id, request.DocumentVersion!.Value, request.ProjectSnapshotIdentity));
            }
        }

        public Document GetDocument(Envelope request)
        {
            EnsureCurrent(request);
            return workspace.CurrentSolution.GetDocument(documents[request.DocumentId!].Id)!;
        }

        public void CloseDocument(Envelope request)
        {
            EnsureCurrent(request);
            var state = documents[request.DocumentId!];
            Require(workspace.TryApplyChanges(workspace.CurrentSolution.RemoveDocument(state.Id)), "LanguageService.WorkspaceFailure", "Unable to remove document.");
            documents.Remove(request.DocumentId!);
        }

        public void EnsureSnapshot(Envelope request) => Require(request.ProjectSnapshotIdentity == SnapshotIdentity, "LanguageService.StaleProject", "Project snapshot identity is stale.");
        public void EnsureCurrent(Envelope request)
        {
            Require(!closed, "LanguageService.ProjectClosed",
                "Project session is closed.");
            EnsureSnapshot(request);
            if (request.DocumentId is null || !documents.TryGetValue(request.DocumentId, out var state))
                throw new ProtocolException("LanguageService.DocumentClosed", "Document is not open.");
            Require(request.DocumentVersion == state.Version, "LanguageService.StaleDocument", "Document version is stale.");
        }
        public void Close()
        {
            closed = true;
            foreach (var document in documents.Values)
                workspace.TryApplyChanges(
                    workspace.CurrentSolution.RemoveDocument(document.Id));
            documents.Clear();
        }
        public void Dispose() => workspace.Dispose();
    }

    private sealed record DocumentState(DocumentId Id, long InitialVersion, string InitialSnapshot)
    { public long Version { get; set; } = InitialVersion; public string SnapshotIdentity { get; set; } = InitialSnapshot; }

    private sealed class Envelope
    {
        public int SchemaVersion { get; set; }
        public int ProtocolVersion { get; set; }
        public string MessageType { get; set; } = "";
        public long RequestId { get; set; }
        public string ProjectSessionId { get; set; } = "";
        public string Method { get; set; } = "";
        public string ProjectSnapshotIdentity { get; set; } = "";
        public string? DocumentId { get; set; }
        public long? DocumentVersion { get; set; }
        public JsonObject? Payload { get; set; }
    }

    internal sealed class ProtocolException(string code, string message) : Exception(message) { public string Code { get; } = code; }
    private static readonly JsonSerializerOptions JsonOptions = new(JsonSerializerDefaults.Web) { WriteIndented = false };
    private static void ValidateEnvelope(Envelope? value)
    {
        Require(value is not null, "LanguageService.MalformedEnvelope", "Envelope is null.");
        Require(value!.SchemaVersion == SchemaVersion && value.ProtocolVersion == ProtocolVersion, "LanguageService.UnsupportedVersion", "Only schema/protocol version 1 is supported.");
        Require(value.MessageType == "request" && value.RequestId > 0 && !string.IsNullOrWhiteSpace(value.ProjectSessionId) && !string.IsNullOrWhiteSpace(value.Method) && value.ProjectSnapshotIdentity is not null, "LanguageService.MalformedEnvelope", "Required envelope fields are missing.");
    }
    internal static void Require(bool condition, string code, string message) { if (!condition) throw new ProtocolException(code, message); }
    internal static string Canonical(string path) =>
        LanguageServicePathSupport.Canonical(path);
    internal static bool IsContained(string root, string path) => path == root || path.StartsWith(root.TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar, OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal);
    private static int PositionToOffset(Document document, JsonObject position)
    {
        var text = document.GetTextAsync().GetAwaiter().GetResult();
        var line = position["line"]?.GetValue<int>() ?? -1;
        var character = position["character"]?.GetValue<int>() ?? -1;
        Require(line >= 0 && line < text.Lines.Count && character >= 0 && character <= text.Lines[line].Span.Length, "LanguageService.InvalidPosition", "Position is outside the document.");
        return text.Lines[line].Start + character;
    }
    private static JsonObject Range(LinePositionSpan span) => new() { ["start"] = new JsonObject { ["line"] = span.Start.Line, ["character"] = span.Start.Character }, ["end"] = new JsonObject { ["line"] = span.End.Line, ["character"] = span.End.Character } };
    private static JsonObject Location(Location location) => new() { ["path"] = location.SourceTree?.FilePath ?? "", ["range"] = Range(location.GetLineSpan().Span) };
}
