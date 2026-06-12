# SagaEngine Test Suites

> Status: Current local testing policy and suite inventory

This document lists the current testing surface without treating historical
milestone evidence as current pass status. It is not a raw full CTest pass
ledger, CI guarantee, release gate, product beta signal, or production readiness
claim.

Use [README.md](README.md) for the shortest testing entry point.

## Local Gate Policy

Repository structural checks:

```sh
scripts/build-default --dry-run
scripts/test-taxonomy --check
scripts/verify-local
```

`scripts/verify-local` is structural by default. It does not run a real build or
CTest unless `--with-build` or `--with-tests` is passed explicitly.

For narrow changes, run the affected target or focused test group first. On this
repo, prefer single-job local execution unless the maintainer explicitly asks
for broader parallel runs.

## Stable Suite Inventory

Forge suite names are stable local profiles, not the complete raw CTest label
inventory:

| Suite | Intended coverage | Non-claims |
| --- | --- | --- |
| `architecture` | Dependency, public/private, Qt, CMake, and docs-boundary checks where registered. | Does not prove all architecture debt is cleaned. |
| `unit` | Focused unit tests for core modules and tools. | Does not prove full product workflows. |
| `runtime` | Runtime/package/startup/asset-focused tests where built. | Does not prove playable runtime product. |
| `server` | Server authority and diagnostics-focused tests where built. | Does not prove production networking or MMO scale. |
| `asset` | Asset/package/manifest-focused tests where built. | Does not prove production import/cook pipeline. |
| `editor` | Editor boundary and authoring model tests where built. | Does not prove finished editor workflow. |
| `tools` | Tool CLI and report-contract tests where built. | Does not prove tool ecosystem or SDK completion. |
| `all-safe` | Local safety profile excluding heavy/slow/stress/load-style tests. | Does not prove raw full CTest health. |

CTest labels may include product, package, UI, collaboration, stress, slow,
load, long-running, benchmark, perf, and other local labels outside this stable
suite table.

## Focused Evidence Appendix

Focused evidence entries should be recorded in this format:

| Area | Evidence shape | Non-claim |
| --- | --- | --- |
| Project validation | Tool command plus report path and exit status. | Not runtime/editor/package readiness. |
| Runtime/server smoke | Bounded command plus report and diagnostics output. | Not playable game, production server, or scale proof. |
| SagaScript/C# blocks | Analyze/compile/projection/patch-preview reports. | Not full visual scripting UX or arbitrary C# roundtrip. |
| Editor shell | No-UI inspection/report model. | Not finished editor workflow. |
| Collaboration/workspace | Local metadata/report-only commands. | Not cloud, auth, CRDT/OT, real permissions, or team-editing product. |
| Publish/distribution | Preflight/archive/checksum/smoke evidence. | Not release pipeline, installer, signing, marketplace, or final package readiness. |
| Diagnostics/performance | Deterministic reports, counters, bounded stress artifacts, and workflow budgets. | Not production observability, crash safety, benchmark suite, or performance readiness. |

Historical milestone commands can remain useful as context under internal
history, but current testing claims should be restated in the format above.

## Heavy Tests

Stress, soak, load, benchmark, bot, real-transport, slow, long-running, and perf
tests are opt-in. They must not be silently included in the default local gate.

Passing focused tests does not imply:

- raw full CTest health;
- product beta or release candidate status;
- production networking or MMO readiness;
- complete editor workflow;
- complete visual scripting;
- enterprise readiness;
- release packaging or distribution readiness.

## Reporting Results

When reporting test results, include:

- exact command;
- pass/fail/blocked status;
- build tree or environment assumption if relevant;
- report path when a tool emits one;
- what the result proves;
- what it explicitly does not prove.
