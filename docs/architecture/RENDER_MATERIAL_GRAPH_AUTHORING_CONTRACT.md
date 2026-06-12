# Render Material Graph Authoring Contract

This document defines the renderer-facing output that a future material graph
authoring tool may produce. It is a data contract, not an editor UI
implementation and not a rendered material claim.

## Scope

- `MaterialGraphOutputSchema` is the cooked graph output accepted by renderer
  contract code.
- Node output may request shader variant features through
  `ShaderVariantFeature`.
- Feature requests are checked against `ShaderVariantBudgetConfig` before the
  graph is accepted.
- Cooked graph output maps into the existing `MaterialAsset` shape; it does not
  replace the material runtime model.

## Supported Node Categories

- Constant scalar parameter.
- Constant vector parameter.
- Texture sample.
- Normal transform.
- Add, multiply, and fresnel math nodes.

Unsupported nodes emit `MaterialGraph.NodeUnsupported`. Schema mismatches emit
`MaterialGraph.SchemaVersionUnsupported`. Feature overflow emits
`MaterialGraph.FeatureBudgetExceeded`.

Custom nodes are not accepted by this v1 contract. Any future escape hatch must
declare its shader feature cost, cooked compatibility version, and fallback
behavior before it can enter renderer-facing graph output.

## Non-Claims

- This contract does not implement an editor graph canvas.
- This contract does not compile shaders.
- This contract does not update native descriptor bindings.
- This contract does not prove a PBR material render path or golden image.
- This contract does not implement shader or material hot reload.
