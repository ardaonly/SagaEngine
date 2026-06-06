# Saga Diligent Integration

SagaEngine treats DiligentCore as a vendored graphics core foundation.

- Vendored source: `Vendor/Diligent`
- Submodule URL: `https://github.com/ardaonly/DiligentCore.git`
- Pinned commit: `650c2f1f4e2b568100af221261a235c570701af3`
- Upstream source: `https://github.com/DiligentGraphics/DiligentCore`
- Saga-owned metadata path: `Vendor/Diligent.Saga`
- Diligent source must not be mixed into `Engine/`.
- Saga-owned Diligent integration code stays under
  `Engine/Private/.../Backends/Diligent`.
- Public SagaEngine headers must not include Diligent headers.
- Diligent is linked through the private backend target only; server/headless
  targets must not link Diligent directly.
