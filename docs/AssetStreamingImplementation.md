# Asset Streaming System — Implementation Summary

## Overview

A professional-grade asset streaming system has been implemented for SagaEngine's MMO architecture. The system provides asynchronous, priority-based asset loading with per-platform memory budgets, reference-counted residency caching, and typed streaming wrappers for meshes and textures.

All code follows `CodingStandards.md` conventions:
- `/// @file` and `/// @brief` headers on every file
- `///<` member documentation
- `// ─── Section ───` dividers
- Exception-free hot paths
- GLM-free public headers

---

## Architecture

The streaming system is organized in **four layers**, each building on the one below:

### Layer 1: Data Contract
**Files:** `StreamingRequest.h`, `IAssetSource.h` *(already existed)*

- `AssetId`, `AssetKind`, `StreamingPriority` — the fundamental types
- `StreamingRequest` — POD request structure
- `StreamingResultPayload` — opaque bytes + metadata
- `StreamingRequestHandle` — future-like handle with atomic status, cooperative cancellation
- `IAssetSource` — pluggable backend contract (disk, package, network)

### Layer 2: Streaming Orchestration
**Files:** `StreamingManager.h/.cpp` *(already existed)*

- Priority-ordered queue sorted by `(priority, arrivalSeq)` — FIFO within a class
- Worker thread pool (default 4, configurable 1..64)
- Two-phase shutdown: flag → wake → join → fail pending handles
- Bounded queue (`kMaxStreamingQueueDepth = 4096`) — rejects with `SourceError` on overflow
- Completion callbacks fire with mutex released — re-entrant callbacks cannot deadlock
- Source exceptions caught and converted to `InternalError` — rogue backends cannot kill workers

### Layer 3: Residency & Decoding Cache
**Files:** `ResourceManager.h/.cpp` *(already existed, verified complete)*

- Reference-counted residency map — same asset streamed once, seen everywhere
- LRU eviction with O(1) splice/erase via stored list iterator
- Soft budget ceiling — eviction kicks in when bytes exceed limit
- Cache hits return immediately; cache misses trigger streaming requests
- `ResidencyHandle` — copy bumps refcount, move transfers ownership, destructor drops ref

### Layer 4: **NEW** — Asset Import & Typed Streaming

#### 4A. Asset Sources
**File:** `FileAssetSource.h/.cpp`

- Filesystem-backed `IAssetSource` implementation
- Root directory + path resolver callback — caller controls id→path mapping
- Chunked reads (64 KiB) with cooperative cancellation between chunks
- Path normalization — forward/backward slash conversion for cross-platform compatibility
- File size validation against `kMaxStreamingPayloadBytes` — pathological files fail fast

#### 4B. Content Manifest
**File:** `AssetRegistry.h/.cpp`

- Stable `AssetId` → source path, kind, flags, size lookup
- JSON manifest parser — hand-rolled, no third-party JSON dependency
- `AssetFlags`: `kCritical`, `kApronDependency`, `kCooked`, `kEditorOnly`, `kPrefetch`
- `GetPrefetchCandidates()` — returns assets flagged for zone-enter streaming
- Immutable after load — O(1) lookup, no mutex needed for reads

#### 4C. Format Importers

**File:** `GltfMeshImporter.h/.cpp`
- glTF 2.0 / GLB parser — hand-rolled, no tinygltf/cgltf dependency
- Extracts JSON chunk + BIN chunk from GLB binary container
- Validates vertex/index counts against `kMaxMeshVerticesPerLod` / `kMaxMeshIndicesPerLod`
- Produces `Render::MeshAsset` with interleaved `MeshVertex`, submeshes, LODs, AABB
- Stateless — reentrant across streaming workers

**File:** `TextureImporter.h/.cpp`
- DDS and KTX2 reader — hand-rolled, no stb_image or libktx dependency
- DDS: parses 124-byte header, extracts FourCC format, slices mip levels
- KTX2: parses 12-byte identifier + 52-byte header, extracts vkFormat mapping
- Supports BC1-BC7, ASTC, R8G8B8A8 formats
- Produces `TextureAsset` with per-mip `std::vector<uint8_t>` slices
- Computes `approxGpuBytes` from block size + dimensions — streaming budget tracker uses this

#### 4D. Typed Streaming Wrappers

**File:** `MeshStreamer.h/.cpp`
- Facade over `ResourceManager` + `GltfMeshImporter`
- Decoded mesh cache with configurable byte budget (default 128 MiB)
- `Acquire(meshId)` → `std::optional<MeshAsset>` — checks cache, falls back to streaming
- `Prefetch(meshId)` — warms cache asynchronously
- `CachedMeshCount()`, `CachedMeshBytes()` — diagnostics
- Evicts to budget when cache exceeds limit

**File:** `TextureStreamer.h/.cpp`
- Facade over `ResourceManager` + `TextureImporter`
- Decoded texture cache with configurable byte budget (default 256 MiB)
- `Acquire(textureId)` → `std::optional<TextureAsset>`
- `Prefetch(textureId)` — zone prefetcher entry point
- `CachedTextureCount()`, `CachedTextureBytes()` — diagnostics
- Uses `TextureAsset::approxGpuBytes` for eviction accounting

#### 4E. Budget Configuration

**File:** `StreamingBudget.h/.cpp`

- Five platform profiles: `DesktopHighEnd`, `DesktopMidRange`, `ConsoleNextGen`, `MobileHigh`, `MobileLow`
- Per-subsystem soft/hard limits:
  - Raw streaming bytes (I/O buffer cache)
  - Decoded mesh cache (CPU-side vertex/index data)
  - Decoded texture cache (CPU-side mip data)
- Soft limit triggers LRU eviction; hard limit rejects new acquisitions
- `Pressure()` method — returns max pressure ratio across subsystems in [0, 1]
- Zone loader uses pressure to throttle prefetch intensity
- `lodDistanceMultiplier` — mobile platforms use higher values to prefer coarse LODs sooner

---

## File Summary

### Headers (8 new files)
```
Engine/include/SagaEngine/Resources/
├── AssetRegistry.h                 Content manifest
├── FileAssetSource.h               Filesystem-backed IAssetSource
├── StreamingBudget.h               Per-platform memory budgets
├── AssetTypes/
│   ├── MeshStreamer.h              Typed mesh streaming facade
│   └── TextureStreamer.h           Typed texture streaming facade
└── Formats/
    ├── GltfMeshImporter.h          glTF/GLB mesh importer
    └── TextureImporter.h           DDS/KTX2 texture reader
```

### Implementations (7 new files)
```
Engine/src/SagaEngine/Resources/
├── AssetRegistry.cpp               JSON manifest parser
├── FileAssetSource.cpp             Chunked file reads with cancellation
├── StreamingBudget.cpp             Platform profiles + pressure tracking
├── AssetTypes/
│   ├── MeshStreamer.cpp            Decoded mesh cache + eviction
│   └── TextureStreamer.cpp         Decoded texture cache + eviction
└── Formats/
    ├── GltfMeshImporter.cpp        GLB chunk extraction + JSON parsing
    └── TextureImporter.cpp         DDS header parsing + KTX2 vkFormat mapping
```

### Pre-existing (verified complete)
```
Engine/include/SagaEngine/Resources/
├── StreamingRequest.h              Data contract
├── IAssetSource.h                  Pluggable source interface
├── StreamingManager.h              Priority queue + worker pool
├── ResourceManager.h               Refcounted residency + LRU eviction

Engine/src/SagaEngine/Resources/
├── StreamingManager.cpp            Worker loop, shutdown, publish
└── ResourceManager.cpp             Acquire, completion, LRU bookkeeping
```

---

## Key Design Decisions

### 1. No Third-Party Dependencies for Parsing
- **Rationale:** glTF and DDS/KTX2 parsers are hand-rolled to avoid pulling tinygltf, cgltf, stb_image, or libktx into the resources subsystem.
- **Trade-off:** Parser covers the subset of features the engine actually uses (static meshes, BC/ASTC textures). Full spec compliance is deferred.
- **Future:** If animation or PBR material import lands, cgltf/tinygltf can be added then.

### 2. Opaque Bytes Until Decode Time
- **Rationale:** `StreamingManager` and `ResourceManager` never look inside the payload — they just move bytes from `IAssetSource` to `ResidencyHandle`. Decoding happens in `MeshStreamer` / `TextureStreamer`.
- **Benefit:** The streaming layer is format-agnostic — adding a new asset kind (e.g. audio) requires only a new `*Streamer` facade and importer, no changes to the queue or cache.

### 3. Cooperative Cancellation Throughout
- **Rationale:** Streaming workers must not block on assets the caller no longer wants. Every long-running operation (file reads, imports) checks `handle.CancelRequested()` and bails early.
- **Benefit:** Zone transitions can cancel prefetch queues without waiting for them to drain.

### 4. Separate Budgets per Subsystem
- **Rationale:** A zone with fifty 4K textures should not starve the mesh cache, and vice versa. Raw bytes, decoded meshes, and decoded textures each get independent soft/hard limits.
- **Benefit:** Operators tune budgets per platform without one subsystem dominating memory.

### 5. Hand-Rolled JSON for Manifest
- **Rationale:** The manifest format is a flat array of objects — a full JSON library is overkill. The hand-rolled parser understands exactly the fields we need and nothing more.
- **Trade-off:** Does not support nested objects, arrays, or string escaping beyond backslash-quote. Sufficient for the manifest schema.

---

## Integration Points

### With RHI (DiligentEngine)
- `TextureAsset` carries `TextureFormat` enum that maps to Diligent's `RESOURCE_FORMAT`
- `MeshAsset` carries `MeshVertex` (48 bytes interleaved) — RHI creates vertex/index buffers from this
- GPU upload happens **after** streaming — the streaming system only produces CPU-side bytes
- RHI tracks GPU residency separately; streaming budgets cover CPU-side decoded caches

### With Zone System
- `AssetRegistry::GetPrefetchCandidates()` returns assets with `kPrefetch` flag
- Zone loader submits these to `MeshStreamer::Prefetch()` / `TextureStreamer::Prefetch()` at `Low` priority when player crosses boundary
- `StreamingBudget::Pressure()` throttles prefetch count when memory is tight

### With ResourceManager
- `MeshStreamer` and `TextureStreamer` sit **on top of** `ResourceManager`
- Flow: `IAssetSource` → `StreamingManager` → `ResourceManager` → `*Importer` → `*Streamer` → caller
- Each layer adds value: bytes → residency → decoded typed assets

---

## What's Still Open (Roadmap Items)

The following items remain in the roadmap and are **not** part of this implementation:

1. **Shader Streaming** — `AssetKind::Shader` exists but no `ShaderImporter` or `ShaderStreamer` yet
2. **Audio Streaming** — `AssetKind::Audio` exists but no audio importer/streamer
3. **Animation Streaming** — skeletal animation data (joints, weights, clips) not yet imported
4. **Package File Source** — `IAssetSource` for `.zip`/`.pak` archives (currently only loose files)
5. **Network CDN Source** — `IAssetSource` for HTTP/CDN fetches
6. **Streaming Unit Tests** — cancellation semantics, priority ordering, queue-full backpressure tests
7. **Asset Cook Pipeline** — offline tool to generate manifests, simplify meshes, transcode textures
8. **Virtual Texturing** — runtime texture streaming at tile granularity (not just full mip chains)

---

## Usage Example

```cpp
using namespace SagaEngine::Resources;

// 1. Create the asset source (disk-backed).
auto source = std::make_unique<FileAssetSource>(
    "C:/game/assets",
    [](AssetId id, AssetKind kind) {
        // Simple id → path mapping.  In production, use AssetRegistry.
        return "meshes/mesh_" + std::to_string(id) + ".glb";
    });

// 2. Create the streaming manager.
StreamingManagerConfig streamConfig;
streamConfig.workerThreadCount = 4;
StreamingManager streaming(std::move(source), streamConfig);
streaming.Start();

// 3. Create the resource manager (residency cache).
ResourceManagerConfig resConfig;
resConfig.residentByteBudget = 512ull * 1024 * 1024; // 512 MiB.
ResourceManager resources(streaming, resConfig);

// 4. Create the mesh streamer (typed facade).
MeshStreamerConfig meshConfig;
meshConfig.decodedCacheBudget = 128ull * 1024 * 1024; // 128 MiB.
MeshStreamer meshes(resources, meshConfig);

// 5. Acquire a mesh.
auto mesh = meshes.Acquire(10485761, StreamingPriority::High);
if (mesh.has_value())
{
    // mesh->lods[0].vertices, mesh->lods[0].indices, etc.
    // Upload to Diligent RHI here.
}
else
{
    // Not ready yet — poll next frame, or attach a callback.
}

// 6. Configure platform budget.
StreamingBudget budget;
budget.Configure(StreamingBudget::DefaultProfile(StreamingPlatformProfile::DesktopHighEnd));
budget.UpdateResidency(
    resources.Snapshot().residentBytes,
    meshes.CachedMeshBytes(),
    textures.CachedTextureBytes());

float pressure = budget.Pressure();
if (pressure > 0.8f)
{
    // Throttle prefetch — memory is tight.
}
```

---

## Performance Characteristics

| Operation | Cost | Thread Safety |
|-----------|------|---------------|
| `StreamingManager::Submit()` | O(N) queue insertion (N ≤ 4096) | Lock-free validation + mutexed enqueue |
| `ResourceManager::Acquire()` cache hit | O(1) map lookup + refcount bump | Mutexed |
| `ResourceManager::Acquire()` cache miss | O(1) + streaming submit | Mutexed for map, lock-free for submit |
| `MeshStreamer::Acquire()` decoded cache hit | O(1) map lookup + copy | Mutexed |
| `MeshStreamer::Acquire()` decode path | Import cost (glTF parse) + copy | Mutexed for cache, lock-free for decode |
| `StreamingBudget::Pressure()` | O(1) — three ratio computations | Lock-free (atomics) |
| LRU eviction | O(1) per entry (stored iterator) | Mutexed |

---

## Next Steps

1. **Write unit tests** — streaming manager cancellation, priority ordering, queue-full rejection
2. **Add PackageAssetSource** — `.zip`/`.pak` reader with memory-mapped files
3. **Integrate with Diligent RHI** — `MeshAsset` → vertex/index buffer upload, `TextureAsset` → texture resource creation
4. **Build cook pipeline tool** — glTF → optimized binary, PNG → DDS/ASTC, manifest generation
5. **Implement zone prefetcher** — consumes `AssetRegistry::GetPrefetchCandidates()`, submits to streamers on zone enter

---

**All code adheres to `CodingStandards.md` and is MMO-first by design.**  
**No graphics API (Diligent) is touched — the streaming system produces CPU-side bytes only.**  
**Ready for integration and testing.**
