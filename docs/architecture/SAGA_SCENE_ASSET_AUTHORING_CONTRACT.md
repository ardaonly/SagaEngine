# Saga Scene and Asset Authoring Contract

## Status

This document source-truth policy and explicit deferrals for Block D.

## Current Source Truth

`.sagaproj` is the current project-level source truth for asset root
references. The accepted MultiplayerSandbox project currently declares an
`Assets` directory reference, but that folder is placeholder-only unless it
contains an accepted asset source manifest such as `asset_manifest.json` or
`assets.source.json`.

Generated package manifests, including `Manifests/assets.json` and
`Manifests/asset_identity.json`, are package evidence. They are not accepted as
authoring source truth for scenes, entities, prefabs, or imported assets.

## Required Honesty

- Missing scene/entity truth must be reported as missing source truth, not as a
  passed workflow.
- Missing accepted asset manifest/source truth must be reported as
  `MissingSourceOfTruth`, not as a passed workflow.
- Placeholder asset folders can be source-controlled evidence, but they do not
  prove asset import, asset cook, scene editing, prefab editing, or entity
  placement.
- Stale or missing generated package asset manifests are package evidence
  diagnostics only unless an explicit package gate consumes them.

## Deferred Work

Full asset import, asset cooking, asset streaming, scene editing, prefab
editing, entity placement, ClientHost asset consumption, and runtime gameplay
source truth remain deferred until later milestones add bounded contracts and
evidence.
