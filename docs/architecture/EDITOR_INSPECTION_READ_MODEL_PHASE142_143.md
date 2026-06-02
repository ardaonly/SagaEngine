# Editor Inspection Read Model Phase 142-143

Phase 142 emits a backend-neutral editor inspection model from scene/entity
source truth. The model includes scenes, entities, component metadata, asset
references, diagnostics, and generated freshness status.

Phase 143 re-reads declared scene source files from the inspection model and
fails if required source files are missing or generated artifacts claim
canonical source truth. `editorMayMutate=false` is explicit.

Prohibited actions include scene editing, entity placement, Editor UI mutation,
Qt UI mutation, and C# source mutation. No playable editor, Editor UI, Qt UI,
asset import, asset cook, or source mutation is claimed.
