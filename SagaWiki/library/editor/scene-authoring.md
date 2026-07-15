---
title: Scene and entity authoring
status: bounded
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Scene and entity authoring

Scene authoring changes project-owned source through semantic operations. Runtime world state is a consumer or instantiated view of that source, not the editor's hidden save format.

## Durable contract

A scene/entity authoring contract should make these facts explicit:

- stable project-relative identity for the scene and referenced assets;
- entity/component identity independent of a particular widget;
- deterministic serialization where the format promises it;
- validation before source mutation;
- clear distinction between authored defaults and runtime state;
- origin and freshness for generated/imported representations.

The current project-tooling schema expects scene identity, `SceneSourceTruth`, entity identity, `EntitySourceTruth`, component metadata, asset references, and optional generated-artifact records marked non-canonical. Generated records carry an expected source hash when freshness matters. `sagaproject scene-entity-validate` reports malformed input, missing identities, duplicate entities, invalid references, and stale generated evidence without silently repairing source.

## Inspector and viewport

Inspectors edit domain properties through authoring commands. Viewports visualize and manipulate a projection of scene state. Neither should bypass transaction, validation, or source ownership merely for interactive convenience.

## Imports and generated data

Imported assets and generated scene data must remain traceable to their inputs. Reimport should preserve intentionally authored data or surface conflicts according to the owning contract; silent replacement is not a safe default.

## Evidence limit

Current editor and scene foundations do not establish a complete scene editor, universal interchange support, production undo/redo across every asset type, or a stable public plugin contract. Historical scene design is retained here only where it protects source identity and transaction semantics.

See [Scene, entity, and project-slice contracts](../reference/scene-entity-and-project-slice-contracts.md) for schema, asset references, semantic edits, runtime prerequisites, slices, visibility, and resolver failure rules.
