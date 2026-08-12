// SPDX-License-Identifier: MPL-2.0

using System.Diagnostics;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using SagaEngine.Scripting.Gameplay;

if (args.Length != 2)
    throw new InvalidOperationException("Expected verified run input and output paths.");

var input = JsonSerializer.Deserialize<VerifiedRunInput>(File.ReadAllBytes(args[0]),
    new JsonSerializerOptions { PropertyNameCaseInsensitive = true })
    ?? throw new InvalidOperationException("Verified benchmark input is empty.");
input.Validate();

for (var index = 0; index < input.Warmup; ++index)
{
    _ = MeasureHigh(input);
    _ = MeasureLow(input);
}

var high = new Trial[input.Trials];
var low = new Trial[input.Trials];
for (var index = 0; index < input.Trials; ++index)
{
    high[index] = MeasureHigh(input);
    low[index] = MeasureLow(input);
    if (high[index].Score != low[index].Score ||
        high[index].ActiveCount != low[index].ActiveCount ||
        high[index].StateHash != low[index].StateHash)
        throw new InvalidOperationException("High-level and low-level deterministic outcomes diverged.");
}

var assembly = Assembly.GetAssembly(typeof(CollectiblesLowLevelBatch))
    ?? throw new InvalidOperationException("RuntimeBridge assembly is unavailable.");
var assemblyPath = assembly.Location;
var report = new
{
    schemaVersion = 1,
    profileId = input.ProfileId,
    profileHash = input.ProfileHash,
    scenarioId = input.ScenarioId,
    seed = input.Seed,
    workload = input.Workload,
    fixedStepSeconds = input.FixedStepSeconds,
    measuredIterations = input.MeasuredIterations,
    trials = input.Trials,
    machine = new
    {
        os = RuntimeInformation.OSDescription,
        architecture = RuntimeInformation.ProcessArchitecture.ToString(),
        framework = RuntimeInformation.FrameworkDescription,
        processorCount = Environment.ProcessorCount,
    },
    build = new
    {
        runtimeBridgeHash = "sha256:" + Convert.ToHexString(
            SHA256.HashData(File.ReadAllBytes(assemblyPath))).ToLowerInvariant(),
        runtimeBridge = assembly.GetName().Version?.ToString() ?? "unknown",
    },
    counters = new
    {
        updateTime = "Available",
        lifecycleTransitions = "Available",
        allocations = "Unavailable",
        memory = "Unavailable",
    },
    highLevel = Summarize(high),
    lowLevel = Summarize(low),
    conclusion = "Measurements cover only the deterministic managed collectible lifecycle workload; no fixed speedup is claimed.",
};
Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(args[1]))!);
File.WriteAllText(args[1], JsonSerializer.Serialize(report,
    new JsonSerializerOptions { WriteIndented = true }) + "\n", new UTF8Encoding(false));
Console.WriteLine(JsonSerializer.Serialize(report));

static Trial MeasureHigh(VerifiedRunInput input)
{
    var states = new CollectibleState[input.Workload];
    for (var index = 0; index < states.Length; ++index)
        states[index] = CollectiblesHighLevel.Create(
            new CollectibleDefinition(input.FixedStepSeconds, input.Workload, index));
    var score = 0;
    var start = Stopwatch.GetTimestamp();
    for (var iteration = 0; iteration < input.MeasuredIterations; ++iteration)
    {
        var selected = (int)((input.Seed + (ulong)iteration) % (ulong)states.Length);
        var collected = CollectiblesHighLevel.Collect(states[selected]);
        if (collected != states[selected])
        {
            states[selected] = CollectiblesHighLevel.ScheduleRespawn(
                CollectiblesHighLevel.Deactivate(collected));
            score += 10;
        }
        for (var index = 0; index < states.Length; ++index)
        {
            var advanced = CollectiblesHighLevel.AdvanceRespawnTime(
                states[index], input.FixedStepSeconds);
            states[index] = CollectiblesHighLevel.CompleteRespawn(advanced, iteration);
        }
    }
    var elapsed = Stopwatch.GetElapsedTime(start).TotalNanoseconds;
    return new Trial(elapsed / input.MeasuredIterations, score,
        states.Count(state => state.Phase == CollectiblePhase.Active),
        StateHash(states.Select(state => ((int)state.Phase, state.SpawnIndex))));
}

static Trial MeasureLow(VerifiedRunInput input)
{
    var batch = new CollectiblesLowLevelBatch(
        input.Workload, input.FixedStepSeconds, input.Workload);
    var score = 0;
    var start = Stopwatch.GetTimestamp();
    for (var iteration = 0; iteration < input.MeasuredIterations; ++iteration)
    {
        var selected = (int)((input.Seed + (ulong)iteration) % (ulong)input.Workload);
        if (batch.TryCollect(selected)) score += 10;
        batch.Advance(input.FixedStepSeconds, iteration);
    }
    var elapsed = Stopwatch.GetElapsedTime(start).TotalNanoseconds;
    var states = Enumerable.Range(0, batch.Capacity)
        .Select(index => ((int)batch.GetPhase(index), batch.GetSpawnIndex(index)));
    return new Trial(elapsed / input.MeasuredIterations, score,
        batch.ActiveCount, StateHash(states));
}

static object Summarize(Trial[] trials)
{
    var raw = trials.Select(trial => trial.NanosecondsPerIteration).Order().ToArray();
    var mean = raw.Average();
    return new
    {
        rawNanosecondsPerIteration = trials.Select(trial => trial.NanosecondsPerIteration).ToArray(),
        medianNanosecondsPerIteration = Percentile(raw, 0.50),
        p95NanosecondsPerIteration = Percentile(raw, 0.95),
        p99NanosecondsPerIteration = Percentile(raw, 0.99),
        varianceNanosecondsSquared = raw.Select(value => (value - mean) * (value - mean)).Average(),
        lifecycleTransitions = trials[0].Score / 10 * 2,
        score = trials[0].Score,
        activeCount = trials[0].ActiveCount,
        stateHash = trials[0].StateHash,
    };
}

static double Percentile(double[] sorted, double percentile) =>
    sorted[Math.Clamp((int)Math.Ceiling(percentile * sorted.Length), 1, sorted.Length) - 1];

static string StateHash(IEnumerable<(int Phase, int Spawn)> states)
{
    var bytes = Encoding.UTF8.GetBytes(string.Join(";", states.Select(
        state => $"{state.Phase}:{state.Spawn}")));
    return "sha256:" + Convert.ToHexString(SHA256.HashData(bytes)).ToLowerInvariant();
}

internal sealed record VerifiedRunInput(
    int SchemaVersion,
    string ProfileId,
    string ProfileHash,
    string ScenarioId,
    int Warmup,
    int Trials,
    int MeasuredIterations,
    double FixedStepSeconds,
    ulong Seed,
    int Workload,
    string[] RequestedCounters,
    string RuntimeMode,
    string EvidencePath)
{
    public void Validate()
    {
        if (SchemaVersion != 1 || ProfileId.Length == 0 ||
            ProfileHash.Length != 71 || !ProfileHash.StartsWith("sha256:", StringComparison.Ordinal) ||
            ScenarioId != "arena-rush.collectibles" || Warmup < 0 || Trials < 1 ||
            MeasuredIterations < 1 || !double.IsFinite(FixedStepSeconds) ||
            FixedStepSeconds <= 0 || Workload < 1 || RuntimeMode != "ManagedRuntime" ||
            RequestedCounters is null || Path.IsPathRooted(EvidencePath) ||
            !EvidencePath.StartsWith("Build/Reports/Benchmarks/", StringComparison.Ordinal) ||
            EvidencePath.Contains("..", StringComparison.Ordinal))
            throw new InvalidOperationException("Verified benchmark run input is invalid.");
    }
}

internal readonly record struct Trial(
    double NanosecondsPerIteration,
    int Score,
    int ActiveCount,
    string StateHash);
