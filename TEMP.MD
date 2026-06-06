# SagaEngine Render Roadmap v3.2 — MMO-First / AAA-Oriented Renderer

> Dil: Türkçe
> Kapsam: **sadece render sistemi**. UI / NoesisGUI / RmlUi / editor UI bu roadmap'in dışında bırakılmıştır.
> Ana karar: **Diligent artık geçici Conan dependency değil; `Vendor/Diligent` altında SagaEngine'in uzun vadeli graphics core temeli olarak kontrol altına alınacaktır.**
> V3.1 ekleri: build/install boundary, device-loss recovery, geometry streaming, shader variant budget, lighting strategy, ray tracing future boundary, deterministic render fixtures ve scalability presets.
> V3.2 ekleri: backend capability matrix, shader/material hot reload, material graph authoring contract, camera/view family, render interest/world streaming contract, MMO avatar/equipment rendering, sky/atmosphere/weather, water rendering strategy ve render crash/telemetry artifact.
> Nihai hedef: MMO-first ama AAA hissine yaklaşan, test edilebilir, vendor lock-in'i kontrol edilmiş, Saga-owned renderer foundation.

---

## 0. V3'ün Amacı

Bu v3.2 dokümanının amacı sadece "phase listesi" vermek değildir.

Bu doküman aynı anda şunları tanımlar:

- [ ] Render'ın uzun vadeli mimari yönü.
- [ ] Diligent'in repo içinde nasıl tutulacağı.
- [ ] Engine-owned render kodu ile vendor kodun nasıl ayrılacağı.
- [ ] AAA hissi için gerekli render sistemleri.
- [ ] MMO renderer için gerekli large-world / visibility / streaming sistemleri.
- [ ] Dosya mimarisinin nasıl kurulacağı.
- [ ] Her phase sonunda gerçekten uygulanıp uygulanmadığını nasıl test edeceğimiz.
- [ ] Hangi iddiaları yapabileceğimiz ve hangi iddiaları yapamayacağımız.
- [ ] Build/CMake target ownership'inin nasıl ayrılacağını.
- [ ] Public/install surface guard ile Diligent sızıntısının nasıl engelleneceğini.
- [ ] Device lost / swapchain failure gibi gerçek dünya render failure case'lerinin nasıl ele alınacağını.
- [ ] Mesh/geometry streaming ve shader variant budget gibi uzun vadeli üretim risklerinin nasıl yönetileceğini.
- [ ] Render scalability preset'lerinin MMO kullanıcı donanım çeşitliliği için nasıl tasarlanacağını.
- [ ] Backend capability matrix ile platform/feature fallback davranışını.
- [ ] Shader/material hot reload ile geliştirici iteration hızını.
- [ ] Material graph / authoring contract ile artist-facing render üretim sınırını.
- [ ] Camera/view family ile multi-view, minimap, probe, shadow ve debug camera ihtiyaçlarını.
- [ ] Render interest / world streaming contract ile MMO world/network/render sınırını.
- [ ] Avatar/equipment rendering ile MMO character customization gerçekliğini.
- [ ] Sky/atmosphere/weather ve water strategy ile açık dünya görsel foundation'ını.
- [ ] Render crash/telemetry artifact ile sahadaki render hatalarını analiz etmeyi.

Bu roadmap'in sonunda hedeflenen şey:

```text
SagaRenderer artık demo renderer değildir.
SagaEngine, Diligent tabanlı vendored graphics core üzerinde çalışan,
vendor-neutral public API'ye sahip,
RenderGraph/Material/Lighting/PostProcess/Visibility/Character/VFX sistemleri olan,
MMO-first ve AAA-oriented renderer foundation'a sahiptir.
```

Ama şu iddia yapılmayacak:

```text
SagaRenderer artık Unreal seviyesinde.
SagaEngine production AAA renderer'a sahip.
```

## 0.1 Güncel Uygulama Durumu

Bu bölüm roadmap'in uygulama gerçeğiyle senkron tutulması için eklenmiştir.

- [x] R1 Diligent vendoring tamamlandı: `Vendor/Diligent` submodule olarak repo kontrolünde, Saga-owned vendor metadata `Vendor/Diligent.Saga` altında.
- [ ] R1B kısmen tamamlandı: `SagaDiligentBackend`, `VendorDiligent`, `SagaDiligentVendor.cmake` ve `SagaGraphics` public interface target mevcut; `SagaGraphicsPrivate` ve install graphics modülü henüz yok.
- [ ] R2 başladı ama tamamlanmadı: mevcut `Engine/Public/SagaEngine/Render/Backend` surface vendor-neutral ve yeni `Engine/Public/SagaEngine/Graphics` shell mevcut; davranış bridge'i ve stable SDK iddiası yok.
- [ ] R2B-lite başladı: public render/backend token guard, `SagaGraphics` shell smoke, install surface guard ve contract dokümantasyonu mevcut; full R2B closure claim yok.
- [ ] R2B-lite install guard verified during R1B-lite target-boundary slice.
- [ ] R3-prep başladı: Diligent backend device/swapchain/frame yolu mevcut; `SagaGraphics`/Render backend preference mapping ve platform order dokümantasyonu eklendi, ama planlanan `Engine/Private/SagaEngine/Graphics/Backends/Diligent` konumu ve `SagaGraphics` üstünden bridge henüz yapılmadı.

Bu slice: R3-prep — Graphics backend preference mapping. Full R3 bridge migration, `Render/Backend` taşıma ve stable external SDK claim yok.

---

## 1. Araştırma Sonucu: AAA / MMO Renderer'da Görülen Ortak Desenler

Bu roadmap, mevcut AAA / high-end engine'lerde görülen render desenlerinden etkilenmiştir:

### 1.1 Large world / MMO tarafı

Modern large-world renderer'larda ortak yönler:

- [ ] Dünya tek seferde yüklenmez.
- [ ] Dünya hücrelere / zone'lara / chunk'lara bölünür.
- [ ] Uzak nesneler için HLOD / proxy / impostor kullanılır.
- [ ] Renderer, network visibility ile render visibility'yi aynı kavram saymaz.
- [ ] Streaming, visibility, culling ve LOD aynı tasarım ailesi içinde düşünülür.
- [ ] Texture ve geometry memory bütçesi açıkça ölçülür.

Unreal World Partition mantığında dünya runtime grid/cell yapısıyla stream edilir; HLOD ise uzaktaki non-interactive actor'ların görünür kalması için kullanılır. SagaEngine'de bunun birebir kopyası yapılmayacak, ama **zone/chunk/HLOD/render visibility** fikri alınacaktır.

### 1.2 AAA görüntü tarafı

AAA hissi için sadece "render backend" yetmez.

Gereken temel sistemler:

- [ ] PBR material model.
- [ ] Shadow pipeline.
- [ ] HDR scene color.
- [ ] Tonemapping.
- [ ] Bloom / color grading.
- [ ] Reflection / IBL / GI strategy.
- [ ] Temporal AA / upscaling foundation.
- [ ] GPU profiling.
- [ ] GPU capture / debug markers.
- [ ] Asset cooking.
- [ ] Texture streaming / memory residency.
- [ ] Character/skinned mesh rendering.
- [ ] VFX / transparency / decals.
- [ ] Render quality gates.

Unreal tarafında Nanite / Lumen / Virtual Shadow Maps; Unity HDRP tarafında high-end lighting, volumetric ve post-processing; O3DE Atom tarafında PBR, ray tracing ve post-processing gibi sistemler bu seviyenin örnekleridir. SagaEngine bu sistemleri birebir kopyalamayacak; ama hedeflenen renderer maturity seviyesi buna göre tanımlanacaktır.

### 1.3 Graphics abstraction tarafı

Diligent'in rolü:

```text
Diligent = düşük seviye graphics API abstraction / graphics core foundation
SagaRenderer = gerçek renderer architecture
```

Diligent modern API'leri soyutlamak için kullanılacak.
Ama SagaEngine public API'si Diligent'e dönüşmeyecek.

---

## 2. Tek Cümlelik Strateji

```text
SagaRenderer  = gerçek renderer
Diligent      = vendored/forked graphics core foundation
The Forge     = production core değil; AAA-oriented research/reference
Native Vulkan = şimdilik ana yol değil; ileride özel backend olabilir
UI            = bu roadmap dışında
```

Bu planın amacı “havalı render kütüphanesi seçmek” değildir.
Amaç, **SagaEngine'in render ownership'ini kurmak** ve render sistemini uzun vadede çöpe gitmeyecek şekilde sahiplenmektir.

---

## 3. Hedeflenen Maturity Seviyesi

### Target

```text
Level 5 — AAA-Oriented Renderer Foundation
```

### Non-target

```text
Level 6 — Production AAA Renderer
```

### Maturity seviyeleri

#### Level 0 — Demo Renderer

```text
triangle / clear / hardcoded mesh
```

#### Level 1 — Engine Renderer Foundation

```text
device, swapchain, resources, shaders, basic mesh
```

#### Level 2 — Structured Renderer

```text
RenderGraph, materials, scene extraction
```

#### Level 3 — Visual Renderer

```text
lighting, shadows, post-process
```

#### Level 4 — MMO Renderer Foundation

```text
culling, LOD, streaming-aware render path, large-world scene
```

#### Level 5 — AAA-Oriented Renderer Foundation

```text
PBR, lighting/shadow/postprocess, diagnostics, quality gates,
large-scene visibility, performance budgets, vertical slice,
character rendering, terrain/vegetation, VFX foundation,
texture streaming/memory residency, GPU capture tooling
```

#### Level 6 — Production AAA Renderer

```text
advanced GI/reflections, mature asset pipeline, artist tooling,
platform optimization, full profiling, console readiness,
large-scale shipped-game proof
```

---

## 4. Nihai Render Nasıl Olmalı?

### 4.1 Hedef mimari

```text
SagaRenderer
├─ SagaGraphicsCore
│  └─ Vendor/Diligent
├─ Backend-Neutral Public Graphics API
├─ GPU Resource Model
├─ Binding / Descriptor / Frame Resource Model
├─ RenderGraph
├─ Shader Pipeline
├─ Render Asset Cooking Pipeline
├─ Texture Streaming / Residency
├─ Material System
├─ Scene Renderer
├─ Render Thread / Frame Ownership Model
├─ Lighting / Shadows
├─ Post Processing
├─ Reflections / IBL / GI Strategy
├─ TAA / Upscaling Foundation
├─ Visibility / Culling / LOD
├─ Occlusion Culling / Visibility Acceleration
├─ GPU-Driven / Indirect Draw Strategy
├─ Render Interest / World Streaming Contract
├─ MMO World Streaming Renderer
├─ Terrain / Vegetation / HLOD Strategy
├─ Character / Skinned Mesh Renderer
├─ MMO Avatar / Equipment Rendering
├─ Sky / Atmosphere / Weather Foundation
├─ Water / Ocean / River Rendering Strategy
├─ Transparency / Decals / VFX Foundation
├─ GPU Capture Tooling
├─ GPU Profiling / Diagnostics
├─ Render Memory Budget / Residency Diagnostics
├─ Device Loss / Swapchain Recovery
├─ Render Backend Capability Matrix
├─ Graphics Build / CMake Target Boundary
├─ Graphics Public / Install Surface Guard
├─ Mesh / Geometry Streaming Strategy
├─ Shader Variant Budget / Permutation Control
├─ Shader / Material Hot Reload Development Loop
├─ Material Graph / Authoring Contract
├─ Camera / View Family / Multi-View Rendering
├─ Forward / Deferred / Clustered Lighting Strategy
├─ Ray Tracing Future Boundary
├─ Deterministic Render Test Fixtures
├─ Render Scalability / Quality Presets
├─ Render Crash / Telemetry Artifact
├─ Render Quality Gates
└─ AAA-Oriented Vertical Slice
```

---

## 5. Nihai Dosya Mimarisi

Bu dosya mimarisi, Diligent'i uzun vadeli graphics core olarak sahiplenecek ama Saga-owned kod ile vendor kodu karıştırmayacak şekilde tasarlanmıştır.

```text
SagaEngine/
├─ Engine/
│  ├─ Public/
│  │  └─ SagaEngine/
│  │     └─ Graphics/
│  │        ├─ Core/
│  │        │  ├─ GraphicsDevice.hpp
│  │        │  ├─ RenderBackend.hpp
│  │        │  ├─ RenderBackendDesc.hpp
│  │        │  ├─ RenderCapabilities.hpp
│  │        │  ├─ RenderFrame.hpp
│  │        │  └─ RenderResult.hpp
│  │        │
│  │        ├─ Handles/
│  │        │  ├─ TextureHandle.hpp
│  │        │  ├─ BufferHandle.hpp
│  │        │  ├─ ShaderHandle.hpp
│  │        │  ├─ PipelineHandle.hpp
│  │        │  ├─ SamplerHandle.hpp
│  │        │  └─ RenderResourceHandle.hpp
│  │        │
│  │        ├─ Descs/
│  │        │  ├─ TextureDesc.hpp
│  │        │  ├─ BufferDesc.hpp
│  │        │  ├─ ShaderDesc.hpp
│  │        │  ├─ PipelineDesc.hpp
│  │        │  ├─ SamplerDesc.hpp
│  │        │  ├─ SwapchainDesc.hpp
│  │        │  ├─ BindingLayoutDesc.hpp
│  │        │  └─ RenderPassDesc.hpp
│  │        │
│  │        ├─ RenderGraph/
│  │        │  ├─ RenderGraph.hpp
│  │        │  ├─ RenderGraphBuilder.hpp
│  │        │  ├─ RenderGraphResource.hpp
│  │        │  └─ RenderGraphDiagnostics.hpp
│  │        │
│  │        ├─ Materials/
│  │        │  ├─ MaterialAsset.hpp
│  │        │  ├─ MaterialInstance.hpp
│  │        │  ├─ MaterialParameter.hpp
│  │        │  └─ MaterialBinding.hpp
│  │        │
│  │        ├─ Scene/
│  │        │  ├─ Renderable.hpp
│  │        │  ├─ RenderSceneSnapshot.hpp
│  │        │  ├─ DrawPacket.hpp
│  │        │  ├─ CameraData.hpp
│  │        │  ├─ ViewFamily.hpp
│  │        │  ├─ RenderView.hpp
│  │        │  └─ LightData.hpp
│  │        │
│  │        ├─ World/
│  │        │  ├─ RenderZone.hpp
│  │        │  ├─ RenderChunk.hpp
│  │        │  ├─ RenderVisibilitySet.hpp
│  │        │  ├─ RenderInterestSet.hpp
│  │        │  ├─ RenderLod.hpp
│  │        │  └─ RenderStreamingHint.hpp
│  │        │
│  │        ├─ Diagnostics/
│  │        │  ├─ RenderDiagnostics.hpp
│  │        │  ├─ RenderStats.hpp
│  │        │  ├─ RenderBudgetReport.hpp
│  │        │  ├─ RenderTelemetryArtifact.hpp
│  │        │  └─ RenderCaptureMarkers.hpp
│  │        │
│  │        └─ Tests/
│  │           └─ RenderConformance.hpp
│  │
│  └─ Private/
│     └─ SagaEngine/
│        └─ Graphics/
│           ├─ Core/
│           │  ├─ GraphicsDevice.cpp
│           │  ├─ RenderBackendRegistry.cpp
│           │  ├─ RenderCapabilities.cpp
│           │  └─ NullRenderBackend.cpp
│           │
│           ├─ Resources/
│           │  ├─ RenderResourceRegistry.cpp
│           │  ├─ TextureManager.cpp
│           │  ├─ BufferManager.cpp
│           │  ├─ SamplerManager.cpp
│           │  ├─ ShaderManager.cpp
│           │  ├─ PipelineCache.cpp
│           │  └─ FrameResourceAllocator.cpp
│           │
│           ├─ Bindings/
│           │  ├─ BindingLayout.cpp
│           │  ├─ MaterialBindingTable.cpp
│           │  ├─ DynamicBufferRing.cpp
│           │  └─ DescriptorValidation.cpp
│           │
│           ├─ RenderGraph/
│           │  ├─ RenderGraph.cpp
│           │  ├─ RenderGraphBuilder.cpp
│           │  ├─ RenderGraphCompiler.cpp
│           │  ├─ RenderGraphExecutor.cpp
│           │  ├─ RenderGraphValidation.cpp
│           │  └─ RenderGraphDump.cpp
│           │
│           ├─ Shaders/
│           │  ├─ ShaderCompiler.cpp
│           │  ├─ ShaderReflection.cpp
│           │  ├─ ShaderVariantKey.cpp
│           │  ├─ ShaderCache.cpp
│           │  └─ ShaderDiagnostics.cpp
│           │
│           ├─ HotReload/
│           │  ├─ ShaderHotReload.cpp
│           │  ├─ MaterialHotReload.cpp
│           │  └─ HotReloadDiagnostics.cpp
│           │
│           ├─ Materials/
│           │  ├─ MaterialSystem.cpp
│           │  ├─ MaterialAsset.cpp
│           │  ├─ MaterialInstance.cpp
│           │  ├─ MaterialCompiler.cpp
│           │  ├─ MaterialGraphContract.cpp
│           │  ├─ MaterialGraphCompiler.cpp
│           │  └─ MaterialFallbacks.cpp
│           │
│           ├─ Scene/
│           │  ├─ RenderSceneExtractor.cpp
│           │  ├─ RenderSceneSnapshot.cpp
│           │  ├─ DrawPacketBuilder.cpp
│           │  ├─ ViewFamilyBuilder.cpp
│           │  ├─ MultiViewRenderer.cpp
│           │  ├─ StaticMeshRenderer.cpp
│           │  ├─ SkinnedMeshRenderer.cpp
│           │  ├─ CharacterRenderer.cpp
│           │  ├─ AvatarEquipmentRenderer.cpp
│           │  └─ InstanceBatcher.cpp
│           │
│           ├─ Passes/
│           │  ├─ ClearPass.cpp
│           │  ├─ DepthPrepass.cpp
│           │  ├─ GBufferPass.cpp
│           │  ├─ ForwardPass.cpp
│           │  ├─ ShadowPass.cpp
│           │  ├─ LightingPass.cpp
│           │  ├─ PostProcessPass.cpp
│           │  ├─ BloomPass.cpp
│           │  ├─ TonemapPass.cpp
│           │  ├─ TaaPass.cpp
│           │  ├─ UpscalePass.cpp
│           │  ├─ DecalPass.cpp
│           │  ├─ ParticlePass.cpp
│           │  └─ DebugViewPass.cpp
│           │
│           ├─ Lighting/
│           │  ├─ LightExtractor.cpp
│           │  ├─ ShadowSystem.cpp
│           │  ├─ CascadedShadowMaps.cpp
│           │  ├─ IblSystem.cpp
│           │  ├─ ReflectionProbeSystem.cpp
│           │  ├─ GiStrategy.cpp
│           │  ├─ SkyAtmosphereSystem.cpp
│           │  ├─ WeatherRenderSystem.cpp
│           │  └─ WaterRenderStrategy.cpp
│           │
│           ├─ World/
│           │  ├─ RenderInterestSystem.cpp
│           │  ├─ RenderZoneSystem.cpp
│           │  ├─ RenderChunkSystem.cpp
│           │  ├─ WorldStreamingRenderer.cpp
│           │  ├─ TerrainRenderer.cpp
│           │  ├─ VegetationRenderer.cpp
│           │  ├─ HlodSystem.cpp
│           │  ├─ ImpostorSystem.cpp
│           │  └─ TextureStreamingSystem.cpp
│           │
│           ├─ Visibility/
│           │  ├─ FrustumCulling.cpp
│           │  ├─ DistanceCulling.cpp
│           │  ├─ LodSelector.cpp
│           │  ├─ OcclusionCulling.cpp
│           │  ├─ HizDepthPyramid.cpp
│           │  ├─ VisibilitySetBuilder.cpp
│           │  └─ GpuDrivenStrategy.cpp
│           │
│           ├─ VFX/
│           │  ├─ TransparencyRenderer.cpp
│           │  ├─ DecalRenderer.cpp
│           │  ├─ ParticleRenderer.cpp
│           │  ├─ FogPlaceholder.cpp
│           │  └─ OverdrawDiagnostics.cpp
│           │
│           ├─ Threading/
│           │  ├─ RenderThread.cpp
│           │  ├─ RenderCommandQueue.cpp
│           │  ├─ RenderSnapshotQueue.cpp
│           │  ├─ FrameOwnership.cpp
│           │  └─ RenderThreadDiagnostics.cpp
│           │
│           ├─ Diagnostics/
│           │  ├─ RenderDiagnostics.cpp
│           │  ├─ RenderStats.cpp
│           │  ├─ GpuProfiler.cpp
│           │  ├─ RenderBudgetReport.cpp
│           │  ├─ RenderCaptureMarkers.cpp
│           │  ├─ RenderMemoryReport.cpp
│           │  ├─ RenderTelemetryArtifact.cpp
│           │  └─ GoldenImageRunner.cpp
│           │
│           └─ Backends/
│              └─ Diligent/
│                 ├─ SagaDiligentBackend.cpp
│                 ├─ SagaDiligentDevice.cpp
│                 ├─ SagaDiligentSwapchain.cpp
│                 ├─ SagaDiligentFrame.cpp
│                 ├─ SagaDiligentTexture.cpp
│                 ├─ SagaDiligentBuffer.cpp
│                 ├─ SagaDiligentSampler.cpp
│                 ├─ SagaDiligentShader.cpp
│                 ├─ SagaDiligentPipeline.cpp
│                 ├─ SagaDiligentBinding.cpp
│                 ├─ SagaDiligentRenderGraphBridge.cpp
│                 ├─ SagaDiligentProfiler.cpp
│                 └─ SagaDiligentDiagnostics.cpp
│
├─ cmake/
│  ├─ SagaGraphicsTargets.cmake
│  ├─ SagaDiligentVendor.cmake
│  ├─ SagaRenderTools.cmake
│  ├─ SagaRenderTests.cmake
│  └─ SagaInstallGraphics.cmake
│
├─ Tools/
│  └─ RenderPipeline/
│     ├─ RenderCooker/
│     │  ├─ RenderCookerMain.cpp
│     │  ├─ TextureCooker.cpp
│     │  ├─ MeshCooker.cpp
│     │  ├─ MaterialCooker.cpp
│     │  ├─ ShaderVariantCooker.cpp
│     │  └─ RenderManifestWriter.cpp
│     │
│     ├─ ShaderCompiler/
│     │  ├─ ShaderCompilerMain.cpp
│     │  ├─ ShaderReflectionTool.cpp
│     │  └─ ShaderVariantScanner.cpp
│     │
│     └─ GoldenImage/
│        ├─ GoldenImageRunner.cpp
│        ├─ GoldenImageCompare.cpp
│        └─ GoldenImageBaselineTool.cpp
│
├─ Tests/
│  ├─ Unit/
│  │  └─ Graphics/
│  │     ├─ RenderHandleTests.cpp
│  │     ├─ RenderGraphTests.cpp
│  │     ├─ BindingModelTests.cpp
│  │     ├─ ShaderPipelineTests.cpp
│  │     ├─ ShaderMaterialHotReloadTests.cpp
│  │     ├─ MaterialGraphContractTests.cpp
│  │     ├─ MaterialSystemTests.cpp
│  │     ├─ VisibilityTests.cpp
│  │     ├─ TextureStreamingTests.cpp
│  │     ├─ RenderThreadTests.cpp
│  │     ├─ ViewFamilyTests.cpp
│  │     ├─ RenderInterestStreamingContractTests.cpp
│  │     ├─ DeviceLossRecoveryTests.cpp
│  │     ├─ MeshGeometryStreamingTests.cpp
│  │     ├─ ShaderVariantBudgetTests.cpp
│  │     ├─ RenderScalabilityPresetTests.cpp
│  │     ├─ BackendCapabilityMatrixTests.cpp
│  │     ├─ AvatarEquipmentRenderingTests.cpp
│  │     ├─ SkyWeatherWaterStrategyTests.cpp
│  │     ├─ RenderTelemetryArtifactTests.cpp
│  │     ├─ PublicInstallSurfaceTests.cpp
│  │     └─ RenderDiagnosticsTests.cpp
│  │
│  ├─ Integration/
│  │  └─ Graphics/
│  │     ├─ DiligentBackendSmokeTests.cpp
│  │     ├─ RenderGraphExecutionTests.cpp
│  │     ├─ SceneRendererSmokeTests.cpp
│  │     ├─ LightingShadowSmokeTests.cpp
│  │     ├─ CharacterRendererSmokeTests.cpp
│  │     ├─ DeviceLossIntegrationTests.cpp
│  │     ├─ DeterministicRenderFixtureTests.cpp
│  │     └─ VerticalSliceRenderTests.cpp
│  │
│  └─ GoldenImages/
│     ├─ baselines/
│     ├─ scenes/
│     └─ README.md
│
├─ Vendor/
│  └─ Diligent/
│     ├─ DiligentCore/
│     ├─ UPSTREAM_COMMIT.txt
│     ├─ SAGA_PATCHES.md
│     ├─ SAGA_INTEGRATION.md
│     └─ LICENSES/
│
├─ Research/
│  └─ TheForge/
│     ├─ notes/
│     ├─ spikes/
│     ├─ technique-audits/
│     ├─ visibility-buffer-study/
│     ├─ shader-pipeline-notes/
│     └─ benchmark-notes/
│
└─ docs/
   └─ architecture/
      ├─ RENDER_STACK_DECISION.md
      ├─ RENDER_PUBLIC_API_CONTRACT.md
      ├─ RENDER_BUILD_TARGET_BOUNDARY.md
      ├─ RENDER_INSTALL_SURFACE_GUARD.md
      ├─ RENDER_THREAD_OWNERSHIP.md
      ├─ RENDER_ASSET_COOKING.md
      ├─ RENDER_MESH_GEOMETRY_STREAMING.md
      ├─ RENDER_SHADER_VARIANT_BUDGET.md
      ├─ RENDER_LIGHTING_STRATEGY.md
      ├─ RENDER_RAY_TRACING_FUTURE_BOUNDARY.md
      ├─ RENDER_BACKEND_CAPABILITY_MATRIX.md
      ├─ RENDER_SHADER_MATERIAL_HOT_RELOAD.md
      ├─ RENDER_MATERIAL_GRAPH_AUTHORING_CONTRACT.md
      ├─ RENDER_CAMERA_VIEW_FAMILY.md
      ├─ RENDER_VISIBILITY_AND_STREAMING.md
      ├─ RENDER_INTEREST_WORLD_STREAMING_CONTRACT.md
      ├─ RENDER_DETERMINISTIC_FIXTURES.md
      ├─ RENDER_SCALABILITY_PRESETS.md
      ├─ RENDER_AVATAR_EQUIPMENT_RENDERING.md
      ├─ RENDER_SKY_ATMOSPHERE_WEATHER.md
      ├─ RENDER_WATER_STRATEGY.md
      ├─ RENDER_CRASH_TELEMETRY_ARTIFACT.md
      ├─ RENDER_QUALITY_GATES.md
      └─ RENDER_MATURITY_CHECKPOINT.md
```

---

## 6. Neden bu dosya mimarisi?

### 6.1 `Engine/Public/SagaEngine/Graphics`

Burada sadece Saga-owned public API bulunur.

Yasak:

```cpp
Diligent::IRenderDevice*
VkImage
ID3D12Resource*
MTLTexture*
```

Doğru:

```cpp
TextureHandle
BufferHandle
ShaderHandle
PipelineHandle
RenderFrame
RenderBackendDesc
```

### 6.2 `Engine/Private/SagaEngine/Graphics/Backends/Diligent`

Burada Diligent'i kullanan Saga backend adaptörü bulunur.

Burada Diligent include edilebilir.

### 6.3 `Vendor/Diligent`

Burada Diligent'in gerçek upstream/fork kaynak kodu bulunur.

Bu klasör `Engine/` içine karışmaz.
Çünkü Diligent artık kontrol edilen vendor/fork temelidir, ama hâlâ köken olarak Saga-owned source değildir.

### 6.4 `Tools/RenderPipeline`

AAA hissi sadece runtime renderer ile olmaz. Cook pipeline şarttır.

Bu yüzden texture/mesh/material/shader cooking araçları engine runtime'dan ayrı tutulur.

### 6.5 `Tests/GoldenImages`

Render regression testleri için görsel baseline alanı gerekir.

Golden image testleri toleranslı çalışmalı; driver/GPU farkları için hash-only yaklaşım yerine threshold/tolerance desteklenmelidir.

### 6.6 `cmake/`

Render mimarisi sadece kaynak dosya dizilimiyle korunmaz. CMake target sınırları da korunmalıdır.

Bu yüzden `cmake/SagaGraphicsTargets.cmake`, `cmake/SagaDiligentVendor.cmake` ve `cmake/SagaInstallGraphics.cmake` gibi dosyalarla şu sınırlar açık tutulur:

```text
SagaGraphics public target        = Diligent include path expose etmez
SagaGraphicsPrivate target        = Saga private graphics implementation
SagaDiligentBackend target        = Diligent'e linklenen backend
VendorDiligent target             = vendored Diligent source
SagaServer / headless targets     = Diligent'e linklenmez
```

### 6.7 `Research/TheForge`

The Forge production core değildir.

Araştırma alanıdır:

```text
visibility buffer
shader pipeline ideas
benchmark method
graphics architecture observations
```

---

## 7. Temel Kurallar

- [ ] Public API **Saga-owned** olacak.
- [ ] Public API'de `Diligent::`, Vulkan, D3D, Metal, The Forge tipi görünmeyecek.
- [ ] Diligent kaynak kodu `Vendor/Diligent` altında duracak.
- [ ] Diligent'i kullanan Saga backend kodu `Engine/Private/.../Backends/Diligent` altında duracak.
- [ ] Server/headless build renderer'a bağımlı olmayacak.
- [ ] RenderGraph, MaterialSystem, ShaderPipeline ve Resource Model Diligent'e değil, Saga kavramlarına bağlı olacak.
- [ ] Renderer UI olmadan tamamlanabilir olacak.
- [ ] Render thread ile game/world thread sınırı açık olacak.
- [ ] Asset cooking render pipeline'ın parçası olarak ele alınacak.
- [ ] Character/skinned mesh rendering MMO için birinci sınıf render problemi sayılacak.
- [ ] Texture streaming / residency MMO large-world için zorunlu sistem sayılacak.
- [ ] Occlusion culling ve HLOD büyük dünya için planlı sistem sayılacak.
- [ ] Performance budget olmadan “AAA-oriented” iddiası yapılmayacak.
- [ ] Golden image / diagnostics / conformance testleri olmadan phase kapanmayacak.
- [ ] Device lost / swapchain recreation gibi failure case'leri yok sayılmayacak.
- [ ] Shader variant sayısı ve shader cache boyutu bütçelenmeden material sistemi büyütülmeyecek.
- [ ] Scalability preset'leri olmadan MMO oyuncu donanım çeşitliliği hedeflenmiş sayılmayacak.
- [ ] Backend capability matrix olmadan feature support varsayılmayacak.
- [ ] World/network interest state ile render visibility state açık sözleşme olmadan birbirine karıştırılmayacak.
- [ ] Avatar/equipment rendering MMO character renderer'ın opsiyonel süsü değil, temel requirement olarak görülecek.
- [ ] Sky/weather/water gibi world-feel sistemleri Level 5 foundation kapsamına strategy/foundation olarak dahil edilecek.
- [ ] Render crash/telemetry artifact olmadan saha debug kabiliyeti var sayılmayacak.

---

## 8. Yasaklar

- [ ] `Engine/Public` içinde `Diligent::` tipi.
- [ ] `Engine/Public` içinde `Vk`, `ID3D`, `MTL`, `Renderer*`, `Texture*`, `Buffer*` gibi vendor/native handle sızıntısı.
- [ ] Diligent kaynak kodunu doğrudan `Engine/` içine karıştırmak.
- [ ] Game/runtime entity'lerinde Diligent resource pointer saklamak.
- [ ] Server/headless build'in Diligent'e linklenmesi.
- [ ] Shader/material sistemini doğrudan Diligent API'sine göre tasarlamak.
- [ ] Render thread dışında GPU resource mutasyonu yapmak.
- [ ] Diligent + The Forge'u aynı frame içinde karıştırmak.
- [ ] Texture streaming/memory residency olmadan open-world render iddiası yapmak.
- [ ] Character renderer olmadan MMO render foundation claim etmek.
- [ ] Triangle/mesh çiziminden sonra “AAA renderer olduk” demek.
- [ ] Shader variant explosion'ı ölçmeden feature flag eklemek.
- [ ] Device lost / swapchain recreation testleri olmadan backend bridge'i production-ready saymak.
- [ ] Tek kalite seviyesiyle MMO oyuncu kitlesini hedeflemek.
- [ ] Backend capability bilinmeden RT, compute, timestamp query, HDR veya compression support varsaymak.
- [ ] Shader/material değişiklikleri için engine restart zorunlu kabul etmek.
- [ ] Avatar ekipman/cosmetic çeşitliliği olmadan MMO character rendering tamamlandı demek.
- [ ] Sky/water/weather strategy olmadan açık dünya görsel foundation claim etmek.

---

## 9. Roadmap Özeti

| Phase | Amaç | Bittiğinde durum |
|---|---|---|
| R0 | Render karar dokümanı | Strateji resmi hale gelir |
| R1 | Diligent vendoring | Diligent Conan'dan çıkıp repo kontrolüne girer |
| R1B | Graphics Build / CMake Target Boundary | Graphics target sınırları netleşir |
| R2 | SagaGraphics public API | Vendor-neutral graphics vocabulary oluşur |
| R2B | Graphics Public / Install Surface Guard | Public/install surface Diligent sızıntısından korunur |
| R3 | Diligent core bridge | Device/swapchain/frame temel çalışır |
| R3B | Device Loss / Swapchain Recovery | Failure path ve swapchain recreation güvenceye alınır |
| R3C | Render Backend Capability Matrix | Backend feature support ve fallback davranışı netleşir |
| R4 | GPU resource model | Texture/buffer/shader/pipeline handle sistemi oluşur |
| R4B | Binding / descriptor / frame resource model | Modern GPU binding modeli netleşir |
| R5 | RenderGraph v0 | Pass/resource dependency sistemi kurulur |
| R6 | Shader pipeline v0 | Shader asset/variant/compile/reflection temeli oluşur |
| R6B | Render asset cooking pipeline | Texture/mesh/shader cook akışı başlar |
| R6C | Texture streaming / virtual texture strategy | Large-world texture memory stratejisi oluşur |
| R6D | Mesh / Geometry Streaming Strategy | Large-world geometry residency stratejisi oluşur |
| R6E | Shader Variant Budget / Permutation Control | Shader variant explosion kontrol altına alınır |
| R6F | Shader / Material Hot Reload Development Loop | Shader/material iteration restart gerektirmeden hızlanır |
| R7 | Material system v0 | PBR material ve material instance modeli başlar |
| R7B | Material Graph / Authoring Contract | Artist-facing material authoring sınırı tanımlanır |
| R8 | Scene renderer v0 | Mesh/camera/transform/draw packet sistemi oluşur |
| R8B | Render thread / frame ownership | Game/render thread sınırı netleşir |
| R8C | Camera / View Family / Multi-View Rendering | Main/shadow/probe/minimap/debug view ailesi oluşur |
| R9 | Lighting/shadows v0 | AAA hissinin ilk görsel temeli oluşur |
| R9B | Forward / Deferred / Clustered Lighting Strategy | Lighting architecture yönü netleşir |
| R10 | Post-process v0 | HDR/tonemap/bloom/final image pipeline başlar |
| R10B | Reflections / IBL / GI strategy | Reflection/GI yönü belirlenir |
| R10C | TAA / upscaling foundation | Temporal AA/upscaling hazırlığı başlar |
| R10D | Ray Tracing Future Boundary | RT capability/fallback sınırı geleceğe açık kalır |
| R11 | MMO visibility/culling/LOD | Büyük dünya render path'i başlar |
| R11B | Occlusion culling / visibility acceleration | Görünmeyen nesnelerin submit edilmemesi başlar |
| R11C | GPU-driven / indirect draw strategy | GPU-driven render yönü planlanır |
| R11D | Render Interest / World Streaming Contract | World/network/render visibility sözleşmesi netleşir |
| R12 | Terrain/vegetation/HLOD | MMO/open-world görsel altyapısı başlar |
| R12B | Character / skinned mesh renderer | MMO karakter render temeli oluşur |
| R12D | MMO Avatar / Equipment Rendering | Cosmetic/equipment/avatar çeşitliliği render foundation'a girer |
| R12E | Sky / Atmosphere / Weather Foundation | Açık dünya sky/weather foundation oluşur |
| R12F | Water / Ocean / River Rendering Strategy | Su rendering yönü ve fallback stratejisi oluşur |
| R12C | Transparency / decals / VFX | VFX/transparent render temeli oluşur |
| R13 | Diagnostics/profiling | Render ölçülebilir hale gelir |
| R13B | GPU capture tooling | RenderDoc/PIX/Nsight marker temeli oluşur |
| R13C | Render memory budget / residency diagnostics | VRAM/resource budget ölçümü başlar |
| R13D | Render Crash / Telemetry Artifact | Saha render crash/debug artifact sistemi oluşur |
| R14 | Quality gates | Görsel/mimari/perf regresyonları yakalanır |
| R14B | Deterministic Render Test Fixtures | Golden image testleri güvenilir fixture'a bağlanır |
| R15 | AAA-oriented vertical slice | Gerçek bir AAA-oriented demo sahne çıkar |
| R15B | Performance budget certification | Dikey sahne ölçülebilir performans hedeflerine bağlanır |
| R15C | Render Scalability / Quality Presets | Low/Medium/High/Ultra kalite presetleri oluşur |
| R16 | Closure checkpoint | Renderer maturity dürüstçe sınıflandırılır |

---

# Phase R0 — Render Stack Decision Record

## Amaç

Render stratejisini tartışma konusu olmaktan çıkarıp resmi karar haline getirmek.

## İşler

- [ ] `docs/architecture/RENDER_STACK_DECISION.md` oluştur.
- [ ] Diligent'in `Vendor/Diligent` altında tutulacağını yaz.
- [ ] Diligent'in SagaEngine'in uzun vadeli graphics core foundation'ı olduğunu yaz.
- [ ] The Forge'un production core değil, research/reference olduğunu yaz.
- [ ] Native Vulkan'ın şimdilik ana yol olmadığını yaz.
- [ ] UI'nin bu roadmap dışında olduğunu açıkça belirt.
- [ ] Server/headless build'in renderer dependency taşımayacağını yaz.
- [ ] Vendor graphics type leak yasağını yaz.
- [ ] Renderer maturity hedefini `Level 5 — AAA-Oriented Renderer Foundation` olarak yaz.
- [ ] Dosya mimarisini bu dokümana veya ayrı mimari dokümanına bağla.

## Test / Doğrulama

- [ ] `git diff --check` geçmeli.
- [ ] Dokümanda şu terimler bulunmalı:
  - `Diligent`
  - `Vendor/Diligent`
  - `SagaRenderer`
  - `SagaGraphicsCore`
  - `The Forge`
  - `server/headless`
  - `vendor type leak`
  - `Level 5`
- [ ] Doküman “Production AAA renderer olduk” gibi overclaim içermemeli.
- [ ] Doküman UI entegrasyonunu bu roadmap kapsamına almamalı.

## Done

- [ ] Render yönü yazılı ve net.
- [ ] Diligent uzun vadeli graphics core olarak kabul edilmiş.
- [ ] The Forge production dependency olarak claim edilmemiş.
- [ ] Roadmap hedefi dürüstçe sınıflandırılmış.

---

# Phase R1 — Diligent'i Repo Kontrolüne Alma

## Amaç

Diligent'i ConanCenter dependency olmaktan çıkarıp SagaEngine repository içinde kontrol edilen vendor/fork haline getirmek.

## İşler

- [x] Diligent kaynaklarını `Vendor/Diligent` altına al.
- [x] Upstream commit hash'i `Vendor/Diligent.Saga/UPSTREAM_COMMIT.txt` içine yaz.
- [x] Yerel değişiklik politikasını `Vendor/Diligent.Saga/SAGA_PATCHES.md` içine yaz.
- [x] Entegrasyon kararını `Vendor/Diligent.Saga/SAGA_INTEGRATION.md` içine yaz.
- [x] `conanfile.py` içindeki `diligent-core/api.252009` dependency'sini kaldır veya opsiyonel fallback yap.
- [x] Conan diğer dependency'leri yönetmeye devam etsin.
- [x] SBOM'a Diligent'i `vendored` olarak ekle.
- [x] Diligent lisans/notice dosyalarını repo içinde izlenebilir hale getir.
- [x] CMake tarafına `SAGA_USE_VENDOR_DILIGENT` opsiyonu ekle.

## Örnek CMake kararı

```cmake
option(SAGA_USE_VENDOR_DILIGENT "Use vendored Diligent source" ON)
```

## Test / Doğrulama

- [x] `conan install` artık Diligent'i remote zorunlu dependency olarak çekmemeli.
- [x] Build graph içinde Diligent local path'ten gelmeli.
- [x] SBOM içinde Diligent `Vendor/Diligent` olarak görünmeli.
- [x] `Vendor/Diligent.Saga/UPSTREAM_COMMIT.txt` boş olmamalı.
- [x] Diligent kaynak kodu `Engine/` altında olmamalı.
- [x] `git diff --check` geçmeli.
- [x] License/notice dosyaları eksik olmamalı.

## Done

- [x] Diligent repo içinde kontrol ediliyor.
- [x] Diligent source ile Saga-owned source fiziksel olarak ayrılmış.
- [x] Remote Conan Diligent artık ana yol değil.
- [x] Vendor/fork ownership politikası belgelenmiş.

---


# Phase R1B — Graphics Build / CMake Target Boundary

## Amaç

Diligent vendoring yapıldıktan sonra build target sınırlarını netleştirmek.

Bu phase'in hedefi, Diligent'in public include path veya link dependency olarak yanlışlıkla tüm engine'e yayılmasını engellemektir.

## İşler

- [ ] `cmake/SagaGraphicsTargets.cmake` oluştur.
- [x] `cmake/SagaDiligentVendor.cmake` oluştur.
- [ ] `cmake/SagaRenderTools.cmake` oluştur.
- [ ] `cmake/SagaRenderTests.cmake` oluştur.
- [ ] `cmake/SagaInstallGraphics.cmake` oluştur.
- [x] `SagaGraphics` public target tanımla.
- [ ] `SagaGraphicsPrivate` private implementation target tanımla.
- [x] `SagaDiligentBackend` target tanımla.
- [x] `VendorDiligent` target veya imported target tanımla.
- [ ] `SagaRenderTools` targetlarını runtime engine targetlarından ayır.
- [x] `SagaServer` ve headless targetların Diligent'e linklenmediğini garanti et.
- [x] Public graphics target'ın Diligent include directory expose etmediğini garanti et.

## Test / Doğrulama

- [x] CMake target graph check.
- [x] `SagaServer` target Diligent'e linklemiyor.
- [x] `SagaGraphics` public target Diligent include path expose etmiyor.
- [x] `SagaDiligentBackend` dışında hiçbir target doğrudan Diligent'e linklemiyor.
- [x] `SAGA_USE_VENDOR_DILIGENT=OFF` fallback veya diagnostic beklenen davranışı veriyor.
- [ ] Build target inventory dokümante edilmiş.

## Done

- [ ] Build sistemi Diligent'i tüm engine'e sızdırmıyor.
- [ ] Public/private/backend/vendor target sınırları CMake seviyesinde korunuyor.
- [x] Server/headless build render backend'ten bağımsız kalıyor.

---

# Phase R2 — SagaGraphics Public API Shell

## Amaç

Diligent'e bağlı olmayan Saga-owned graphics vocabulary oluşturmak.

## Public API adayları

```text
Engine/Public/SagaEngine/Graphics/
├─ Core/
├─ Handles/
├─ Descs/
├─ RenderGraph/
├─ Materials/
├─ Scene/
├─ World/
├─ Diagnostics/
└─ Tests/
```

## İşler

- [x] Opaque handle sistemi tasarla: index + generation önerilir.
- [x] `IGraphicsBackend` veya benzeri dar lifecycle interface ekle.
- [x] `NullGraphicsBackend` shell ekle.
- [x] `RenderBackendDesc`, `TextureDesc`, `BufferDesc`, `ShaderDesc`, `PipelineDesc`, `SamplerDesc` ekle.
- [x] Public API'de Diligent/Vulkan/D3D/Metal tipi geçmediğini garantile.
- [x] Architecture test ile public header vendor token taraması ekle.
- [ ] Headless/server build için Null backend kullanılabilir hale getir.

## Test / Doğrulama

- [x] `rg "Diligent|Vk|Vulkan|ID3D|D3D|MTL|Metal|TheForge" Engine/Public/SagaEngine/Graphics` boş dönmeli.
- [x] Public `Graphics.h` umbrella header compile smoke testten geçmeli.
- [x] Null graphics backend build edilmeli.
- [ ] Server/headless target Diligent linklememeli.
- [x] Architecture test vendor type leak tespit edebilmeli.

## Done

- [x] SagaGraphics public surface vendor-neutral foundation shell olarak mevcut.
- [x] Null graphics backend mevcut.
- [x] Diligent entegrasyonundan önce Saga-owned vocabulary oluşmuş.
- [ ] Server/headless bağımsızlığı başlamış.

---


# Phase R2B-lite — Graphics Public / Install Surface Guard

## Amaç

Yeni `SagaGraphics` shell için public/install yüzeyinde Diligent veya native graphics API sızıntısı olmamasını garanti etmek.

Bu phase full R2/R2B closure değildir. `Render/Backend` public API bu slice'ta taşınmayacak, R3 Diligent bridge migration yapılmayacak, RenderGraph/material/shader/resource davranışı eklenmeyecek ve stable external SDK claim edilmeyecek.

## İşler

- [ ] Public graphics header install listesi tanımla.
- [x] `Vendor/Diligent` install edilmeyecek şekilde kural koy.
- [ ] Diligent headers'ın Saga public include zinciri üzerinden reachable olmadığını test et.
- [x] Public `Graphics.h` umbrella header self-contained compile test ekle.
- [x] Public API forbidden token scan ekle.
- [x] Install artifact scan ekle.
- [x] `RENDER_PUBLIC_API_CONTRACT.md` içine install surface kurallarını yaz.

## Test / Doğrulama

- [x] Install surface scan.
- [x] Public `Graphics.h` include smoke scan.
- [ ] Forbidden token scan:
  - `Diligent`
  - `Vk`
  - `ID3D`
  - `MTL`
  - `TheForge`
- [x] Public `Graphics.h` standalone compile test.
- [x] Installed include directory içinde Vendor/Diligent yok.
- [x] Installed SagaGraphics API Diligent include path istemiyor.

## Done

- [ ] SagaGraphics SDK yüzeyi vendor-neutral.
- [x] Diligent kaynak kodu repo içinde olsa bile yeni `SagaGraphics` public/install shell'e taşmıyor.
- [ ] Public API cleanup disiplini render için de uygulanmış.

---

# Phase R3 — Diligent Core Bridge v0

## Amaç

SagaGraphics API'yi Diligent üstünde ilk defa çalıştırmak.

## Konum

```text
Engine/Private/SagaEngine/Graphics/Backends/Diligent/
```

## İşler

- [x] Diligent device oluştur.
- [x] Swapchain oluştur.
- [x] BeginFrame / EndFrame uygula.
- [x] Clear color pass uygula.
- [x] Resize handling ekle.
- [x] Shutdown clean yap.
- [x] Validation/debug layer opsiyonu ekle.
- [x] Render backend diagnostics raporu üret.
- [x] Diligent include'ları sadece private backend dosyalarında tut.
- [x] Backend capability report üret.
- [x] Platform backend order dokümante et:
  - Linux: `NativePortable`, sonra `Compatibility`
  - Windows: `NativePrimary`, sonra `NativePortable`, sonra `Compatibility`
  - Other platforms: `NativePortable`, sonra `Compatibility`

## Test / Doğrulama

- [x] Linux Vulkan build geçmeli.
- [ ] Windows D3D12 veya Vulkan build geçmeli.
- [ ] Headless/server build Diligent olmadan geçmeli.
- [x] Clear screen smoke test.
- [x] Resize smoke test.
- [x] Shutdown leak/crash smoke test.
- [x] Public API vendor leak architecture test geçmeli.
- [ ] `rg "Diligent" Engine/Public` boş dönmeli.
- [x] Backend capability report beklenen API/backend bilgisini üretmeli.

## Done

- [x] SagaEngine Diligent üstünde frame açabiliyor.
- [x] Public API hâlâ vendor-neutral.
- [ ] Server/headless bağımsızlığı korunuyor.
- [x] Backend capability raporlanabiliyor.

---


# Phase R3B — Device Loss / Swapchain Recovery

## Amaç

Renderer'ı sadece happy-path device/swapchain init senaryosuna bağlı bırakmamak.

Gerçek dünyada window minimize, resize sırasında swapchain invalidation, driver reset, backend unavailable ve failed init gibi durumlar oluşabilir. Bu phase, Diligent bridge'in bu failure case'leri diagnostic ile yönetmesini sağlar.

## İşler

- [ ] Device lost diagnostic modeli yaz.
- [ ] Swapchain recreation path yaz.
- [ ] Minimized window handling ekle.
- [ ] Resize during frame handling ekle.
- [ ] Backend unavailable fallback diagnostic ekle.
- [ ] Failed init sonrası clean shutdown garantisi ekle.
- [ ] Device/swapchain state machine yaz.
- [ ] Device loss sonrası resource invalidation policy yaz.
- [ ] RenderGraph frame skip policy yaz.

## Test / Doğrulama

- [ ] Forced swapchain recreate smoke.
- [ ] Minimized window frame skip test.
- [ ] Invalid device init diagnostic test.
- [ ] Shutdown after failed init test.
- [ ] Resize during frame stress smoke.
- [ ] Device lost simulated state transition test.
- [ ] Resource invalidation diagnostic test.
- [ ] Headless/null backend unaffected test.

## Done

- [ ] Diligent bridge sadece happy path değildir.
- [ ] Swapchain recreation ve failure path diagnostic ile yönetilir.
- [ ] Renderer failed init veya device loss sonrası temiz kapanabilir.

---


# Phase R3C — Render Backend Capability Matrix

## Amaç

Backend feature support'u varsaymak yerine ölçmek ve raporlamak.

Diligent birden fazla graphics API'yi soyutlayabilir; ancak her backend aynı capability seviyesine sahip değildir. Renderer kalite presetleri ve feature fallback davranışı capability matrix'e bağlı olmalıdır.

## Kapsam

- [ ] Compute support.
- [ ] Timestamp query support.
- [ ] Bindless/resource array support.
- [ ] Ray tracing support.
- [ ] HDR swapchain support.
- [ ] Texture compression format support.
- [ ] Mesh shader future support.
- [ ] Indirect draw support.
- [ ] Conservative raster / depth bounds gibi optional feature notları.
- [ ] Backend-specific fallback diagnostics.

## İşler

- [ ] `RenderCapabilities` detaylandır.
- [ ] Capability report JSON üret.
- [ ] Backend feature query abstraction yaz.
- [ ] Unsupported feature fallback diagnostic ekle.
- [ ] Quality preset clamp-to-capability davranışı yaz.
- [ ] Platform/backend capability matrix dokümantasyonu oluştur.
- [ ] Linux Vulkan / Windows D3D12-Vulkan / later Metal-WebGPU-D3D11 satırlarını ayır.

## Test / Doğrulama

- [ ] Capability report JSON schema test.
- [ ] Unsupported feature fallback diagnostic test.
- [ ] Quality preset clamps to backend capability test.
- [ ] RT unavailable fallback test.
- [ ] Timestamp query unavailable fallback test.
- [ ] Texture compression format capability test.
- [ ] Backend matrix document exists.

## Done

- [ ] Renderer feature support'u varsaymıyor, raporluyor.
- [ ] Quality presetler backend capability'ye göre güvenli şekilde clamp ediliyor.
- [ ] Platform/backend farkları görünür ve test edilebilir.

---

# Phase R4 — GPU Resource Model

## Amaç

Texture, buffer, sampler, shader ve pipeline resource yönetimini Saga-owned handle sistemiyle kurmak.

## İşler

- [ ] `TextureDesc` tamamla.
- [ ] `BufferDesc` tamamla.
- [ ] `SamplerDesc` tamamla.
- [ ] `ShaderDesc` tamamla.
- [ ] `PipelineDesc` tamamla.
- [ ] Resource lifetime registry yaz.
- [ ] Invalid handle protection ekle.
- [ ] Generation-based stale handle reject ekle.
- [ ] Diligent resource pointer'larını private storage içinde sakla.
- [ ] Upload path için staging/initial data modeli kur.
- [ ] CPU/GPU lifetime ayrımı yaz.
- [ ] Resource debug name desteği ekle.
- [ ] Per-resource memory accounting başlangıcı ekle.

## Test / Doğrulama

- [ ] Texture create/destroy testleri.
- [ ] Buffer create/destroy testleri.
- [ ] Sampler create/destroy testleri.
- [ ] Invalid handle testleri.
- [ ] Double destroy testleri.
- [ ] Stale handle testleri.
- [ ] Resource leak diagnostics testleri.
- [ ] Texture upload smoke test.
- [ ] Vertex/index buffer draw smoke test.
- [ ] Public API vendor leak test geçmeli.
- [ ] Memory accounting resource create/destroy ile değişmeli.

## Done

- [ ] Saga resource handle sistemi Diligent resource pointer'larını gizliyor.
- [ ] GPU resource lifecycle test edilebilir.
- [ ] Mesh/texture çizimine hazır temel oluşmuş.
- [ ] Resource leak ve stale handle hataları yakalanabiliyor.

---

# Phase R4B — Binding / Descriptor / Frame Resource Model

## Amaç

Modern GPU binding modelini Saga-owned hale getirmek.

## Problem

Texture/buffer handle sistemi tek başına yetmez.
Renderer'ın şu modeli de net olmalıdır:

```text
per-frame buffers
descriptor/binding layout
material resource table
constant/uniform buffer updates
dynamic buffer allocation
sampler strategy
frame-in-flight ownership
```

## İşler

- [ ] Binding layout abstraction tasarla.
- [ ] Material binding table tasarla.
- [ ] Per-frame uniform/constant buffer allocation modeli kur.
- [ ] Dynamic buffer ring allocator ekle.
- [ ] Descriptor/binding update policy yaz.
- [ ] Missing texture/material fallback binding ekle.
- [ ] Frame-in-flight resource ownership modeli yaz.
- [ ] Binding validation diagnostics ekle.
- [ ] Frame resource memory budget başlangıcı ekle.

## Test / Doğrulama

- [ ] Per-frame buffer overwrite test.
- [ ] Material texture binding deterministic test.
- [ ] Stale descriptor/binding reject test.
- [ ] Missing texture fallback binding test.
- [ ] 1000 material instance binding perf smoke.
- [ ] Frame-in-flight resource lifetime test.
- [ ] Binding layout mismatch diagnostic test.
- [ ] Frame resource memory report test.

## Done

- [ ] Renderer modern GPU binding problemini Saga-owned modelle çözüyor.
- [ ] Material/instance/resource binding Diligent'e doğrudan sızmıyor.
- [ ] Frame resource lifecycle testlenebilir.

---

# Phase R5 — RenderGraph v0

## Amaç

AAA renderer için zorunlu olan pass/resource dependency sisteminin temelini kurmak.

## Kapsam

- [ ] Pass registration.
- [ ] Texture read/write declarations.
- [ ] Buffer read/write declarations.
- [ ] Pass order resolve.
- [ ] Basit transient resource allocation.
- [ ] Backbuffer pass.
- [ ] Clear pass.
- [ ] Mesh draw pass.
- [ ] Debug graph dump.
- [ ] Resource state transition abstraction.
- [ ] Pass debug names.

## İşler

- [ ] RenderGraph builder yaz.
- [ ] RenderGraph compiler yaz.
- [ ] RenderGraph executor yaz.
- [ ] Resource dependency validation yaz.
- [ ] RenderGraph dump formatı yaz.
- [ ] Minimal Diligent execution bridge yaz.
- [ ] Transient resource memory accounting başlangıcı ekle.

## Non-goal

- [ ] Tam async compute yok.
- [ ] Tam GPU-driven renderer yok.
- [ ] Tam framegraph optimizer yok.
- [ ] Full transient aliasing yok.

## Test / Doğrulama

- [ ] Pass dependency order unit test.
- [ ] Read-before-write validation test.
- [ ] Duplicate write conflict test.
- [ ] Missing resource validation test.
- [ ] Graph dump snapshot test.
- [ ] Resource state transition smoke test.
- [ ] Clear pass render smoke.
- [ ] Triangle/mesh pass render smoke.
- [ ] Resize sonrası graph rebuild smoke.
- [ ] Transient resource memory report test.

## Done

- [ ] RenderGraph gerçek frame execution'a bağlı.
- [ ] Dependency hataları testle yakalanıyor.
- [ ] Renderer rastgele imperative pass çağrısı yerine graph üzerinden ilerliyor.

---

# Phase R6 — Shader Pipeline v0

## Amaç

Shader'ları file-path hack'i olmaktan çıkarıp asset pipeline'a bağlanabilir hale getirmek.

## İşler

- [ ] Shader source description.
- [ ] Shader stage enum.
- [ ] Shader variant key.
- [ ] Shader compile profile.
- [ ] Shader include path policy.
- [ ] Shader reflection başlangıcı.
- [ ] Pipeline binding layout.
- [ ] Shader cache path.
- [ ] Build-time ve runtime shader ayrımı.
- [ ] Diligent shader creation bridge.
- [ ] Shader diagnostic format.
- [ ] Shader debug name policy.

## Karar

Başlangıçta ortak shader dili olarak HLSL tabanlı akış kullanılabilir.
Shader API Diligent'e kilitlenmeyecek.

## Test / Doğrulama

- [ ] Shader compile success test.
- [ ] Shader compile failure diagnostic test.
- [ ] Missing include test.
- [ ] Invalid stage test.
- [ ] Variant key deterministic test.
- [ ] Pipeline creation smoke test.
- [ ] Rendered triangle/mesh shader smoke test.
- [ ] Shader cache path reproducibility test.
- [ ] Reflection output schema test.

## Done

- [ ] Shader pipeline deterministik ve test edilebilir.
- [ ] Shader compile hataları düzgün diagnostic üretiyor.
- [ ] Shader system Diligent bridge üzerinden çalışıyor ama Saga-owned kalıyor.

---

# Phase R6B — Render Asset Cooking Pipeline

## Amaç

AAA-oriented renderer için asset'leri runtime hack olmaktan çıkarıp cooked render asset haline getirmek.

## Kapsam

- [ ] Texture compression strategy.
- [ ] Mipmap generation.
- [ ] Normal map validation.
- [ ] Mesh optimization.
- [ ] Tangent generation.
- [ ] LOD import/generation strategy.
- [ ] Shader variant cooking.
- [ ] Material package validation.
- [ ] Render asset manifest output.
- [ ] Cooked asset compatibility version.

## İşler

- [ ] Texture cook metadata tanımı.
- [ ] Mesh cook metadata tanımı.
- [ ] Material cook metadata tanımı.
- [ ] Shader variant cache metadata tanımı.
- [ ] Runtime loader ile cook output bağlantısı.
- [ ] Missing cooked asset diagnostic.
- [ ] Deterministic cook output policy.
- [ ] Render asset manifest schema yaz.

## Test / Doğrulama

- [ ] Texture cook output deterministic.
- [ ] Mipmap count doğru.
- [ ] Normal map format validation test.
- [ ] Material references cooked textures test.
- [ ] Mesh tangent presence validation test.
- [ ] Missing cooked asset startup diagnostic.
- [ ] Shader variant cache deterministic.
- [ ] Cooked render manifest schema test.

## Done

- [ ] Renderer raw asset path'lere doğrudan bağımlı değil.
- [ ] Texture/mesh/material/shader cook süreci render pipeline'a bağlanmış.
- [ ] Runtime render asset compatibility kontrol edilebiliyor.

---

# Phase R6C — Texture Streaming / Virtual Texture Strategy

## Amaç

MMO/open-world renderer için texture memory ve streaming stratejisini kurmak.

## Problem

Büyük dünyada bütün texture'ları tam çözünürlükte VRAM'e yüklemek gerçekçi değildir.
Texture streaming veya virtual texture stratejisi olmadan open-world/AAA render iddiası zayıf kalır.

## Kapsam

- [ ] Texture residency model.
- [ ] Mip streaming policy.
- [ ] Texture pool budget.
- [ ] Missing mip fallback.
- [ ] Terrain/large-world texture strategy.
- [ ] Virtual texture research note.
- [ ] Streaming request priority.
- [ ] Render distance / material importance etkisi.
- [ ] Texture memory diagnostics.

## İşler

- [ ] Texture residency enum tasarla.
- [ ] Texture streaming request queue tasarla.
- [ ] Per-texture mip residency state tut.
- [ ] Texture pool budget raporu ekle.
- [ ] Terrain texture streaming decision yaz.
- [ ] Virtual texture geleceği için research note yaz.
- [ ] Missing mip fallback texture sistemi ekle.
- [ ] Streaming diagnostics üret.

## Test / Doğrulama

- [ ] Texture residency state transition test.
- [ ] Mip load/unload simulation test.
- [ ] Texture pool budget overflow diagnostic.
- [ ] Missing mip fallback render smoke.
- [ ] Terrain texture streaming smoke.
- [ ] Streaming priority deterministic test.
- [ ] Texture memory report test.

## Done

- [ ] Renderer büyük dünya texture memory problemini açıkça ele alıyor.
- [ ] Texture streaming / residency foundation oluşmuş.
- [ ] Virtual texture yönü belgelenmiş ama overclaim yapılmamış.

---


# Phase R6D — Mesh / Geometry Streaming Strategy

## Amaç

MMO/open-world renderer için geometry memory ve mesh residency stratejisini kurmak.

Texture streaming tek başına yetmez. Büyük dünyada mesh/LOD/terrain/character geometry de bütçelenmeli ve stream edilmelidir.

## Kapsam

- [ ] Mesh residency model.
- [ ] Static mesh streaming policy.
- [ ] LOD mesh streaming policy.
- [ ] Skinned mesh streaming policy.
- [ ] Terrain chunk geometry streaming.
- [ ] Missing mesh fallback.
- [ ] Mesh memory budget report.
- [ ] Geometry streaming priority.
- [ ] Asset package compatibility.

## İşler

- [ ] Mesh residency enum tasarla.
- [ ] Mesh streaming request queue tasarla.
- [ ] Mesh LOD residency state tut.
- [ ] Skinned mesh residency state tut.
- [ ] Terrain geometry chunk load/unload policy yaz.
- [ ] Missing mesh fallback primitive veya proxy yaz.
- [ ] Geometry memory report üret.
- [ ] Mesh streaming diagnostics ekle.

## Test / Doğrulama

- [ ] Mesh load/unload lifecycle test.
- [ ] LOD mesh residency test.
- [ ] Skinned mesh residency test.
- [ ] Terrain chunk geometry streaming test.
- [ ] Missing mesh fallback render smoke.
- [ ] Geometry memory budget test.
- [ ] Mesh streaming priority deterministic test.
- [ ] Shutdown after streamed mesh unload leak test.

## Done

- [ ] Renderer large-world geometry memory problemini açıkça ele alıyor.
- [ ] Mesh/LOD/skinned/terrain geometry residency takip ediliyor.
- [ ] Geometry streaming texture streaming ile aynı mimari aileye bağlanmış.

---


# Phase R6E — Shader Variant Budget / Permutation Control

## Amaç

Shader variant explosion riskini kontrol altına almak.

AAA material sistemi büyüdükçe shader permutation sayısı kontrolden çıkabilir. Bu phase, material feature flags ve platform-specific shader variant üretimini bütçeye bağlar.

## Kapsam

- [ ] Max variant count policy.
- [ ] Material feature flags budget.
- [ ] Variant pruning.
- [ ] Platform-specific variant filtering.
- [ ] Shader compile time report.
- [ ] Shader cache size budget.
- [ ] Invalid feature combination rejection.
- [ ] Variant dependency diagnostics.

## İşler

- [ ] Shader variant budget config yaz.
- [ ] Material feature flag matrix yaz.
- [ ] Variant pruning rules yaz.
- [ ] Platform/backend variant filter yaz.
- [ ] Shader compile time report üret.
- [ ] Shader cache size report üret.
- [ ] Invalid feature combination diagnostics ekle.
- [ ] CI/gate threshold belirle.

## Test / Doğrulama

- [ ] Variant count threshold test.
- [ ] Invalid feature combination reject test.
- [ ] Shader cache size report test.
- [ ] Platform variant filtering test.
- [ ] Compile time report exists test.
- [ ] Variant pruning deterministic test.
- [ ] Material feature flag matrix validation test.

## Done

- [ ] Shader pipeline feature ekledikçe kontrolsüz şişmiyor.
- [ ] Shader compile/cache maliyeti raporlanıyor.
- [ ] Material sistemi shader variant bütçesine bağlı çalışıyor.

---


# Phase R6F — Shader / Material Hot Reload Development Loop

## Amaç

Shader ve material geliştirme sürecini engine restart gerektirmeden hızlandırmak.

Bu production runtime requirement değildir; developer iteration speed requirement'tır.

## Kapsam

- [ ] Shader file watch veya explicit reload command.
- [ ] Material parameter live update.
- [ ] Failed reload keeps previous valid shader.
- [ ] RenderGraph pass shader reload.
- [ ] Pipeline cache invalidation.
- [ ] Shader diagnostic overlay/log.
- [ ] Hot reload disabled production mode.

## İşler

- [ ] Shader reload command yaz.
- [ ] Material reload command yaz.
- [ ] Pipeline cache invalidation policy yaz.
- [ ] Failed shader reload fallback davranışı yaz.
- [ ] Previous-valid-shader retention ekle.
- [ ] RenderGraph pass shader dependency reload bağlantısı kur.
- [ ] Hot reload diagnostics üret.
- [ ] Production build hot reload disable seçeneği ekle.

## Test / Doğrulama

- [ ] Shader reload without engine restart test.
- [ ] Failed reload keeps previous valid shader test.
- [ ] Material parameter live update test.
- [ ] RenderGraph pass shader reload test.
- [ ] Pipeline cache invalidation test.
- [ ] Hot reload disabled build test.
- [ ] Reload diagnostic contains filename/stage/error.

## Done

- [ ] Shader/material iteration restart gerektirmiyor.
- [ ] Failed reload renderer'ı bozmayıp önceki geçerli state'i koruyor.
- [ ] Developer loop hızlanıyor ama production build kirlenmiyor.

---

# Phase R7 — Material System v0

## Amaç

AAA hissinin temel taşı olan material sistemini başlatmak.

## İlk material modeli

- [ ] BaseColor.
- [ ] Metallic.
- [ ] Roughness.
- [ ] NormalMap.
- [ ] Emissive.
- [ ] AlphaMode.
- [ ] DoubleSided.
- [ ] Texture bindings.
- [ ] Material instance.
- [ ] Material parameter override.

## İşler

- [ ] `MaterialAsset` tanımı.
- [ ] `MaterialInstance` tanımı.
- [ ] PBR material shader.
- [ ] Material -> Pipeline mapping.
- [ ] Material -> binding mapping.
- [ ] Default material.
- [ ] Missing texture fallback.
- [ ] Asset registry entegrasyon noktası.
- [ ] Cooked material compatibility kontrolü.
- [ ] Material texture residency hints ekle.

## Test / Doğrulama

- [ ] Default material render smoke.
- [ ] Texture-bound material render smoke.
- [ ] Missing texture fallback test.
- [ ] Metallic/roughness parameter deterministic test.
- [ ] Material instance override test.
- [ ] Invalid material diagnostic test.
- [ ] Golden image test başlangıcı.
- [ ] Cooked material load test.
- [ ] Material residency hint test.

## Done

- [ ] Mesh artık hardcoded shader/color ile çizilmiyor.
- [ ] Material asset ve material instance ayrımı başlamış.
- [ ] PBR-ish görsel temel oluşmuş.
- [ ] Material cook/runtime bağı kurulmuş.

---


# Phase R7B — Material Graph / Authoring Contract

## Amaç

Material system'in ileride artist-facing authoring ve editor graph sistemine bağlanabilmesi için render tarafı contract'ını tanımlamak.

Bu phase editor UI implementation değildir. Sadece renderer'ın kabul edeceği material graph output sözleşmesidir.

## Kapsam

- [ ] Material graph future contract.
- [ ] Node-to-shader variant policy.
- [ ] Cooked material graph output.
- [ ] Graph feature budget.
- [ ] Unsupported node diagnostic.
- [ ] Custom node policy.
- [ ] Material graph versioning.
- [ ] Shader variant budget ile ilişki.

## İşler

- [ ] `RENDER_MATERIAL_GRAPH_AUTHORING_CONTRACT.md` oluştur.
- [ ] Material graph output schema yaz.
- [ ] Supported node category listesi yaz.
- [ ] Unsupported node diagnostic formatı yaz.
- [ ] Node feature flag -> shader variant mapping yaz.
- [ ] Cooked graph output -> MaterialAsset mapping yaz.
- [ ] Custom node ve native shader escape hatch policy yaz.
- [ ] Shader variant budget ile graph feature budget'ı ilişkilendir.

## Test / Doğrulama

- [ ] Material graph contract document exists.
- [ ] Simple graph -> cooked material schema test.
- [ ] Unsupported node diagnostic test.
- [ ] Node feature budget threshold test.
- [ ] Graph version mismatch diagnostic test.
- [ ] Cooked graph output loads as MaterialAsset test.

## Done

- [ ] Material system ileride editor/artist graph'a bağlanmaya hazır.
- [ ] Material graph shader variant explosion'a kontrolsüz kapı açmıyor.
- [ ] Render tarafı authoring contract'ı UI'dan bağımsız şekilde tanımlanmış.

---

# Phase R8 — Scene Renderer v0

## Amaç

Tek tek demo draw çağrısından gerçek sahne renderer'a geçmek.

## İşler

- [ ] Renderable component model.
- [ ] Mesh reference.
- [ ] Material reference.
- [ ] Transform upload.
- [ ] Camera data.
- [ ] Per-frame scene extraction.
- [ ] Draw packet generation.
- [ ] Static mesh draw path.
- [ ] Basic instancing.
- [ ] Debug wireframe/normal option.
- [ ] Draw packet sorting policy.

## Test / Doğrulama

- [ ] Tek mesh render smoke.
- [ ] Çoklu mesh render smoke.
- [ ] Transform correctness golden image.
- [ ] Camera movement smoke.
- [ ] Material assignment test.
- [ ] Invalid mesh/material diagnostic test.
- [ ] 1k static object perf smoke.
- [ ] Draw packet deterministic ordering test.

## Done

- [ ] Renderer gerçek sahne verisini okuyup draw packet üretiyor.
- [ ] Hardcoded demo render path kapanmış.
- [ ] MMO entity rendering için temel oluşmuş.

---

# Phase R8B — Render Thread / Frame Ownership Model

## Amaç

Game/world state ile render submission arasındaki ownership sınırını kurmak.

## Hedef model

```text
Game Thread:
  world/entity state üretir

Render Extraction:
  immutable render snapshot çıkarır

Render Thread:
  RenderGraph compile + GPU submission yapar

GPU:
  N frames-in-flight ile çalışır
```

## İşler

- [ ] Render snapshot modeli tasarla.
- [ ] Game thread'in GPU resource'a doğrudan dokunmasını engelle.
- [ ] Render command queue veya render submission queue tanımla.
- [ ] Frame-in-flight sayısını konfigüre et.
- [ ] Per-frame resource lifetime policy yaz.
- [ ] Shutdown/drain frame policy yaz.
- [ ] Render thread diagnostics ekle.
- [ ] Thread-safety architecture testleri ekle.

## Test / Doğrulama

- [ ] Render snapshot immutable test.
- [ ] Game thread direct GPU mutation guard test.
- [ ] 2 veya 3 frames-in-flight smoke test.
- [ ] Shutdown sırasında frame resources clean test.
- [ ] Race/leak smoke test.
- [ ] Render thread disabled/single-thread fallback test.
- [ ] Frame queue overflow diagnostic test.

## Done

- [ ] Renderer world state'e doğrudan bağlı değil.
- [ ] Game/render thread sınırı açık.
- [ ] MMO world update ile GPU submission ayrılmış.

---


# Phase R8C — Camera / View Family / Multi-View Rendering

## Amaç

Renderer'ı tek main camera varsayımından çıkarıp view family modeline geçirmek.

MMO/AAA renderer'da sadece player camera yoktur. Shadow view, reflection probe view, minimap view, spectator/debug camera ve render-to-texture camera gibi farklı view tipleri gerekir.

## Kapsam

- [ ] Main camera view.
- [ ] Shadow camera/view.
- [ ] Reflection/probe view.
- [ ] Minimap/render-to-texture view.
- [ ] Debug/spectator camera.
- [ ] Cinematic camera future note.
- [ ] View family resource lifetime.
- [ ] Per-view visibility set.
- [ ] Per-view render graph pass relationship.

## İşler

- [ ] `RenderView` tanımı yaz.
- [ ] `ViewFamily` tanımı yaz.
- [ ] Main view extraction yaz.
- [ ] Shadow view extraction yaz.
- [ ] Render-to-texture/minimap view placeholder yaz.
- [ ] Debug camera switch policy yaz.
- [ ] Per-view visibility/culling hook yaz.
- [ ] Per-view resource lifetime diagnostics ekle.

## Test / Doğrulama

- [ ] Main view render smoke.
- [ ] Shadow view render smoke.
- [ ] Minimap/render texture view smoke.
- [ ] Debug camera switch test.
- [ ] Multiple view resource lifetime test.
- [ ] Per-view culling set test.
- [ ] Resize multi-view resources test.

## Done

- [ ] Renderer tek kamera varsayımına kilitli değil.
- [ ] Shadow/probe/minimap/debug gibi view tiplerine mimari hazır.
- [ ] Multi-view resource lifetime test edilebilir.

---

# Phase R9 — Lighting / Shadows v0

## Amaç

AAA hissi için ilk ciddi lighting ve shadow pipeline'ını kurmak.

## Başlangıç kapsamı

- [ ] Directional light.
- [ ] Shadow map.
- [ ] Cascaded shadow map hazırlığı.
- [ ] Ambient term.
- [ ] IBL placeholder.
- [ ] Light constant buffer.
- [ ] Shadow debug view.
- [ ] Exposure-compatible output.

## İşler

- [ ] Light extraction.
- [ ] Shadow pass RenderGraph node.
- [ ] Main lighting pass.
- [ ] Shadow map resource lifecycle.
- [ ] Shadow bias/slope bias config.
- [ ] Light/shadow diagnostics.
- [ ] Shadow debug visualization.

## Test / Doğrulama

- [ ] Directional light golden image.
- [ ] Shadow caster/receiver golden image.
- [ ] Light disabled render test.
- [ ] Shadow disabled render test.
- [ ] Resize shadow resource test.
- [ ] Invalid shadow map config diagnostic.
- [ ] Perf smoke with 1 directional shadow light.

## Done

- [ ] Renderer artık sadece unlit/demo görüntü vermiyor.
- [ ] En az bir shadow-casting light çalışıyor.
- [ ] Lighting/shadow sonucu testlenebilir.

---


# Phase R9B — Forward / Deferred / Clustered Lighting Strategy

## Amaç

Renderer'ın lighting architecture yönünü netleştirmek.

GBufferPass, ForwardPass ve LightingPass dosyalarının var olması yeterli değildir. Strateji açık olmalıdır: forward, deferred, forward+, clustered veya hybrid.

## Önerilen ilk karar

```text
v0:
  Forward veya Forward+ tabanlı yol

later:
  Deferred / hybrid research

MMO gerekçesi:
  çok sayıda karakter
  transparency/VFX ihtiyacı
  açık dünya sahneleri
  geniş donanım aralığı
```

## İşler

- [ ] Lighting strategy document yaz.
- [ ] v0 lighting path seçimini yaz.
- [ ] Deferred/hybrid future note yaz.
- [ ] Clustered/Forward+ feasibility note yaz.
- [ ] Max visible lights budget tanımla.
- [ ] Light culling modelini visibility sistemiyle ilişkilendir.
- [ ] Transparency/VFX lighting interaction notu yaz.

## Test / Doğrulama

- [ ] Lighting strategy document exists.
- [ ] Max lights budget defined.
- [ ] Visible light list test.
- [ ] Clustered/Forward+ placeholder validation.
- [ ] Invalid light count diagnostic test.
- [ ] Lighting mode switch compile/config test.
- [ ] Transparency/VFX lighting compatibility note exists.

## Done

- [ ] Renderer lighting mimarisi belirsiz değil.
- [ ] v0 lighting path bilinçli seçilmiş.
- [ ] Gelecekte deferred/hybrid/clustered yönüne kapanmıyor.

---

# Phase R10 — Post Processing v0

## Amaç

AAA hissini görsel olarak artıracak final image pipeline'ını başlatmak.

## İlk kapsam

- [ ] HDR render target.
- [ ] Tonemapping.
- [ ] Gamma correctness.
- [ ] Bloom.
- [ ] Color grading placeholder.
- [ ] FXAA veya TAA hazırlığı.
- [ ] Exposure parameter.
- [ ] Debug pass view.

## İşler

- [ ] HDR scene color target.
- [ ] Luminance/exposure parameter.
- [ ] Tonemap pass.
- [ ] Bloom extraction/downsample/upsample minimal path.
- [ ] Final composite pass.
- [ ] Debug final image controls.
- [ ] Post-process chain validation.

## Test / Doğrulama

- [ ] HDR target creation test.
- [ ] Tonemapping golden image.
- [ ] Bloom on/off golden image.
- [ ] Gamma correctness test scene.
- [ ] Resize post-process resources test.
- [ ] Invalid post-process chain diagnostic.
- [ ] RenderGraph pass order test.

## Done

- [ ] Final image bilinçli post-process chain üzerinden backbuffer'a gidiyor.
- [ ] Basic cinematic output pipeline başlamış.
- [ ] Debug view ile pass'ler görülebiliyor.

---

# Phase R10B — Reflections / IBL / GI Strategy

## Amaç

AAA-oriented render için reflection/GI yönünü en azından temel strateji olarak kurmak.

## İlk kapsam

- [ ] IBL environment map.
- [ ] Reflection probe placeholder.
- [ ] SSR strategy decision.
- [ ] Ray tracing future note.
- [ ] Missing environment fallback.
- [ ] Reflection debug mode.
- [ ] GI non-goal / future research document.
- [ ] Probe-based GI future research note.

## İşler

- [ ] Environment map asset description.
- [ ] IBL sampling path.
- [ ] Reflection probe data structure.
- [ ] SSR on/off pipeline placeholder.
- [ ] GI strategy document.
- [ ] The Forge GI/reference audit notu.
- [ ] Probe/cascade strategy research note.

## Test / Doğrulama

- [ ] IBL environment render smoke.
- [ ] Missing environment fallback test.
- [ ] Reflection probe sample scene.
- [ ] SSR on/off golden image veya placeholder validation.
- [ ] Reflection debug view smoke.
- [ ] GI strategy doc exists test.

## Done

- [ ] Renderer'ın reflection/IBL/GI yönü belirsiz değil.
- [ ] Level 5 için gerekli reflection foundation başlamış.
- [ ] Full GI iddiası yapılmıyor.

---

# Phase R10C — Temporal AA / Upscaling Foundation

## Amaç

AAA-oriented final image kalitesi için temporal veri ve upscaling altyapısını başlatmak.

## İlk kapsam

- [ ] Camera jitter.
- [ ] Motion vector buffer.
- [ ] Depth buffer history.
- [ ] Exposure/history buffer lifecycle.
- [ ] TAA placeholder veya minimal TAA.
- [ ] Dynamic resolution scale.
- [ ] FSR integration spike opsiyonu.
- [ ] History reset policy.
- [ ] Debug AA modes.

## İşler

- [ ] Motion vector pass veya output tasarla.
- [ ] Camera jitter sequence oluştur.
- [ ] TAA history resource lifecycle yaz.
- [ ] Dynamic resolution render target scale modelini yaz.
- [ ] FSR için input requirement notu yaz.
- [ ] AA debug modes ekle.

## Test / Doğrulama

- [ ] Motion vector buffer exists test.
- [ ] Camera jitter deterministic test.
- [ ] TAA history resize/reset test.
- [ ] Dynamic resolution scale test.
- [ ] AA mode switch smoke.
- [ ] FSR spike optional build test.
- [ ] Ghosting/debug scene baseline.

## Done

- [ ] Renderer temporal data üretmeye başlamış.
- [ ] Upscaling/TAA için gerekli kaynaklar ve lifecycle kurulmuş.
- [ ] Final image pipeline modern AA/upscale yönüne hazır.

---


# Phase R10D — Ray Tracing Future Boundary

## Amaç

Ray tracing'i bugün production hedef yapmadan, renderer mimarisinin gelecekte RT path'e tamamen kapalı olmasını engellemek.

## Non-goal

- [ ] Bu phase ray tracing implementation değildir.
- [ ] Hardware RT zorunlu dependency yapılmaz.
- [ ] Level 5 için ray tracing şart koşulmaz.

## Kapsam

- [ ] Ray tracing capability query.
- [ ] Acceleration structure future abstraction.
- [ ] Material compatibility note.
- [ ] Fallback path required.
- [ ] No hard dependency on RT hardware.
- [ ] Reflection/GI strategy ile ilişki.
- [ ] Diligent backend RT capability araştırması.
- [ ] The Forge RT/GI research note bağlantısı.

## İşler

- [ ] `RenderCapabilities` içine RT capability field future slot ekle veya dokümante et.
- [ ] Acceleration structure future abstraction notu yaz.
- [ ] Material/ray tracing compatibility notu yaz.
- [ ] Non-RT fallback zorunluluğunu yaz.
- [ ] Reflection/GI phase ile bağlantı kur.
- [ ] RT research doc oluştur.

## Test / Doğrulama

- [ ] RT capability query placeholder compile test.
- [ ] Non-RT backend path unaffected test.
- [ ] RT unavailable diagnostic test.
- [ ] Fallback path document exists.
- [ ] Material compatibility note exists.
- [ ] No hard dependency on RT hardware scan.

## Done

- [ ] Renderer gelecekte RT path'e tamamen kapalı değil.
- [ ] RT şu an production dependency yapılmamış.
- [ ] Fallback path zorunluluğu net.

---

# Phase R11 — MMO Visibility / Culling / LOD v0

## Amaç

SagaEngine'i genel amaçlı renderer olmaktan çıkarıp MMO-first renderer haline getirmek.

## İşler

- [ ] Frustum culling.
- [ ] Distance culling.
- [ ] LOD selection.
- [ ] Zone/chunk visibility source.
- [ ] Static/dynamic renderable ayrımı.
- [ ] Instance batching.
- [ ] Crowd/character render budget.
- [ ] Network visibility ile render visibility ayrımını dokümante et.
- [ ] Debug culling overlay.
- [ ] Render budget categories.

## MMO ilkeleri

- Renderer tüm dünyayı çizmemeli.
- Renderer görünür region/zone/chunk setiyle çalışmalı.
- Network visibility ile render visibility aynı kavram olarak ele alınmamalı.
- Client cosmetic object rendering, server authority'den ayrılmalı.
- Character/crowd rendering ayrı bütçelenmeli.

## Test / Doğrulama

- [ ] Frustum culling unit test.
- [ ] Distance culling unit test.
- [ ] LOD selection unit test.
- [ ] Zone visibility smoke test.
- [ ] 10k object culling perf smoke.
- [ ] Instance batching count test.
- [ ] Debug overlay görüntü smoke.
- [ ] Culling determinism test.

## Done

- [ ] Renderer MMO ölçeğinde görünür nesne seçebiliyor.
- [ ] Büyük dünya render path'i başlamış.
- [ ] Culling/LOD testlenebilir.

---

# Phase R11B — Occlusion Culling / Visibility Acceleration

## Amaç

Frustum/distance culling sonrası hâlâ görünmeyen nesneleri submit etmemek.

## Kapsam

- [ ] CPU occlusion candidate model.
- [ ] Hi-Z depth pyramid strategy.
- [ ] Occlusion query strategy.
- [ ] Large object / small object policy.
- [ ] Static occluder strategy.
- [ ] Debug occlusion overlay.
- [ ] False positive/negative safety policy.

## İşler

- [ ] Occlusion candidate classification yaz.
- [ ] Hi-Z depth pyramid placeholder veya v0 oluştur.
- [ ] Occlusion decision cache tasarla.
- [ ] Debug occlusion view ekle.
- [ ] Performance fallback policy yaz.
- [ ] Occlusion correctness diagnostics ekle.

## Test / Doğrulama

- [ ] Hidden objects are not submitted test.
- [ ] Visible objects are not incorrectly culled test.
- [ ] Occlusion debug view smoke.
- [ ] 10k object occlusion perf smoke.
- [ ] Camera cut/reset occlusion cache test.
- [ ] False cull diagnostic test.

## Done

- [ ] Renderer sadece frustum/distance ile sınırlı değil.
- [ ] Görünmeyen nesnelerin submit edilmemesi için foundation oluşmuş.
- [ ] Occlusion sistemi debug edilebilir.

---

# Phase R11C — GPU-Driven / Indirect Draw Strategy

## Amaç

Uzun vadeli AAA/MMO renderer için CPU-driven submission sınırlarını şimdiden planlamak.

## Not

Bu phase'in amacı hemen tam GPU-driven renderer yazmak değildir.
Ama render architecture'ın buna kapanmasını engellemektir.

## Kapsam

- [ ] Indirect draw future strategy.
- [ ] GPU culling future strategy.
- [ ] Meshlet/cluster rendering research note.
- [ ] Draw packet format compatibility.
- [ ] Diligent backend compatibility.
- [ ] Debug/profiling implications.
- [ ] The Forge visibility buffer research linkage.

## İşler

- [ ] GPU-driven strategy document yaz.
- [ ] DrawPacket formatını indirect draw'a uygunluk açısından denetle.
- [ ] Instance data layout future-proof yap.
- [ ] Meshlet/cluster research note yaz.
- [ ] The Forge visibility buffer notes ile ilişkilendir.
- [ ] Non-goal ve future gate yaz.

## Test / Doğrulama

- [ ] DrawPacket format review test/document exists.
- [ ] Instance data alignment test.
- [ ] Indirect draw placeholder compile test.
- [ ] GPU-driven strategy doc exists.
- [ ] The Forge reference note linked.

## Done

- [ ] Renderer gelecekte GPU-driven path'e kapalı değil.
- [ ] CPU-driven v0 uygulanırken uzun vadeli yol korunuyor.
- [ ] Meshlet/cluster/visibility buffer yönü araştırma seviyesinde belgeli.

---


# Phase R11D — Render Interest / World Streaming Contract

## Amaç

MMO world, network interest, client streaming ve render visibility kavramlarını açıkça ayırmak.

Renderer, server/network relevance sisteminin aynısı değildir. Ancak MMO'da render resource preload, zone visibility ve entity lifecycle bu sistemlerle sözleşme yapmalıdır.

## Hedef akış

```text
Server / world interest state
↓
Client streaming state
↓
Render visibility state
↓
Render resource residency
```

## Kapsam

- [ ] Server interest set ile render visibility set ayrımı.
- [ ] Client-side streaming region contract.
- [ ] Unloaded zone fallback render behavior.
- [ ] Entity enters/leaves visibility lifecycle.
- [ ] Render resource preload hints.
- [ ] Network relevance != render relevance rule.
- [ ] Zone handoff behavior.
- [ ] Interest/streaming diagnostics.

## İşler

- [ ] `RENDER_INTEREST_WORLD_STREAMING_CONTRACT.md` oluştur.
- [ ] Render interest state tanımı yaz.
- [ ] Render visibility state tanımı yaz.
- [ ] Client streaming region contract yaz.
- [ ] Entity enter/leave lifecycle hooks yaz.
- [ ] Render resource preload hint sistemi tasarla.
- [ ] Unloaded zone fallback visual policy yaz.
- [ ] Network relevance ile render relevance ayrımını testlenebilir hale getir.

## Test / Doğrulama

- [ ] Network relevance != render relevance test.
- [ ] Entity enters visibility lifecycle test.
- [ ] Entity leaves visibility lifecycle test.
- [ ] Zone preload hint test.
- [ ] Unloaded zone fallback render smoke.
- [ ] Streaming region transition test.
- [ ] Interest/visibility diagnostics schema test.

## Done

- [ ] MMO world/network/render kavramları birbirine karışmıyor.
- [ ] Renderer streaming ve visibility kararlarını açık sözleşmeyle alıyor.
- [ ] Zone/entity lifecycle render resource residency ile bağlanmış.

---

# Phase R12 — Terrain / Vegetation / HLOD v0

## Amaç

MMO/open-world hissi için dünya render altyapısını başlatmak.

## İlk kapsam

- [ ] Terrain chunk abstraction.
- [ ] Terrain material.
- [ ] Terrain LOD.
- [ ] Vegetation instance path.
- [ ] HLOD placeholder.
- [ ] Impostor strategy document.
- [ ] Streaming-aware render resource load/unload.
- [ ] Debug terrain/LOD overlay.

## İşler

- [ ] Terrain chunk render data tasarla.
- [ ] Terrain material binding tasarla.
- [ ] Terrain LOD selection yaz.
- [ ] Vegetation instance buffer path yaz.
- [ ] HLOD placeholder object modeli yaz.
- [ ] Impostor strategy document yaz.
- [ ] Chunk load/unload render resource lifecycle yaz.

## Test / Doğrulama

- [ ] Single terrain chunk render smoke.
- [ ] Multi-chunk terrain render smoke.
- [ ] Terrain LOD switch test.
- [ ] Vegetation instancing perf smoke.
- [ ] Chunk load/unload resource leak test.
- [ ] HLOD placeholder selection test.
- [ ] Golden image terrain scene.

## Done

- [ ] Renderer sadece küçük demo sahnesi değil, açık dünya parçası gösterebiliyor.
- [ ] Chunk/LOD mantığı render sistemine girmiş.
- [ ] Asset streaming entegrasyon noktası hazır.

---

# Phase R12B — Character / Skinned Mesh Renderer

## Amaç

MMO için kritik olan karakter, animasyon ve ekipman render temelini kurmak.

## İlk kapsam

- [ ] Skinned mesh asset model.
- [ ] Skeleton reference.
- [ ] Animation pose buffer.
- [ ] Bone matrix upload.
- [ ] Character material assignment.
- [ ] Equipment/attachment render path.
- [ ] Character LOD.
- [ ] Crowd render budget.
- [ ] Debug skeleton/pose view.

## İşler

- [ ] Skinned mesh render data tasarla.
- [ ] Skeleton/bone palette upload path yaz.
- [ ] Animation pose buffer lifecycle yaz.
- [ ] Character material binding yaz.
- [ ] Equipment attachment render path yaz.
- [ ] Character LOD selection yaz.
- [ ] Crowd render budget diagnostics ekle.
- [ ] Debug skeleton view ekle.

## Test / Doğrulama

- [ ] Single skinned character render smoke.
- [ ] Bone transform correctness test.
- [ ] Animation pose buffer lifecycle test.
- [ ] 100 animated characters perf smoke.
- [ ] Attachment/equipment render test.
- [ ] Character LOD switch test.
- [ ] Missing skeleton/mesh diagnostic test.
- [ ] Debug skeleton view smoke.

## Done

- [ ] MMO karakter rendering temel olarak çalışıyor.
- [ ] Static mesh renderer dışında karakter pipeline oluşmuş.
- [ ] Crowd/character render budget ölçülebilir.

---


# Phase R12D — MMO Avatar / Equipment Rendering

## Amaç

MMO karakter rendering'i sadece skinned mesh çizmekten çıkarıp avatar/equipment/cosmetic gerçekliğine taşımak.

MMO'da oyuncular aynı karakter değildir. Armor, weapon, hair, clothing layer, dye/material variant, mount/pet ve cosmetic skin gibi sistemler render budget'ın parçasıdır.

## Kapsam

- [ ] Armor attachment render path.
- [ ] Weapon attachment render path.
- [ ] Clothing layer strategy.
- [ ] Hair/cosmetic mesh strategy.
- [ ] Dye/material variant strategy.
- [ ] Mount/pet render hook.
- [ ] Character equipment LOD.
- [ ] Cosmetic fallback.
- [ ] 100 farklı ekipmanlı character perf smoke.

## İşler

- [ ] Avatar render descriptor tasarla.
- [ ] Equipment attachment slot modeli yaz.
- [ ] Weapon attachment transform/bone binding yaz.
- [ ] Cosmetic material variant binding yaz.
- [ ] Dye/material override policy yaz.
- [ ] Missing cosmetic fallback sistemi yaz.
- [ ] Character equipment LOD policy yaz.
- [ ] Avatar/equipment diagnostics ekle.

## Test / Doğrulama

- [ ] Armor attachment render test.
- [ ] Weapon attachment render test.
- [ ] Cosmetic material variant test.
- [ ] 100 characters with different equipment perf smoke.
- [ ] Missing cosmetic fallback test.
- [ ] Equipment LOD switch test.
- [ ] Attachment bone mismatch diagnostic test.
- [ ] Dye/material override deterministic test.

## Done

- [ ] MMO character renderer customization gerçeğini taşıyor.
- [ ] Ekipman/cosmetic çeşitliliği render budget'a dahil.
- [ ] Missing cosmetic/equipment durumları fallback ile yönetiliyor.

---


# Phase R12E — Sky / Atmosphere / Weather Foundation

## Amaç

Açık dünya MMO görsel hissi için sky, atmosphere, time-of-day ve weather foundation kurmak.

Bu phase gerçekçi volumetric cloud simülasyonu değildir. Ama world renderer'ın gökyüzü, gün ışığı yönü, fog/weather hook'ları ve day/night geçişi için mimari foundation oluşturur.

## Kapsam

- [ ] Sky render pass.
- [ ] Sun/moon direction.
- [ ] Time-of-day parameter.
- [ ] Atmospheric scattering strategy.
- [ ] Cloud placeholder.
- [ ] Fog/weather hook.
- [ ] Rain/snow VFX integration hook.
- [ ] Day/night lighting interaction.
- [ ] Weather diagnostics.

## İşler

- [ ] Sky pass placeholder yaz.
- [ ] Time-of-day render parameters tasarla.
- [ ] Sun/moon direction modelini light system ile bağla.
- [ ] Fog/weather parameter contract yaz.
- [ ] Cloud placeholder strategy yaz.
- [ ] Rain/snow VFX hook'u VFX system ile ilişkilendir.
- [ ] Day/night golden image fixture oluştur.
- [ ] Weather diagnostics üret.

## Test / Doğrulama

- [ ] Sky render smoke.
- [ ] Time-of-day light direction test.
- [ ] Fog/weather parameter test.
- [ ] Weather VFX render smoke.
- [ ] Day/night golden image pair.
- [ ] Missing sky asset fallback test.
- [ ] Weather diagnostics schema test.

## Done

- [ ] Açık dünya renderer gökyüzü/time-of-day/weather foundation'a sahip.
- [ ] Sky/weather sistemi lighting/postprocess/VFX ile ilişkilendirilmiş.
- [ ] World feel sadece terrain/vegetation'dan ibaret değil.

---


# Phase R12F — Water / Ocean / River Rendering Strategy

## Amaç

Open-world/MMO renderer için water/ocean/river rendering yönünü tanımlamak.

Bu phase tam ocean simulation değildir. Ama water material, reflection/refraction ilişkisi, shoreline/foam placeholder ve underwater/fog interaction gibi temel kararları verir.

## Kapsam

- [ ] Water material.
- [ ] River/lake/ocean category.
- [ ] Planar reflection future note.
- [ ] Screen-space reflection compatibility.
- [ ] Refraction placeholder.
- [ ] Shoreline/foam placeholder.
- [ ] Underwater/fog interaction note.
- [ ] Water quality preset.
- [ ] Water debug view.

## İşler

- [ ] Water material descriptor yaz.
- [ ] Water render pass placeholder yaz.
- [ ] Reflection/IBL/SSR phase ile bağlantı kur.
- [ ] Refraction placeholder strategy yaz.
- [ ] Shoreline/foam placeholder strategy yaz.
- [ ] Underwater/fog interaction notu yaz.
- [ ] Water quality preset knobs tanımla.
- [ ] Water diagnostics/debug view ekle.

## Test / Doğrulama

- [ ] Basic water plane render smoke.
- [ ] Water material parameter test.
- [ ] SSR/reflection compatibility note exists.
- [ ] Underwater/fog parameter smoke.
- [ ] Water quality preset test.
- [ ] Missing water material fallback test.
- [ ] Water debug view smoke.

## Done

- [ ] Açık dünya renderer su rendering yönüne sahip.
- [ ] Water system reflection/postprocess/fog/scalability ile ilişkilendirilmiş.
- [ ] Tam ocean sim iddiası yapılmadan Level 5 foundation kurulmuş.

---

# Phase R12C — Transparency / Decals / VFX Foundation

## Amaç

AAA hissi için gerekli opaque dışı render altyapısını başlatmak.

## İlk kapsam

- [ ] Alpha cutout.
- [ ] Transparent sorting.
- [ ] Decal projection.
- [ ] Basic particle system render path.
- [ ] Overdraw diagnostics.
- [ ] Fog/volumetric placeholder.
- [ ] VFX material category.
- [ ] Debug transparent/decal/VFX view.

## İşler

- [ ] Transparent render queue yaz.
- [ ] Alpha cutout material path yaz.
- [ ] Transparent sorting policy yaz.
- [ ] Decal render data path yaz.
- [ ] Particle render buffer path yaz.
- [ ] Overdraw diagnostics ekle.
- [ ] Fog placeholder pass ekle.
- [ ] VFX material category tanımla.

## Test / Doğrulama

- [ ] Opaque + alpha cutout order test.
- [ ] Transparent sorting smoke.
- [ ] Decal projection smoke.
- [ ] 10k particle perf smoke.
- [ ] Overdraw diagnostics smoke.
- [ ] Missing VFX material fallback test.
- [ ] Fog placeholder render smoke.

## Done

- [ ] Renderer sadece opaque mesh çizmekle sınırlı değil.
- [ ] Transparent/VFX/decal temeli oluşmuş.
- [ ] Overdraw ölçümü başlamış.

---

# Phase R13 — Render Diagnostics / GPU Profiling

## Amaç

Renderer'ın performansını ve hatalarını ölçülebilir hale getirmek.

## İşler

- [ ] CPU frame timing.
- [ ] GPU timestamp query.
- [ ] Per-pass timing.
- [ ] Draw call count.
- [ ] Triangle count.
- [ ] Texture/buffer memory report.
- [ ] Shader/pipeline cache stats.
- [ ] RenderGraph diagnostics dump.
- [ ] Render resource leak report.
- [ ] Frame capture marker naming.
- [ ] Scene visibility stats.
- [ ] Culling/LOD stats.
- [ ] Character/crowd stats.

## Test / Doğrulama

- [ ] Diagnostics JSON schema test.
- [ ] Per-pass timing exists test.
- [ ] Resource count after shutdown = 0 test.
- [ ] Draw count smoke test.
- [ ] RenderGraph dump contains expected passes.
- [ ] GPU timer availability fallback test.
- [ ] Diagnostics disabled build test.
- [ ] Culling/LOD stats present test.

## Done

- [ ] Render performansı ölçülebilir.
- [ ] Pass bazlı maliyet görülebilir.
- [ ] Resource leak tespiti başlamış.
- [ ] MMO visibility/render stats raporlanabilir.

---

# Phase R13B — GPU Capture Tooling / Debug Marker Integration

## Amaç

RenderDoc / PIX / Nsight gibi araçlarda renderer'ın okunabilir ve debug edilebilir hale gelmesi.

## İşler

- [ ] RenderGraph pass debug marker isimleri.
- [ ] Texture/buffer debug names.
- [ ] Pipeline debug names.
- [ ] Frame marker.
- [ ] Zone/terrain/character pass naming convention.
- [ ] RenderDoc capture guide.
- [ ] PIX/Nsight compatibility notes.
- [ ] GPU debug label abstraction.

## Test / Doğrulama

- [ ] RenderGraph pass names GPU capture'da görünüyor.
- [ ] Texture/buffer debug names set ediliyor.
- [ ] Capture smoke document mevcut.
- [ ] Debug markers disabled build mode test.
- [ ] Marker naming deterministic test.

## Done

- [ ] Renderer GPU capture araçlarında okunabilir.
- [ ] RenderGraph pass/resource isimleri debug edilebilir.
- [ ] Capture workflow dokümante edilmiş.

---

# Phase R13C — Render Memory Budget / Residency Diagnostics

## Amaç

VRAM/resource memory yönetimini ölçülebilir hale getirmek.

## Kapsam

- [ ] Per-resource memory accounting.
- [ ] Transient resource memory accounting.
- [ ] Texture pool budget.
- [ ] Buffer pool budget.
- [ ] Frame resource memory budget.
- [ ] RenderGraph transient budget report.
- [ ] Texture streaming residency report.
- [ ] Budget warning thresholds.
- [ ] Memory leak report.

## İşler

- [ ] RenderMemoryReport schema yaz.
- [ ] Texture memory accounting yaz.
- [ ] Buffer memory accounting yaz.
- [ ] Transient resource accounting yaz.
- [ ] Texture pool budget config ekle.
- [ ] Buffer pool budget config ekle.
- [ ] Budget warning diagnostic ekle.
- [ ] Residency report üret.

## Test / Doğrulama

- [ ] Texture memory accounting test.
- [ ] Buffer memory accounting test.
- [ ] Transient memory accounting test.
- [ ] Texture pool budget overflow test.
- [ ] Buffer pool budget overflow test.
- [ ] Residency report schema test.
- [ ] Shutdown memory leak report = 0 test.
- [ ] Vertical slice memory report exists test.

## Done

- [ ] Renderer memory usage ölçülebilir.
- [ ] Texture/buffer/transient bütçeleri raporlanabilir.
- [ ] Large-world texture/resource residency izlenebilir.

---


# Phase R13D — Render Crash / Telemetry Artifact

## Amaç

Sahada oluşan render crash, GPU failure, shader/pipeline hatası veya resource budget sorunlarını analiz edilebilir artifact'e dönüştürmek.

MMO motorunda sadece local debug yeterli değildir. Oyuncu veya test cihazındaki render hatası sonrası anlamlı telemetry snapshot gerekir.

## Kapsam

- [ ] Render crash report.
- [ ] GPU capability snapshot.
- [ ] Last N frames diagnostics.
- [ ] Shader/pipeline cache state.
- [ ] Resource budget snapshot.
- [ ] Active scene/view summary.
- [ ] Backend/device/swapchain state.
- [ ] Recent render graph pass list.
- [ ] Privacy-safe artifact policy.
- [ ] Offline attachment format.

## İşler

- [ ] `RenderTelemetryArtifact` schema yaz.
- [ ] GPU capability snapshot serialize et.
- [ ] Last N frames diagnostics ring buffer yaz.
- [ ] Shader/pipeline cache state summary yaz.
- [ ] Resource budget snapshot yaz.
- [ ] Active view/render graph summary yaz.
- [ ] Device/swapchain state snapshot yaz.
- [ ] Crash/failure artifact writer yaz.
- [ ] Privacy-safe artifact policy dokümante et.

## Test / Doğrulama

- [ ] Telemetry artifact schema test.
- [ ] Forced shader compile failure artifact test.
- [ ] Forced resource budget overflow artifact test.
- [ ] Device init failure artifact test.
- [ ] Last N frames diagnostics retained test.
- [ ] Privacy-safe artifact contains no raw user data test.
- [ ] Artifact file can be parsed by diagnostic tool test.

## Done

- [ ] Render failure sahada analiz edilebilir artifact üretir.
- [ ] GPU/backend/resource/shader state crash sonrası kaybolmaz.
- [ ] MMO/test environment debugging için temel telemetry vardır.

---

# Phase R14 — Render Quality Gates

## Amaç

Render sisteminin değişikliklerle sessizce bozulmasını engellemek.

## Gate türleri

- [ ] Architecture gate.
- [ ] Build gate.
- [ ] Headless gate.
- [ ] Resource lifecycle gate.
- [ ] Binding/descriptor lifecycle gate.
- [ ] RenderGraph validation gate.
- [ ] Shader pipeline gate.
- [ ] Material system gate.
- [ ] Texture streaming gate.
- [ ] Visibility/culling gate.
- [ ] Golden image gate.
- [ ] Performance smoke gate.
- [ ] Diagnostics schema gate.
- [ ] GPU capture marker gate.
- [ ] Memory budget gate.

## Test / Doğrulama

- [ ] Public header vendor leak scan.
- [ ] `Vendor/Diligent` source location scan.
- [ ] Diligent include only private backend scan.
- [ ] Render conformance tests.
- [ ] Golden image tests with tolerance.
- [ ] 1k object perf smoke.
- [ ] 10k culling perf smoke.
- [ ] 100 character perf smoke.
- [ ] Shutdown clean leak test.
- [ ] Headless/server no-render dependency test.
- [ ] Shader/material cook determinism test.
- [ ] Texture streaming residency tests.
- [ ] Memory budget threshold tests.

## Done

- [ ] Render değişiklikleri test gate'e bağlı.
- [ ] Görsel regresyonlar yakalanmaya başlamış.
- [ ] Architecture debt otomatik yakalanıyor.
- [ ] MMO/character/render asset/memory regression'ları gate altında.

---


# Phase R14B — Deterministic Render Test Fixtures

## Amaç

Golden image ve render smoke testlerinin güvenilir olması için deterministik fixture temeli kurmak.

Golden image testleri sadece screenshot almak değildir. Aynı sahne, aynı asset, aynı kamera, aynı zaman ve aynı kalite ayarıyla çalışmalıdır.

## Kapsam

- [ ] Fixed camera.
- [ ] Fixed time.
- [ ] Fixed random seed.
- [ ] Fixed asset versions.
- [ ] Fixed resolution.
- [ ] Fixed quality preset.
- [ ] Fixed lighting setup.
- [ ] Fixed animation frame.
- [ ] Driver/GPU tolerance policy.
- [ ] Golden image baseline metadata.

## İşler

- [ ] Deterministic render fixture schema yaz.
- [ ] Golden image baseline metadata yaz.
- [ ] Fixed camera/time/random seed policy ekle.
- [ ] Fixed quality preset tanımla.
- [ ] Animation frame lock ekle.
- [ ] Golden image tolerance policy yaz.
- [ ] Fixture validator tool ekle.

## Test / Doğrulama

- [ ] Fixed camera test.
- [ ] Fixed time test.
- [ ] Fixed random seed test.
- [ ] Fixed quality preset test.
- [ ] Golden baseline metadata schema test.
- [ ] Fixture validator rejects non-deterministic fixture.
- [ ] Same fixture produces stable image within tolerance.

## Done

- [ ] Golden image testleri rastgele ve güvenilmez değil.
- [ ] Render regression fixture'ları deterministik.
- [ ] Görsel testler driver/GPU farkına karşı toleranslı ama kontrollü.

---

# Phase R15 — AAA-Oriented Vertical Slice

## Amaç

“AAA kafasında render” iddiasını küçük ama gerçek bir sahneyle kanıtlamak.

## Sahne içeriği

- [ ] PBR material'lı birkaç mesh.
- [ ] Directional light + shadow.
- [ ] HDR + tonemapping.
- [ ] Bloom.
- [ ] IBL/reflection placeholder.
- [ ] TAA/upscaling placeholder veya minimal implementation.
- [ ] Terrain chunk.
- [ ] Vegetation instances.
- [ ] HLOD/impostor placeholder.
- [ ] Texture streaming/residency aktif.
- [ ] Occlusion culling aktif veya debug mode'da.
- [ ] 1000+ static renderable object.
- [ ] 100+ animated/skinned character.
- [ ] Transparent/decal/VFX örnekleri.
- [ ] Debug/profiling output.
- [ ] MMO-style zone visibility.

## Not

Bu faz “Unreal seviyesindeyiz” iddiası değildir. Bu fazın amacı şudur:

```text
SagaRenderer artık demo renderer değil.
Diligent üstünde çalışan, RenderGraph/Material/Lighting/PostProcess/Visibility/Character/VFX pipeline'ı olan,
MMO-first ve AAA-oriented bir render vertical slice üretmektedir.
```

## Test / Doğrulama

- [ ] Vertical slice scene açılıyor.
- [ ] Golden image baseline alınmış.
- [ ] Lighting/shadow/postprocess aktif.
- [ ] Reflection/IBL placeholder aktif.
- [ ] TAA/upscaling path aktif veya placeholder doğrulanmış.
- [ ] Culling/LOD aktif.
- [ ] Occlusion debug/visibility path aktif.
- [ ] Terrain/vegetation aktif.
- [ ] Texture streaming/residency raporu aktif.
- [ ] Skinned character aktif.
- [ ] Transparent/VFX/decal aktif.
- [ ] GPU/CPU frame diagnostics üretiliyor.
- [ ] Resource leak yok.
- [ ] 1k+ object perf smoke geçiyor.
- [ ] 100+ character perf smoke geçiyor.
- [ ] Resize/shutdown clean.
- [ ] Public API vendor-neutral kalıyor.
- [ ] Headless/server build etkilenmiyor.

## Done

- [ ] Renderer “AAA-oriented foundation” olarak sınıflandırılabilir.
- [ ] Diligent Saga graphics core foundation olarak çalışıyor.
- [ ] RenderGraph, Material, Lighting, Post-process, Visibility, Character, VFX sistemleri gerçek sahnede beraber çalışıyor.
- [ ] Large-world/memory/visibility riskleri temel seviyede ele alınmış.

---

# Phase R15B — Performance Budget Certification

## Amaç

Vertical slice'ın “çalışıyor” demekle kalmayıp ölçülebilir bütçelere bağlanması.

## İlk hedef bütçeler

Bu değerler ilk hedef olarak kabul edilir; donanım sınıfına göre ayrıca güncellenebilir.

```text
Resolution: 1920x1080
Target FPS: 60
Frame budget: 16.6 ms

Render Thread CPU:
  hedef <= 4 ms
  warning <= 6 ms

GPU Frame:
  hedef <= 16.6 ms
  warning <= 22 ms

Scene:
  1000+ static renderable
  100+ animated character
  terrain + vegetation + shadow + post-process aktif

Memory:
  VRAM report zorunlu
  resource leak = 0
  texture pool warning threshold tanımlı
```

## İşler

- [ ] Baseline hardware tanımı.
- [ ] FPS/frame budget belgesi.
- [ ] Render thread CPU budget.
- [ ] GPU pass budget.
- [ ] Draw/triangle/material/texture count budget.
- [ ] VRAM budget report.
- [ ] Texture pool budget.
- [ ] Buffer/transient budget.
- [ ] Slow pass diagnostics.
- [ ] Performance regression warning threshold.

## Test / Doğrulama

- [ ] Vertical slice perf report üretiliyor.
- [ ] GPU frame time raporlanıyor.
- [ ] Render thread CPU time raporlanıyor.
- [ ] Draw count / triangle count / material count raporlanıyor.
- [ ] VRAM report mevcut.
- [ ] Texture pool report mevcut.
- [ ] Slow pass warning çalışıyor.
- [ ] Performance smoke gate threshold uyguluyor.

## Done

- [ ] Renderer performance bütçeleri yazılı.
- [ ] Vertical slice ölçülebilir.
- [ ] “AAA-oriented” iddiası sadece görsel değil, performans raporuyla destekleniyor.

---


# Phase R15C — Render Scalability / Quality Presets

## Amaç

MMO oyuncu donanım çeşitliliği için render kalite seviyelerini tanımlamak.

AAA-oriented renderer tek kalite seviyesinde çalışmamalıdır. Low/Medium/High/Ultra gibi presetler; shadow, texture, post-process, vegetation, character crowd ve resolution scale gibi sistemlere bağlanmalıdır.

## Kapsam

- [ ] Low preset.
- [ ] Medium preset.
- [ ] High preset.
- [ ] Ultra preset.
- [ ] Shadow quality tiers.
- [ ] Texture quality tiers.
- [ ] Post-process quality tiers.
- [ ] Vegetation density budget.
- [ ] Character crowd budget.
- [ ] Resolution scale policy.
- [ ] Dynamic resolution compatibility.
- [ ] Preset diagnostics.

## İşler

- [ ] `RenderQualityPreset` enum/tanım yaz.
- [ ] Shadow quality tier mapping yaz.
- [ ] Texture streaming budget per preset yaz.
- [ ] Post-process feature flags per preset yaz.
- [ ] Vegetation density budget per preset yaz.
- [ ] Character/crowd budget per preset yaz.
- [ ] Resolution scale policy yaz.
- [ ] Preset validation diagnostics ekle.
- [ ] Vertical slice preset testleri ekle.

## Test / Doğrulama

- [ ] Low preset vertical slice opens.
- [ ] Medium preset vertical slice opens.
- [ ] High preset vertical slice opens.
- [ ] Ultra preset vertical slice opens.
- [ ] Preset changes produce expected diagnostics.
- [ ] Invalid preset rejected.
- [ ] Texture/shadow/post-process budgets change by preset.
- [ ] Character/vegetation budgets change by preset.
- [ ] Dynamic resolution works with preset.

## Done

- [ ] Renderer tek kalite seviyesine kilitli değil.
- [ ] MMO oyuncu donanım çeşitliliği için scalability foundation oluşmuş.
- [ ] Performance budget presetlere bağlanmış.

---

# Phase R16 — Render Maturity Closure Checkpoint

## Amaç

Renderer'ın hangi seviyeye geldiğini dürüstçe sınıflandırmak.

## Test / Doğrulama

- [ ] R0-R15B phase checklistleri tamamlanmış.
- [ ] Render stack karar dokümanı güncel.
- [ ] Diligent vendored/forked durumda.
- [ ] Public API vendor-neutral.
- [ ] Vertical slice golden image baseline mevcut.
- [ ] Diagnostics raporu mevcut.
- [ ] Performance budget report mevcut.
- [ ] Memory budget report mevcut.
- [ ] Device loss/swapchain recovery tests mevcut.
- [ ] Deterministic render fixtures mevcut.
- [ ] Scalability preset tests mevcut.
- [ ] Backend capability matrix mevcut.
- [ ] Shader/material hot reload tests mevcut.
- [ ] Material graph authoring contract mevcut.
- [ ] Camera/view family tests mevcut.
- [ ] Render interest/world streaming contract tests mevcut.
- [ ] Avatar/equipment rendering tests mevcut.
- [ ] Sky/weather/water strategy tests mevcut.
- [ ] Render crash/telemetry artifact tests mevcut.
- [ ] Headless/server dependency check geçiyor.
- [ ] Açık debt listesi yazılmış.
- [ ] Overclaim yok: “Production AAA renderer” denmemiş.
- [ ] Sonraki roadmap için Level 6 debt listesi çıkarılmış.

## Done

- [ ] Renderer maturity seviyesi belgelenmiş.
- [ ] Açık borçlar dürüstçe yazılmış.
- [ ] Sonraki roadmap için net karar kapısı oluşmuş.

---

## 10. Her Faz İçin Ortak Kalite Kapıları

Her render phase sonunda mümkünse şunlar çalıştırılmalı:

```text
[ ] git diff --check
[ ] build: SagaEngine
[ ] build: SagaRenderer / Graphics tests
[ ] test: Architecture vendor leak tests
[ ] test: RenderGraph validation tests
[ ] test: Resource lifecycle tests
[ ] test: Binding/descriptor lifecycle tests
[ ] test: Shader pipeline tests
[ ] test: Material system tests
[ ] test: Render asset cooking tests
[ ] test: Texture streaming/residency tests
[ ] test: Mesh/geometry streaming tests
[ ] test: Shader variant budget tests
[ ] test: Visibility/culling tests
[ ] test: Occlusion culling tests
[ ] test: Character renderer tests
[ ] test: Scalability preset tests
[ ] test: deterministic render fixture tests
[ ] test: backend capability matrix tests
[ ] test: shader/material hot reload tests
[ ] test: material graph contract tests
[ ] test: camera/view family tests
[ ] test: render interest/world streaming contract tests
[ ] test: avatar/equipment rendering tests
[ ] test: sky/weather/water strategy tests
[ ] test: render crash/telemetry artifact tests
[ ] test: focused render smoke tests
[ ] test: headless/server no-render dependency
[ ] test: golden image tests where applicable
[ ] test: performance smoke tests where applicable
[ ] test: memory budget tests where applicable
[ ] scan: Engine/Public içinde Diligent/Vulkan/D3D/Metal/TheForge token yok
[ ] scan: Diligent include sadece private backend altında
[ ] docs: touched docs whitespace clean
```

---

## 11. Roadmap Sonunda Gerçek Durum

Bu roadmap tamamlandığında hedeflenen dürüst durum:

```text
Diligent:
  - Conan remote dependency değil
  - Vendor/Diligent altında kontrol edilen graphics core
  - Saga-specific patch geçmişi belgeli

SagaGraphics:
  - Vendor-neutral public API
  - Resource handle sistemi
  - Binding/frame resource sistemi
  - Shader/material/pipeline desc sistemi
  - Backend leak guard testleri

SagaRenderer:
  - Build/CMake target boundary korunuyor
  - Public/install surface Diligent sızıntısından korunuyor
  - Backend capability matrix raporlanıyor
  - RenderGraph üzerinden frame çalıştırıyor
  - Render thread / snapshot ownership modeli var
  - Camera/view family ve multi-view foundation var
  - Scene extraction yapıyor
  - PBR material çiziyor
  - Lighting/shadow/postprocess çalıştırıyor
  - Reflection/IBL/GI stratejisi var
  - TAA/upscaling foundation var
  - Culling/LOD/visibility yapıyor
  - Occlusion culling foundation var
  - GPU-driven/indirect draw strategy belgeli
  - Terrain/vegetation/HLOD temeli var
  - Texture streaming/residency temeli var
  - Mesh/geometry streaming stratejisi var
  - Shader variant budget/permutation kontrolü var
  - Shader/material hot reload development loop var
  - Material graph/authoring contract var
  - Render interest/world streaming contract var
  - Character/skinned mesh rendering temeli var
  - MMO avatar/equipment rendering foundation var
  - Sky/atmosphere/weather foundation var
  - Water/ocean/river rendering strategy var
  - Transparency/decal/VFX temeli var
  - MMO world rendering için temel taşıyor
  - Diagnostics, GPU capture marker ve golden image testleri var
  - Performance budget raporu var
  - Memory budget raporu var
  - Device loss/swapchain recovery path'i var
  - Deterministic render fixture sistemi var
  - Render scalability/quality presetleri var
  - Render crash/telemetry artifact sistemi var

Server/headless:
  - Renderer'a bağımlı değil
```

---

## 12. Sert Gerçek

Bu roadmap sonunda bile şu iddia yapılmayacak:

```text
SagaRenderer artık Unreal seviyesinde.
SagaEngine production AAA renderer'a sahip.
```

Yapılabilecek dürüst iddia:

```text
SagaRenderer artık demo renderer değil.
SagaEngine, Diligent tabanlı vendored graphics core üzerinde çalışan,
vendor-neutral public API'ye sahip,
RenderGraph/Material/Lighting/PostProcess/Visibility/Character/VFX sistemleri olan,
MMO-first ve AAA-oriented renderer foundation'a sahiptir.
```

---

## 13. Son Karar

```text
Diligent'i atılacak dependency gibi değil,
SagaEngine'in vendored uzun vadeli graphics core temeli gibi kullanıyoruz.

The Forge'u ana üretim dependency yapmıyoruz.
The Forge'dan AAA teknikleri, visibility buffer yaklaşımı, shader/pipeline fikirleri
ve benchmark gözlemleri öğreniyoruz.

Native Vulkan'a bugün girmiyoruz.
Önce SagaGraphics ve SagaRenderer'ı sağlamlaştırıyoruz.

UI bu roadmap'in dışında.
```

Bu planın amacı render'ı “dependency seçimi” olmaktan çıkarıp gerçek engine ownership seviyesine taşımaktır.
