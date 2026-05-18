# Iteration Notes

> Status: Active development note
> Current iteration: `0.0.8-dev.4`
> Purpose: Track the current implementation batch before roadmap, architecture, diagnostics, manifest, and test documents are updated.

---

## 0. Purpose

This document records the current development iteration.

It is not a changelog.

It is not a release note.

It is not a roadmap.

It is not permanent architecture truth.

It is a temporary working note used to capture what changed during the current batch.

Permanent decisions belong in:

```txt
docs/*_ROADMAP.md
docs/DependencyGraph.md
docs/SCHEMA.md
```

This file may be rewritten for each iteration.

---

## 1. Iteration

```txt
Version: 0.0.8-dev.4
Status: Draft
Date: 2026-05-18
```

Short summary:

```txt
Next implementation slice pending.
```

---

## 2. What Was Added

```txt
- Nothing added yet.
```

---

## 3. What Was Changed

```txt
- Nothing changed yet.
```

---

## 4. What Was Removed

```txt
- Nothing removed.
```

---

## 5. Files Changed

```txt
- docs/dev/ITERATION_NOTES.md
```

---

## 6. Ownership / Boundary Notes

```txt
Allowed:
- No new ownership boundary yet.

Forbidden:
- Preserve existing DependencyGraph.md ownership boundaries.
- Keep SDE standalone.
- Keep Forge as an orchestrator only.
- Keep Prism as an analyzer only.
- Keep runtime/server as manifest/artifact consumers.
- Keep editor as authoring UX only.
- Keep SagaShared implementation-free.
```

---

## 7. Diagnostics / Manifests / Reports

Diagnostics:

```txt
- None added yet.
```

Manifests:

```txt
- None added yet.
```

Reports:

```txt
- None added yet.
```

---

## 8. Verification

```txt
Command:
Not run

Result:
Not verified

Notes:
No implementation work has been done in this new iteration yet.
```

---

## 9. Roadmaps To Update

```txt
[ ] EDITOR_ROADMAP.md
[ ] SHARED_ROADMAP.md
[ ] ENGINE_ROADMAP.md
[ ] SDE_ROADMAP.md
[ ] FORGE_ROADMAP.md
[ ] PRISM_ROADMAP.md
[ ] COLLABORATION_ROADMAP.md
[ ] TOOLS_ROADMAP.md
[ ] DependencyGraph.md
[ ] DIAGNOSTICS_ROADMAP.md
[ ] ASSET_PIPELINE_ROADMAP.md
[ ] SAGA_SCRIPTING_ROADMAP.md
[ ] BUILD_PUBLISH_PIPELINE_ROADMAP.md
[ ] AssetStreamingImplementation.md
[ ] SCHEMA.md
[ ] Other:
```

Reason:

```txt
No roadmap files are affected yet.
```

---

## 10. Known Problems

```txt
- None recorded yet for this iteration.
```

---

## 11. Next Actions

```txt
[ ] Decide the next implementation slice.
[ ] If continuing EditorLab, add a narrow EditorHost-backed adapter using only SagaEditor public APIs.
[ ] If continuing runtime package work, decide the production AssetKey to AssetId mapping source before automatic registry bootstrap.
```

---

## 12. Roadmap Update Request

Use this block when updating roadmaps from this iteration.

```txt
Read docs/dev/ITERATION_NOTES.md.

Update only the roadmap files marked in section 9.

Rules:
- Do not rewrite unrelated sections.
- Do not mark unverified work as shipped.
- If status is Draft or Partially Implemented, keep roadmap items open.
- Preserve DependencyGraph.md ownership boundaries.
- Keep SDE standalone.
- Keep Forge as orchestrator only.
- Keep Prism as analyzer only.
- Keep runtime/server as manifest/artifact consumers.
- Keep editor as authoring UX only.
- Keep SagaShared implementation-free.
```

---

## 13. Next Iteration

```txt
Next iteration: 0.0.8-dev.5

Possible focus:
- Public SagaEditor-backed EditorLab scenario adapter.
- Product-facing runtime/server child diagnostic capture after a narrow child diagnostic contract is defined.
- Package/build output source for AssetKey to AssetId resolver mappings.
```
