---
title: UI and audio contracts
status: bounded
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# UI and audio contracts

Runtime UI and Audio are domain modules with backend-neutral contracts. UI turns logical resources and input events into screen state and named actions. Audio turns gameplay/audio events into queued backend work. Neither domain should expose its concrete vendor as the application contract.

## UI pipeline

The current UI surface separates resource resolution, backend document work, screen lifecycle, event collection, input capture, action dispatch, and managed action handling:

```text
logical UI resource
  -> resource provider
  -> backend document handle
  -> screen manager state
  -> UI event queue
  -> action dispatcher
  -> named native or C# handler
```

`IUiResourceProvider` resolves logical document/style identifiers from content or package mounts. Logical identifiers keep callers independent of an unpacked directory layout. Missing identifiers, duplicate mounts, and invalid resource identities must remain diagnosable.

`IUiScreenManager` owns registered, loaded, visible, hidden, and unloaded screen state. Registration is not loading, and loading is not showing. Focused tests cover idempotent load behavior, preload policy, deterministic transitions, backend failure, and snapshots of the current states.

## Events, input capture, and actions

`IUiEventQueue` records bounded UI events in deterministic queue order. Events preserve screen, document, element, focus, and payload context where relevant. The queue does not execute gameplay or scripts by itself.

`IUiInputRouter` reports capture state for mouse, keyboard, text input, and focused surfaces. Capture filters which categories may continue into gameplay; it does not erase the raw device observation. Reset and focus transitions must be explicit so a destroyed screen does not leave gameplay input permanently blocked.

`IUiActionDispatcher` maps UI events to actions such as showing, hiding, or toggling a screen and emitting a named action. `IUiNamedActionHandler` resolves that semantic name to native behavior; `CSharpUiNamedActionHandler` is the controlled adapter into Runtime Scripting. Missing bindings and handlers produce deterministic diagnostics instead of accidentally invoking a similarly named method.

`RuntimeUiBootstrapper` coordinates the bounded startup of a configured screen. Its tests prove disabled startup, missing documents, missing backend, and one connect-screen path. They do not establish a complete menu or HUD product.

## UI backend boundary

`IUiBackend` is the vendor-neutral document/render boundary. A null implementation supports focused tests and headless behavior. RmlUi is the current concrete integration.

`RmlUiUiBackend.h` and its implementation live under UI `Private/Backends`. Public UI contracts expose engine-owned types and failure information without RmlUi types, and architecture checks protect that vendor boundary. A private adapter test does not make RmlUi classes part of the installed SagaEngine API.

The UI backend may translate logical documents and input to the vendor. Render owns graphics submission and native graphics lifecycle. Input owns device collection. Scripting owns managed invocation. UI coordinates those ports without taking over their implementations.

## Audio event path

The Audio public surface includes `AudioEngine`, `AudioEvent`, `AudioEventMapper`, `AudioQueue`, filters, shared audio types, and `IAudioBackend`.

An audio event is semantic intent: play, stop, update, or another declared operation against an audio identity. The mapper converts domain/event identity into backend-ready work. The queue decouples producers from backend processing and provides a location for ordering and capacity policy. Filters shape which events proceed without mutating their source owner.

`IAudioBackend` owns the narrow interaction with the selected audio implementation. `AudioEngine` coordinates the event path and backend lifetime; it should not expose vendor objects to gameplay callers. Asset decoding, resource streaming, and package identity remain Assets/Resources responsibilities even when Audio consumes their results.

## Threading and lifetime

UI resources and backend document handles have explicit screen-manager lifetime. Callbacks must not retain a screen, script instance, or UI element after unload without an owning handle protocol. Event queues should be drained at a defined point so the same event is not dispatched twice.

Audio queues need bounded capacity and observable rejection/backpressure. Shutdown stops new submissions, drains or cancels according to policy, releases voices/resources, and destroys the backend last. A queued event is not proof that a sound played or reached an output device.

## Capability limits

Current UI tests establish logical resource resolution, screen transitions, event ordering, capture behavior, action routing, named C# invocation, and null/RmlUi failure boundaries. They do not prove accessibility completeness, localization, arbitrary RmlUi document compatibility, a finished game UI, or a stable UI extension SDK.

Current Audio code and Sandbox scenarios establish engine/event/backend foundations. They do not prove platform-wide device coverage, spatial-audio fidelity, streaming codecs, mixing quality, or production audio tooling. Manual audible output requires device/environment evidence distinct from a queue or backend unit result.
