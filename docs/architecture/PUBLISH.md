# Publish

> Last updated: 2026-05-26

Publish-related tooling can check selected project/package inputs and report
deterministic blockers. It is not a full release packaging or publishing
system.

## Current State

- Publish report schema version 1 is stable for current fields.
- Current hard blockers include deterministic project, package, and explicit
  diagnostics failures.
- Missing cooked assets, unsupported asset kinds, duplicate identity mappings,
  unresolved raw full CTest, and heavy checks remain warnings or deferred work.
- Release packaging and CI hard enforcement are absent.

## Evidence Boundary

Current evidence supports deterministic publish reports and selected
project/package blocker reporting. It does not prove a complete release
pipeline, production packaging, CI hard enforcement, installer, updater,
marketplace, signing, or final distribution readiness.
