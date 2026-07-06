<!--
SPDX-FileCopyrightText: 2026 Arda Koyuncu
SPDX-License-Identifier: CC-BY-4.0
-->

# LIC-DEC-0001 — MPL-Centered Mixed-License Direction

Status: Draft — non-effective
Recorded: 2026-07-06

## Decision

SagaEngine will use an MPL-2.0-centered mixed-license model:

- Saga-owned Engine, Runtime, Server, Shared, Collaboration, Backends, Editor,
  applications, tests, build logic, and Saga-specific tooling target MPL-2.0.
- Generic Forge remains Apache-2.0.
- User-created project material remains user-licensed.
- Approved templates and minimal generated boilerplate may use 0BSD.
- Documentation prose targets CC-BY-4.0; separately identified copyable
  snippets may use 0BSD.
- Eligible Saga-owned sample assets may use CC0-1.0.
- Vendor and third-party material remains under verified upstream terms.

## Transitional constraint

This decision does not itself relicense existing files. Current grants remain
effective until provenance, authority, metadata, validation, distribution
review, and an effective migration commit exist for the affected domain.

The Editor's restricted license is transitional; the approved target for
Saga-owned Editor source is MPL-2.0.

## Enforcement

`LICENSE_POLICY.toml` is the machine-readable inventory. It records current and
target treatment separately. Conflicts and unresolved rights fail closed.
