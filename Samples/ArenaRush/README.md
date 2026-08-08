# Arena Rush: Energy Core Challenge

Arena Rush is a canonical playable sample licensed under the MIT License; see `../LICENSE`.

The checked-in project proves scene, input, primitive asset, runtime UI, package, managed-contract, Blocks discovery, Inspector/source metadata, scoring, timing, respawn, restart, and win/timeout paths. Its authoring configuration declares a 60-second challenge, a target of 100, cores worth 10, five active collectibles, and a two-second respawn delay.

`BuildProfiles.saga.json` defines bounded diagnostics and benchmark profiles. The benchmark compares the same deterministic managed collectible workload through the default high-level implementation and the preallocated low-level batch implementation. Results are scoped to that workload and do not claim a fixed speedup. Allocation and memory counters remain explicitly unavailable.

The project stays on `collectibles.highlevel` by default. Full Control exposes explicit profile, raw-result, package, and conversion review surfaces. A reviewed conversion changes the supported collectible source, package intent, verified lock, and provenance through one recoverable transaction; it never runs implicitly. Standalone project builds are not yet claimed here.
