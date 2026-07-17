SPDX-License-Identifier: MPL-2.0

# GitHub maintenance contract

GitHub Actions is an evidence router for the current repository; it does not create a second test architecture. Executable tests remain with their owners under `Engine/**/Tests`, `Tests`, or `Tools/**/tests`.

## Main branch checks

The expected stable job display names are:

- `Repository contracts`
- `C++ Linux all-safe`
- `C++ Windows core`
- `Licensing gate`

These names must remain globally unique. Matrix expansion or an external status with the same name can make the evidence ambiguous; renaming a job requires a coordinated maintenance update.

`Repository contracts` and `Licensing gate` run automatically on repository updates. The C++ checks are intentionally not started by every push: run the `C++ Evidence` workflow manually against the commit that needs native evidence. This produces `C++ Linux all-safe` and `C++ Windows core` without replacing them with skipped placeholder jobs.

Saga currently uses a single-branch direct-push model: `main` is the only persistent development branch and pull requests are not required. Protect `main` from deletion and force-push, but do not configure pre-merge required checks while this model is active because there is no pull-request merge boundary. Automatic checks report each pushed commit after it lands; run the two C++ checks manually when native evidence is required. If required pre-merge checks are introduced later, first restore a short-lived branch and pull-request workflow.

## Evidence tiers

| Tier | Trigger | Evidence |
| --- | --- | --- |
| Main contracts | Every `main` push | Python/tool contracts, boundaries, Wiki, workflow policy, and strict repository license policy. |
| C++ evidence | Manual | Linux all-safe CTest and Windows unit/architecture CTest. |
| Configured license graph | Nightly or manual | CMake File API ownership and target inventory, installed SDK consumer, machine-readable license report. |
| Nightly | Daily | Integration and replication tests, serial execution. |
| Weekly | Sunday | StressTests, serial execution with a four-hour job limit. |
| GPU | Manual preflight | Explicitly blocked until a trusted self-hosted runner carrying the `saga-gpu` label exists. |

Visible StarterArena runtime cases require `SAGA_RUN_VISIBLE_TESTS=1` and a qualified native display. Ordinary automated runs leave that opt-in unset; those cases publish explicit GTest skip reasons while headless cases continue.

Contract artifacts are retained for 14 days, nightly artifacts for 30 days, and licensing/weekly/GPU evidence for 90 days. Artifact upload and job summaries use `if: always()` so a failing test still publishes diagnostics. Test commands themselves never use `continue-on-error`.

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
