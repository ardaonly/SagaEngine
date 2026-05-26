# Phase 9D-2 Normal Gate Dry Run

> Last updated: 2026-05-26
> Status: Phase 9D-2 normal gate passed with heavy label exclusion
> Phase 9: Local Evidence Gates

Phase 9D-2 tested the normal local gate after the raw full CTest stability
blocker was documented. This slice uses heavy label exclusion and does not
change source, CMake, labels, quarantine, tests, CI, or publish enforcement.

Phase 9 remains open at this slice because the local gate matrix, raw full
CTest policy, and closure checkpoint still had to be documented after this
evidence was collected.

## Commands

List-only dry run:

```sh
ctest --test-dir build/RelWithDebInfo-0.0.9 -N -LE "stress|slow|load|long-running|benchmark|perf"
```

Execution dry run:

```sh
ctest --test-dir build/RelWithDebInfo-0.0.9 -LE "stress|slow|load|long-running|benchmark|perf" --output-on-failure --timeout 120 -j 1
```

## Result

The list-only normal gate selected 36 entries. The heavy label exclusion removed
`ReplicationTests` and `StressTests`.

The execution dry run passed:

| Gate | Result |
|---|---|
| Normal local gate with heavy label exclusion | 36 passed, 0 failed |
| Timeout | 120 seconds per test |
| Parallelism | `-j 1` |
| Total real time | about 12 seconds |

`CSharpGameplayProofTests` was included as test #33 and passed in 7.81 seconds.
That makes the C# gameplay proof status known for this build tree without a
separate isolated Phase 9D-3 run.

## Included Normal Gate Entries

The normal gate included the broad `UnitTests`, the focused server/runtime/
asset/package/product/tooling entries, `CSharpScriptHostTests`,
`CSharpGameplayProofTests`, `ArchitectureTests`,
`EditorQtPublicAbiBoundaryTests`, and `IntegrationTests`.

`IntegrationTests` remains timing-sensitive evidence, but it is not tagged with
`stress`, `slow`, `load`, `long-running`, `benchmark`, or `perf`, so it is part
of this normal local gate command.

## Excluded Heavy Coverage

The normal gate excludes and does not prove:

- `ReplicationTests`, because it is labeled `slow;long-running`
- `StressTests`, because it is labeled `stress;load;slow`
- stress coverage
- slow coverage
- load coverage
- long-running coverage
- benchmark or perf coverage

The `benchmark` and `perf` labels currently list no entries, but they remain in
the exclusion expression as normal-gate policy.

## Non-Claims

This Phase 9D-2 result does not claim:

- raw full CTest pass evidence
- heavy `ReplicationTests` pass evidence
- heavy `StressTests` pass evidence
- stress, slow, load, long-running, benchmark, or perf readiness
- release readiness
- package or publish enforcement readiness
- CI readiness for every environment

Raw full CTest is still not a pass. It remains unresolved because the complete
raw command has not returned a passing CTest summary after the previous
terminal/session instability.

## Next Decision

Because `CSharpGameplayProofTests` was clearly included and passed, the separate
Phase 9D-3 isolated C# run is not required for this continuation. The next
required slice is the Phase 9D-4 raw full CTest policy decision.

## Verification

Completed:

- normal gate list-only check with heavy label exclusion
- normal gate execution with heavy label exclusion

Follow-up verification for the docs slice:

- `git diff --check`
- touched-doc trailing whitespace scan
- targeted phrase scan for `Phase 9D-2`, `normal gate`,
  `heavy label exclusion`, `raw full CTest`, `stress`, `slow`, `load`,
  `long-running`, and `Phase 9 remains open`
