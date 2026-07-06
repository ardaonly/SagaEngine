<!--
SPDX-FileCopyrightText: 2026 Arda Koyuncu
SPDX-License-Identifier: CC-BY-4.0
-->

# Third-Party Notices

> Status: Reviewed repository-level dependency and notice index.

SagaEngine uses third-party components under their respective upstream
licenses. Third-party software is not relicensed by the SagaEngine repository
license.

This document records reviewed repository dependency boundaries. It does not
replace upstream license texts, copyright notices, package metadata, or
artifact-specific release records.

## Reviewed configuration

The current dependency review is based on:

- SagaEngine version `0.0.10`;
- the `linux-gcc` Conan profile;
- the `RelWithDebInfo` build type;
- `with_sde=False`;
- Qt configured as a shared library;
- OpenSSL declared as a direct dependency;
- the recursively initialized `Vendor/Diligent` submodule graph.

Conan metadata is used as inventory evidence. Upstream license files and
upstream project terms remain authoritative.

Dependency composition may vary by platform and build profile. Each
distributed artifact must include only the license materials applicable to
components actually shipped with that artifact.

## Runtime and engine dependencies

| Component | Reviewed version | Reported license | Integration |
|---|---:|---|---|
| SDL | 2.30.2 | Zlib | Engine platform and window backend |
| RmlUi | 4.4 | MIT | Runtime UI backend |
| GLM | 0.9.9.8 | MIT | Header-only graphics mathematics dependency |
| nlohmann/json | 3.11.3 | MIT | Serialization and configuration dependency |

Listing a component here does not imply that it is copied into every runtime
distribution.

## Graphics vendor boundary

`Vendor/Diligent/` is a recursively pinned upstream submodule and remains under
its upstream licensing terms.

Projects under `Vendor/Diligent/ThirdParty/` remain under their respective
upstream licenses. SagaEngine must not replace, obscure, or relicense those
terms.

The reviewed Saga graphics SDK installation includes applicable Diligent
license material under `Licenses/ThirdParty/Vendor/Diligent/`.

This installed payload supplements, rather than replaces, license files
retained in the vendor tree.

## Editor dependencies

| Component | Reviewed version | Reported license | Integration |
|---|---:|---|---|
| Qt | 6.8.3 | LGPL-3.0-only | Shared Editor UI dependency |
| OpenSSL | 3.6.2 | Apache-2.0 | Direct cryptographic dependency |
| nlohmann/json | 3.11.3 | MIT | Serialization and configuration dependency |

Reviewed host profiles require `qt/*:shared=True` for the Editor distribution
model.

A release containing Qt must preserve applicable license texts, copyright
notices, attribution notices, and recipient rights.

Static Qt distribution is outside the reviewed release configuration and
requires a separate redistribution review.

## Server and persistence dependencies

| Component | Reviewed version | Reported license | Integration |
|---|---:|---|---|
| Asio | 1.30.2 | BSL-1.0 | Header-only server networking dependency |
| libpqxx | 7.9.0 | BSD-3-Clause | PostgreSQL C++ client |
| hiredis | 1.2.0 | BSD-3-Clause | Redis C client |
| redis-plus-plus | 1.3.12 | Apache-2.0 | Redis C++ client |

The PostgreSQL `libpq` client is an explicitly pinned transitive dependency
listed below.

These dependencies apply only to server or persistence artifacts that actually
link or package them. They are not automatically part of the graphics SDK.

## Sandbox and development UI

| Component | Reviewed version | Reported license | Integration |
|---|---:|---|---|
| Dear ImGui docking branch | 1.91.5-docking | MIT | Sandbox and development UI |

Its notice is required only for artifacts that ship the corresponding compiled
implementation or source.

## Test-only dependencies

| Component | Reviewed version | Reported license |
|---|---:|---|
| GoogleTest / GoogleMock | 1.17.0 | BSD-3-Clause |
| RapidCheck | cci.20231215 | BSD-2-Clause |

Test dependencies are not runtime components unless they are actually included
in a distributed artifact.

## Build-only tools

| Component | Reviewed version | Reported license |
|---|---:|---|
| CMake | 3.28.1 | BSD-3-Clause |
| Ninja | 1.12.1 | Apache-2.0 |

Build tools are not runtime components merely because Conan resolves them
during a build.

## Transitive version constraints

The project pins or overrides these transitive versions:

| Component | Reviewed version | Reported license |
|---|---:|---|
| FreeType | 2.13.2 | FTL |
| libpq | 16.13 | PostgreSQL |
| robin-hood-hashing | 3.11.5 | MIT |
| Wayland | 1.24.0 | MIT |
| xkbcommon | 1.5.0 | MIT |

An override does not prove that a component is redistributed. Actual artifact
composition determines whether its license material must accompany a release.

Transitive dependencies introduced by Qt, SDL, RmlUi, Diligent, or another
component must be reviewed against the actual distribution. The complete Conan
graph must not be copied blindly into every notice bundle.

## Optional dependency

SystemDefinitionEngine (`sde/0.1.1`) is independently packaged and disabled in
the reviewed dependency graph.

Enabling it changes artifact composition and requires a new configured graph
and distribution review.

## Dependency acceptance policy

| License or condition | Default decision |
|---|---|
| MIT, BSD, Zlib, BSL-1.0, and comparable permissive licenses | `allow` after notice and redistribution verification |
| Apache-2.0 | `allow` after NOTICE and patent-term verification |
| MPL-2.0 | `review` |
| LGPL | `review` |
| GPL or AGPL | `explicit-review` |
| Unknown or custom terms | `deny-until-reviewed` |
| Non-commercial restrictions | `deny` |
| Field-of-use restrictions | `deny` |
| Binary-only SDK | `explicit-review` |
| NDA or platform SDK | separate restricted review |

A permissive source license does not by itself resolve codec patents,
royalties, registrations, platform restrictions, export controls, or binary
redistribution conditions.

## Artifact-specific release rule

Before publishing a binary, SDK, installer, archive, container, or package, the
release record must determine:

1. which third-party components are actually included;
2. whether each component is linked statically, dynamically, or used
   header-only;
3. which upstream license texts and notices must accompany the artifact;
4. whether source, relinking, replacement, or reverse-engineering rights
   apply;
5. whether codec, patent, royalty, platform, registration, or export review is
   required;
6. whether system-provided components are redistributed or merely discovered
   at runtime.

A generic repository dependency list must not be copied blindly into every
artifact. The shipped artifact composition is authoritative for its notice
bundle.
