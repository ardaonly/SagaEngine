# Current Capabilities

This page lists what the repository contains today. It is not a release note for
a finished engine and it is not a roadmap.

## Engine And Server Code

The repo contains C++ engine, runtime, server, diagnostics, editor, app, and
test code. Some focused targets build and run locally. A complete end-to-end
game creation workflow is not available yet.

The safest product claim is that SagaEngine has bounded engine/toolchain
foundations with explicit source-truth and report/evidence discipline. A
capability listed here should not be read as product completion unless another
current product doc says so directly.

## Project Tooling

SagaProjectKit supports project manifest validation, resolution, and
doctor-style reporting for supported inputs. The `.sagaproj` schema is still
early and should be treated as a project/tooling contract, not a public SDK.

## Scripting And Authoring Tooling

SagaScript supports selected C# / SagaScript analysis and source-authoring
workflows. This is useful infrastructure, but it is not a complete visual
scripting product and it does not support arbitrary C# to blocks round trip.

## Packaging And Launch-Adjacent Tooling

SagaPackager and related tooling can inspect package/profile inputs and produce
local reports for supported paths. This is not a complete publishing pipeline.
Selected Linux staging/archive/checksum/smoke paths exist as limited local
evidence, not as a production distribution channel.

## Diagnostics And Verification

The repo contains diagnostics primitives, focused tests, and internal reporting
tools. These help developers review subsystem health. They do not establish full
CTest health, heavy stress coverage, production networking, soak testing, or
scale behavior. Reports are evidence outputs; they are not source truth,
enforcement, or readiness guarantees.

## Samples

`samples/MultiplayerSandbox` is currently a deterministic fixture for project,
script, package, and validation workflows. It should not be described as a
finished or playable multiplayer sample.
