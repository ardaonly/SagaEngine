---
title: Input and platform contracts
status: bounded
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Input and platform contracts

The Input module owns device state, action mapping, input commands, and the narrow platform abstractions currently used by runtime programs. Concrete SDL window and input behavior is an implementation selected behind those contracts. Input collection, gameplay intent, prediction, and authoritative processing are related stages, not one mutable global state.

## From devices to gameplay intent

The current public surface separates the pipeline:

1. A platform backend observes native events and updates registered devices.
2. Keyboard, mouse, and gamepad devices contribute a `RawInputFrame`.
3. An `InputActionMap` interprets raw controls as named actions.
4. A `GameplayInputFrame` records the bounded intent used by gameplay.
5. An `InputCommand` and serializer provide a transportable representation where a networked workflow needs one.
6. Client prediction may apply local commands while authoritative processing validates and orders received commands.

This separation matters for replay and evidence. Native key codes and window events are not a stable gameplay protocol. A gameplay action can be fed by a keyboard, deterministic synthetic input, replay data, or a focused test without changing the simulation command contract.

## Device and frame ownership

`DeviceRegistry` owns the set of active input devices; `InputManager` coordinates per-frame collection and exposes input state. A device has an explicit type and lifetime. Disconnect, reconnect, focus loss, and the absence of input must not be inferred from an arbitrary zeroed memory block.

A raw frame records what the input layer observed during a bounded update. A gameplay frame records interpreted intent. Consumers should not retain mutable references into the device backend after the frame boundary. If a subsystem needs history, it owns a copy or a documented ring buffer.

Action maps turn physical controls into semantic names. Duplicate or missing mappings and unsupported input values should produce deterministic outcomes. UI capture is a separate decision described in [UI and audio contracts](ui-and-audio-contracts.md): it can block selected gameplay categories without rewriting device truth.

## Commands and serialization

`InputCommandBuffer` orders commands for simulation or transport. `InputCommandSerializer` converts the command vocabulary to and from bytes. Serialization must validate sizes, identifiers, sequence/tick values, and malformed input before a command reaches gameplay state.

The serialized command is an input request, not proof that the requested action occurred. Server or authority code decides whether it is timely, allowed, and applicable. A client-supplied position or success flag must not become authoritative merely because it was carried in an input packet.

`InputCommandInbox`, `InputPacketHandler`, and `ServerInputProcessor` currently connect input to networking/authority workflows. They belong to Input because they process the command vocabulary, but transport connection ownership belongs to Networking and server decisions belong to ServerAuthority/Simulation. This is a boundary to review if those types begin to absorb connection or world ownership.

## Prediction

`ClientPrediction` can apply local intent before an authoritative result arrives. Prediction is derived client state. It needs sequence/tick identity, retained commands, reconciliation, and a bounded correction policy. Prediction must not mutate server truth or suppress a detected mismatch.

Focused replication and StarterArena evidence can show that one command path is deterministic or reconciles in a fixture. It does not prove production latency handling, anti-cheat protection, packet security, or behavior under every loss/reorder condition.

## Platform abstraction

The same module currently exposes `IWindow`, `IInputDevice`, `IFileSystem`, `IHighPrecisionTimer`, `IDebugRenderer2D`, and `PlatformFactory`. These interfaces give programs a vendor-neutral construction and usage boundary. They do not make Input the owner of general asset I/O, rendering, or diagnostics.

`IFileSystem` at this layer supports platform access required by the host. Asset identity, VFS policy, package mounts, and resource streaming remain Assets/Resources concerns. `IDebugRenderer2D` is a narrow diagnostic/platform port, not an alternate Render API.

## SDL boundary

SDL implementation files live under the Input private tree and the build exposes them through the platform target selected by composition. Runtime and editor public contracts should not require SDL event, window, renderer, or controller types. A program may select the SDL implementation without acquiring ownership of it.

When adding a platform feature:

- add or extend a vendor-neutral contract only if more than the SDL implementation needs it;
- keep SDL headers and event translation private;
- convert native values at the adapter boundary;
- preserve a testable null, synthetic, or interface-driven path where practical;
- do not expose a native handle unless its ownership and lifetime are explicit.

## Evidence limits

StarterArena includes deterministic synthetic-input and explicit keyboard workflows. Automated evidence does not claim a physical key press unless a manual report observes a real device and a resulting state change. Headless tests can prove mapping, serialization, buffering, and command behavior without proving native window focus or device hot-plug behavior.

The current Wiki therefore treats input as implemented foundations with bounded sample paths, not as a finished cross-platform input product or production network-input security layer.

