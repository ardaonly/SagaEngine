# Graphics Capability Matrix v0

> Last updated: 2026-06-06

This document records the R3C conservative capability matrix v0 for
`SagaEngine/Graphics`. It is a vocabulary and fallback contract, not a runtime
GPU feature probe.

## Scope

The public API stays vendor-neutral. Matrix rows use `BackendPreference`
vocabulary and public scalar capability fields only. They do not expose
backend-native types, Diligent types, or runtime-selected native API objects.

## Platform Vocabulary

| Platform | Preference row | Meaning in v0 |
| --- | --- | --- |
| Linux | `NativePortable` | Preferred native-portable path. Reports conservative ready values when the adapter is ready. |
| Linux | `Compatibility` | Compatibility fallback path. Reports conservative ready values when the adapter is ready. |
| Windows | `NativePrimary` | Preferred native-primary path. Reports conservative ready values when the adapter is ready. |
| Windows | `NativePortable` | Portable native fallback path. Reports conservative ready values when the adapter is ready. |
| Windows | `Compatibility` | Compatibility fallback path. Reports conservative ready values when the adapter is ready. |
| Other | `NativePortable` | Default vocabulary row until a platform-specific policy is documented. |
| Other | `Compatibility` | Conservative fallback vocabulary row. |

## Conservative Values

When a backend is unavailable, failed, minimized, uninitialized, shutdown, or
headless, the shell reports the conservative baseline:

| Field | Value |
| --- | --- |
| `qualityCeiling` | `Low` |
| `compute` | `Unsupported` |
| `timestampQueries` | `Unsupported` |
| `bindlessResourceArrays` | `Unsupported` |
| `rayTracing` | `Unsupported` |
| `hdrSwapchain` | `Unsupported` |
| `textureCompressionBC` | `Unsupported` |
| `textureCompressionASTC` | `Unsupported` |
| `textureCompressionETC2` | `Unsupported` |
| `meshShaders` | `Unsupported` |
| `indirectDraw` | `Unsupported` |
| `conservativeRaster` | `Unsupported` |
| `depthBounds` | `Unsupported` |
| `maxTexture2DSize` | `1024` |
| `maxColorAttachments` | `1` |
| `maxFramesInFlight` | `1` |

When the Diligent adapter is ready, v0 raises only the conservative scalar
ceiling that is currently test-backed:

| Field | Value |
| --- | --- |
| `qualityCeiling` | `Medium` |
| `maxTexture2DSize` | `4096` |
| `maxColorAttachments` | `1` |
| `maxFramesInFlight` | `1` |

All native feature fields remain `Unsupported` in v0 unless a later runtime
probe proves and tests a higher value.

## Fallback Policy

`ResolveRenderCapabilityFallback` is the public fallback decision helper:

- unsupported and not requested -> `NotRequested`
- supported and requested -> `Available`
- unsupported and requested -> `DisabledUnsupported`

This is deterministic fallback vocabulary. Human-readable fallback diagnostics,
JSON capability artifacts, and runtime-probed native backend rows remain future
work.
