/// @file ScenarioResult.h
/// @brief Verdict + diagnostics emitted after a scenario runs.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace SagaEditorLab
{

// ─── Verdict ──────────────────────────────────────────────────────────────────

/// Top-level outcome of a scenario run. CI maps `Pass` and
/// `PassWithWarnings` to exit-0; everything else is exit-1.
enum class ScenarioVerdict : std::uint8_t
{
    Pass,
    PassWithWarnings,
    Fail,
    Error,        ///< Lab itself crashed / could not start the scenario.
    Skipped,      ///< Scenario opted out (e.g. requires a backend not present).
};

[[nodiscard]] const char* ScenarioVerdictId(ScenarioVerdict v) noexcept;

// ─── Diagnostic ───────────────────────────────────────────────────────────────

/// One issue raised during a scenario run. Severity decides whether
/// the verdict degrades; `stepIndex` points back at the offending step.
struct ScenarioDiagnostic
{
    enum class Severity : std::uint8_t { Info, Warning, Error, Fatal };

    Severity      severity      = Severity::Info;
    std::string   message;
    std::int32_t  stepIndex     = -1; ///< -1 means "outside any step".
    std::string   stepLabel;          ///< Echo of the step's label (if any).
};

// ─── Snapshot Reference ───────────────────────────────────────────────────────

/// The lab tags every captured `StateSnapshot` with a label and a hash
/// so reports can list "snapshot 'after-undo' (sha=…)" without dragging
/// the full snapshot blob into the result struct.
struct ScenarioSnapshotRef
{
    std::string  label;
    std::string  sha256;        ///< Hex digest, empty if hashing is disabled.
    std::int32_t stepIndex = -1;
};

// ─── Result ───────────────────────────────────────────────────────────────────

/// Full result of a scenario run. The lab CLI dumps this to stdout
/// (markdown / JUnit XML / JSON depending on `--report` flag); CI
/// consumes the exit code, the diagnostics list, and optionally the
/// snapshot SHAs for golden-file comparison.
struct ScenarioResult
{
    std::string                         scenarioId;
    std::string                         scenarioName;
    ScenarioVerdict                     verdict       = ScenarioVerdict::Skipped;
    std::uint64_t                       wallTimeMicros = 0;
    std::uint32_t                       stepsExecuted  = 0;
    std::uint32_t                       stepsTotal     = 0;
    std::vector<ScenarioDiagnostic>     diagnostics;
    std::vector<ScenarioSnapshotRef>    snapshots;

    // ─── Convenience Queries ──────────────────────────────────────────────────

    [[nodiscard]] bool   Ok()        const noexcept; ///< Pass or PassWithWarnings.
    [[nodiscard]] bool   HasErrors() const noexcept; ///< Any Error/Fatal diagnostic.
    [[nodiscard]] std::size_t WarningCount() const noexcept;
    [[nodiscard]] std::size_t ErrorCount()   const noexcept;

    /// Append a diagnostic and degrade the verdict if needed.
    void AddDiagnostic(ScenarioDiagnostic diag);
};

} // namespace SagaEditorLab
