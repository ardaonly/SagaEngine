# What Is SagaEngine?

SagaEngine is an active game engine and toolchain codebase. The current repo is
focused on engine runtime code, dedicated server code, editor and authoring
infrastructure, project validation, scripting analysis, packaging, diagnostics,
and tests.

It is not a finished game engine product. A new developer should treat it as
source for engine/toolchain development, not as an installable editor for making
games end to end.

## Current Shape

- C++ engine, runtime, server, editor, app, and test code exists.
- Project, scripting, packaging, diagnostics, and verification tools exist.
- `samples/MultiplayerSandbox` is the main fixture for project/toolchain work.
- The public CLI surface is centered on Forge, SagaScript, SagaProjectKit, and
  SagaPackager.

## What It Is Not Yet

SagaEngine does not currently provide a finished editor workflow, complete
runtime gameplay loop, complete server gameplay loop, production asset pipeline,
complete visual scripting environment, arbitrary C# round trip, or public SDK.

## How To Read The Repo

Start with the product docs in this directory, then use the architecture and
testing indexes for detail. Treat phase, target, and recovery files as
background unless a current index links to them for a specific task.
