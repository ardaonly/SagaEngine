# Breaking Render API Change: Generic Overlay Rendering

Status: active migration note.

The render backend no longer exposes an ImGui-specific rendering surface. The
old backend entry points for initializing ImGui rendering, submitting ImGui draw
data, and shutting ImGui rendering down were intentionally removed instead of
kept as deprecated aliases.

Use these public, vendor-neutral overlay APIs from
`SagaEngine/Render/Backend/RenderBackendFactory.h`:

- `InitBackendOverlayRendering`
- `CreateBackendOverlayTexture`
- `DestroyBackendOverlayTexture`
- `RenderBackendOverlayFrame`
- `ShutdownBackendOverlayRendering`

The replacement path consumes `RenderOverlayFrame`, `RenderOverlayDrawList`,
`RenderOverlayDrawCommand`, and `RenderOverlayTextureHandle`. Applications that
use Dear ImGui must convert ImGui draw data at the application boundary, then
submit only the generic overlay frame to the backend.

Reason: engine public headers and backend call sites must stay independent from
Dear ImGui and concrete Diligent types. Overlay textures are now represented by
generation-checked public handles and are owned by the backend's native resource
lifetime system.

Overlay texture handles are translation handles, not native resource owners.
Destroying an overlay texture immediately invalidates the overlay translation
slot, advances its generation, and allows that slot index to be reused. The
underlying `Graphics::TextureHandle` is passed to `DiligentNativeResourceOwner`;
native texture and SRV retirement remain fence-safe there and are independent
from overlay translation slot reuse. The overlay registry intentionally does
not model a deferred retired state.

Overlay operations are render-thread-affine after initialization. Initialization
captures the owning render thread, and init/create/destroy/render/shutdown plus
frame-slot reconfiguration must run on that same thread.

Generation zero is invalid and generation advancement skips zero. The public
generation field remains 32-bit; wrapping from `UINT32_MAX` to `1` is a bounded
theoretical stale-handle limitation after a full generation cycle on the same
slot.

Compatibility: there is no stable external SDK compatibility shim for the
removed ImGui-specific functions in this repository state. Consumers must
migrate source to the overlay API.
