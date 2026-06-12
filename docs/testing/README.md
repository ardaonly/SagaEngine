# Testing

This directory explains how to verify SagaEngine locally without overstating
what the checks prove.

Use [TEST_SUITES.md](TEST_SUITES.md) for detailed suite names, labels, and
known limitations.

## Start With Focused Checks

For a narrow change, build and run the affected target or test group first. The
repo currently has useful focused tests, but a universal full-suite pass should
not be assumed.

Common examples:

```sh
git diff --check
ctest --test-dir <build-dir> -R "<test-name>" --output-on-failure
```

On NixOS:

```sh
nix-shell --run "ctest --test-dir <build-dir> -R '<test-name>' --output-on-failure"
```

## Local Gates

Use these repository scripts for baseline verification:

```sh
scripts/build-default --dry-run
scripts/test-taxonomy --check
scripts/verify-local
```

`scripts/verify-local` is structural by default. It does not run a real build or
CTest unless `--with-build` or `--with-tests` is passed explicitly.

## Heavy Tests

Stress, soak, load, benchmark, bot, and real-transport tests should stay
opt-in. Do not include them in a default quick verification path unless the test
is explicitly documented as safe and bounded.

## Reporting Results

When reporting test results, include:

- exact command;
- pass/fail status;
- missing target or environment failure if applicable;
- whether the result proves product readiness or only focused subsystem health.
