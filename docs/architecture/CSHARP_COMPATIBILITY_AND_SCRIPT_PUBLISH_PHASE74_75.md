# C# Compatibility And Script Publish Evidence Phase 74-75

Phase 74 adds a conservative Saga-compatible C# profile report. Phase 75 wires
that report into read-only script artifact validation, optional publish-check
evidence, and AlphaGate evidence recording.

## SagaScript Outputs

`sagascript compatibility-profile --source <file-or-dir> --out <dir> [--json]`
writes `csharp_compatibility_profile_v2.json`.

The profile classifies source constructs as:

- `EditableByPatch` only for supported `StringLiteral` source-map targets that
  can use `ReplaceStringLiteral`.
- `ReadOnlyProjectable` for Saga behavior metadata, behavior methods, simple
  conditions, supported invocations, Saga libraries, Saga nodes, ProjectionOnly
  nodes, and Deferred nodes disclosed as read-only.
- `Opaque` for advanced C# regions such as LINQ, lambdas, anonymous functions,
  async/await, patterns, and unknown invocation shapes.
- `Unsupported` for valid C# constructs outside the current Saga-compatible
  authoring profile.
- `Invalid` for syntax errors and malformed or inconsistent Saga metadata.

`sagascript validate-artifacts --artifact-root <dir> --out <report> [--json]`
checks the script artifacts already present under an artifact root. It validates
artifact envelopes, source-hash presence, node capability consistency, passed
compatibility profile status, patch report mutation flags, and runtime-proof
state.

## Runtime Proof Rules

`runtime_bindings.json` now preserves node ids, node capability, projection
compatibility, compatibility classification, library ids, and runtime proof
state.

`ProjectionOnly` is not runtime proof. `Deferred` is not runtime proof.
`RuntimeBacked` requires explicit runtime evidence; without that evidence,
artifact validation fails with a missing-runtime-proof diagnostic.

## Publish And Alpha Evidence

`sagapack publish-check` accepts optional
`--script-validation <script_artifact_validation_report.json>`. When supplied,
the report becomes required evidence and the
`ScriptArtifactValidationAccepted` gate must pass. When absent, existing
publish-check behavior remains unchanged.

`sagaalphagate script-evidence --script-validation <report> --out <report>`
records the SagaScript validation status and runtime-proof summary. It is
report-only evidence and does not claim alpha completion.

## Non-Claims

Phase 74-75 do not add runtime node execution, runtime gameplay, server
gameplay, ClientHost work, graph editing, Editor UI, Qt UI, Phase 76+ work, or
Hedef 3 work. They also do not claim full visual scripting or arbitrary C# to
blocks.
