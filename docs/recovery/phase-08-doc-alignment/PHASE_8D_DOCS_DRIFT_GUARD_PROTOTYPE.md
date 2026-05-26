# Phase 8D Docs Drift Guard Prototype

> Last updated: 2026-05-26
> Status: Phase 8D report-only docs drift guard prototype
> Phase 8: Documentation / Code Alignment Enforcement

Phase 8D defines a report-only prototype for detecting high-risk stale or
unsupported documentation phrases. It does not add a script, CI step, CMake
target, hard-fail guard, or publish enforcement.

Full CTest remains unverified.

## Guard Intent

The future guard should report risky wording before it becomes accepted
engineering truth. It should not block docs that explicitly state non-claims,
foundation closures, deferred debt, or future roadmap intent.

High-risk phrase families:

- unqualified `production-ready`
- broad `Qt-free`
- full `server-authoritative`
- unqualified `publish-ready`
- broad `shipped`
- unqualified `complete`
- unqualified `implemented`

## Prototype Report Model

A future report-only scanner should emit:

- file path
- line number
- matched phrase
- classification candidate
- nearby context snippet
- suggested evidence source or correction owner

Recommended initial output:

```txt
Build/Reports/docs_drift_report.txt
```

JSON output can follow later only after the classification categories prove
stable.

## False-Positive Controls

Allowed contexts before any hard-fail behavior:

- explicit negative claims, such as `not production-ready`
- explicit non-claims, such as `does not claim complete Qt removal`
- foundation/checkpoint closures, such as `Foundation established`
- future roadmap intent, especially unchecked roadmap items
- bounded local evidence, such as a named panel public header being Qt-free
- quoted command/test names or historical release references

Suspicious contexts to report:

- unqualified product readiness claims
- broad subsystem completion claims without nearby evidence
- `shipped` claims without release or build evidence
- `Qt-free` claims that do not name the bounded surface
- `server-authoritative` claims that imply full multiplayer authority
- `publish-ready` claims that imply hard-gate enforcement

## Enforcement Readiness

Hard-fail enforcement is not ready in Phase 8D.

Prerequisites before any blocking guard:

- report-only scanner exists and is reviewed across recovery and non-recovery
  docs
- false positives are classified and reduced
- allowed contexts are encoded explicitly
- stale phrase owners are documented
- guard scope starts with recovery docs before broad roadmap coverage
- CI behavior is opt-in or warning-only before blocking

## Non-Goals

Phase 8D does not:

- add a scanner script
- add a CMake or CI target
- block builds or tests
- change source, tests, labels, or publish policy
- rewrite roadmap claims
- assert all docs are aligned
- prove full CTest health

## Verification

Verification completed for this prototype:

- `git diff --check`
- touched-doc trailing whitespace check
- targeted `rg` for Phase 8D wording, high-risk phrases, false-positive
  controls, report-only guard status, non-goals, and full CTest caveats

Full CTest remains unverified.
