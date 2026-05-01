# SagaTools (`tools`) — Roadmap

> Last updated: 2026-04-30
> Primary home: `Tools/SagaTools/` inside the SagaEngine repository.
> Future: may graduate to a standalone repository whenever the registry
> schema and the dispatcher's CLI surface are deemed stable enough to
> tag a 1.0.

`tools` is the **thin dispatcher** for the SagaEngine tool ecosystem.
It does one thing: take a logical name, look it up in a flat JSON
registry, and run the matching executable with the user's arguments
forwarded verbatim. It owns no tool logic; it does not orchestrate
pipelines; it does not know what Forge, Prism, or any other registered
tool actually does.

The single load-bearing invariant of this tool: **deleting it does not
break any tool it can launch.** Anything that would violate that
invariant is automatically out of scope.

Conventions: `[x]` shipped and committed, `[ ]` open.
When an item ships, the note after it names the files that represent the work.

---

## 1. Dispatcher Core

| Status | Item |
|--------|------|
| [x] | Built-in commands: `list`, `which`, `where`, `run`, `install`, `--help`, `--version`. → `src/main.cpp` |
| [x] | Implicit dispatch — any first word that is not a built-in is treated as a tool name and forwarded. → `src/main.cpp` |
| [x] | Argument forwarding is verbatim; the dispatcher never rewrites, splits, or quotes. → `src/main.cpp` |
| [x] | `Registry` — two-section JSON (`tools` + `installers`) loaded once, immutable thereafter. → `include/SagaTools/Registry.h`, `src/Registry.cpp` |
| [x] | Embedded zero-dependency JSON parser limited to the registry schema. → `src/Registry.cpp` |
| [x] | `Resolver` — `${VAR}` env expansion → path-vs-bare-name disambiguation → PATH search. → `include/SagaTools/Resolver.h`, `src/Resolver.cpp` |
| [x] | `ProcessRunner` — POSIX `fork`+`execvp`+`waitpid` and Windows `_spawnvp` with stdio inheritance. → `include/SagaTools/ProcessRunner.h`, `src/ProcessRunner.cpp` |
| [x] | Registry-file discovery: `--registry` → `$SAGATOOLS_REGISTRY` → `<exe-dir>/config/tools.registry.json` → `./tools.registry.json`. → `src/Registry.cpp` |
| [x] | Diagnostic output for unavailable tools (raw value, expanded value, hint that points at `tools install <name>` when an installer is registered). → `src/main.cpp` |
| [x] | `tools install <name>` — discovers the registered bootstrap script (`build.py`) and invokes it via `python3` (override with `$PYTHON`). → `src/main.cpp` |
| [x] | `tools where <name>` — alias for `tools which <name>`. → `src/main.cpp` |
| [ ] | `tools env` — print the resolved registry path, the executable directory, and the effective tool table as JSON for scripting. |
| [ ] | `tools register <name> <executable> [--installer <script>]` — append an entry to the active registry from the command line; refuses to overwrite without `--force`. |
| [ ] | `tools unregister <name>` — drop an entry from the active registry. |
| [ ] | `tools install --download` — when an installer supports a download mode, pass it through. |
| [ ] | Per-tool `cwd` interpretation — currently the child inherits the parent CWD; consider supporting an alternate registry value form `{ "executable": "...", "cwd": "..." }` while keeping the simple string form as the default. |

---

## 2. Loose-Coupling Contract (binding)

These rules are non-negotiable. Each one keeps the dispatcher from
sliding into "god tool" territory.

| Status | Item |
|--------|------|
| [x] | `tools` does NOT include any header from `SagaEngine/`, `SagaEditor/`, `SagaServer/`, `SagaPrism/`, `SagaForge/`, or any other tool. |
| [x] | `tools` does NOT walk up to find `SagaEngineRoot.marker`; it operates in any working directory. |
| [x] | `tools` does NOT link against any third-party library. |
| [x] | `tools` does NOT contain any registered-tool's logic, defaults, or special-case handling. The Forge entry and the Prism entry pass through the same code path as a future `asset-lint` or `codegen`. |
| [x] | `tools` does NOT name a registered tool inside its source code. The fact that the default registry happens to mention "forge" and "prism" is a property of the JSON file, not of the binary. |
| [x] | Every binary in the registry remains directly runnable; the dispatcher is an additive convenience layer, not a gateway. |
| [ ] | `Tools/Scripts/check_tools_isolation.py` learns to grep `Tools/SagaTools/src/**` for forbidden includes and for any string literal containing `forge` / `prism`; fail CI on hit. |

---

## 3. Registry Schema

The registry is the dispatcher's only public contract with the rest of
the world. It must stay tiny.

| Status | Item |
|--------|------|
| [x] | Two-section JSON document: `tools: {name → executable}` and `installers: {name → script}`. → `config/tools.registry.json` |
| [x] | Either section is optional per tool. A tool with only `installers` is bootstrap-able but not yet launchable; a tool with only `tools` is launchable but ships pre-built. → `src/Registry.cpp` |
| [x] | `${VAR}` substring expansion against the process environment. → `src/Resolver.cpp` |
| [x] | Path values resolved relative to the registry-file directory. → `src/Resolver.cpp` |
| [x] | Bare-name values searched on `PATH` with platform-appropriate suffixes. → `src/Resolver.cpp` |
| [x] | Forward-compatibility — unknown top-level keys (including `schema_version`) are ignored silently; unknown value shapes inside the two known sections are an error. → `src/Registry.cpp` |
| [ ] | Schema-version bump policy documented in `SCHEMA.md`; required when the next breaking change lands. |
| [ ] | Per-tool checksum field for installers — `installers.<name>.sha256`; verified before `tools install` invokes the script. (Will move installers into structured objects in v2.0.) |

---

## 4. Build System and Distribution

| Status | Item |
|--------|------|
| [x] | Standalone `CMakeLists.txt` — `cmake_minimum_required(VERSION 3.22)`, `project(saga_tools)`, no `find_package` calls, no Conan, no marker file. → `CMakeLists.txt` |
| [x] | Post-build registry staging — `config/tools.registry.json` is copied next to the binary so a freshly built tree is usable without environment setup. → `CMakeLists.txt` |
| [x] | `install(TARGETS tools)` + registry install rule. → `CMakeLists.txt` |
| [x] | Strict warnings: `/W4 /permissive-` (MSVC), `-Wall -Wextra -Wpedantic` (GCC/Clang). → `CMakeLists.txt` |
| [x] | One-shot bootstrap (`setup.py`) — Python-3 stdlib script that builds the dispatcher, stages `bin/tools` + `bin/config/tools.registry.json`, runs a smoke test, and prints PATH instructions. The README's quick-start is a single invocation of this script. → `setup.py` |
| [x] | `setup.py --all` — chains `tools install forge` and `tools install prism` after building the dispatcher; partial failures are soft warnings (the dispatcher still ships even when one cascade fails). → `setup.py` |
| [ ] | CI workflow — matrix build across Linux (GCC, Clang) and Windows (MSVC, clang-cl); run `setup.py --all --no-smoke`, then `tools --help`, `tools list`, and a synthetic `tools <fixture-tool>` against a no-op binary. |
| [ ] | Pre-built signed binaries for Linux x86_64, Linux arm64, Windows x86_64, macOS arm64 attached to tagged releases. |
| [ ] | `setup.py --uninstall` — wipe `bin/`, `build/`, and (with `--purge`) the cascaded tool directories. |
| [ ] | `setup.py --download` — fetch a signed pre-built dispatcher binary instead of compiling from source; respects `--no-verify` for offline mirrors. |
| [ ] | Windows-friendly `setup.cmd` shim that just execs `python setup.py %*`. |

---

## 5. Standalone-Repo Extraction

| Status | Item |
|--------|------|
| [x] | Tool source lives entirely under `Tools/SagaTools/`; the contents become the new repo root with zero rewrites. → `README.md` |
| [x] | `README.md` is written for a reader who has never heard of SagaEngine. → `README.md` |
| [x] | No SagaEngine-specific paths, marker references, or include statements in any source / CMake / JSON file under the folder. → entire subtree |
| [ ] | `LICENSE` placeholder until the licensing decision is made. |
| [ ] | `.github/workflows/ci.yml` — a CI definition that works in a fresh repo with no SagaEngine context. |
| [ ] | `CHANGELOG.md` populated retroactively when the first release is tagged. |
| [ ] | Repo-split rehearsal — a one-shot script under `Tools/Scripts/` that copies `Tools/SagaTools/` into a temporary directory, runs the build, and reports success. |

---

## 6. Encoding & Path Robustness

The dispatcher routinely sees real-world paths with non-ASCII characters
(Turkish `Masaüstü`, German `Müller`, Japanese filenames, …). Every part
of the pipeline must round-trip them losslessly.

| Status | Item |
|--------|------|
| [x] | JSON parser decodes `\uXXXX` escapes (BMP) and surrogate pairs (>U+FFFF) into UTF-8. → `src/Registry.cpp` |
| [x] | `setup.py` writes the staged registry with `ensure_ascii=False`, so paths land as real UTF-8 bytes rather than `\uXXXX` escapes. → `setup.py` |
| [x] | MSVC: `/utf-8` flag on the `tools` target so source-file UTF-8 round-trips through string literals. → `CMakeLists.txt` |
| [x] | `Encoding.h` — UTF-8 ↔ UTF-16 conversion via `MultiByteToWideChar` and a `PathFromUtf8()` / `PathToUtf8()` pair that uses `fs::u8path` on Windows so the filesystem APIs treat dispatcher bytes as UTF-8 instead of ANSI. → `include/SagaTools/Encoding.h`, `src/Encoding.cpp` |
| [x] | `Resolver` and `Registry` use `PathFromUtf8` everywhere a `std::string` enters the filesystem layer — `fs::exists`, `fs::absolute`, `fs::weakly_canonical`, `ifstream`. → `src/Resolver.cpp`, `src/Registry.cpp` |
| [x] | Windows `ProcessRunner::Run` switched to `_wspawnvp` with per-arg UTF-8 → UTF-16 conversion, so `tools forge --version` actually launches when forge.exe lives at `C:\Users\…\Masaüstü\…\forge.exe`. → `src/ProcessRunner.cpp` |
| [x] | `GetEnvUtf8` — env var lookup via `_wgetenv` on Windows, `getenv` on POSIX; replaces every direct `std::getenv` call in the dispatcher. → `src/Encoding.cpp` |
| [x] | UTF-8 console at process start — `SetConsoleOutputCP(CP_UTF8)` + `SetConsoleCP(CP_UTF8)` so paths printed by the dispatcher render as `Masaüstü` instead of `Masa├╝st├╝`. → `src/main.cpp` |
| [x] | Mojibake diagnostic — when `tools <name>` resolves a path that doesn't exist and the path contains `uXXXX`-style fragments, the dispatcher tells the user to re-run `setup.py`. → `src/main.cpp` |
| [x] | OneDrive diagnostic — when an unavailable path lives under OneDrive, suggest moving the project to a clean location. → `src/main.cpp` |
| [x] | OneDrive auto-diagnostic removed — fired too often as a false positive (e.g. when the real problem was a failed install). The smarter "is this an unhydrated OneDrive placeholder?" check now lives in the open list below. → `src/main.cpp` |
| [x] | `setup.py` uses `os.path.abspath(__file__)` instead of `Path(__file__).resolve()` so Windows directory junctions (e.g. `C:\dev\SagaEngine` → `C:\Users\…\OneDrive\…`) are NOT silently followed. → `setup.py` |
| [x] | Windows command-line quoting — `ProcessRunner::Run` uses `CreateProcessW` directly with manually-built `lpCommandLine`, so paths containing spaces (`SagaEngine Versions/`) survive `_spawnvp`'s no-quoting concatenation behaviour. → `src/ProcessRunner.cpp` |
| [ ] | Test fixture — `Tests/encoding/` directory contains a registry and installer paths with deliberately "spicy" characters (`ü`, `ß`, `日本語`, surrogate-pair emoji) AND spaces in path components; CI runs `tools list` against it and diffs the output. |
| [ ] | Path-too-long diagnostic — on Windows, when a path > 260 chars fails the existence check, surface the long-path opt-in (`\\?\` prefix or `LongPathsEnabled` registry key). |
| [ ] | OneDrive placeholder detection — call `GetFileAttributesW` and check `FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS` / `FILE_ATTRIBUTE_RECALL_ON_OPEN`; fire a precise "this file is an unhydrated OneDrive cloud placeholder" diagnostic. |

---

## 7. One-Click Onboarding

The single most important property of this layer: a brand-new contributor
clones the repo and is productive in one command, with no manual
build-system knowledge.

| Status | Item |
|--------|------|
| [x] | `python3 Tools/SagaTools/setup.py --all` — the canonical one-command bootstrap. → `setup.py` |
| [x] | `Tools/SagaTools/setup.cmd` — Windows double-click launcher that locates Python (`py` / `python3` / `python`) and execs `setup.py --all` with `pause` so the window stays open. → `setup.cmd` |
| [x] | Generated registry uses absolute paths (no `${FORGE_BIN}` env var setup required after bootstrap). → `setup.py` |
| [ ] | `git clone … && tools build` — top-level convenience target that detects whether SagaTools has been bootstrapped and does it on demand. |
| [ ] | macOS double-click launcher — `Tools/SagaTools/setup.command` with a shebang and an executable bit so Finder will run it. |
| [ ] | Status check — `tools doctor` runs through every registered tool, reports resolvability, executable bit, version mismatch with manifest pins, and prints a single colour-coded summary. Sole purpose: a contributor's first sanity check. |
| [ ] | `setup.py --uninstall` — wipe `bin/`, `build/`, and (with `--purge`) the cascaded tool directories. |
| [ ] | `setup.py --download` — fetch a signed pre-built dispatcher binary instead of compiling from source. |

---

## 8. Future Direction — Rust Rewrite of the Dispatcher

The dispatcher's job is exactly the kind of work where memory-safe
systems languages shine: process spawn, path normalisation, JSON
parsing, environment expansion. The current C++ implementation has
already shipped four real bugs that a Rust rewrite would have caught
at compile time:

1. JSON `\uXXXX` decoding silently dropped the backslash and produced
   `Masau00fcstu00fc` instead of `Masaüstü`.
2. `argv[0]` was used to derive the executable directory, which fails
   when `tools` is invoked via `PATH`.
3. `std::filesystem::path(std::string)` interprets bytes through the
   active **ANSI code page** on Windows (CP1254 on Turkish, CP1252 on
   Western, …). UTF-8 bytes were silently mojibake'd into something
   that does not exist on disk — the dispatcher's `tools list` would
   show real UTF-8 (`Masaüstü`) but `fs::exists()` would resolve to
   `Masaüstü` and miss the file.
4. `_spawnvp` (narrow) routed argv through the same ANSI code page,
   so `tools forge --version` could not even launch a binary whose
   path contained non-ASCII characters.

Bugs 3 and 4 are direct consequences of the C++ filesystem and process
APIs taking `const char*` and quietly assuming the platform's narrow
encoding. **Rust's `PathBuf` / `OsString` / `Command` eliminate this
entire class of bug by construction:** there is no narrow vs. wide split
to forget, no console code page to set, no `u8path` to remember to call.

The Rust rewrite is **not** an excuse to redesign the architecture.
The boundary stays exactly as it is today:

```text
SagaTools (Rust)  →  os::Command::spawn(forge_exe, args)  →  forge.exe (C++)
```

No FFI. No shared library. No header sharing. The dispatcher launches
each tool the same way `make` launches `gcc` — process boundary, exit
code, nothing else.

| Status | Item |
|--------|------|
| [ ] | Decision recorded — Rust is the target language for a future v1.0 dispatcher. C++ remains for Forge (cmake / conan wrapper) and Prism (Clang LibTooling); both have hard reasons to live in C++. |
| [ ] | Crate layout sketch — `cargo new --bin sagatools-dispatcher` with modules `registry`, `resolver`, `runner`, mirroring today's C++ structure. |
| [ ] | Migration plan — the Rust binary publishes the SAME `tools` CLI surface and the SAME registry schema; a user upgrading sees no change. |
| [ ] | Path encoding — Rust's `PathBuf` plus `serde_json` removes the entire mojibake risk class by construction. |
| [ ] | Boundary lint — CI checks that the Rust dispatcher's `Cargo.toml` has zero `cc`/`bindgen`/FFI dependencies. The contract is "process spawn only". |
| [ ] | Stop point — the Rust rewrite is allowed to reuse the existing C++ test fixtures, but is not allowed to introduce features that Forge or Prism would also need. |

---

## 9. Future Scope (Post-1.0)

Items deferred until the dispatcher and at least two production tools
(Forge + Prism) have shipped through it in real use. Anything in this
list that turns the dispatcher into an orchestrator — pipelines,
caching, dependency graphs across tools — is automatically out of scope:
that role lives in a future, separate "pipeline" tool, not in `tools`.

| Status | Item |
|--------|------|
| [ ] | Shell-completion scripts for `bash` / `zsh` / PowerShell, generated from the built-in command list and the active registry. |
| [ ] | Optional XDG-style discovery — `~/.config/sagatools/tools.registry.json` and `/etc/sagatools/tools.registry.json` so system-wide installs are picked up automatically. |
| [ ] | `tools log` — re-print the last invocation's resolved path and exit code from a tiny on-disk audit file (off by default; opt-in via env). |
| [ ] | Transparent multi-host execution — `tools --host <ssh-target> <tool> [args]` exec'd over SSH; rejected unless the dispatcher can stay under ~500 LoC. |

---

## Definitions

- **Shipped**: implemented, code-reviewed, and present in the current
  repository state. Marked `[x]`.
- **Open**: planned and scoped but not yet implemented. Marked `[ ]`.
- **Post-1.0**: explicitly deferred; will not block a 1.0 tag.

When an item ships, replace `[ ]` with `[x]` and append a note naming the
files that represent the work, matching the convention used in the other
SagaEngine roadmap documents.
