<!--
SPDX-FileCopyrightText: 2026 Arda Koyuncu
SPDX-License-Identifier: CC-BY-4.0
-->

# SagaEngine Licensing Guide

> Status: DRAFT — transitional and non-effective for bulk relicensing.

SagaEngine is moving to an MPL-2.0-centered mixed-license model. This document
explains the approved target, the current transitional state, and the boundary
between Saga-owned source, user projects, generated output, documentation,
assets, and third-party material.

## Approved target

- Saga-owned Engine, Runtime, Server, Shared, Collaboration, Backends, Editor,
  applications, tests, build logic, and Saga-specific tools: MPL-2.0.
- Generic Forge: Apache-2.0.
- User-created game code, scripts, data, scenes, saves, and assets:
  user-selected license.
- Approved templates and generated gameplay boilerplate: 0BSD.
- Documentation prose and illustrations: CC-BY-4.0.
- Separately identified copyable snippets: 0BSD.
- Eligible Saga-owned sample assets: CC0-1.0.
- Vendor and third-party material: verified upstream terms.

The Editor's read-only license is transitional. The approved final target for
Saga-owned Editor source is MPL-2.0.

## Current versus target

A target recorded in `LICENSE_POLICY.toml` is not an effective legal grant.
Existing files change license only through an approved migration batch that
has sufficient provenance and licensing-authority evidence, file metadata,
documentation updates, validation, and an effective commit record.

Historical releases retain the licensing state contained in their own source
snapshot.

## File-level MPL behavior

MPL-2.0 is file-level copyleft. After a Saga-owned file is migrated:

- modifications distributed in that MPL-covered file remain available under
  MPL-2.0;
- separate files may use another compatible license;
- merely linking to Saga APIs does not automatically place an independent
  plugin or user game under MPL-2.0;
- copying or modifying Saga implementation files may carry MPL obligations.

This repository does not attempt to classify legal edge cases solely from a
build graph. Distribution composition must be reviewed.

## User projects

Using SagaEngine does not by itself relicense independent user material.
Users may choose a license only for material they have authority to license.
Third-party assets and code retain their own terms.

## Generated output

Generated outputs are not presumed to have a single license. Each generator
must declare:

- the generator implementation license;
- the template/input license;
- whether expressive Saga-owned material is copied;
- the intended output license;
- deterministic provenance metadata.

Approved minimal project scaffolding and gameplay boilerplate may use 0BSD.
Generated copies of MPL-covered implementation remain subject to applicable
MPL obligations.

See `docs/licensing/GENERATED_OUTPUTS.md`.

## Plugins

Independent plugin files are not automatically MPL merely because they use
public Saga APIs. A plugin that copies, modifies, or incorporates MPL-covered
Saga implementation may have additional obligations.

See `docs/licensing/PLUGIN_LICENSING.md`.

## Documentation and snippets

Documentation prose is targeted for CC-BY-4.0. Copyable code snippets must be
separately marked 0BSD or moved into an explicitly 0BSD-licensed snippet path.
Unmarked snippets are not silently reclassified.

## Assets

CC0-1.0 is used only for Saga-owned assets that are eligible for dedication.
Third-party assets, trademarks, personality rights, privacy rights, patents,
and other non-copyright rights require separate review.

## Vendor and submodules

`Vendor/**` is upstream-controlled. Parent policy does not override nested
license texts, submodule terms, or third-party notices. Diligent and recursive
submodules must remain separately inventoried.

## Contributions and DCO

SagaEngine uses Developer Certificate of Origin 1.1 and
inbound-equals-outbound.

A contribution is accepted under the current effective license of the affected
file or path at merge time. A future MPL target is not the inbound license
before its migration becomes effective.

Every contributor must have authority to submit the contribution. AI-assisted
work must be reviewed for provenance and must not import incompatible material.

## Policy and validation

`LICENSE_POLICY.toml` is the machine-readable inventory. Stable domain IDs are
the policy identity; paths may move.

Run:

```bash
python3 Tools/Licensing/validate_license_policy.py --report
```

Configured CMake target validation can be enabled with:

```bash
python3 Tools/Licensing/validate_license_policy.py \
  --report \
  --cmake-build-dir Build/<configured-preset>
```

Strict CI mode will be enabled after transitional findings are resolved.

## Conflict handling

License conflicts, unresolved file-specific content, missing upstream evidence,
ambiguous domain overlap, or migration records without authority evidence are
release blockers. They must not be resolved by choosing the most convenient
license.


## Canonical license-file layout

The obsolete duplicate paths `LICENSES/LICENSE`,
`LICENSES/LICENSE-EDITOR`, and `LICENSES/NOTICE` have been removed.

Canonical license texts use explicit names such as
`LICENSES/Apache-2.0.txt` and
`LICENSES/LicenseRef-Saga-Editor-Restricted.txt`.

Repository-level third-party notice inventory is maintained in
`docs/licensing/THIRD_PARTY_NOTICES.md`; nested upstream notices remain in
their upstream locations.
