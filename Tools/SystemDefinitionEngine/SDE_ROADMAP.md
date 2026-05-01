# System Definition Engine (SDE) — Roadmap

> Last updated: 2026-04-28
> Status: foundation laid; substantial open work below.
> Repository expectation: this folder is designed to extract into its
> own git repo on a single day's work.

The System Definition Engine is the **model definition + validation
layer** for game systems. It is not a save format, not an editor
feature, not a runtime asset. It is the compile-time-style stage
where item, skill, recipe, status-effect, dialogue, quest, and every
other game-system definition is *modelled, validated, and resolved*
before the runtime ever loads it.

## What problem this solves

In every previous engine the team has shipped, the same failure mode
recurs: a content author saves a JSON, the runtime loads it, and a
field that should reference an `Item` references an `Effect` instead
— or carries a number where the design says it must be `> 0` — or
references an id that does not exist. The crash happens hours later,
in a play test, miles away from the typo. SDE pulls that failure
forward into authoring time. By the time the runtime reads the data,
the data has already been validated against:

- a typed schema (every field has a declared type),
- relational constraints (every reference resolves to an existing
  entity of the right kind),
- per-field rules (range, regex, enum, custom predicate),
- cross-field rules (`if Recipe.timed = true then Recipe.duration > 0`),
- cross-system rules (`Skill.cost.element ∈ Item.element_pool`).

If anything fails, SDE refuses to compile and emits a structured
diagnostic that points at the offending field with line numbers.
The runtime never sees a malformed model.

## What SDE is *not*

- **Not a serialiser.** JSON / YAML / TOML are transport. SDE doesn't
  care which one is used; it cares about the model the transport
  describes.
- **Not the editor.** The editor is a UI for SDE. SDE has no UI.
- **Not the runtime.** The runtime consumes SDE's compiled output;
  SDE never runs gameplay.
- **Not a DSL** (yet). It's a typed model + validation engine. A DSL
  may grow on top once the model is stable; the current scope is
  smaller.

## Architectural Position

```
                                ┌─────────────────────┐
            (input, JSON/etc.)  │ ModelLoader         │
                            ───▶│                     │───▶ ModelGraph
                                └──────────┬──────────┘
                                           │
                          ┌────────────────┴───────────────┐
                          ▼                                ▼
                ┌─────────────────────┐          ┌──────────────────┐
                │ ReferenceResolver   │          │ ModelCompiler    │
                │  (cross-model refs) │          │  (rules, types)  │
                └──────────┬──────────┘          └────────┬─────────┘
                           │                              │
                           └─────────────┬────────────────┘
                                         ▼
                              ┌──────────────────────┐
                              │ CompiledModelGraph   │ ← consumed by editor + runtime
                              │ + Diagnostics        │
                              └──────────────────────┘
```

**The arrow is one-way.** Editor and runtime consume the compiled
output. SDE itself depends on neither.

## Sub-systems

### 1. Model Schema

The vocabulary content authors and engineers use to describe what
"an Item is" or what "a Skill is".

| Status | Item |
|--------|------|
| [ ] | `ModelDefinition` — the schema for one model kind (e.g. "Item"). Carries id, display name, fields, declared rules. |
| [ ] | `FieldDefinition` — one field on a model (id, type, optionality, default, per-field rules). |
| [ ] | `TypeDescriptor` — the type vocabulary: primitives (Number / Text / Boolean / Color), enums, references (`Ref<Other>`), collections (`List<T>`, `Map<K,V>`), structs. |
| [ ] | `Relation` — declared cross-model relationships ("Recipe.inputs is a non-empty list of Refs to Item"). |
| [ ] | `EnumDefinition` — closed set of named values; loaded once, referenced everywhere. |
| [ ] | Schema versioning — every model declaration carries a `schemaVersion`; older instances migrate forward through declared migration steps. |

### 2. Validation Rules

The constraints that turn a "compiles" yes/no into "compiles, with
this list of issues at these locations".

| Status | Item |
|--------|------|
| [ ] | `Rule` — abstract base; concrete kinds: `RangeRule`, `RegexRule`, `EnumMembershipRule`, `RefIntegrityRule`, `CrossFieldRule`, `CustomPredicateRule`. |
| [ ] | `Validator` — applies rules to a model instance and produces diagnostics. |
| [ ] | `Diagnostic` — file path, line, column, severity (Info / Warning / Error / Fatal), message, fix suggestion. |
| [ ] | `ValidationResult` — diagnostic list + a single overall verdict (`Pass / PassWithWarnings / Fail`). |
| [ ] | Rule registry — built-in rules + extension hook for project-specific rules (e.g. "every weapon has a damage curve"). |

### 3. Compilation

Turns raw input + schema + rules into a compiled graph the runtime
can consume directly.

| Status | Item |
|--------|------|
| [ ] | `ModelCompiler` — walks raw model instances, resolves references, runs rules, emits `CompiledModelGraph`. |
| [ ] | `ReferenceResolver` — turns string ids into typed pointers in the compiled graph. Detects dangling references and cycles. |
| [ ] | `CompiledModelGraph` — the validated, reference-resolved output. Read-only after compile. |
| [ ] | Incremental recompile — only the touched model + its dependents are recompiled when a source file changes. |
| [ ] | Deterministic output — the same input bytes always produce the same compiled bytes (hashable). |

### 4. I/O

Where SDE stops caring about syntax. Loaders translate any format
into the in-memory model instance representation; writers do the
reverse.

| Status | Item |
|--------|------|
| [ ] | `ModelLoader` — abstract loader interface. |
| [ ] | `JsonModelLoader` — concrete JSON-backed loader, friendly error messages with line / column. |
| [ ] | `BinaryModelLoader` — content-addressed compact format for shipped builds. |
| [ ] | `ModelWriter` — serialises a `CompiledModelGraph` back out (e.g. to a build artifact the runtime mmap's). |
| [ ] | Source maps — every compiled-graph node remembers the file/line/column it came from. |

### 5. Tooling

Optional surfaces built on top. Not required to use SDE; they exist
because the same engine that validates the data is the right place
to drive the developer-experience tools.

| Status | Item |
|--------|------|
| [ ] | `sde` CLI — `sde validate <project>`, `sde compile <project>`, `sde watch <project>`, `sde diff <a> <b>`. |
| [ ] | LSP server — text-editor integration: hover types, go-to-definition for references, real-time diagnostics. |
| [ ] | HTML report — pretty diagnostic dump for non-engineers. |
| [ ] | DSL stage — once the model is stable, a small DSL grows on top so authors can write rules and definitions in a domain-specific syntax. |

## Layout Map

```
Tools/SystemDefinitionEngine/
├── SDE_ROADMAP.md                     ← this file
├── README.md                          ← stand-alone repo entry point (when split off)
├── include/SagaSDE/
│   ├── Model/
│   │   ├── ModelDefinition.h          ← schema for one model kind
│   │   ├── FieldDefinition.h          ← one field
│   │   ├── TypeDescriptor.h           ← type vocabulary
│   │   ├── Relation.h                 ← declared cross-model relationships
│   │   └── EnumDefinition.h
│   ├── Validation/
│   │   ├── Diagnostic.h               ← framework-free; matches editor's IPanel style
│   │   ├── ValidationResult.h
│   │   ├── Rule.h
│   │   └── Validator.h
│   ├── Compilation/
│   │   ├── ModelCompiler.h
│   │   ├── ReferenceResolver.h
│   │   └── CompiledModelGraph.h
│   └── IO/
│       ├── ModelLoader.h
│       ├── JsonModelLoader.h
│       └── ModelWriter.h
├── src/SagaSDE/                       ← implementations mirror include/
└── tests/                             ← stand-alone test suite
```

## Strict Isolation Rules

- SDE **must not** include any `SagaEditor/` or `SagaEngine/` header.
  Verified by `Tools/Scripts/check_tools_isolation.py`.
- SDE depends only on the C++ standard library and (optionally) one
  JSON parser. Both vendored or fetched as conan deps.
- SDE produces a static library. Editor and runtime link it; SDE
  never links them.
- The `CompiledModelGraph` and `Diagnostic` types are the only
  public surface the editor / runtime need.

## Repository extraction checklist

When the day comes to split this folder into its own repo:

1. Copy `Tools/SystemDefinitionEngine/` to `<new-repo>/`.
2. Add a CMakeLists.txt at the new repo root (the static library +
   tests + CLI binary are already self-contained).
3. Move `SDE_ROADMAP.md` into the repo root or keep it as-is.
4. Done. No source rewrites. No shared headers.

## Where to start

1. `Diagnostic` value type — landed.
2. `TypeDescriptor` — primitives + Ref + List, designated for round-trip JSON.
3. `FieldDefinition` + `ModelDefinition` — schema vocabulary.
4. `Validator` + the first three rule kinds (`RangeRule`,
   `RefIntegrityRule`, `EnumMembershipRule`).
5. `JsonModelLoader` — parse a project's models, hand them to the
   compiler, emit diagnostics. CLI binary follows.
