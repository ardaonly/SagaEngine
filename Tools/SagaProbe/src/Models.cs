namespace SagaProbe;

internal sealed class SourceReportInfo
{
    public string Path { get; init; } = "";
    public string ReportKind { get; init; } = "";
}

internal sealed class ProbeDiagnostic
{
    public string Severity { get; init; } = "";
    public string Code { get; init; } = "";
    public string Category { get; init; } = "";
    public string Path { get; init; } = "";
    public string Message { get; init; } = "";

    public string StableKey()
    {
        return string.Join('\u001f', Severity, Code, Category, Path, Message);
    }
}

internal sealed class ProbeMetric
{
    public string Name { get; init; } = "";
    public double Value { get; init; }
}

internal sealed class ProbeSection
{
    public string Name { get; init; } = "";
    public bool Present { get; init; }
    public int Count { get; init; }
}

internal sealed class DiagnosticsSummaryCounts
{
    public int DiagnosticCount { get; init; }
    public int CriticalDiagnosticCount { get; init; }
    public int WarningDiagnosticCount { get; init; }
    public int MetricCount { get; init; }
    public int SectionCount { get; init; }
    public int LifecycleEventCount { get; init; }
    public int LifecycleRecordCount { get; init; }
    public int FaultCount { get; init; }
    public int LeakCount { get; init; }
}

internal sealed class DiagnosticsSummaryReport
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = "sagaprobe";
    public string Command { get; init; } = "summarize";
    public string Status { get; init; } = "Failed";
    public SourceReportInfo SourceReport { get; init; } = new();
    public DiagnosticsSummaryCounts Summary { get; init; } = new();
    public List<ProbeMetric> Metrics { get; init; } = [];
    public List<ProbeDiagnostic> CriticalDiagnostics { get; init; } = [];
    public List<ProbeDiagnostic> WarningDiagnostics { get; init; } = [];
    public List<ProbeSection> Sections { get; init; } = [];
}

internal sealed class ChangedMetric
{
    public string Name { get; init; } = "";
    public double BaselineValue { get; init; }
    public double CandidateValue { get; init; }
}

internal sealed class MissingSection
{
    public string Name { get; init; } = "";
    public string MissingFrom { get; init; } = "";
}

internal sealed class ReportDiffSummary
{
    public int AddedDiagnosticCount { get; init; }
    public int RemovedDiagnosticCount { get; init; }
    public int ChangedMetricCount { get; init; }
    public int MissingSectionCount { get; init; }
}

internal sealed class ReportDiff
{
    public int SchemaVersion { get; init; } = 1;
    public string Tool { get; init; } = "sagaprobe";
    public string Command { get; init; } = "compare";
    public string Status { get; init; } = "Failed";
    public SourceReportInfo Baseline { get; init; } = new();
    public SourceReportInfo Candidate { get; init; } = new();
    public List<ProbeDiagnostic> AddedDiagnostics { get; init; } = [];
    public List<ProbeDiagnostic> RemovedDiagnostics { get; init; } = [];
    public List<ChangedMetric> ChangedMetrics { get; init; } = [];
    public List<MissingSection> MissingSections { get; init; } = [];
    public ReportDiffSummary Summary { get; init; } = new();
}

internal sealed class NormalizedReport
{
    public string SourcePath { get; init; } = "";
    public string ReportKind { get; init; } = "";
    public string SourceStatus { get; set; } = "";
    public List<ProbeDiagnostic> Diagnostics { get; } = [];
    public SortedDictionary<string, double> Metrics { get; } = new(StringComparer.Ordinal);
    public SortedDictionary<string, int> Sections { get; } = new(StringComparer.Ordinal);
    public int LifecycleEventCount { get; set; }
    public int LifecycleRecordCount { get; set; }
    public int FaultCount { get; set; }
    public int LeakCount { get; set; }

    public SourceReportInfo SourceInfo()
    {
        return new SourceReportInfo
        {
            Path = SourcePath,
            ReportKind = ReportKind,
        };
    }
}
