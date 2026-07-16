SPDX-License-Identifier: MPL-2.0

# GitHub maintenance contract

GitHub Actions is an evidence router for the current repository; it does not create a second test architecture. Executable tests remain with their owners under `Engine/**/Tests`, `Tests`, or `Tools/**/tests`.

## Required pull-request checks

Configure branch protection for these stable check names:

- `Repository contracts`
- `C++ Linux all-safe`
- `C++ Windows core`
- `Licensing gate`

Branch protection is a GitHub repository setting and cannot be enforced by the files in this directory. Renaming one of these checks requires a coordinated branch-protection update.

## Evidence tiers

| Tier | Trigger | Evidence |
| --- | --- | --- |
| PR contracts | Every push and pull request | Python/tool contracts, boundaries, Wiki, workflow policy, Linux all-safe CTest, Windows unit/architecture, strict license policy. |
| Configured license graph | Relevant PR changes, nightly, or manual | CMake File API ownership and target inventory, installed SDK consumer, machine-readable license report. |
| Nightly | Daily | Integration and replication tests, serial execution. |
| Weekly | Sunday | StressTests, serial execution with a four-hour job limit. |
| GPU | Manual preflight | Explicitly blocked until a trusted self-hosted runner carrying the `saga-gpu` label exists. |

Visible StarterArena runtime cases require `SAGA_RUN_VISIBLE_TESTS=1` and a qualified native display. Ordinary PR runs leave that opt-in unset; those cases publish explicit GTest skip reasons while headless cases continue.

PR artifacts are retained for 14 days, nightly artifacts for 30 days, and licensing/weekly/GPU evidence for 90 days. Artifact upload and job summaries use `if: always()` so a failing test still publishes diagnostics. Test commands themselves never use `continue-on-error`.

## Dependency and security policy

Actions are pinned to full commit SHAs. The upstream `actionlint` Go module is pinned to an explicit release and provides GitHub schema/expression/shell validation in addition to Saga's semantic workflow checker. Dependabot proposes weekly `github-actions` updates; each update must retain workflow-contract tests and should name the upstream release represented by the new SHA. Workflows use read-only repository permissions, explicit timeouts, and concurrency controls.

## GPU boundary

GPU execution is not inferred from a successful native build or from a hosted runner exposing a software graphics device. Until a trusted runner is labeled `saga-gpu`, manual GPU preflight intentionally fails with a `blocked` JSON report. When such a runner is introduced, add driver/device identity and native GPU CTest execution before changing this status.

## Local workflow checks

```sh
python3 -m unittest discover -s Tools/Developer/CI/tests -p 'test_*.py'
python3 Tools/Developer/CI/check_github_workflows.py --repo-root .
nix-shell -p actionlint --run "actionlint"
python3 -m unittest discover -s Tools/Licensing/tests -p 'test_*.py'
python3 Tools/Licensing/generate_legacy_path_rules.py --check
python3 Tools/Licensing/regenerate_license_checksums.py --check
```

Do not add retired `Apps`, old source roots, `.saga-private` state, or historical capability promises to CI merely to make an old plan appear current.
