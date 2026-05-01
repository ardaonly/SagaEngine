# Forge

> **Standalone build helper for C++ projects.**
> Knows nothing about Prism, nothing about the `tools` dispatcher,
> nothing about SagaEngine. Just a thin wrapper around `cmake`.

---

## Why this folder exists separately from `Tools/Forge/`

The parent folder `Tools/Forge/` historically holds the **engine-build**
orchestration (PowerShell scripts, Conan profiles, the engine's CMake
root). That code is engine-coupled and stays where it is.

This subfolder — `Tools/Forge/tool/` — is the **standalone Forge tool**.
It follows the SagaEngine Tools isolation policy
(`TOOLS_ROADMAP.md`, *Strict Isolation Policy*):

- No include of `SagaEngine/`, `SagaEditor/`, `SagaServer/`, `SagaSDE/`,
  `SagaPrism/`, or `SagaTools/` headers.
- No use of `SagaEngineRoot.marker`.
- No knowledge of any other tool's CLI, registry, build graph, or files.
- Lives in its own CMake target (`forge`).

The day the tool is extracted into its own repository, **the contents
of `tool/` become the new repo root with zero rewrites.**

---

## What Forge does

Forge wraps the three operations that every CMake-based C++ project
needs every day:

```text
forge configure  [--source=DIR] [--build=DIR] [--preset=NAME] [-- <extra cmake args>]
forge build      [--build=DIR] [--target=NAME] [--config=Release] [-- <extra cmake args>]
forge clean      [--build=DIR]
forge --help
forge --version
```

Each subcommand spawns the `cmake` binary as a child process and
forwards its exit code unchanged. Forge does not parse `CMakeLists.txt`,
does not maintain a build cache of its own, does not generate code,
does not ship a plugin API. It is a habit-saving keystroke wrapper, and
nothing more.

If `cmake` is not on the user's PATH, Forge prints a clean diagnostic
and exits non-zero. There is no fallback "Forge will build it itself"
mode — the wrapper boundary is deliberately strict.

---

## What Forge will never do

These are hard rules, enforced by code review:

| Rule | Why |
|------|-----|
| **No tool registry.** | Routing requests to other tools is the dispatcher's job, not Forge's. |
| **No knowledge of Prism.** | If Forge knew about Prism, splitting the repos would be a rewrite. |
| **No plugin discovery.** | Same reason. The day Forge starts loading external `.json` registries it has stopped being a build helper. |
| **No engine includes.** | Forge ships to non-SagaEngine consumers as well. |
| **No third-party libraries.** | The only thing Forge calls is `cmake`. Adding a JSON library, a YAML library, or a logger would dwarf the source code. |

If a user wants to chain Forge with Prism (or anything else), they
invoke them through the `tools` dispatcher (`Tools/SagaTools/`) or by
hand. **Forge does not orchestrate; it builds.**

---

## Build

Standalone build (no engine dependency, no Conan, no marker file):

```bash
cmake -S Tools/Forge/tool -B Tools/Forge/tool/build -DCMAKE_BUILD_TYPE=Release
cmake --build Tools/Forge/tool/build --target forge
```

Output: `Tools/Forge/tool/build/forge` (or `forge.exe` on Windows).

Once built, Forge has zero runtime dependencies beyond a `cmake` binary
on PATH at the moment a build subcommand is invoked.

---

## Smoke test

```bash
./Tools/Forge/tool/build/forge --help
./Tools/Forge/tool/build/forge --version

# Build something with Forge — point it at any CMake project on disk
./Tools/Forge/tool/build/forge configure --source=path/to/project --build=path/to/project/build
./Tools/Forge/tool/build/forge build     --build=path/to/project/build
./Tools/Forge/tool/build/forge clean     --build=path/to/project/build
```

---

## Relationship to the `tools` dispatcher

`tools` is a separate project that lives in `Tools/SagaTools/`. It maps
short logical names to executable paths and forwards arguments. From
Forge's perspective `tools` does not exist; from `tools`'s perspective
Forge is just one of several executables it can launch.

```text
$ tools forge build --build=path/to/project/build
            │
            └─ tools resolves "forge" → /usr/local/bin/forge
               and execs:  forge build --build=path/to/project/build
```

If you delete `tools` tomorrow, every `forge ...` command above keeps
working unchanged.
