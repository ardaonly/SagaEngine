# Arena Rush: Energy Core Challenge

Arena Rush is a canonical playable sample licensed under the MIT License; see `../LICENSE`.

The checked-in project proves scene, input, primitive asset, runtime UI, package, managed-contract, Blocks discovery, Inspector/source metadata, scoring, timing, respawn, restart, and win/timeout paths. Its authoring configuration declares a 60-second challenge, a target of 100, cores worth 10, five active collectibles, and a two-second respawn delay.

`BuildProfiles.saga.json` defines bounded diagnostics and benchmark profiles. The benchmark compares the same deterministic managed collectible workload through the default high-level implementation and the preallocated low-level batch implementation. Results are scoped to that workload and do not claim a fixed speedup. Allocation and memory counters remain explicitly unavailable.

The project stays on `collectibles.highlevel` by default. Full Control exposes explicit profile, raw-result, package, conversion review, and standalone build surfaces. A reviewed conversion changes the supported collectible source, package intent, verified lock, and provenance through one recoverable transaction; it never runs implicitly.

Standalone builds require saved documents and a validated benchmark profile. `SagaStandaloneBuild` and the Editor use the same snapshot, staging, verification, and publication coordinator. The build records project, lock, package generation, profile, tool, runtime, source inventory, and artifact hashes. It compiles only a materialized snapshot, publishes through an owned journal, and preserves the previous valid output on failure or cancellation. The standalone manifest separates its stable content identity from publication-time metadata. This is a bounded local project-build capability, not a claim of cross-platform distribution, signing, installer generation, or byte-identical output across different machines and toolchains.
