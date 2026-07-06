<!--
SPDX-FileCopyrightText: 2026 Arda Koyuncu
SPDX-License-Identifier: CC-BY-4.0
-->

# LIC-DEC-0001 — MPL-Centered Mixed-License Direction

Status: Active — effective for the recorded Saga-owned source domains
Recorded: 2026-07-06

## Decision

SagaEngine uses an MPL-2.0-centered mixed-license model:

- Saga-owned Engine, Runtime, Server, Shared, Collaboration, Backends, Editor,
  applications, tests, build logic, and Saga-specific tooling are licensed under
  MPL-2.0 in the policy domains recorded as effective.
- Generic Forge remains Apache-2.0.
- User-created project material remains user-licensed.
- Approved templates and minimal generated boilerplate may use 0BSD.
- Documentation prose targets CC-BY-4.0; separately identified copyable
  snippets may use 0BSD.
- Eligible Saga-owned sample assets may use CC0-1.0.
- Vendor and third-party material remains under verified upstream terms.

## Effective migration

The effective source migration covers Engine, Runtime, Server, Shared, Collaboration, Backends, applications, Editor, tests, CMake and build logic, scripts, profiles, core metadata, schemas, Saga-specific tools, Diligent integration metadata, repository automation, and repository root build files.

The repository remains mixed-license:

- Generic Forge remains Apache-2.0.
- Vendor and third-party material retains its upstream licensing.
- Documentation, samples, templates, content, generated outputs, and
  user-created material continue to follow their separate policy domains.
- Historical commits, tags, and releases retain their historical grants.

The previous Saga Editor restricted treatment is historical for the Editor
source domain migrated by this decision.

Effective migration commit: `8a39e5fbfc3fa9c687dde14a033d6c795763946b`.

## Enforcement

`LICENSE_POLICY.toml` is the machine-readable inventory. It records current and
target treatment separately. Conflicts and unresolved rights fail closed.
