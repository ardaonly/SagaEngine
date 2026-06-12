# Scene Schema V1

## Status

Scene schema v1 describes source-controlled scene truth for validation and
inventory. It does not add Runtime gameplay, Server gameplay, ClientHost
behavior, Editor UI, Qt UI, asset import, or asset cook paths.

## Required Fields

A scene source file is a JSON object with:

- `schemaVersion`: integer `1`
- `sceneId`: stable non-empty string
- `displayName`: string
- `sourceKind`: `SceneSourceTruth`
- `entities`: array of entity schema v1 objects
- `generatedArtifacts`: optional array of non-canonical generated evidence
- `readBoundaries`: report-only boundary metadata

## Generated Artifacts

Scene `generatedArtifacts` entries may point to projections or reports, but
they must not claim canonical source truth. Each entry should declare:

- `artifactId`
- `path`
- `artifactKind`
- `canonical=false`
- `expectedSourceHash`

## Validation

`sagaproject scene-entity-validate` diagnoses malformed JSON, missing scene ids,
unsupported schema versions, missing entity arrays, duplicate entity ids,
missing asset owners, and generated artifact canonical claims.
