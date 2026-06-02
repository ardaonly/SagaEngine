using System.Text.RegularExpressions;

namespace SagaDocGuard;

internal static class ClaimScanner
{
    private static readonly RegexOptions Options =
        RegexOptions.IgnoreCase | RegexOptions.CultureInvariant | RegexOptions.Compiled;
    private static readonly TimeSpan MatchTimeout = TimeSpan.FromMilliseconds(100);

    private static readonly IReadOnlyList<CompiledClaimRule> ForbiddenRules =
    [
        Rule("claim.productBeta", "ForbiddenClaims", @"\bproduct\s+beta\b|\bbeta\s+product\b"),
        Rule("claim.releaseCandidate", "ForbiddenClaims", @"\brelease\s+candidate\b"),
        Rule("claim.productionMmo", "ForbiddenClaims", @"\bproduction[- ]ready\s+MMO\s+server\b"),
        Rule("claim.productionNetwork", "ForbiddenClaims", @"\bproduction\s+network\s+readiness\b"),
        Rule("claim.fullVisualScripting", "ForbiddenClaims", @"\b(full|complete)\s+visual\s+scripting\b"),
        Rule("claim.arbitraryCSharpBlocks", "ForbiddenClaims", @"\barbitrary\s+C#\s+(to|roundtrip|conversion)[^\n]{0,120}\bblocks?\b|\barbitrary\s+C#\s+roundtrip\b"),
        Rule("claim.editableGraph", "ForbiddenClaims", @"\beditable\s+graph\b"),
        Rule("claim.sourceMutation", "ForbiddenClaims", @"\bsource\s+mutation\b|\bmutates\s+C#\s+source\b"),
        Rule("claim.patchApply", "ForbiddenClaims", @"\bpatch\s+apply\b|\bappl(y|ies)\s+patch"),
        Rule("claim.fullEditorMvp", "ForbiddenClaims", @"\bfull\s+editor\s+MVP\b|\bproduction\s+editor\s+MVP\b"),
        Rule("claim.fullCollaboration", "ForbiddenClaims", @"\bfull\s+collaboration\b"),
        Rule("claim.enterpriseReady", "ForbiddenClaims", @"\benterprise[- ]ready\b"),
        Rule("claim.enterpriseCollaboration", "ForbiddenClaims", @"\benterprise[- ]ready\s+collaboration\b"),
        Rule("claim.cloudWorkspace", "ForbiddenClaims", @"\bcloud\s+workspace\b"),
        Rule("claim.secureCloudPlatform", "ForbiddenClaims", @"\bsecure\s+cloud\s+platform\b"),
        Rule("claim.complianceReady", "ForbiddenClaims", @"\bSOC2\b|\bISO\s+27001\b|\bcompliance[- ]ready\b"),
        Rule("claim.authSystemImplemented", "ForbiddenClaims", @"\bauth\s+system\s+implemented\b"),
        Rule("claim.permissionSystemImplemented", "ForbiddenClaims", @"\bpermission\s+system\s+implemented\b"),
        Rule("claim.fullSecurityModel", "ForbiddenClaims", @"\bfull\s+security\s+model\b"),
        Rule("claim.rawFullCtest", "ForbiddenClaims", @"\braw\s+full\s+CTest\s+passed\b"),
        Rule("claim.heavyStress", "ForbiddenClaims", @"\bheavy\s+stress\s+passed\b"),
        Rule("claim.realTransportStress", "ForbiddenClaims", @"\breal\s+transport\s+stress\s+passed\b"),
        Rule("claim.zeroTrust", "ForbiddenClaims", @"\bzero[- ]trust\b"),
        Rule("claim.hedef3Started", "ForbiddenClaims", @"\bHedef\s+3\s+started\b"),
        Rule("claim.productionReady", "ForbiddenClaims", @"\bproduction[- ]ready\b"),
    ];

    private static readonly IReadOnlyList<CompiledRequiredNonClaimRule> RequiredNonClaims =
    [
        Required("nonclaim.productBeta", @"\bno\s+(product\s+beta|beta\s+product\s+status)\b"),
        Required("nonclaim.releaseCandidate", @"\bno\s+(release\s+candidate|candidate\s+release\s+status)\b"),
        Required("nonclaim.productionMmo", @"\bno\s+production\s+MMO\s+server\b"),
        Required("nonclaim.visualScripting", @"\bno\s+(full|complete)\s+visual\s+scripting\b"),
        Required("nonclaim.csharpRoundtrip", @"\bno\s+arbitrary\s+C#\s+roundtrip\b"),
        Required("nonclaim.enterprise", @"\bno\s+enterprise\s+readiness\b"),
    ];

    private static readonly Regex NonClaimContext = new(
        @"\b(no|not|non[- ]claim|cannot|does\s+not|do\s+not|deferred|forbidden|out\s+of\s+scope|without\s+claiming)\b",
        Options,
        MatchTimeout);

    public static DocGuardReport Scan(string docsPath, string evidenceRoot)
    {
        var root = Path.GetFullPath(docsPath);
        var files = EnumerateMarkdownFiles(root);
        var forbidden = new List<ClaimMatch>();
        var reviewed = new List<ClaimMatch>();
        var allText = new List<string>();

        foreach (var file in files)
        {
            var lines = File.ReadAllLines(file);
            for (var i = 0; i < lines.Length; ++i)
            {
                var line = lines[i];
                allText.Add(line);
                var relativePath = Normalize(root, file);
                foreach (var rule in ForbiddenRules)
                {
                    if (!rule.Regex.IsMatch(line))
                    {
                        continue;
                    }

                    var match = new ClaimMatch
                    {
                        Category = rule.Category,
                        RuleId = rule.RuleId,
                        Path = relativePath,
                        Line = i + 1,
                        Text = line.Trim(),
                    };
                    if (IsNonClaimContext(relativePath, lines, i))
                    {
                        reviewed.Add(match);
                    }
                    else
                    {
                        forbidden.Add(match);
                    }
                }
            }
        }

        var joined = string.Join('\n', allText);
        var missingNonClaims = RequiredNonClaims
            .Where(rule => !rule.Regex.IsMatch(joined))
            .Select(rule => new MissingRequiredNonClaim
            {
                NonClaimId = rule.NonClaimId,
                ExpectedPattern = rule.Pattern,
            })
            .OrderBy(item => item.NonClaimId, StringComparer.Ordinal)
            .ToArray();

        var missingEvidence = EvidenceChecker.Check(evidenceRoot);
        var status = forbidden.Count == 0 && missingNonClaims.Length == 0 && missingEvidence.Count == 0
            ? "Passed"
            : "Failed";

        return new DocGuardReport
        {
            Status = status,
            ScannedFiles = files.Select(file => Normalize(root, file)).ToArray(),
            ForbiddenMatches = forbidden.OrderBy(item => item.Path, StringComparer.Ordinal)
                .ThenBy(item => item.Line)
                .ThenBy(item => item.RuleId, StringComparer.Ordinal)
                .ToArray(),
            MissingRequiredNonClaims = missingNonClaims,
            MissingEvidence = missingEvidence,
            ReviewedNonClaimMatches = reviewed.OrderBy(item => item.Path, StringComparer.Ordinal)
                .ThenBy(item => item.Line)
                .ThenBy(item => item.RuleId, StringComparer.Ordinal)
                .ToArray(),
            Diagnostics =
            [
                new Diagnostic
                {
                    Severity = status == "Passed" ? "Info" : "Error",
                    Code = status == "Passed" ? "DocGuard.Scan.Passed" : "DocGuard.Scan.Failed",
                    Message = status == "Passed" ? "Documentation honesty scan passed." : "Documentation honesty scan failed.",
                    Path = root,
                },
            ],
        };
    }

    private static bool IsNonClaimContext(string relativePath, string[] lines, int index)
    {
        if (relativePath.StartsWith("architecture/", StringComparison.Ordinal) ||
            relativePath.StartsWith("dev/", StringComparison.Ordinal) ||
            relativePath.StartsWith("recovery/", StringComparison.Ordinal) ||
            relativePath.StartsWith("roadmaps/", StringComparison.Ordinal) ||
            relativePath.StartsWith("testing/", StringComparison.Ordinal))
        {
            return true;
        }

        if (lines[index].TrimStart().StartsWith(">", StringComparison.Ordinal))
        {
            return true;
        }

        if (NonClaimContext.IsMatch(lines[index]))
        {
            return true;
        }

        var start = Math.Max(0, index - 12);
        for (var i = start; i <= index; ++i)
        {
            var line = lines[i];
            if (IsMatch(line, @"\b(non[- ]claims?|non[- ]goals?|blocked\s+claims?|blocked|limitations|future|deferred|not\s+accepted|not\s+proven|out\s+of\s+scope|forbidden\s+claims?)\b"))
            {
                return true;
            }
        }

        return false;
    }

    private static IReadOnlyList<string> EnumerateMarkdownFiles(string root)
    {
        if (File.Exists(root))
        {
            return [root];
        }
        if (!Directory.Exists(root))
        {
            throw new FileNotFoundException($"Docs path does not exist: {root}");
        }

        return Directory.EnumerateFiles(root, "*.md", SearchOption.AllDirectories)
            .OrderBy(path => path, StringComparer.Ordinal)
            .ToArray();
    }

    private static string Normalize(string root, string path)
    {
        return Path.GetRelativePath(root, path).Replace('\\', '/');
    }

    private static bool IsMatch(string input, string pattern) =>
        Regex.IsMatch(input, pattern, Options, MatchTimeout);

    private static CompiledClaimRule Rule(string id, string category, string pattern) =>
        new(id, category, pattern, new Regex(pattern, Options, MatchTimeout));

    private static CompiledRequiredNonClaimRule Required(string id, string pattern) =>
        new(id, pattern, new Regex(pattern, Options, MatchTimeout));

    private sealed record CompiledClaimRule(string RuleId, string Category, string Pattern, Regex Regex);
    private sealed record CompiledRequiredNonClaimRule(string NonClaimId, string Pattern, Regex Regex);
}
