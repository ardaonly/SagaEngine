# What Is SagaEngine?

SagaEngine is an active engine/toolchain architecture codebase. The current
repo is focused on source-truth discipline, deterministic local
reports/evidence, bounded runtime/server/render/editor/toolchain foundations,
project validation, scripting analysis, packaging, diagnostics, and tests.

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

SagaEngine does not currently provide a finished editor workflow, finished
runtime gameplay loop, finished server gameplay loop, production renderer,
production networking stack, production asset pipeline, cloud collaboration
product, enterprise security platform, release pipeline, installer/updater
ecosystem, production observability system, end-to-end visual scripting
environment, broad C# roundtrip support, performance-ready MMO product, or
public SDK.

## How To Read The Repo

Start with the product docs in this directory, then use the architecture and
testing indexes for detail. Treat milestone, target, candidate, quickstart,
tutorial, closure, and historical planning files as background unless a current
index links to them for a specific task.
