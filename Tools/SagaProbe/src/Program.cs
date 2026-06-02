namespace SagaProbe;

internal static class Program
{
    private const int Passed = 0;
    private const int Failed = 1;
    private const int InvalidUsage = 2;
    private const int MissingInput = 3;
    private const int InternalError = 4;

    public static int Main(string[] args)
    {
        try
        {
            if (args.Length == 0 || args[0] is "--help" or "-h" or "help")
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
                "summarize" => RunSummarize(options),
                "compare" => RunCompare(options),
                "latest" => RunLatest(options),
                _ => InvalidUsage,
            };
        }
        catch (ReportLoadException e)
        {
            Console.Error.WriteLine($"{e.Code}: {e.Message}");
            return e.Code == "Probe.Input.ReportMissing" ? MissingInput : Failed;
        }
        catch (Exception e)
        {
            Console.Error.WriteLine($"Internal sagaprobe error: {e.Message}");
            return InternalError;
        }
    }

    private static int RunSummarize(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.Input) ||
            string.IsNullOrWhiteSpace(options.OutPath))
        {
            Console.Error.WriteLine("summarize requires --input and --out.");
            return InvalidUsage;
        }

        var report = ReportNormalizer.Normalize(options.Input);
        var summary = ReportNormalizer.BuildSummary(report, "summarize");
        ReportWriter.Write(options.OutPath, summary);
        PrintSummary(summary);
        return Passed;
    }

    private static int RunCompare(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.Baseline) ||
            string.IsNullOrWhiteSpace(options.Candidate) ||
            string.IsNullOrWhiteSpace(options.OutPath))
        {
            Console.Error.WriteLine("compare requires --baseline, --candidate, and --out.");
            return InvalidUsage;
        }

        var baseline = ReportNormalizer.Normalize(options.Baseline);
        var candidate = ReportNormalizer.Normalize(options.Candidate);
        var diff = ReportComparer.Compare(baseline, candidate);
        ReportWriter.Write(options.OutPath, diff);
        Console.WriteLine(
            $"SagaProbe compare: {diff.Status}; metrics={diff.Summary.ChangedMetricCount}; diagnostics+={diff.Summary.AddedDiagnosticCount}; diagnostics-={diff.Summary.RemovedDiagnosticCount}; missingSections={diff.Summary.MissingSectionCount}");
        return diff.Status == "Passed" ? Passed : Failed;
    }

    private static int RunLatest(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.ReportsDir) ||
            string.IsNullOrWhiteSpace(options.OutPath))
        {
            Console.Error.WriteLine("latest requires --reports-dir and --out.");
            return InvalidUsage;
        }

        var reportsDir = Path.GetFullPath(options.ReportsDir);
        if (!Directory.Exists(reportsDir))
        {
            throw new ReportLoadException(
                "Probe.Input.ReportsDirectoryMissing",
                $"Reports directory does not exist: {reportsDir}");
        }

        var candidates = Directory
            .EnumerateFiles(reportsDir, "*.json", SearchOption.AllDirectories)
            .Select(path => new
            {
                Absolute = Path.GetFullPath(path),
                Relative = Path.GetRelativePath(reportsDir, path).Replace('\\', '/'),
            })
            .OrderBy(item => item.Relative, StringComparer.Ordinal)
            .Where(item => ReportNormalizer.LooksSupported(item.Absolute))
            .ToList();
        if (candidates.Count == 0)
        {
            throw new ReportLoadException(
                "Probe.Latest.NoSupportedReports",
                "No supported diagnostics reports were found.");
        }

        var selected = candidates[^1].Absolute;
        var report = ReportNormalizer.Normalize(selected);
        var summary = ReportNormalizer.BuildSummary(report, "latest");
        ReportWriter.Write(options.OutPath, summary);
        Console.WriteLine($"SagaProbe latest: selected {selected}");
        PrintSummary(summary);
        return Passed;
    }

    private static void PrintSummary(DiagnosticsSummaryReport summary)
    {
        Console.WriteLine(
            $"SagaProbe {summary.Command}: {summary.Status} ({summary.SourceReport.ReportKind})");
        Console.WriteLine($"source: {summary.SourceReport.Path}");
        Console.WriteLine(
            $"diagnostics: critical={summary.Summary.CriticalDiagnosticCount}, warnings={summary.Summary.WarningDiagnosticCount}, total={summary.Summary.DiagnosticCount}");
        Console.WriteLine(
            $"metrics={summary.Summary.MetricCount}, sections={summary.Summary.SectionCount}, faults={summary.Summary.FaultCount}, leaks={summary.Summary.LeakCount}");
    }

    private static void PrintUsage()
    {
        Console.Error.WriteLine("SagaProbe CLI");
        Console.Error.WriteLine("  sagaprobe summarize --input <report> --out <summary>");
        Console.Error.WriteLine("  sagaprobe compare --baseline <report> --candidate <report> --out <diff>");
        Console.Error.WriteLine("  sagaprobe latest --reports-dir <dir> --out <summary>");
    }

    private sealed record CommandOptions(
        bool Ok,
        string Error,
        string Input,
        string Baseline,
        string Candidate,
        string ReportsDir,
        string OutPath)
    {
        public static CommandOptions Parse(string[] args)
        {
            string input = "";
            string baseline = "";
            string candidate = "";
            string reportsDir = "";
            string output = "";

            for (var i = 0; i < args.Length; ++i)
            {
                switch (args[i])
                {
                    case "--input" when i + 1 < args.Length:
                        input = args[++i];
                        break;
                    case "--baseline" when i + 1 < args.Length:
                        baseline = args[++i];
                        break;
                    case "--candidate" when i + 1 < args.Length:
                        candidate = args[++i];
                        break;
                    case "--reports-dir" when i + 1 < args.Length:
                        reportsDir = args[++i];
                        break;
                    case "--out" when i + 1 < args.Length:
                        output = args[++i];
                        break;
                    default:
                        return Fail($"Unknown or incomplete argument: {args[i]}");
                }
            }

            return new CommandOptions(true, "", input, baseline, candidate, reportsDir, output);
        }

        private static CommandOptions Fail(string error)
        {
            return new CommandOptions(false, error, "", "", "", "", "");
        }
    }
}
