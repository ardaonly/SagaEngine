# Entity Schema V1

## Status

Entity schema v1 describes source-controlled entity truth inside a scene source
file. It is not a runtime spawn, replication, gameplay authority, or editor
placement implementation.

## Required Fields

An entity is a JSON object with:

- `entityId`: stable non-empty string unique within the scene
- `displayName`: string
- `sourceKind`: `EntitySourceTruth`
- `components`: array of component metadata objects
- `assetReferences`: optional array of asset reference objects

## Asset References

Asset references are owned by scene/entity source truth and resolved against
project asset roots or source-controlled asset manifests. Each accepted
reference should declare:

- `assetId`
- `owner`
- `path`
- `usage`

Missing owners are source truth debt. Generated artifacts and generated/package
asset manifests cannot own entity asset references.

## Validation Report

`scene_entity_validation_report.json` lists scenes, entities, asset references,
generated artifacts, schema checks, diagnostics, and the report-only invariants:
`localOnly=true`, `networkExposure="None"`, `mutatesSource=false`, and
`enforcement="ReportOnly"`.
