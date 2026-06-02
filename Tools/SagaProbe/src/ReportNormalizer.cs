using System.Globalization;
using System.Text.Json.Nodes;

namespace SagaProbe;

internal static class ReportNormalizer
{
    private static readonly string[] KnownSections =
    [
        "health",
        "serverLifecycle",
        "faults",
        "chaos",
        "headless",
        "launch",
        "acceptance",
    ];

    public static NormalizedReport Normalize(string inputPath)
    {
        var fullPath = NormalizePath(inputPath);
        var root = ReportLoader.LoadObject(fullPath);
        var kind = DetectKind(root);
        if (kind == "unsupported")
        {
            throw new ReportLoadException(
                "Probe.Report.UnsupportedKind",
                "Diagnostics report kind is not supported by SagaProbe v1.");
        }

        var report = new NormalizedReport
        {
            SourcePath = fullPath,
            ReportKind = kind,
            SourceStatus = ReadString(root, "status"),
        };

        foreach (var section in KnownSections)
        {
            report.Sections[section] = 0;
        }

        switch (kind)
        {
            case "engine_diagnostics_report":
            case "engine_reliability_report":
                NormalizeEngineReport(root, report);
                break;
            case "saga_chaos_report":
                NormalizeChaosReport(root, report);
                break;
            case "multiplayer_sandbox_headless_report":
                NormalizeHeadlessReport(root, report);
                break;
            case "sagalaunch_launch_report":
                NormalizeLaunchReport(root, report);
                break;
            case "sagalaunch_acceptance_report":
                NormalizeAcceptanceReport(root, report);
                break;
        }

        NormalizeCommonDiagnostics(root, report, "diagnostics", "Source");
        AddSyntheticSourceFailureIfNeeded(report);
        SortDiagnostics(report.Diagnostics);
        return report;
    }

    public static DiagnosticsSummaryReport BuildSummary(
        NormalizedReport report,
        string command)
    {
        var critical = report.Diagnostics
            .Where(IsCritical)
            .OrderBy(d => d.StableKey(), StringComparer.Ordinal)
            .ToList();
        var warnings = report.Diagnostics
            .Where(IsWarning)
            .OrderBy(d => d.StableKey(), StringComparer.Ordinal)
            .ToList();
        var metrics = report.Metrics
            .Select(pair => new ProbeMetric { Name = pair.Key, Value = pair.Value })
            .ToList();
        var sections = report.Sections
            .Select(pair => new ProbeSection
            {
                Name = pair.Key,
                Present = pair.Value > 0,
                Count = pair.Value,
            })
            .ToList();

        return new DiagnosticsSummaryReport
        {
            Command = command,
            Status = critical.Count > 0 ? "Failed" :
                warnings.Count > 0 || IsAttentionStatus(report.SourceStatus)
                    ? "Attention"
                    : "Passed",
            SourceReport = report.SourceInfo(),
            Summary = new DiagnosticsSummaryCounts
            {
                DiagnosticCount = report.Diagnostics.Count,
                CriticalDiagnosticCount = critical.Count,
                WarningDiagnosticCount = warnings.Count,
                MetricCount = metrics.Count,
                SectionCount = sections.Count(s => s.Present),
                LifecycleEventCount = report.LifecycleEventCount,
                LifecycleRecordCount = report.LifecycleRecordCount,
                FaultCount = report.FaultCount,
                LeakCount = report.LeakCount,
            },
            Metrics = metrics,
            CriticalDiagnostics = critical,
            WarningDiagnostics = warnings,
            Sections = sections,
        };
    }

    public static string DetectKind(JsonObject root)
    {
        var tool = ReadString(root, "tool");
        var command = ReadString(root, "command");
        if (tool == "SagaChaosLab")
        {
            return "saga_chaos_report";
        }
        if (tool == "MultiplayerSandboxHeadless")
        {
            return "multiplayer_sandbox_headless_report";
        }
        if (tool == "sagalaunch" && command == "server")
        {
            return "sagalaunch_launch_report";
        }
        if (tool == "sagalaunch" && command == "accept")
        {
            return "sagalaunch_acceptance_report";
        }
        if (!string.IsNullOrEmpty(ReadString(root, "reportKind")))
        {
            return root.ContainsKey("healthRuleViolations") ||
                root.ContainsKey("reason") ||
                root.ContainsKey("lifetimeLeakSummary")
                    ? "engine_reliability_report"
                    : "engine_diagnostics_report";
        }
        return "unsupported";
    }

    public static bool LooksSupported(string path)
    {
        try
        {
            return DetectKind(ReportLoader.LoadObject(path)) != "unsupported";
        }
        catch (ReportLoadException)
        {
            return false;
        }
    }

    private static void NormalizeEngineReport(JsonObject root, NormalizedReport report)
    {
        report.SourceStatus = string.IsNullOrEmpty(report.SourceStatus)
            ? "Passed"
            : report.SourceStatus;

        if (root["healthMetrics"] is JsonArray metrics)
        {
            report.Sections["health"] = metrics.Count;
            foreach (var item in metrics.OfType<JsonObject>())
            {
                var name = ReadString(item, "name");
                if (!string.IsNullOrEmpty(name) && TryReadDouble(item, "value", out var value))
                {
                    report.Metrics[$"health.{name}"] = value;
                }
            }
        }

        AddNumericObjectMetrics(root["summary"] as JsonObject, report, "summary");
        AddNumericObjectMetrics(root["resourceSummary"] as JsonObject, report, "resourceSummary");
        AddNumericObjectMetrics(root["lifetimeLeakSummary"] as JsonObject, report, "lifetimeLeakSummary");

        if (root["lifetimeLeaks"] is JsonArray lifetimeLeaks)
        {
            report.LeakCount += lifetimeLeaks.Count;
            report.Metrics["lifetimeLeaks.count"] = lifetimeLeaks.Count;
        }

        if (root["healthRuleViolations"] is JsonArray violations)
        {
            report.Sections["health"] = Math.Max(report.Sections["health"], violations.Count);
            foreach (var item in violations.OfType<JsonObject>())
            {
                if (ReadBool(item, "passed") == false)
                {
                    AddDiagnostic(
                        report,
                        ReadString(item, "severity", "Error"),
                        "Health.RuleViolation",
                        "Health",
                        ReadString(item, "metricName"),
                        ReadString(item, "message", ReadString(item, "ruleName")));
                }
            }
        }

        if (root["serverLifecycle"] is JsonObject lifecycle)
        {
            var eventCount = ArrayCount(lifecycle, "events");
            var recordCount = ArrayCount(lifecycle, "records");
            var leakCount = ArrayCount(lifecycle, "leaks");
            report.Sections["serverLifecycle"] = eventCount + recordCount + leakCount;
            report.LifecycleEventCount = eventCount;
            report.LifecycleRecordCount = recordCount;
            report.LeakCount += leakCount;
            AddNumericObjectMetrics(lifecycle["summary"] as JsonObject, report, "serverLifecycle.summary");
            NormalizeLifecycleEvents(lifecycle["events"] as JsonArray, report);
        }

        if (root["faults"] is JsonObject faults)
        {
            var faultCount = ArrayCount(faults, "reports");
            report.Sections["faults"] = faultCount;
            report.FaultCount = faultCount;
            AddNumericObjectMetrics(faults["summary"] as JsonObject, report, "faults.summary");
            foreach (var item in (faults["reports"] as JsonArray ?? []).OfType<JsonObject>())
            {
                AddDiagnostic(
                    report,
                    ReadString(item, "severity", "Error"),
                    ReadString(item, "diagnosticCode", ReadString(item, "faultId", "Fault.Report")),
                    "Fault",
                    ReadString(item, "subsystem"),
                    ReadString(item, "message"));
            }
        }

        foreach (var item in (root["recentLogEvents"] as JsonArray ?? []).OfType<JsonObject>())
        {
            var level = ReadString(item, "level");
            if (IsCriticalSeverity(level) || IsWarningSeverity(level))
            {
                AddDiagnostic(
                    report,
                    level,
                    $"Log.{level}",
                    "Log",
                    ReadString(item, "tag"),
                    ReadString(item, "message"));
            }
        }
    }

    private static void NormalizeChaosReport(JsonObject root, NormalizedReport report)
    {
        report.Sections["chaos"] = 1;
        AddNumericObjectMetrics(root["chaosMetrics"] as JsonObject, report, "chaosMetrics");
        AddStringDiagnostics(root["diagnostics"] as JsonArray, report, "Warning", "Chaos.Diagnostic", "Chaos");
    }

    private static void NormalizeHeadlessReport(JsonObject root, NormalizedReport report)
    {
        report.Sections["headless"] = 1;
        AddMetricIfNumber(root, report, "headless.tickCount", "tickCount");
        AddMetricIfNumber(root, report, "headless.entityCount", "entityCount");
        AddMetricIfNumber(root, report, "headless.inputQueuedCount", "inputQueuedCount");
        AddMetricIfNumber(root, report, "headless.inputAcceptedCount", "inputAcceptedCount");
        if (root["dirtyEntityIds"] is JsonArray dirty)
        {
            report.Metrics["headless.dirtyEntityCount"] = dirty.Count;
        }
    }

    private static void NormalizeLaunchReport(JsonObject root, NormalizedReport report)
    {
        report.Sections["launch"] = 1;
        if (root["process"] is JsonObject process)
        {
            if (TryReadDouble(process, "exitCode", out var exitCode))
            {
                report.Metrics["launch.process.exitCode"] = exitCode;
            }
            report.Metrics["launch.process.timedOut"] =
                ReadBool(process, "timedOut") == true ? 1.0 : 0.0;
        }
        if (root["diagnostics"] is JsonArray diagnostics)
        {
            report.Metrics["launch.diagnosticCount"] = diagnostics.Count;
        }
    }

    private static void NormalizeAcceptanceReport(JsonObject root, NormalizedReport report)
    {
        report.Sections["acceptance"] = 1;
        if (root["deferredStages"] is JsonArray stages)
        {
            report.Metrics["acceptance.deferredStageCount"] = stages.Count;
            foreach (var item in stages.OfType<JsonObject>())
            {
                AddDiagnostic(
                    report,
                    "Warning",
                    "Acceptance.StageDeferred",
                    "Acceptance",
                    ReadString(item, "name"),
                    ReadString(item, "reason"));
            }
        }
    }

    private static void NormalizeCommonDiagnostics(
        JsonObject root,
        NormalizedReport report,
        string key,
        string defaultCategory)
    {
        if (root[key] is not JsonArray diagnostics)
        {
            return;
        }
        foreach (var item in diagnostics)
        {
            if (item is JsonObject obj)
            {
                AddDiagnostic(
                    report,
                    ReadString(obj, "severity", "Error"),
                    ReadString(obj, "code", ReadString(obj, "diagnosticId", "Diagnostic")),
                    ReadString(obj, "category", defaultCategory),
                    ReadString(obj, "path"),
                    ReadString(obj, "message"));
            }
            else if (item is JsonValue value && value.TryGetValue<string>(out var text))
            {
                AddDiagnostic(report, "Warning", "Diagnostic.Message", defaultCategory, "", text);
            }
        }
    }

    private static void NormalizeLifecycleEvents(JsonArray? events, NormalizedReport report)
    {
        foreach (var item in (events ?? []).OfType<JsonObject>())
        {
            var severity = ReadString(item, "severity");
            if (IsCriticalSeverity(severity) || IsWarningSeverity(severity))
            {
                AddDiagnostic(
                    report,
                    severity,
                    ReadString(item, "eventName", "ServerLifecycle.Event"),
                    "ServerLifecycle",
                    ReadString(item, "zoneId"),
                    ReadString(item, "message"));
            }
        }
    }

    private static void AddSyntheticSourceFailureIfNeeded(NormalizedReport report)
    {
        if (!IsFailedStatus(report.SourceStatus) ||
            report.Diagnostics.Any(IsCritical))
        {
            return;
        }
        AddDiagnostic(
            report,
            "Error",
            "Probe.Source.StatusFailed",
            "Source",
            report.SourcePath,
            "Source report status is Failed.");
    }

    private static void AddNumericObjectMetrics(
        JsonObject? obj,
        NormalizedReport report,
        string prefix)
    {
        if (obj is null)
        {
            return;
        }
        foreach (var (key, value) in obj.OrderBy(p => p.Key, StringComparer.Ordinal))
        {
            if (value is JsonValue jsonValue &&
                jsonValue.TryGetValue<double>(out var number))
            {
                report.Metrics[$"{prefix}.{key}"] = number;
            }
        }
    }

    private static void AddMetricIfNumber(
        JsonObject root,
        NormalizedReport report,
        string metricName,
        string jsonKey)
    {
        if (TryReadDouble(root, jsonKey, out var value))
        {
            report.Metrics[metricName] = value;
        }
    }

    private static void AddStringDiagnostics(
        JsonArray? array,
        NormalizedReport report,
        string severity,
        string code,
        string category)
    {
        foreach (var item in (array ?? []))
        {
            if (item is JsonValue value && value.TryGetValue<string>(out var text))
            {
                AddDiagnostic(report, severity, code, category, "", text);
            }
        }
    }

    private static void AddDiagnostic(
        NormalizedReport report,
        string severity,
        string code,
        string category,
        string path,
        string message)
    {
        report.Diagnostics.Add(new ProbeDiagnostic
        {
            Severity = NormalizeSeverity(severity),
            Code = string.IsNullOrWhiteSpace(code) ? "Diagnostic" : code,
            Category = string.IsNullOrWhiteSpace(category) ? "Diagnostics" : category,
            Path = path,
            Message = message,
        });
    }

    private static void SortDiagnostics(List<ProbeDiagnostic> diagnostics)
    {
        diagnostics.Sort((left, right) =>
            string.CompareOrdinal(left.StableKey(), right.StableKey()));
    }

    private static string NormalizePath(string path)
    {
        return Path.GetFullPath(path).Replace('\\', '/');
    }

    private static string ReadString(JsonObject obj, string key, string fallback = "")
    {
        if (obj.TryGetPropertyValue(key, out var node) &&
            node is JsonValue value &&
            value.TryGetValue<string>(out var text))
        {
            return text;
        }
        return fallback;
    }

    private static bool? ReadBool(JsonObject obj, string key)
    {
        if (obj.TryGetPropertyValue(key, out var node) &&
            node is JsonValue value &&
            value.TryGetValue<bool>(out var result))
        {
            return result;
        }
        return null;
    }

    private static bool TryReadDouble(JsonObject obj, string key, out double result)
    {
        result = 0;
        if (obj.TryGetPropertyValue(key, out var node) &&
            node is JsonValue value)
        {
            if (value.TryGetValue<double>(out result))
            {
                return true;
            }
            if (value.TryGetValue<int>(out var integer))
            {
                result = integer;
                return true;
            }
        }
        return false;
    }

    private static int ArrayCount(JsonObject obj, string key)
    {
        return obj[key] is JsonArray array ? array.Count : 0;
    }

    private static string NormalizeSeverity(string severity)
    {
        if (string.IsNullOrWhiteSpace(severity))
        {
            return "Info";
        }
        return severity.Equals("Warn", StringComparison.OrdinalIgnoreCase)
            ? "Warning"
            : CultureInfo.InvariantCulture.TextInfo.ToTitleCase(severity.ToLowerInvariant());
    }

    private static bool IsCritical(ProbeDiagnostic diagnostic)
    {
        return IsCriticalSeverity(diagnostic.Severity);
    }

    private static bool IsWarning(ProbeDiagnostic diagnostic)
    {
        return IsWarningSeverity(diagnostic.Severity);
    }

    private static bool IsCriticalSeverity(string severity)
    {
        return severity is "Critical" or "Fatal" or "Error";
    }

    private static bool IsWarningSeverity(string severity)
    {
        return severity is "Warning" or "Warn";
    }

    private static bool IsFailedStatus(string status)
    {
        return status.Equals("Failed", StringComparison.OrdinalIgnoreCase) ||
            status.Equals("ScenarioFailed", StringComparison.OrdinalIgnoreCase) ||
            status.Equals("report_write_failed", StringComparison.OrdinalIgnoreCase);
    }

    private static bool IsAttentionStatus(string status)
    {
        return status.Equals("Changed", StringComparison.OrdinalIgnoreCase) ||
            status.Equals("Deferred", StringComparison.OrdinalIgnoreCase);
    }
}
