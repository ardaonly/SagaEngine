# Authoring Authority Model

> Status: Compact authority contract

This document records the current authority boundary for Saga authoring docs.
It is a contract for ownership and diagnostics language, not implementation
evidence, a rollout plan, or a product readiness claim.

## Core Rule

Saga uses server-authoritative multiplayer semantics for gameplay state:

```txt
Clients may request.
Clients may predict.
Clients may render and interpolate.
Servers decide authoritative truth.
```

Authoring tools must make that distinction visible. A graph, block, script, or
preview artifact must not imply that client-side execution creates durable
world truth.

## Authority Contexts

Authoring metadata may describe these execution contexts:

- `EditorOnly`: editor inspection, preview, reports, and authoring diagnostics.
- `ClientVisual`: presentation, UI, interpolation, and local-only effects.
- `ClientPredicted`: bounded prediction that must reconcile against server
  truth.
- `ServerAuthoritative`: gameplay mutations accepted as authoritative state.
- `SharedValidation`: deterministic validation metadata consumed by tools,
  runtime, server, or publish checks.

The context vocabulary is a boundary contract. It does not prove that every
runtime, server, editor, or publish path enforces the vocabulary today.

## Ownership

Saga product surfaces may select modes, display diagnostics, and block publish
or preview flows when accepted validation evidence says an artifact is unsafe.
They do not become the compiler, server authority implementation, network
transport, or security boundary.

SagaEditor may display authority context, block requirements, source spans,
prediction safety, replication effects, and diagnostics. It must not write C#
source directly or silently override authority errors.

Scripting and package tools own deterministic authoring validation artifacts where
implemented. Their artifacts may describe required authority, actual authority,
source ranges, severity, and suggested correction.

Runtime may consume client-safe artifacts for local preview, UI, visual, and
prediction-safe behavior. Runtime is not authoritative for multiplayer state.

Server owns authoritative gameplay decisions where server-authoritative flows
exist. Client-authored or client-predicted behavior must reconcile to server
truth.

Publish and policy gates may consume diagnostics and report blockers. Current
publish evidence is local/report-oriented unless a focused contract says
otherwise.

## Diagnostics Contract

Authority diagnostics should be machine-readable when a tool emits them:

- diagnostic code and severity;
- artifact, graph, node, source file, or source span where available;
- required authority and actual authority;
- prediction, replication, and persistence effects where relevant;
- whether the diagnostic blocks preview, build, publish, or only reports risk.

Diagnostics must be explicit about missing enforcement. A report-only authority
diagnostic is not permission enforcement, anti-cheat, cloud policy, or secure
source hiding.

## Current Non-Claims

- No finished editor authoring workflow.
- No complete Visual Blocks editor.
- No arbitrary C# roundtrip.
- No runtime graph VM claim.
- No production networking or complete multiplayer product loop.
- No production security, permissions, or source redaction boundary.
- No public SDK or final extension API.

## Related Current Boundaries

- [C# / Visual Authoring Boundary](SAGA_CSHARP_VISUAL_AUTHORING_BOUNDARY.md)
- [C# Visual Blocks Compatibility Profile](CSHARP_VISUAL_BLOCKS_COMPATIBILITY_PROFILE.md)
- [Visual Block Patch Contract](VISUAL_BLOCK_PATCH_CONTRACT.md)
- [Source Patch Application Policy](SOURCE_PATCH_APPLY_POLICY.md)
- [Runtime](RUNTIME.md)
- [Server Authority](SERVER.md)
- [Publish](PUBLISH.md)
- [Collaboration Current Boundary](SAGA_COLLABORATION_CURRENT_BOUNDARY.md)
