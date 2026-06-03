# Phase 19 Known Limitations

- This phase is not verified.
- No public product claim should be derived from this file.
- Current SDE manifest JSON emits `artifactVersion`; it does not emit a separate
  `schemaVersion` field.
- The manifest's `Diagnostics` output entry currently has an empty hash.
- Phase 19 proves deterministic artifact/manifest behavior through focused SDE
  tests and the standalone SDE package/test flow. It is not SagaEngine
  package/distribution proof.
- Direct `python3 Tools/SystemDefinitionEngine/build.py --tests --jobs 1`
  remains limited by the Phase 18 local dependency-resolution issue. The
  verified SDE-specific path is the low-load Conan package/test command.
- No SagaScript, Visual Blocks, runtime, server, editor, package/distribution,
  StarterArena gameplay, or CSharpScriptHost behavior is changed.
