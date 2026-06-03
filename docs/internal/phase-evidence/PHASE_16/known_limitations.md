# Phase 16 Known Limitations

- This phase is not verified.
- Only `StringLiteralEdit` is applied.
- `apply-block-edit` writes a patched copy by default; it does not overwrite the
  original C# source file.
- No in-place block edit mode is exposed in this phase.
- No Visual Blocks editor UI exists.
- No block graph runtime exists.
- No arbitrary C# to blocks conversion is claimed.
- No method, class, file, using, or formatting regeneration is implemented.
- Runtime, server, package/distribution, SDE, StarterArena gameplay, and
  CSharpScriptHost behavior are unchanged.
