# Current Capabilities

This page lists what the repository can reasonably be said to contain today. It
is not a release note for a finished engine.

## Engine And Server Code

The repo contains C++ engine, runtime, server, diagnostics, editor, app, and
test code. Some focused targets build and run locally, but the repo does not yet
prove a complete end-to-end product workflow.

## Project Tooling

SagaProjectKit supports project manifest validation, resolution, and
doctor-style reporting for supported inputs. The `.sagaproj` schema is still
early and should be treated as a project/tooling contract, not a public SDK.

## Scripting And Authoring Tooling

SagaScript supports selected C# / SagaScript analysis and source-authoring
workflows. This is useful infrastructure, but it is not a complete visual
scripting product and it does not support arbitrary C# to blocks roundtrip.

## Packaging And Launch-Adjacent Tooling

SagaPackager and related tooling can inspect package/profile inputs and produce
local reports for supported paths. This is not a complete publishing pipeline.

## Diagnostics And Verification

The repo contains diagnostics primitives, focused tests, and internal reporting
tools. These help developers review subsystem health, but they do not prove full
CTest health, heavy stress readiness, production networking, soak readiness, or
scale readiness.

## Samples

`samples/MultiplayerSandbox` is currently a deterministic fixture for project,
script, package, and validation workflows. It should not be described as a
finished or playable multiplayer sample.
