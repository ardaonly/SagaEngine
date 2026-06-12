# Saga Gameplay Node Library V1 Policy

Status: Gameplay node vocabulary policy.

This document defines the only gameplay node names that 75 may treat
as accepted policy inputs. It does not implement node extraction, runtime node
APIs, gameplay systems, or editor graph behavior.

## Policy Purpose

75 needs a bounded node vocabulary before implementation starts.
Without this boundary, metadata can accidentally imply inventory, audio, ECS,
spawn, or timer systems that are not proven by runtime evidence.

The accepted policy is:

- node metadata visibility is not proof of runtime behavior;
- generated `node_metadata.json` is descriptive evidence only until matched by
  compile, binding, and runtime proof;
- nodes must be classified honestly as `RuntimeBacked`, `ProjectionOnly`, or
  `Deferred`;
- unsupported or placeholder behavior must be reported explicitly.

## Classification Meanings

`RuntimeBacked` means there is direct evidence that a node can bind to compiled
C# or native runtime behavior in the current repository.

`ProjectionOnly` means the node name may appear in projection or metadata policy
for authoring review, but it must not claim working gameplay behavior.

`Deferred` means the node is accepted as a future design target only. Tools may
report it as unavailable, unsupported, or blocked; they must not present it as a
usable runtime node.

## Gameplay.High Nodes

| Node | Classification | Policy |
|---|---|---|
| `Gameplay.High.Inventory.Has` | ProjectionOnly | May describe an inventory check in visual metadata. It does not prove a production inventory system or a real item database. |
| `Gameplay.High.Door.Open` | ProjectionOnly | May describe an authored door action. It does not prove a complete door framework, animation system, or networked door authority. |
| `Gameplay.High.Score.Add` | ProjectionOnly | May describe a score increment. It does not prove a full scoring, persistence, leaderboard, or rewards system. |
| `Gameplay.High.Audio.PlayEvent` | ProjectionOnly | May describe an audio event call. It does not prove a full audio event bus, middleware integration, or production audio feature set. |
| `Gameplay.High.Entity.SetTag` | ProjectionOnly | May describe a simple entity tag operation. It does not prove full ECS authoring, replication, or gameplay state authority. |

## Gameplay.Low Nodes

| Node | Classification | Policy |
|---|---|---|
| `Gameplay.Low.Trigger.OnEnter` | Deferred | Accepted as a scenario target only until trigger runtime proof exists. |
| `Gameplay.Low.Spawn.Entity` | Deferred | Accepted as a controlled placeholder only until spawn/runtime proof exists. |
| `Gameplay.Low.Timer.Delay` | Deferred | Accepted as a controlled placeholder only until deterministic timer proof exists. |
| `Gameplay.Low.Entity.DestroyOrDeactivate` | Deferred | Accepted as a controlled placeholder only until safe entity lifecycle proof exists. |

## Runtime Truth Rules

Node extraction work must not create fake APIs that imply shipped gameplay
systems. A node name is allowed only when its report also carries an honest
classification and diagnostics.

Forbidden implications:

- inventory nodes proving a complete inventory system;
- audio nodes proving an audio engine feature set;
- entity nodes proving a full ECS authoring layer;
- spawn nodes proving a production spawning framework;
- timer nodes proving a complex async or scheduler framework.

Runtime proof requires compile and binding evidence plus a focused runtime or
tool report for the exact node behavior. Metadata alone is insufficient.

## Boundary

This document may add metadata extraction only after this policy is satisfied.
Future metadata fixtures may choose from the nodes above, but each node must keep
its classification until evidence changes it. This document publish integration must
reject stale or missing node metadata rather than silently accepting it.
