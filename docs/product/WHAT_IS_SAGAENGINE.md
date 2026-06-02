# What Is SagaEngine?

SagaEngine is a long-term game engine and toolchain project. The current repo is
focused on the technical base needed for engine runtime code, dedicated server
code, authoring workflows, project validation, scripting analysis, packaging,
and diagnostics.

It is not a finished game engine product today. A new developer should treat it
as an active engine/toolchain codebase, not as an installable editor for making
games end to end.

## Current Shape

- C++ engine, runtime, server, editor, app, and test code exists.
- Project, scripting, packaging, diagnostics, and verification tools exist.
- `samples/MultiplayerSandbox` is the main fixture for project/toolchain work.
- Documentation and tool boundaries are being cleaned up after a period of
  internal recovery work.

## What It Is Not Yet

SagaEngine does not currently provide a finished editor workflow, complete
runtime gameplay loop, complete server gameplay loop, production asset pipeline,
complete visual scripting environment, round-tripping arbitrary C#, or public
SDK distribution.

## How To Read The Repo

Start with the product docs in this directory, then use the architecture,
testing, and roadmap indexes for detail. Treat historical phase or target files
as context, not current product promises.
