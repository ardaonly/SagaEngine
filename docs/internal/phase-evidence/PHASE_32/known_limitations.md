# Phase 32 Known Limitations

- This phase is not verified.
- This phase closes only the `missing sde executable` package preflight blocker.
- `Tools/SystemDefinitionEngine/bin/sde` is ignored build output produced by the
  real SDE bootstrap path, not a tracked binary.
- The package preflight still does not build missing tools by itself.
- No fake `sde` binary or placeholder wrapper is created.
- No SDE compiler semantics are changed.
- No final Linux distribution layout is created.
- No `VERSION`, `VERIFY.txt`, or `KNOWN_LIMITATIONS.md` files are created under
  `build/dist/linux/Saga`.
- No `Saga.tar.zst` archive is created.
- No `Saga.sha256` checksum is created.
- Package readiness and distribution readiness remain unclaimed.
- No public product claim should be derived from this file.
