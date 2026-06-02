namespace SagaDocGuard;

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
            if (command != "scan")
            {
                Console.Error.WriteLine($"Unknown command: {command}");
                PrintUsage();
                return InvalidUsage;
            }

            var options = CommandOptions.Parse(args.Skip(1).ToArray());
            if (!options.Ok)
            {
                Console.Error.WriteLine(options.Error);
                PrintUsage();
                return InvalidUsage;
            }
            if (string.IsNullOrWhiteSpace(options.DocsPath) ||
                string.IsNullOrWhiteSpace(options.EvidenceRoot) ||
                string.IsNullOrWhiteSpace(options.OutPath))
            {
                Console.Error.WriteLine("scan requires --docs, --evidence-root, and --out.");
                return InvalidUsage;
            }

            var report = ClaimScanner.Scan(options.DocsPath, options.EvidenceRoot);
            ReportWriter.Write(options.OutPath, report);
            return report.Status == "Passed" ? Passed : Failed;
        }
        catch (FileNotFoundException e)
        {
            Console.Error.WriteLine(e.Message);
            return MissingInput;
        }
        catch (Exception e)
        {
            Console.Error.WriteLine($"Internal sagadocguard error: {e.Message}");
            return InternalError;
        }
    }

    private static void PrintUsage()
    {
        Console.Error.WriteLine("SagaDocGuard CLI");
        Console.Error.WriteLine("  sagadocguard scan --docs <path> --evidence-root <path> --out <report>");
    }

    private sealed record CommandOptions(
        bool Ok,
        string Error,
        string DocsPath,
        string EvidenceRoot,
        string OutPath)
    {
        public static CommandOptions Parse(string[] args)
        {
            string docs = "";
            string evidenceRoot = "";
            string output = "";

            for (var i = 0; i < args.Length; ++i)
            {
                switch (args[i])
                {
                    case "--docs" when i + 1 < args.Length:
                        docs = args[++i];
                        break;
                    case "--evidence-root" when i + 1 < args.Length:
                        evidenceRoot = args[++i];
                        break;
                    case "--out" when i + 1 < args.Length:
                        output = args[++i];
                        break;
                    default:
                        return Fail($"Unknown or incomplete argument: {args[i]}");
                }
            }

            return new CommandOptions(true, "", docs, evidenceRoot, output);
        }

        private static CommandOptions Fail(string error) => new(false, error, "", "", "");
    }
}
