SPDX-License-Identifier: MPL-2.0

# GitHub maintenance contract

GitHub Actions is an evidence router for the current repository; it does not create a second test architecture. Executable tests remain with their owners under `Engine/**/Tests`, `Tests`, or `Tools/**/tests`.

## Required pull-request checks

The expected stable job display names are:

- `Repository contracts`
- `C++ Linux all-safe`
- `C++ Windows core`
- `Licensing gate`

Do not copy these labels into a ruleset before the corrected workflows have completed successfully on GitHub. Required status checks must use the exact, globally unique `check_run.name` values reported for a successful commit; matrix expansion or an external status with the same name can change or make that context ambiguous. Renaming one of these jobs requires a coordinated ruleset update.

`Repository contracts` and `Licensing gate` run automatically for pull requests. The C++ checks are intentionally not started by every PR synchronization: run the `C++ Evidence` workflow manually against the final candidate branch before merge. This produces `C++ Linux all-safe` and `C++ Windows core` on the exact head commit without replacing them with skipped placeholder jobs. A relevant merge to `main` runs the same C++ evidence once more.

After the four check runs have succeeded on the same candidate commit, protect `main` with an active branch ruleset that requires a pull request with zero approvals, requires those exact checks in strict/up-to-date mode, blocks deletion and force-push, and grants repository administrators bypass access for pull requests only. Direct pushes remain blocked; a solo maintainer works through `feature branch -> pull request -> automatic contracts -> manual C++ evidence -> self-merge`.

## Evidence tiers

| Tier | Trigger | Evidence |
| --- | --- | --- |
| PR contracts | Every pull request update | Python/tool contracts, boundaries, Wiki, workflow policy, and strict repository license policy. |
| C++ evidence | Manual candidate-branch run and relevant `main` push | Linux all-safe CTest and Windows unit/architecture CTest. |
| Configured license graph | Relevant `main` changes, nightly, or manual | CMake File API ownership and target inventory, installed SDK consumer, machine-readable license report. |
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
