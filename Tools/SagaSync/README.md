# SagaSync

SagaSync is an internal developer dashboard for the SagaEngine multirepo workflow.

The first version is intentionally safe: it shows repository state, export state,
commit queue suggestions, and verification output, but it does not stage files,
create commits, publish branches, switch branches, call external hosting services,
repair files automatically, or mutate mirror clones.

## Requirements

SagaSync uses Python 3 and PySide6 for the GUI:

```sh
python3 -m pip install PySide6
```

On NixOS or Nix-enabled systems:

```sh
cd Tools/SagaSync
nix-shell
python3 sagasync.py
```

The non-GUI smoke check does not require PySide6:

```sh
python3 Tools/SagaSync/sagasync.py --smoke
```

## Usage

Run directly:

```sh
python3 Tools/SagaSync/sagasync.py
```

Or via SagaTools after running `Tools/SagaTools/setup.py`:

```sh
tools sagasync
```

The registry points to `Tools/SagaSync/sagasync` on Linux/macOS and
`Tools/SagaSync/sagasync.cmd` on Windows.

## Structure

`sagasync.py` is only the entrypoint. Testable non-UI logic lives under
`core/`; PySide6 widgets live under `ui/`.

## MVP Scope

SagaSync v1 manages the SagaEngine monorepo plus the mirror tools configured in
`core/export/manifest.json`.

It can:

- show SagaEngine branch, dirty count, ahead/behind, and head commit,
- show Forge/Prism/SDE export state from `.core/export/state`,
- evaluate export health as Ready, Stale, Blocked, or Unknown,
- show Quick, Forge, and Export Safety verification profiles,
- track session-local verification run results, duration, and last run time,
- mark ready export state as unverified until Export Safety has run,
- preview a read-only commit plan with grouped files and suggested messages,
- run export dry-runs,
- run basic verification commands,
- suggest a conservative commit/export queue.

It does not:

- stage files,
- create commits,
- publish branches,
- switch branches,
- call external hosting APIs,
- repair files automatically,
- manage separately checked-out mirror repositories.
