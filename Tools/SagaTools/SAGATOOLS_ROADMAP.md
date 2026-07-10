# SagaTools — Tool Dispatcher Roadmap

> Last updated: 2026-05-14  
> Status: Active roadmap  
> Target: A thin, stable command dispatcher for Saga’s standalone developer tools.  

---

## 0. Roadmap Convention

- `[x]` — Shipped. The note after the item names the files, modules, or integration points that represent the work and highlights any decisions worth preserving.
- `[ ]` — Open. Either unstarted or partially explored; the item describes the finished production state rather than interim scaffolding.
- Shipped items must name the files, modules, or integration points that represent the completed work.
- Open items must describe the finished state, not temporary scaffolding.
- SagaTools must stay thin.
- SagaTools may discover, list, launch, and route commands to tools.
- SagaTools must not become a second build system, compiler, code intelligence engine, project shell, or editor backend.

---

## 1. Document Purpose

This document defines the roadmap for `SagaTools`.

`SagaTools` is the shared entry point for Saga’s standalone developer tools.

It exists to provide a consistent command surface such as:

```txt
sagatools forge ...
sagatools doctor ...
sagatools list
```

It does not own the tools it invokes.

Correct ownership:

```txt
SagaTools
  thin dispatcher

  deterministic data compiler

Forge
  build workflow frontend

  code intelligence and graph builder
```

Incorrect ownership:

```txt
SagaTools
  compiler
  build frontend
  graph database
  project shell
  editor helper
  random scripts
  unrelated runtime helpers
```

A dispatcher that owns everything is not a dispatcher; it is uncontrolled tool orchestration.

---

## 2. Companion Documents

| Document | Purpose |
|---|---|
| `docs/roadmaps/TOOLS_ROADMAP.md` | Tool ecosystem index and ownership map |
| `Tools/SagaTools/SAGATOOLS_ROADMAP.md` | SagaTools dispatcher roadmap |
| `Tools/Forge/FORGE_ROADMAP.md` | Forge build workflow frontend roadmap |
| `DependencyGraph.md` | Dependency and ownership boundary rules |
| `SHARED_ROADMAP.md` | Shared contracts consumed by approved tools |

---

## 3. Ownership Boundary

- [x] Define SagaTools as a thin tool dispatcher.

  Represented by:

  ```txt
  Tools/SagaTools/SAGATOOLS_ROADMAP.md
  docs/roadmaps/TOOLS_ROADMAP.md
  DependencyGraph.md
  ```

  Preserved decision:

  ```txt
  SagaTools dispatches to tools.
  SagaTools does not own tool implementation.
  ```

- [ ] Keep SagaTools responsible only for common command routing.

  Done means SagaTools owns:

  - top-level CLI entry point,
  - command discovery,
  - command routing,
  - tool listing,
  - process invocation,
  - argument forwarding,
  - common help formatting,
  - exit code forwarding,
  - common diagnostics envelope,
  - workspace-aware command context.

- [ ] Prevent SagaTools from owning tool internals.

  Done means SagaTools does not own:

  - Forge build graph execution,
  - Forge package build policy,
  - editor UI,
  - runtime/server execution,
  - product project lifecycle.

---

## 4. Tool Ecosystem Relationship

- [x] Define SagaTools as one member of the tool ecosystem, not the owner of the ecosystem.

  Represented by:

  ```txt
  docs/roadmaps/TOOLS_ROADMAP.md
  Tools/SagaTools/SAGATOOLS_ROADMAP.md
  ```


  Done means SagaTools can route commands such as:

  ```txt
  ```


- [ ] Integrate Forge as a dispatched tool.

  Done means SagaTools can route commands such as:

  ```txt
  sagatools forge build ...
  sagatools forge clean ...
  sagatools forge package ...
  sagatools forge doctor ...
  ```

  without owning Forge build workflow implementation.


  Done means SagaTools can route commands such as:

  ```txt
  ```


- [ ] Support future tools through a stable registration/discovery model.

  Done means a new tool can be added without editing unrelated tool implementation code.

---

## 5. CLI Surface

### 5.1 Top-Level Commands

- [ ] Provide stable top-level CLI commands.

  Required commands:

  ```txt
  sagatools help
  sagatools version
  sagatools list
  sagatools doctor
  sagatools forge
  ```

- [ ] Provide consistent help output.

  Done means:

  - every command has a short description,
  - every command has usage text,
  - every command lists arguments,
  - every command lists options,
  - unknown commands fail with helpful suggestions.

- [ ] Provide consistent version output.

  Done means version output includes:

  - SagaTools version,
  - build commit if available,
  - build profile,
  - supported tool protocol version,
  - discovered tool versions where available.

---

### 5.2 Argument Forwarding

- [ ] Forward unknown tool-specific arguments to the selected tool.

  Done means:

  - SagaTools parses only top-level routing arguments,
  - tool-specific parsing remains with the owning tool,
  - argument quoting is preserved,
  - paths with spaces work,
  - exit code is forwarded.

Correct:

```txt
```



Incorrect:

```txt
```

That is how “thin launcher” becomes “second compiler wearing a hat”.

---

### 5.3 Common Flags

- [ ] Support common top-level flags.

  Required common flags:

  ```txt
  --help
  --version
  --verbose
  --quiet
  --json
  --workspace <path>
  --config <path>
  --no-color
  ```

- [ ] Keep common flags separate from tool-specific flags.

  Done means:

  - SagaTools consumes only agreed top-level flags,
  - tool-specific flags are passed through,
  - conflicts are documented,
  - ambiguous flags produce clear errors.

---

## 6. Tool Discovery

- [ ] Add tool registry.

  Done means SagaTools can discover tools by:

  - built-in registry,
  - configured tool path,
  - workspace-local tool manifest,
  - environment variable,
  - development build output directory.

Expected files:

```txt
Tools/SagaTools/include/SagaTools/ToolRegistry.hpp
Tools/SagaTools/include/SagaTools/ToolDescriptor.hpp
Tools/SagaTools/src/ToolRegistry.cpp
```

- [ ] Add tool descriptor format.

  Done means each tool can describe:

  - tool id,
  - display name,
  - executable path,
  - version,
  - supported protocol version,
  - command summary,
  - capabilities,
  - stability level.

Expected files:

```txt
Tools/SagaTools/include/SagaTools/ToolDescriptor.hpp
Tools/SagaTools/include/SagaTools/ToolCapability.hpp
```

- [ ] Add missing-tool diagnostics.

  Done means missing tools report:

  - requested tool id,
  - searched paths,
  - workspace path,
  - expected executable names,
  - suggested build/install action.

---

## 7. Process Dispatch

- [ ] Add robust process launcher.

  Done means SagaTools can:

  - launch selected tool executable,
  - pass arguments safely,
  - pass environment variables,
  - set working directory,
  - stream stdout,
  - stream stderr,
  - forward exit code,
  - detect launch failure.

Expected files:

```txt
Tools/SagaTools/include/SagaTools/ProcessLauncher.hpp
Tools/SagaTools/include/SagaTools/ProcessResult.hpp
Tools/SagaTools/src/ProcessLauncher.cpp
```

- [ ] Support process timeout where appropriate.

  Done means:

  - optional timeout can be configured,
  - timeout produces clear diagnostic,
  - child process is terminated safely,
  - timeout exit code is stable.

- [ ] Support JSON diagnostic forwarding.

  Done means:

  - tools may emit structured diagnostics,
  - SagaTools can pass them through,
  - `--json` output remains machine-readable,
  - non-JSON tool output does not corrupt JSON mode unless explicitly allowed.

---

## 8. Workspace Awareness

- [ ] Add workspace context detection.

  Done means SagaTools can identify:

  - current working directory,
  - workspace root,
  - project manifest path,
  - tool config path,
  - build output root,
  - generated artifact root.

Expected files:

```txt
Tools/SagaTools/include/SagaTools/WorkspaceContext.hpp
Tools/SagaTools/include/SagaTools/WorkspaceLocator.hpp
Tools/SagaTools/src/WorkspaceLocator.cpp
```

- [ ] Pass workspace context to tools.

  Done means dispatched tools receive workspace context through:

  - command-line flags,
  - environment variables,
  - config file path,
  - explicit tool protocol input where supported.

- [ ] Keep project lifecycle ownership outside SagaTools.

  Done means SagaTools does not own:

  - project dashboard,
  - recent project registry,
  - product project creation,
  - product project opening,
  - editor mode switching.

Saga owns product lifecycle.

SagaTools helps tools run inside or against a workspace.

Those are not the same thing, despite humanity’s ongoing war against boundaries.

---

## 9. Diagnostics

- [ ] Add common diagnostics envelope.

  Done means SagaTools diagnostics include:

  - severity,
  - code,
  - message,
  - tool id,
  - command,
  - workspace path where relevant,
  - suggested next action where useful,
  - exit code.

Expected files:

```txt
Tools/SagaTools/include/SagaTools/Diagnostic.hpp
Tools/SagaTools/include/SagaTools/DiagnosticSeverity.hpp
Tools/SagaTools/src/Diagnostic.cpp
```

- [ ] Support human-readable diagnostics.

  Done means CLI output is clear for:

  - unknown command,
  - missing tool,
  - failed process launch,
  - invalid workspace,
  - tool crashed,
  - unsupported tool protocol version.

- [ ] Support machine-readable diagnostics.

  Done means `--json` can produce stable output for scripts and CI.

- [ ] Forward child tool diagnostics without pretending SagaTools created them.

  Done means diagnostics preserve source tool identity:

  ```txt
  sourceTool: forge
  ```

---

## 10. Exit Codes

- [ ] Define stable SagaTools exit codes.

  Required categories:

  ```txt
  0   success
  1   general failure
  2   invalid command
  3   invalid arguments
  4   missing tool
  5   tool launch failure
  6   tool execution failure
  7   workspace error
  8   configuration error
  9   protocol mismatch
  10  internal error
  ```

- [ ] Forward tool exit codes where appropriate.

  Done means:

  - child tool failure is visible,
  - SagaTools-specific launch/routing failures are distinguishable,
  - CI scripts can reliably detect failure class.

---

## 11. Configuration

- [ ] Add SagaTools configuration loading.

  Done means configuration can define:

  - tool search paths,
  - default workspace path,
  - output mode,
  - color mode,
  - logging level,
  - timeout policy,
  - custom tool aliases.

Expected files:

```txt
Tools/SagaTools/include/SagaTools/SagaToolsConfig.hpp
Tools/SagaTools/include/SagaTools/ConfigLoader.hpp
Tools/SagaTools/src/ConfigLoader.cpp
```

- [ ] Support workspace-local config.

  Done means SagaTools can read workspace-local tool config without requiring global installation.

- [ ] Support environment overrides.

  Expected environment variables:

  ```txt
  SAGATOOLS_HOME
  SAGATOOLS_TOOL_PATH
  SAGATOOLS_WORKSPACE
  SAGATOOLS_NO_COLOR
  SAGATOOLS_LOG_LEVEL
  ```

---

## 12. Tool Protocol

- [ ] Define minimal tool protocol versioning.

  Done means SagaTools and child tools can negotiate or report:

  - tool id,
  - tool version,
  - protocol version,
  - supported capabilities,
  - output format support.

Expected files:

```txt
Tools/SagaTools/include/SagaTools/ToolProtocol.hpp
Tools/SagaTools/include/SagaTools/ToolProtocolVersion.hpp
```

- [ ] Reject unsupported tool protocol versions clearly.

  Done means protocol mismatch reports:

  - expected version,
  - actual version,
  - tool path,
  - suggested rebuild/update action.

- [ ] Keep protocol minimal.


  It only needs enough metadata to route and report.

---

## 13. Language and Implementation Direction

- [ ] Pick one implementation direction and remove contradictory roadmap text.

  Done means the roadmap no longer mixes incompatible old C++ implementation assumptions with newer Rust direction unless both are explicitly justified.

- [ ] Keep implementation language decision separate from ownership decision.

  Ownership rule:

  ```txt
  SagaTools is thin dispatcher regardless of implementation language.
  ```

  If implemented in C++:

  ```txt
  Keep dependencies minimal.
  Avoid linking tool internals directly.
  Prefer process dispatch or narrow plugin interface.
  ```

  If implemented in Rust:

  ```txt
  Keep binary small.
  Keep command routing explicit.
  Avoid embedding tool implementations unless explicitly approved.
  ```

- [ ] Do not use implementation language migration as an excuse to expand scope.

  Done means rewriting SagaTools does not also turn it into:

  - compiler,
  - build orchestrator,
  - project manager,
  - editor integration layer,
  - code intelligence database.

A rewrite is not a personality transplant.

---



  Example commands:

  ```txt
  ```


  Done means SagaTools does not include:

  ```txt
  ```



---

## 15. Integration with Forge

- [ ] Route Forge commands through SagaTools.

  Example commands:

  ```txt
  sagatools forge build
  sagatools forge clean
  sagatools forge package
  sagatools forge doctor
  ```

- [ ] Preserve Forge ownership.

  Done means SagaTools does not own:

  - build graph policy,
  - dependency resolution,
  - package cooking,
  - build cache implementation,
  - artifact publishing.

- [ ] Forward Forge output and exit status.

  Done means build failures remain Forge failures, not vague SagaTools failures.

Because “tool failed” is not a diagnostic.

It is a shrug with a stack trace nearby.

---



  Example commands:

  ```txt
  ```


  Done means SagaTools does not own:

  - source indexing,
  - symbol graph generation,
  - include graph analysis,
  - code intelligence database,
  - semantic query engine.



---

## 17. Doctor Command

- [ ] Add `sagatools doctor`.

  Done means doctor checks:

  - SagaTools configuration,
  - workspace detection,
  - tool discovery,
  - Forge availability,
  - executable permissions,
  - protocol compatibility,
  - basic command launchability.

- [ ] Keep doctor shallow.

  Done means `sagatools doctor` does not deeply validate:

  - Forge build correctness,

Instead, it delegates deep checks:

```txt
sagatools forge doctor
```

SagaTools doctor verifies the dispatch layer.

Tool doctors verify their own tool internals.

Wild idea: the owning tool should own its own health checks.

---

## 18. List Command

- [ ] Add `sagatools list`.

  Done means list output includes:

  - tool id,
  - display name,
  - version if available,
  - path,
  - availability status,
  - protocol compatibility,
  - short description.

Example:

```txt
forge    Forge Build Frontend       available   Tools/Forge/...
```

- [ ] Support `sagatools list --json`.

  Done means machine-readable tool inventory is available for CI and scripts.

---

## 19. Logging

- [ ] Add SagaTools logging.

  Done means logs can capture:

  - selected command,
  - resolved workspace,
  - selected tool path,
  - process launch result,
  - exit code,
  - elapsed time,
  - errors.

- [ ] Keep logs free of unnecessary noise by default.

  Done means normal output remains readable.

  Verbose output can be enabled through:

  ```txt
  --verbose
  SAGATOOLS_LOG_LEVEL
  ```

---

## 20. Testing Roadmap

### 20.1 Unit Tests

- [ ] Add command parser tests.

  Required coverage:

  - known command,
  - unknown command,
  - help command,
  - common flags,
  - argument forwarding,
  - quoted paths,
  - invalid arguments.

- [ ] Add tool registry tests.

  Required coverage:

  - built-in tool registration,
  - missing tool,
  - duplicate tool id,
  - invalid descriptor,
  - protocol mismatch.

- [ ] Add workspace locator tests.

  Required coverage:

  - workspace found from current directory,
  - workspace found from explicit flag,
  - missing workspace,
  - invalid manifest,
  - environment override.

---

### 20.2 Integration Tests

- [ ] Add process dispatch integration tests.

  Done means tests verify:

  - child process launch,
  - stdout forwarding,
  - stderr forwarding,
  - exit code forwarding,
  - failed launch diagnostics,
  - timeout behavior.

- [ ] Add fake tool integration tests.

  Done means a fake tool can verify:

  - argument forwarding,
  - environment forwarding,
  - JSON output forwarding,
  - protocol metadata behavior.

- [ ] Add real tool smoke tests.

  Done means SagaTools can launch:

  - Forge doctor,

  when those tools are available in the build environment.

---

## 21. CI Requirements

- [ ] Add SagaTools CLI smoke test to CI.

  Required checks:

  ```txt
  sagatools --help
  sagatools version
  sagatools list
  sagatools doctor
  ```

- [ ] Add dependency boundary checks.

  Required forbidden dependencies:

  ```txt
  SagaTools → Forge implementation internals
  SagaTools → SagaEditor UI
  SagaTools → SagaServer private headers
  SagaTools → Saga product shell internals
  ```

- [ ] Add JSON output compatibility test.

  Done means `--json` output remains parseable and schema-stable.

---

## 22. Recommended File Layout

Recommended target layout:

```txt
Tools/SagaTools/
  SAGATOOLS_ROADMAP.md
  CMakeLists.txt or Cargo.toml
  README.md

Tools/SagaTools/include/SagaTools/
  CommandLine.hpp
  CommandResult.hpp
  Diagnostic.hpp
  DiagnosticSeverity.hpp
  ExitCode.hpp
  ProcessLauncher.hpp
  ProcessResult.hpp
  SagaToolsConfig.hpp
  ToolCapability.hpp
  ToolDescriptor.hpp
  ToolProtocol.hpp
  ToolProtocolVersion.hpp
  ToolRegistry.hpp
  WorkspaceContext.hpp
  WorkspaceLocator.hpp

Tools/SagaTools/src/
  main.cpp or main.rs
  CommandLine.cpp
  Diagnostic.cpp
  ProcessLauncher.cpp
  ToolRegistry.cpp
  WorkspaceLocator.cpp
  ConfigLoader.cpp

Tools/SagaTools/tests/
  CommandLineTests.cpp or command_line_tests.rs
  ToolRegistryTests.cpp or tool_registry_tests.rs
  ProcessLauncherTests.cpp or process_launcher_tests.rs
  WorkspaceLocatorTests.cpp or workspace_locator_tests.rs
```

This layout is illustrative.

The important architecture rule is:

```txt
SagaTools may know tools exist.
SagaTools must not absorb what tools do.
```

---

## 23. Migration Plan

- [ ] Remove contradictory old/new implementation language assumptions.

  Done means roadmap text no longer describes two incompatible SagaTools identities.

- [ ] Rewrite SagaTools as dispatcher-only.

  Done means the roadmap and code both reflect:

  ```txt
  route
  launch
  forward
  diagnose
  exit
  ```


- [ ] Move Forge-specific roadmap content back to `FORGE_ROADMAP.md`.


- [ ] Keep common tool ecosystem overview in `TOOLS_ROADMAP.md`.

- [ ] Add dependency checks preventing SagaTools from importing tool internals.

---

## 24. Non-Goals

SagaTools does not own:

- Forge build workflow implementation,
- Saga product shell,
- editor UI,
- runtime/server execution,
- collaboration implementation,
- package cooking internals,
- source indexing internals,
- graph database internals,
- project dashboard UX.

Related ownership:

| Area | Owner |
|---|---|
| Tool ecosystem index | `docs/roadmaps/TOOLS_ROADMAP.md` |
| Tool dispatcher | `Tools/SagaTools/SAGATOOLS_ROADMAP.md` |
| Build workflow frontend | `Tools/Forge/FORGE_ROADMAP.md` |
| Product shell | `Saga` |
| Shared contracts | `SagaShared` |

---

## 25. Production Definition of Done

- [ ] SagaTools has a stable top-level CLI.

- [ ] SagaTools can list available tools.


- [ ] SagaTools can route commands to Forge.


- [ ] SagaTools forwards arguments safely.

- [ ] SagaTools forwards stdout, stderr, and exit codes.

- [ ] SagaTools supports human-readable and JSON diagnostics.

- [ ] SagaTools detects workspace context.

- [ ] SagaTools reports missing tools clearly.

- [ ] SagaTools validates tool protocol compatibility.

- [ ] SagaTools has unit and integration tests.

- [ ] CI verifies basic commands.

- [ ] CI prevents SagaTools from depending on tool implementation internals.

---

## 26. Final Architecture Rule

SagaTools should remain:

```txt
thin,
boring,
stable,
predictable,
and easy to replace.
```

It should know:

```txt
which tool was requested,
where that tool is,
which arguments to forward,
how to report failure,
and what exit code to return.
```

It should not know:

```txt
how Forge builds packages,
how Saga opens projects,
how the editor renders panels,
or how the server simulates authority.
```

A dispatcher is successful when nothing interesting happens inside it.

That may offend someone’s urge to engineer a cathedral around `argv`.

Good.
