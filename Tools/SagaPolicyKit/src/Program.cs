using System.Text.Json;

namespace SagaPolicyKit;

internal static class Program
{
    private const int Passed = 0;
    private const int Failed = 1;
    private const int InvalidUsage = 2;
    private const int InternalError = 4;

    public static int Main(string[] args)
    {
        try
        {
            if (args.Length == 0 || args[0] is "-h" or "--help" or "help")
            {
                PrintUsage();
                return args.Length == 0 ? InvalidUsage : Passed;
            }

            var command = args[0];
            var options = CommandOptions.Parse(args.Skip(1).ToArray());
            if (!options.Ok)
            {
                Console.Error.WriteLine(options.Error);
                PrintUsage();
                return InvalidUsage;
            }

            return command switch
            {
                "evaluate" => RunEvaluate(options),
                _ => InvalidUsage,
            };
        }
        catch (Exception e)
        {
            Console.Error.WriteLine($"Internal sagapolicy error: {e.Message}");
            return InternalError;
        }
    }

    private static int RunEvaluate(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.PolicyPath) ||
            string.IsNullOrWhiteSpace(options.RequestPath) ||
            string.IsNullOrWhiteSpace(options.OutPath))
        {
            Console.Error.WriteLine("evaluate requires --policy, --request, and --out.");
            return InvalidUsage;
        }

        var diagnostics = new List<Diagnostic>
        {
            Info(
                "Policy.ReportOnly",
                "SagaPolicyKit evaluates local policy and does not enforce permissions, authenticate users, contact a network, or mutate source.",
                options.PolicyPath),
        };

        if (!File.Exists(options.PolicyPath))
        {
            diagnostics.Add(Error("Policy.Missing", "Policy file is missing.", options.PolicyPath));
            WriteReport(options.OutPath, "MissingEvidence", "MissingEvidence", new(), diagnostics, []);
            return Failed;
        }
        if (!File.Exists(options.RequestPath))
        {
            diagnostics.Add(Error("Policy.RequestMissing", "Policy request file is missing.", options.RequestPath));
            WriteReport(options.OutPath, "MissingEvidence", "MissingEvidence", new(), diagnostics, []);
            return Failed;
        }

        PolicyDefinition policy;
        PolicyRequest request;
        try
        {
            policy = JsonSerializer.Deserialize<PolicyDefinition>(
                    File.ReadAllText(options.PolicyPath),
                    ReportWriter.Options)
                ?? new PolicyDefinition();
            request = JsonSerializer.Deserialize<PolicyRequest>(
                    File.ReadAllText(options.RequestPath),
                    ReportWriter.Options)
                ?? new PolicyRequest();
        }
        catch (JsonException e)
        {
            diagnostics.Add(Error("Policy.InvalidJson", $"Policy input is invalid JSON: {e.Message}", options.PolicyPath));
            WriteReport(options.OutPath, "Failed", "MissingEvidence", new(), diagnostics, []);
            return Failed;
        }

        var decision = Evaluate(policy, request, diagnostics);
        var status = decision switch
        {
            "Allow" => "Passed",
            "Warn" => "PassedWithWarnings",
            "RequiresReview" => "ReviewRequired",
            "Deny" => "Denied",
            "MissingEvidence" => "MissingEvidence",
            "UnknownSubject" => "UnknownSubject",
            "UnknownAction" => "UnknownAction",
            "UnknownResource" => "UnknownResource",
            _ => "NotApplicable",
        };
        WriteReport(options.OutPath, status, decision, request, diagnostics, request.Evidence);
        return decision is "Allow" or "Warn" ? Passed : Failed;
    }

    private static string Evaluate(PolicyDefinition policy, PolicyRequest request, List<Diagnostic> diagnostics)
    {
        var role = policy.Roles.FirstOrDefault(item => item.Id == request.Role);
        if (role is null)
        {
            diagnostics.Add(Error("Policy.UnknownRole", "Policy role is unknown.", request.Role));
            return "UnknownSubject";
        }

        var knownActions = policy.Roles
            .SelectMany(item => item.Allow.Concat(item.Deny).Concat(item.Warn).Concat(item.RequiresReview))
            .Concat(policy.DangerousOperations)
            .Concat(policy.RequiredEvidence.Select(item => item.Action))
            .ToHashSet(StringComparer.Ordinal);
        if (!knownActions.Contains(request.Action))
        {
            diagnostics.Add(Error("Policy.UnknownAction", "Policy action is unknown.", request.Action));
            return "UnknownAction";
        }

        if (!policy.Resources.Any(item => item.Id == request.Resource))
        {
            diagnostics.Add(Error("Policy.UnknownResource", "Policy resource is unknown.", request.Resource));
            return "UnknownResource";
        }

        var missingEvidence = policy.RequiredEvidence
            .Where(item => item.Action == request.Action && item.Resource == request.Resource)
            .Where(item => !request.Evidence.Any(evidence =>
                evidence.EvidenceId == item.EvidenceId &&
                EvidenceStatusPassed(evidence.Status)))
            .ToArray();
        if (missingEvidence.Length > 0)
        {
            foreach (var item in missingEvidence)
            {
                diagnostics.Add(Error("Policy.MissingEvidence", "Required policy evidence is missing.", item.Path));
            }
            return "MissingEvidence";
        }

        if (role.Deny.Contains(request.Action, StringComparer.Ordinal))
        {
            diagnostics.Add(Error("Policy.Denied", "Policy role denies this action.", request.Action));
            return "Deny";
        }

        if (role.RequiresReview.Contains(request.Action, StringComparer.Ordinal))
        {
            diagnostics.Add(Warn("Policy.RequiresReview", "Policy role requires review before this dangerous operation.", request.Action));
            return "RequiresReview";
        }

        if (role.Warn.Contains(request.Action, StringComparer.Ordinal))
        {
            diagnostics.Add(Warn("Policy.Warn", "Policy role allows this action with a warning.", request.Action));
            return "Warn";
        }

        if (role.Allow.Contains(request.Action, StringComparer.Ordinal))
        {
            diagnostics.Add(Info("Policy.Allowed", "Policy role allows this action in report-only mode.", request.Action));
            return "Allow";
        }

        if (policy.DangerousOperations.Contains(request.Action, StringComparer.Ordinal))
        {
            diagnostics.Add(Warn("Policy.RequiresReview", "Dangerous operation requires review by default.", request.Action));
            return "RequiresReview";
        }

        diagnostics.Add(Info("Policy.NotApplicable", "No policy rule applies to this request.", request.Action));
        return "NotApplicable";
    }

    private static bool EvidenceStatusPassed(string status) =>
        status is "Passed" or "Accepted" or "PartiallyProven" ||
        status.StartsWith("PassedWith", StringComparison.Ordinal);

    private static void WriteReport(
        string outPath,
        string status,
        string decision,
        PolicyRequest request,
        IReadOnlyList<Diagnostic> diagnostics,
        IReadOnlyList<EvidenceReference> evidence)
    {
        var report = new PolicyEvaluationReport
        {
            Status = status,
            Decision = decision,
            Subject = request.Subject,
            Resource = request.Resource,
            Action = request.Action,
            Role = request.Role,
            Scope = request.Scope,
            Diagnostics = diagnostics
                .OrderBy(item => item.Code, StringComparer.Ordinal)
                .ThenBy(item => item.Path, StringComparer.Ordinal)
                .ToArray(),
            Evidence = evidence
                .OrderBy(item => item.EvidenceId, StringComparer.Ordinal)
                .ThenBy(item => item.Path, StringComparer.Ordinal)
                .ToArray(),
            MutatesSource = false,
            Enforcement = "ReportOnly",
        };
        ReportWriter.Write(outPath, report);
    }

    private static Diagnostic Error(string code, string message, string path = "") =>
        new() { Severity = "Error", Code = code, Message = message, Path = path };

    private static Diagnostic Warn(string code, string message, string path = "") =>
        new() { Severity = "Warning", Code = code, Message = message, Path = path };

    private static Diagnostic Info(string code, string message, string path = "") =>
        new() { Severity = "Info", Code = code, Message = message, Path = path };

    private static void PrintUsage()
    {
        Console.Error.WriteLine("SagaPolicyKit CLI");
        Console.Error.WriteLine("  sagapolicy evaluate --policy <policy.json> --request <request.json> --out <policy_evaluation_report.json>");
    }

    private sealed record CommandOptions(bool Ok, string Error, string PolicyPath, string RequestPath, string OutPath)
    {
        public static CommandOptions Parse(string[] args)
        {
            string policy = "";
            string request = "";
            string output = "";
            for (var i = 0; i < args.Length; ++i)
            {
                switch (args[i])
                {
                    case "--policy" when i + 1 < args.Length:
                        policy = args[++i];
                        break;
                    case "--request" when i + 1 < args.Length:
                        request = args[++i];
                        break;
                    case "--out" when i + 1 < args.Length:
                        output = args[++i];
                        break;
                    default:
                        return Fail($"Unknown or incomplete argument: {args[i]}");
                }
            }

            return new CommandOptions(true, "", policy, request, output);
        }

        private static CommandOptions Fail(string error) => new(false, error, "", "", "");
    }
}
