namespace SagaProbe;

internal static class ReportComparer
{
    public static ReportDiff Compare(NormalizedReport baseline, NormalizedReport candidate)
    {
        var baselineDiagnostics = baseline.Diagnostics
            .ToDictionary(d => d.StableKey(), d => d, StringComparer.Ordinal);
        var candidateDiagnostics = candidate.Diagnostics
            .ToDictionary(d => d.StableKey(), d => d, StringComparer.Ordinal);

        var added = candidateDiagnostics
            .Where(pair => !baselineDiagnostics.ContainsKey(pair.Key))
            .Select(pair => pair.Value)
            .OrderBy(d => d.StableKey(), StringComparer.Ordinal)
            .ToList();
        var removed = baselineDiagnostics
            .Where(pair => !candidateDiagnostics.ContainsKey(pair.Key))
            .Select(pair => pair.Value)
            .OrderBy(d => d.StableKey(), StringComparer.Ordinal)
            .ToList();

        var changedMetrics = new List<ChangedMetric>();
        foreach (var name in baseline.Metrics.Keys
                     .Union(candidate.Metrics.Keys, StringComparer.Ordinal)
                     .OrderBy(name => name, StringComparer.Ordinal))
        {
            _ = baseline.Metrics.TryGetValue(name, out var baselineValue);
            _ = candidate.Metrics.TryGetValue(name, out var candidateValue);
            if (!baseline.Metrics.ContainsKey(name) ||
                !candidate.Metrics.ContainsKey(name) ||
                baselineValue != candidateValue)
            {
                changedMetrics.Add(new ChangedMetric
                {
                    Name = name,
                    BaselineValue = baselineValue,
                    CandidateValue = candidateValue,
                });
            }
        }

        var missingSections = new List<MissingSection>();
        foreach (var name in baseline.Sections.Keys
                     .Union(candidate.Sections.Keys, StringComparer.Ordinal)
                     .OrderBy(name => name, StringComparer.Ordinal))
        {
            var baselinePresent =
                baseline.Sections.TryGetValue(name, out var baselineCount) && baselineCount > 0;
            var candidatePresent =
                candidate.Sections.TryGetValue(name, out var candidateCount) && candidateCount > 0;
            if (baselinePresent && !candidatePresent)
            {
                missingSections.Add(new MissingSection { Name = name, MissingFrom = "candidate" });
            }
            if (!baselinePresent && candidatePresent)
            {
                missingSections.Add(new MissingSection { Name = name, MissingFrom = "baseline" });
            }
        }

        var changed = added.Count > 0 ||
            removed.Count > 0 ||
            changedMetrics.Count > 0 ||
            missingSections.Count > 0;

        return new ReportDiff
        {
            Status = changed ? "Changed" : "Passed",
            Baseline = baseline.SourceInfo(),
            Candidate = candidate.SourceInfo(),
            AddedDiagnostics = added,
            RemovedDiagnostics = removed,
            ChangedMetrics = changedMetrics,
            MissingSections = missingSections,
            Summary = new ReportDiffSummary
            {
                AddedDiagnosticCount = added.Count,
                RemovedDiagnosticCount = removed.Count,
                ChangedMetricCount = changedMetrics.Count,
                MissingSectionCount = missingSections.Count,
            },
        };
    }
}
