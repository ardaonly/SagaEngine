# Graphics Backend Preference Order

> Last updated: 2026-06-06

This document records the internal meaning of `SagaGraphics` backend
preferences while the Diligent bridge remains behind the existing render
backend implementation. It is not a stable external SDK claim.

## Preference Meaning

`BackendPreference::Auto` leaves platform selection to the private backend.
For automatic selection, the intended order is:

- Linux: prefer `NativePortable`, then `Compatibility`.
- Windows: prefer `NativePrimary`, then `NativePortable`, then
  `Compatibility`.
- Other platforms: prefer `NativePortable`, then `Compatibility` until a
  platform-specific backend policy is documented.

`NativeLegacy` is reserved for older native backend paths when a platform needs
them. It is not the default first choice.

## Mapping Boundary

`SagaEngine/Graphics` uses `BackendPreference` in its public shell. The
existing render backend uses `GraphicsBackendAPI`. Private mapping code may
translate between the two, but public graphics headers must not expose Diligent
or native API types.

This mapping does not create a `SagaGraphics` factory, does not move
`SagaEngine/Render/Backend`, and does not perform the R3 bridge migration.
