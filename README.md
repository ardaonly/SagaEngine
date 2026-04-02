# SagaEngine

SagaEngine is a foundation for an MMO-first engine core.

It is not a platform product.
It is not production-ready.
It is not trying to be a general-purpose engine for everything humans can imagine at 3 a.m.

The project is intentionally structured around:
- MMO runtime and authoritative server boundaries
- replication and interest management
- persistence and backend service separation
- moddability and scripting boundaries
- future platform expansion without hard-coding platform behavior into the core

The current focus is on building a clean technical base that can support:
- a client runtime
- a dedicated server runtime
- backend services
- eventual modding and content delivery workflows

## What this project is

- A core engine foundation
- MMO-oriented by design
- Architecture-first and extension-friendly
- Built to separate runtime, server, and backend concerns early

## What this project is not

- A finished game platform
- A Roblox-like ecosystem today
- A production-ready engine
- A generic engine meant to solve every use case

## Current status

- Architectural maturity: around 70%
- Platform coverage: Windows-first
- Production readiness: not ready
- Scope: actively evolving

## Repository structure

- `Engine/`  
  Core runtime systems: ECS, rendering, RHI, simulation, input, physics, scripting, resources, platform interfaces

- `Runtime/`  
  Client runtime glue and client-side networking/input flow

- `Server/`  
  Dedicated server runtime, replication orchestration, interest handling, shard and zone logic

- `Backends/`  
  Persistence and backend service infrastructure

- `Editor/`  
  Editor tooling

- `Tests/`  
  Unit, integration, and stress tests

## Build system

### Windows

```powershell
.\build.ps1 lock -Profile windows-msvc

.\build.ps1 setup -Profile windows-msvc -Preset windows-msvc-14.38

.\build.ps1 build -Profile windows-msvc -Preset windows-msvc-14.38

.\build.ps1 test -Preset windows-msvc-14.38