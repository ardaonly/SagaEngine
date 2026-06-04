# Phase 31 Known Limitations

- This phase is not verified.
- `scripts/package-linux-saga` is preflight-only.
- No Linux package is built by this phase.
- No final distribution layout is staged by this phase.
- No archive or checksum is created by this phase.
- No placeholder `sde` executable is created by this phase.
- The current preflight report is blocked by the missing `sde` executable,
  missing final distribution layout, missing distribution metadata files,
  missing archive, and missing checksum.
- A passing future preflight report would not prove runtime, editor, server, or
  tool workflows beyond input existence checks.
- No public package or distribution readiness claim should be derived from this
  file.
