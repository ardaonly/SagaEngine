# SagaEngine Small-Team Alpha Quickstart

Status: Phase 94 onboarding evidence for Target 2 / Small-Team Alpha.

This quickstart is for a technical user validating the local Small-Team Alpha
foundation with focused reports. It is not a public release path.

## Evidence Root

Use `Build/SmallTeamAlpha` for generated reports:

```sh
mkdir -p Build/SmallTeamAlpha
```

## Focused Gate Commands

Run only the focused commands needed for the local evidence set:

```sh
Tools/SagaAlphaGate/sagaalphagate accept-alpha --evidence-root Build/SmallTeamAlpha --out Build/SmallTeamAlpha/small_team_alpha_acceptance_report.json
Tools/SagaAlphaGate/sagaalphagate evidence-matrix --evidence-root Build/SmallTeamAlpha --out Build/SmallTeamAlpha/small_team_alpha_evidence_matrix.json
Tools/SagaAlphaGate/sagaalphagate close-alpha --evidence-root Build/SmallTeamAlpha --out Build/SmallTeamAlpha/small_team_alpha_closure_report.json
```

`accept-alpha` and `evidence-matrix` read existing focused evidence. They do not
run raw full CTest, heavy stress, ClientHost preview, Runtime gameplay, Server
gameplay, editor UI, or patch application.

## Expected Closure Meaning

The successful local closure means Small-Team Alpha foundation evidence exists:
project validation, C# source-preserving authoring evidence, backend-neutral
editor workflow models, package/launch/asset/performance reports, and
local/offline collaboration review metadata.

## Non-Claims

- no product beta
- no release candidate
- no production MMO server
- no complete visual scripting
- no arbitrary C# roundtrip
- no enterprise readiness
- no cloud workspace platform
- no full collaboration
- no raw full CTest claim
