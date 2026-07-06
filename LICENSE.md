<!--
SPDX-FileCopyrightText: 2026 Arda Koyuncu
SPDX-License-Identifier: CC-BY-4.0
-->

# SagaEngine Licensing

SagaEngine is a mixed-license repository.

This document is an index. It does not replace the license text, file-level
metadata, verified upstream terms, or the machine-readable repository policy
that applies to a specific file.

## Current effective licensing state

The MPL-centered mixed-license model is effective for the Saga-owned source
domains recorded as `migration_status = "effective"` in
`LICENSE_POLICY.toml`.

The effective source scope includes Engine, Runtime, Server, Shared, Collaboration, Backends, applications, Editor, tests, CMake and build logic, scripts, profiles, core metadata, schemas, Saga-specific tools, Diligent integration metadata, repository automation, and repository root build files.

The following boundaries remain separate:

- Generic Forge remains under Apache-2.0.
- Vendor and third-party files remain under their actual upstream terms.
- Documentation, samples, templates, generated outputs, content, and
  user-created material continue to follow their separately recorded policy
  domains.
- Historical tags, commits, and releases remain governed by the licensing
  state in their own source snapshots.

This migration does not retroactively change an earlier release.

Effective migration commit: `8a39e5fbfc3fa9c687dde14a033d6c795763946b`.

## Approved target model

The repository's effective and remaining target treatments are:

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

The previous Editor-specific restricted treatment is historical for the
Saga-owned Editor source domain recorded as effective. That source is now
licensed under MPL-2.0.

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

This licensing index is active for policy domains recorded as effective. The
repository remains transitional only for domains that retain a target-only,
excluded, user-selected, file-specific, or upstream-controlled treatment.

Detailed public guidance is maintained in `docs/LICENSING.md`. Unresolved
content continues to fail closed rather than being assigned a license by
assumption.
