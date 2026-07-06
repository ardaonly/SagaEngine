<!--
SPDX-FileCopyrightText: 2026 Arda Koyuncu
SPDX-License-Identifier: CC-BY-4.0
-->

# SagaEngine Licensing

SagaEngine is a mixed-license repository.

This document is an index. It does not replace the license text, file-level
metadata, verified upstream terms, or the machine-readable repository policy
that applies to a specific file.

## Current transitional state

The repository is undergoing a controlled licensing migration.

The intended licensing model is not yet effective for every existing file.
Current and target licenses must therefore be read separately.

- Most existing Saga-owned engine, runtime, server, build, test and tooling
  files are currently under Apache-2.0.
- Existing Editor-owned paths remain under the restricted
  `LicenseRef-Saga-Editor-Restricted` terms until their dedicated MPL migration
  becomes effective.
- Vendor and third-party files remain under their actual upstream terms.
- Historical tags and releases remain governed by the licensing state in their
  own source snapshots.

The transition must not be interpreted as retroactively changing an earlier
release.

## Approved target model

Subject to verified rights, provenance and dedicated migration commits, the
approved target model is:

| Area | Approved target treatment |
|---|---|
| Saga-owned Engine, Runtime and Server source | MPL-2.0 |
| Saga-owned Shared, Collaboration and Backends source | MPL-2.0 |
| Saga-owned Editor source | MPL-2.0 |
| Saga-specific applications, tests, build scripts and tooling | MPL-2.0 |
| Generic Forge | Apache-2.0 |
| User-created game code, scripts, data and assets | User-selected license |
| Approved project templates and generated gameplay boilerplate | 0BSD |
| Documentation prose and illustrations | CC-BY-4.0 |
| Separately identified copyable documentation snippets | 0BSD |
| Eligible Saga-owned sample assets | CC0-1.0 |
| Vendor and third-party content | Upstream terms |

The Editor's restricted license is transitional. The final target for
Saga-owned Editor source is MPL-2.0, not a permanent read-only model.

## License resolution

The applicable license is determined from the legal grant and verified facts,
with more specific file-level information taking precedence over broad path
defaults.

Relevant sources include:

1. applicable file-specific, nested, root or upstream license terms;
2. verified copyright and provenance information;
3. file-level SPDX metadata;
4. adjacent `.license` sidecars;
5. `.reuse/dep5`;
6. effective migration records;
7. `LICENSE_POLICY.toml`.

A target license recorded in policy is not effective until the corresponding
migration requirements have been completed.

Conflicts are release blockers and must not be resolved silently.

## Canonical texts

Canonical and custom texts are stored under `LICENSES/`.

Important entries include:

- `LICENSES/MPL-2.0.txt`
- `LICENSES/Apache-2.0.txt`
- `LICENSES/0BSD.txt`
- `LICENSES/CC-BY-4.0.txt`
- `LICENSES/CC0-1.0.txt`
- `LICENSES/LicenseRef-Saga-Editor-Restricted.txt`

The Developer Certificate of Origin is stored in `DCO`.

## Machine-readable policy

`LICENSE_POLICY.toml` records current treatment, approved targets, stable
domain IDs, repository paths, target composition and submodule boundaries.

Paths may change. Stable domain IDs are the policy identity. A move or rename
does not itself relicense a file.

Repository policy validation is performed with:

```bash
python3 Tools/Licensing/validate_license_policy.py --report
```

Strict enforcement will be enabled only after the transitional policy and
metadata coverage are complete.

## User projects and plugins

Using SagaEngine does not, by itself, place independent user-created game code,
scripts, project data, scenes, saves or assets under MPL-2.0.

A user may select a license only for material the user has authority to
license. Third-party content and copied or modified Saga files retain their
applicable obligations.

Independent plugin files are not automatically MPL merely because they use
Saga APIs. Modified MPL-covered Saga files remain subject to MPL-2.0 after
their migration becomes effective.

## Contributions

Contributions use Developer Certificate of Origin 1.1 and an
inbound-equals-outbound model.

A contribution is accepted under the current effective license of the affected
file or path at merge time. A future target recorded in policy is not the
inbound license before that target becomes effective.

## Status

This licensing index is transitional. It establishes no bulk relicensing by
itself.

Detailed public guidance is maintained in `docs/LICENSING.md`. During the
transition, unresolved content must fail closed rather than being assigned a
license by assumption.
