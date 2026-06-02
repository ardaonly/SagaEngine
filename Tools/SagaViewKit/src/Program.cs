using System.Text.Json;

namespace SagaViewKit;

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
                "emit-profiles" => RunEmitProfiles(options),
                "validate" => RunValidate(options),
                "check-simple" => RunCheckSimple(options),
                "slice-compatibility" => RunSliceCompatibility(options),
                _ => InvalidUsage,
            };
        }
        catch (FileNotFoundException e)
        {
            Console.Error.WriteLine(e.Message);
            return MissingInput;
        }
        catch (Exception e)
        {
            Console.Error.WriteLine($"Internal sagaviewkit error: {e.Message}");
            return InternalError;
        }
    }

    private static int RunEmitProfiles(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.OutPath))
        {
            Console.Error.WriteLine("emit-profiles requires --out.");
            return InvalidUsage;
        }

        Directory.CreateDirectory(options.OutPath);
        foreach (var profile in BuiltInProfiles.All.OrderBy(profile => profile.ViewId, StringComparer.Ordinal))
        {
            ReportWriter.Write(Path.Combine(options.OutPath, $"{profile.ViewId}.json"), profile);
        }

        var report = new ViewCapabilityReport
        {
            Command = "emit-profiles",
            ProfileSummaries = BuiltInProfiles.All
                .OrderBy(profile => profile.ViewId, StringComparer.Ordinal)
                .Select(ViewCapabilityValidator.Summarize)
                .ToArray(),
            Diagnostics =
            [
                new Diagnostic
                {
                    Severity = "Info",
                    Code = "View.Profiles.Emitted",
                    Message = "Built-in view capability profiles were emitted.",
                    Path = Path.GetFullPath(options.OutPath),
                },
            ],
        };
        ReportWriter.Write(Path.Combine(options.OutPath, "view_capability_report.json"), report);
        return Passed;
    }

    private static int RunValidate(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.ManifestPath) || string.IsNullOrWhiteSpace(options.OutPath))
        {
            Console.Error.WriteLine("validate requires --manifest and --out.");
            return InvalidUsage;
        }
        if (!File.Exists(options.ManifestPath))
        {
            WriteMissingInputReport("validate", options.OutPath, options.ManifestPath);
            return MissingInput;
        }

        var manifest = ViewCapabilityValidator.LoadManifest(options.ManifestPath);
        var result = ViewCapabilityValidator.Validate(manifest, options.EvidenceRoot);
        var report = new ViewCapabilityReport
        {
            Command = "validate",
            Status = result.Passed ? "Passed" : "Failed",
            ProfileSummaries = [ViewCapabilityValidator.Summarize(manifest)],
            Violations = result.Violations,
            Diagnostics = result.Diagnostics,
        };
        ReportWriter.Write(options.OutPath, report);
        return result.Passed ? Passed : Failed;
    }

    private static int RunCheckSimple(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.ProjectionPath) ||
            string.IsNullOrWhiteSpace(options.ProfilePath) ||
            string.IsNullOrWhiteSpace(options.OutPath))
        {
            Console.Error.WriteLine("check-simple requires --projection, --profile, and --out.");
            return InvalidUsage;
        }
        if (!File.Exists(options.ProjectionPath))
        {
            WriteMissingInputReport("check-simple", options.OutPath, options.ProjectionPath);
            return MissingInput;
        }

        var profile = LoadProfile(options.ProfilePath);
        var report = SimpleViewHonestyValidator.Check(
            options.ProjectionPath,
            profile,
            options.NodeMetadataPath,
            options.EvidenceRoot);
        ReportWriter.Write(options.OutPath, report);
        return report.Status == "Passed" ? Passed : Failed;
    }

    private static int RunSliceCompatibility(CommandOptions options)
    {
        if (string.IsNullOrWhiteSpace(options.View) ||
            string.IsNullOrWhiteSpace(options.SliceResolutionPath) ||
            string.IsNullOrWhiteSpace(options.OutPath))
        {
            Console.Error.WriteLine("slice-compatibility requires --view, --slice-resolution, and --out.");
            return InvalidUsage;
        }
        if (!File.Exists(options.SliceResolutionPath))
        {
            WriteMissingInputReport("slice-compatibility", options.OutPath, options.SliceResolutionPath);
            return MissingInput;
        }

        var report = ViewSliceCompatibilityChecker.Check(options.View, options.SliceResolutionPath);
        ReportWriter.Write(options.OutPath, report);
        return report.Status == "Passed" ? Passed : Failed;
    }


    private static ViewCapabilityManifest LoadProfile(string profile)
    {
        if (BuiltInProfiles.TryGet(profile, out var builtIn))
        {
            return builtIn;
        }
        if (!File.Exists(profile))
        {
            throw new FileNotFoundException($"Profile does not exist: {profile}");
        }
        return JsonSerializer.Deserialize<ViewCapabilityManifest>(
            File.ReadAllText(profile),
            ReportWriter.Options) ?? throw new InvalidOperationException("Profile is empty.");
    }

    private static void WriteMissingInputReport(string command, string outPath, string path)
    {
        if (string.IsNullOrWhiteSpace(outPath))
        {
            return;
        }

        ReportWriter.Write(outPath, new ViewCapabilityReport
        {
            Command = command,
            Status = "Failed",
            Diagnostics =
            [
                new Diagnostic
                {
                    Severity = "Error",
                    Code = "View.Input.Missing",
                    Message = "Input path does not exist.",
                    Path = path,
                },
            ],
        });
    }

    private static void PrintUsage()
    {
        Console.Error.WriteLine("SagaViewKit CLI");
        Console.Error.WriteLine("  sagaviewkit emit-profiles --out <dir>");
        Console.Error.WriteLine("  sagaviewkit validate --manifest <view_capability.json> --out <report> [--evidence-root <path>]");
        Console.Error.WriteLine("  sagaviewkit check-simple --projection <projection_report.json> --profile <manifest-or-built-in> --out <report> [--node-metadata <node_metadata.json>] [--evidence-root <path>]");
        Console.Error.WriteLine("  sagaviewkit slice-compatibility --view <Simple|Pro|CSharpSource|Diagnostics> --slice-resolution <report> --out <report>");
    }

    private sealed record CommandOptions(
        bool Ok,
        string Error,
        string OutPath,
        string ManifestPath,
        string ProjectionPath,
        string ProfilePath,
        string NodeMetadataPath,
        string View,
        string SliceResolutionPath,
        string EvidenceRoot)
    {
        public static CommandOptions Parse(string[] args)
        {
            string output = "";
            string manifest = "";
            string projection = "";
            string profile = "";
            string nodeMetadata = "";
            string view = "";
            string sliceResolution = "";
            string evidenceRoot = "";

            for (var i = 0; i < args.Length; ++i)
            {
                switch (args[i])
                {
                    case "--out" when i + 1 < args.Length:
                        output = args[++i];
                        break;
                    case "--manifest" when i + 1 < args.Length:
                        manifest = args[++i];
                        break;
                    case "--projection" when i + 1 < args.Length:
                        projection = args[++i];
                        break;
                    case "--profile" when i + 1 < args.Length:
                        profile = args[++i];
                        break;
                    case "--node-metadata" when i + 1 < args.Length:
                        nodeMetadata = args[++i];
                        break;
                    case "--view" when i + 1 < args.Length:
                        view = args[++i];
                        break;
                    case "--slice-resolution" when i + 1 < args.Length:
                        sliceResolution = args[++i];
                        break;
                    case "--evidence-root" when i + 1 < args.Length:
                        evidenceRoot = args[++i];
                        break;
                    default:
                        return Fail($"Unknown or incomplete argument: {args[i]}");
                }
            }

            return new CommandOptions(true, "", output, manifest, projection, profile, nodeMetadata, view, sliceResolution, evidenceRoot);
        }

        private static CommandOptions Fail(string error) => new(false, error, "", "", "", "", "", "", "", "");
    }
}
