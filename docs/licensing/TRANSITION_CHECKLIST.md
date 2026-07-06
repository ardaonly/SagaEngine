<!--
SPDX-FileCopyrightText: 2026 Arda Koyuncu
SPDX-License-Identifier: CC-BY-4.0
-->

# Licensing Transition Checklist

> Status: ACTIVE — operational checklist and migration record; not a standalone legal grant.

## A. Review overlay

1. Apply the current review overlay without staging or committing.
2. Run the canonical text verifier.
3. Run policy validation in report mode.
4. Generate configured CMake File API evidence.
5. Review every unresolved `Content/**`, `samples/**`, generated, vendor, and
   package/install surface.
6. Confirm the DCO section in `CONTRIBUTING.md`.

## B. Prepare the atomic foundation commit

Before the foundation commit:

1. Replace the stale root licensing index with the mixed-license index.
2. Make `LICENSE_POLICY.toml` and file metadata internally consistent.
3. Regenerate the non-authoritative compatibility `path_rules.json`.
4. Decide whether `LICENSES/NOTICE` is removed or replaced by the third-party
   notice index.
5. Remove duplicate legacy license paths only in the same reviewed change set:
   `LICENSES/LICENSE` and `LICENSES/LICENSE-EDITOR`.
6. Change `[legacy].cleanup_status` to `complete` and
   `files_are_normative_until_cleanup` to `false`.
7. Mark legacy-copy domains `migration_status = "removed"`, set
   `required = false`, move their old patterns to `historical_paths`, and set
   active `paths = []`.
8. Regenerate `SHA256SUMS` without legacy paths.
9. Run `verify_license_texts.py --strict-cleanup`.
10. Run policy validation, build, tests, install/export checks, and package
   notice checks.
11. Review `git diff`, then create one DCO-signed foundation commit.

The foundation commit establishes policy and metadata. It does not bulk
relicense existing Saga source.

## C. Effective MPL migration batches

Migrate only rights-cleared domains. For each batch:

1. verify provenance and licensing authority;
2. add or update file-level SPDX metadata;
3. update current and target policy state;
4. record the full effective commit;
5. run build, tests, install/export, and distribution checks;
6. preserve historical release behavior.

Recommended order:

1. Engine core and platform;
2. Runtime;
3. Server;
4. Shared and Collaboration;
5. Backends;
6. Saga-specific tools and applications;
7. Tests;
8. Editor;
9. documentation, templates, generated outputs, and eligible assets.

## D. Enforcement after stabilization

After report-mode findings are resolved:

1. change required policy actions from `report` to `deny`;
2. enable strict local pre-push checks;
3. add `.github/workflows/licensing.yml`;
4. require the licensing CI status check before merge;
5. add CODEOWNERS and pull-request licensing questions.


## Legacy duplicate cleanup status

Completed in the current working tree:

- `LICENSES/LICENSE` removed;
- `LICENSES/LICENSE-EDITOR` removed;
- `LICENSES/NOTICE` removed;
- canonical `LICENSES/Apache-2.0.txt` retained;
- canonical `LICENSES/LicenseRef-Saga-Editor-Restricted.txt` retained;
- `[legacy].cleanup_status` set to `complete`;
- foundation verification runs with `--strict-cleanup`.

Do not restore the removed duplicate paths.

## E. Completed Saga-owned source migration batch

The completed Saga-owned source migration batch covers Engine, Runtime, Server, Shared, Collaboration, Backends, applications, Editor, tests, CMake and build logic, scripts, profiles, core metadata, schemas, Saga-specific tools, Diligent integration metadata, repository automation, and repository root build files.

The following boundaries remain unchanged:

- Generic Forge remains Apache-2.0.
- Vendor and third-party files retain upstream licensing.
- Historical Apache-2.0 releases remain valid.
- Documentation, samples, templates, generated outputs, content, and
  user-created material follow their separate policy domains.

Effective migration commit: `8a39e5fbfc3fa9c687dde14a033d6c795763946b`.
