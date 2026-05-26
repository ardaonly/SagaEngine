# Testing Entry

> Last updated: 2026-05-26
> Current truth: Foundation Established accepts limited local gate evidence, not
> Product Beta or Release Candidate evidence.

Use [TEST_SUITES.md](TEST_SUITES.md) for exact suite names and detailed command
policy. This file is the short gate map.

## Normal Local Gate

Current accepted local evidence is the Phase 9E normal local gate:

```sh
ctest --test-dir build/RelWithDebInfo-0.0.9 -LE "stress|slow|load|long-running|benchmark|perf" --output-on-failure --timeout 120 -j 1
```

It passed 36/36 on 2026-05-26 with heavy labels excluded. It is not raw full
CTest.

## raw full CTest

raw full CTest remains unresolved and is not pass evidence. Do not treat raw
CTest registration, labels, or previous incomplete attempts as a passing result.

## heavy gates

heavy gates are opt-in only. `StressTests` and heavy `ReplicationTests` are not
part of the normal local gate and are not current pass evidence.

## Focused Package / Publish / Runtime Gate

Phase 10 accepted a focused 12-test package/publish/runtime compatibility proof.
It supports the report-first publish readiness foundation, but it does not prove
release readiness, full product packaging, full AssetPipeline cook/import,
ClientHost asset consumption, raw full CTest health, or heavy gates.

## Detail Source

Read [TEST_SUITES.md](TEST_SUITES.md) for Forge suite names, CTest label
behavior, focused architecture entries, focused package/publish/runtime entries,
known limitations, and exact non-claims.
