# System Definition Engine (SDE) — Roadmap

> Last updated: 2026-05-08
> Status: core pipeline complete; open work documented below.
> Namespace: `SDE` (not `SagaSDE` — designed for standalone-repo extraction).
> Repository expectation: this folder is designed to extract into its
> own git repo on a single day's work.

The System Definition Engine is the **model definition + validation + compilation
layer** for game systems. It is not a save format, not an editor feature, not a
runtime asset. It is the compile-time stage where item, skill, recipe,
status-effect, dialogue, quest, and every other game-system definition is
*modelled, validated, and resolved* before the runtime ever loads it.

## What problem this solves

In every previous engine the team has shipped, the same failure mode recurs: a
content author saves a JSON, the runtime loads it, and a field that should
reference an `Item` references an `Effect` instead — or carries a number where the
design says it must be `> 0` — or references an id that does not exist. The crash
happens hours later, in a play test, miles away from the typo. SDE pulls that
failure forward into authoring time. By the time the runtime reads the data, the
data has already been validated against:

- a typed schema (every field has a declared type),
- relational constraints (every reference resolves to an existing entity of the
  right kind, identified by a compound `(modelId, instanceId)` key),
- per-field rules (range, regex, enum membership),
- cross-field rules (`if Recipe.timed = true then Recipe.duration > 0`),
- cross-system rules (`Skill.cost.element ∈ Item.element_pool`).

If anything fails, SDE refuses to compile and emits a structured diagnostic that
points at the offending field with file path, line, and column. The runtime never
sees a malformed model.

## What SDE is *not*

- **Not a serialiser.** JSON / YAML / TOML are transport. SDE doesn't care which
  one is used; it cares about the model the transport describes.
- **Not the editor.** The editor is a UI for SDE. SDE has no UI.
- **Not the runtime.** The runtime consumes SDE's compiled output; SDE never runs
  gameplay.
- **Not a DSL** (yet). It is a typed model + validation engine. A DSL may grow on
  top once the model is stable; the current scope is smaller.

## Architectural position

```
  File Input (JSON / YAML / …)
          │
          ▼
  ┌──────────────────┐
  │   ModelLoader    │  parse bytes → RawValue tree (no business logic)
  └────────┬─────────┘
           │  std::vector<ModelInstance>
           ▼
  ┌──────────────────┐
  │    Validator     │  schema checks + per-field rules + cross-field Predicate AST
  └────────┬─────────┘
           │  ValidationResult  (CompileState + Diagnostics)
           ▼
  ┌──────────────────────┐
  │  ReferenceResolver   │  (modelId, instanceId) compound-key lookup
  │                      │  dangling-ref detection, DFS cycle detection
  └────────┬─────────────┘
           │  ResolveResult (resolvedHandles map)
           ▼
  ┌──────────────────────┐
  │   ModelCompiler      │  assemble CompiledModelGraph via ValueSlab
  └────────┬─────────────┘
           │
           ▼
  ┌──────────────────────┐
  │  CompiledModelGraph  │  ← consumed by editor + runtime (read-only, move-only)
  │  + Diagnostics       │
  └──────────────────────┘
```

The arrow is one-way. Editor and runtime consume the compiled output. SDE itself
depends on neither.

---

## Sub-systems

### 1. Model Schema

| Status | Item |
|--------|------|
| [x] | `TypeNode` / `TypeRegistry` — flat handle-based type system. `TypeNodeId` is a `uint32_t`; `TypeRegistry` owns all nodes and is frozen before the pipeline runs. Replaces the recursive `std::variant<..., unique_ptr<T>>` TypeDescriptor design. |
| [x] | `TypeKind` — Number, Integer, Text, Boolean, Color, Enum, Ref, List, Map, Struct. `Integer` and `Number` are distinct (int64 vs double). |
| [x] | `EnumDefinition` — closed set of named values (id, displayName, members). `ContainsMember()` and `FindMember()` helpers. |
| [x] | `FieldDefinition` — id, displayName, `TypeNodeId`, `FieldPresence`, optional default, per-field `shared_ptr<Rule>` list. |
| [x] | `Relation` — declared cross-model relationship with cardinality and `nonEmpty` constraint. |
| [x] | `ModelDefinition` — id, displayName, schemaVersion, fields, relations, `crossFieldRules`. |
| [ ] | **Schema versioning / migration** — every model declaration carries a `schemaVersion`. When a schema changes, instances at the old version must migrate forward through declared migration steps before validation runs. No design exists yet. |
| [ ] | **Struct field nesting** — `TypeKind::Struct` is defined in `TypeNode` but `StructFieldSpec` registration and the inline anonymous struct path in `JsonModelLoader` are not implemented. |

### 2. Validation Rules

| Status | Item |
|--------|------|
| [x] | `Severity` — Info / Warning / Error / Fatal. |
| [x] | `SourceLocation` — file, line, column. |
| [x] | `Diagnostic` — MakeInfo / MakeWarning / MakeError / MakeFatal factory methods. |
| [x] | `CompileState` — 6-state enum (Clean / WithWarnings / UnresolvedRefs / ValidationFailed / Aborted / IOError). `Merge()`, `IsUsable()`, `StateName()`. Relational operators (`<`, `<=`, `>`, `>=`) for severity comparisons. |
| [x] | `ValidationResult` — state + diagnostics, `HasErrors()`, `HasWarnings()`, `Merge()`. |
| [x] | `Rule` — abstract base with `Apply(RawValue, FieldDefinition, SourceLocation, diagnostics)` + `RuleId()`. |
| [x] | `RangeRule` — enforces `min <= value <= max` on Number and Integer fields. Emits `SDE_RANGE`. |
| [x] | `RegexRule` — enforces text matches `std::regex`. Emits `SDE_REGEX`. |
| [x] | `EnumMembershipRule` — stores `enumId`; **stub only** (see open work). Emits `SDE_ENUM_MEMBER`. |
| [x] | `Predicate` — abstract serializable AST node for cross-field conditions. `PredicateKind()` returns a stable string for JSON round-trip. |
| [x] | `FieldExistsPredicate` — true when a named field is present and non-null. |
| [x] | `FieldEqualsPredicate` — true when field value == expected string. |
| [x] | `FieldRangePredicate` — true when numeric field satisfies `[min, max]`. |
| [x] | `AndPredicate`, `OrPredicate` — logical combinators. |
| [x] | `ImplicationPredicate` — P → Q (implemented as ¬P ∨ Q). |
| [x] | `CrossFieldRule` — applies a `Predicate` tree to an entire `ModelInstance`. Not a `Rule` subclass — held separately in `ModelDefinition::crossFieldRules`. |
| [x] | `RuleRegistry` — dependency-injected (no singleton). `Register()`, `Freeze()`, `Find()`, static `RegisterBuiltIns()`. |
| [x] | `Validator` — `Validate()` (batch), `ValidateOne()`, `CheckTypeMatch()`, `ApplyFieldRules()`, `ApplyCrossFieldRules()`. |
| [ ] | **`EnumMembershipRule::Apply()` is a stub.** It always returns `true`. The actual membership check requires the `Validator` to hold a map of `EnumDefinition` objects (keyed by enumId), which it does not currently receive. Design needed: pass an `EnumRegistry` (or equivalent) alongside `TypeRegistry` in `Validator`'s constructor. |
| [ ] | **`RefIntegrityRule`** — per-field rule enforcing that a Ref value string is non-empty and syntactically valid (before the resolver runs). Currently missing from the implementation. |
| [ ] | **Predicate JSON serialization.** Each `Predicate` subclass has a `PredicateKind()` string to enable round-trips, but `ToJson()` / `FromJson()` are not implemented. Required before DSL compilation or editor visualization of cross-field rules. |
| [ ] | **Rule declaration in schema files.** The `JsonModelLoader::ParseFieldDefinition()` reads id/type/presence/default but ignores a `"rules"` array. Per-field rules (RangeRule, RegexRule) cannot yet be declared in JSON schema files; they must be wired in code. |

### 3. Compilation

| Status | Item |
|--------|------|
| [x] | `ValueSlab` — flat contiguous storage for array and object values. `Reserve()` returns a `SlabRange`; `At()` gives const access. Only `ModelCompiler` may write to it (`friend class ModelCompiler`). |
| [x] | `CompiledValue` — `std::variant<CompiledNull, CompiledInteger, CompiledNumber, CompiledBool, CompiledText, CompiledInstanceRef, CompiledArrayRef, CompiledObjectRef>`. No recursive heap pointers. |
| [x] | `CompiledInstance` — modelId, instanceId, origin, fields map, `GetField()`. |
| [x] | `CompiledModelGraph` — move-only, compound `(modelId, instanceId)` key (`PairHash`), `Find()`, `AllOf()`, `ModelIds()`, `TotalCount()`, `Slab()`. Immutable after `ModelCompiler::Compile()` returns. |
| [x] | `ReferenceResolver` — two-pass (BuildIndex → Resolve). Compound `(modelId, instanceId)` key prevents cross-model id collisions. Emits `SDE_DANGLING_REF` on missing targets. DFS cycle detection (White/Gray/Black coloring) emitting `SDE_CYCLE`. |
| [x] | `CompileOptions` — `abortOnFirstError`, `inferDefaults`. |
| [x] | `CompileResult` — state, `optional<CompiledModelGraph>`, `ValidationResult`. Graph is `nullopt` when state ≥ `UnresolvedRefs`. |
| [x] | `ModelCompiler` — `InferDefaults()` → `Validator` → `ReferenceResolver` → `BuildGraph()`. Stops before graph build when references are broken. |
| [ ] | **Array and object values in `BuildGraph()` are shallow.** `CompiledArrayRef` slots are filled with `CompiledNull` placeholders; nested `RawValue` elements are not recursively compiled into the slab. Full recursive compilation is needed before array/object fields are usable at runtime. |
| [ ] | **Incremental recompile.** When a single source file changes, only the touched model and its direct dependents should be recompiled. No dependency tracking or dirty-marking mechanism exists yet. |
| [ ] | **Deterministic output.** The same input bytes must always produce identical compiled bytes (required for content hashing and caching). Map iteration order in `CompiledModelGraph` is hash-map order (non-deterministic). `std::map` used in `CompiledInstance::fields` is deterministic, but `mInstances` (unordered) is not. |

### 4. I/O

| Status | Item |
|--------|------|
| [x] | `RawValue` — `variant<RawNull, RawInteger, RawNumber, RawBool, RawText, RawArray, RawObject>`. Carries its own `SourceLocation` at every nesting level. `IsInteger()` / `IsNumber()` / `AsDouble()` / `AsInteger()` helpers. `RawInteger` (int64) and `RawNumber` (double) are distinct arms. |
| [x] | `ModelInstance` — modelId, instanceId, sourceFile, origin, fields map. |
| [x] | `ModelLoader` — abstract interface. |
| [x] | `JsonModelLoader::Load()` — data file format: `{ "modelId": "...", "data": [...] }`. Emits `SDE_IO_ERROR`, `SDE_PARSE_ERROR`, `SDE_BAD_FORMAT`. nlohmann/json isolated to `JsonModelLoader.cpp`. |
| [x] | `JsonModelLoader::LoadDefinition()` — schema file loading. Parses id, displayName, schemaVersion, fields (id, type string, presence, default). Registers types into a caller-supplied `TypeRegistry`. |
| [x] | Type string parser — handles `"Number"`, `"Integer"`, `"Text"`, `"Boolean"`, `"Color"`, `"Ref<X>"`, `"Enum<X>"`, `"List<T>"`, `"Map<K,V>"` (recursive). |
| [x] | `JsonModelWriter` — serialises a `CompiledModelGraph` to JSON. `CompiledInstanceRef` → `{"$ref":"modelId/instanceId"}`. |
| [ ] | **Source locations carry `line = 0`, `column = 0` for all JSON values.** nlohmann/json's DOM API does not expose per-token line numbers. Fixing this requires switching to a SAX-style parse or wrapping nlohmann's internal lexer. Until fixed, diagnostics point at the file but not the line. |
| [ ] | **`BinaryModelLoader`** — compact content-addressed format for shipped builds. Not designed or started. |
| [ ] | **`ModelLoader` for YAML / TOML.** Only JSON is implemented. Adding YAML requires a vendored parser (e.g. yaml-cpp); the abstract interface is already in place. |

### 5. Tooling

| Status | Item |
|--------|------|
| [x] | `sde validate <project-dir>` — runs Validator only; exit codes 0 (Clean), 1 (WithWarnings), 2 (Fail), 3 (Usage), 4 (IO). |
| [x] | `sde compile <project-dir> [--out=<file>]` — full pipeline; writes `compiled.json` via `JsonModelWriter`. |
| [x] | `sde --version` / `sde --help`. |
| [x] | Diagnostic output format: `file:line:col: [SEVERITY] [CODE] message`. |
| [ ] | **`sde watch <project-dir>`** — file-system watcher; recompile on change, print diffs. Requires `inotify` / `kqueue` / `FSEvents` integration or a polling fallback. |
| [ ] | **`sde diff <compiled-a> <compiled-b>`** — structural diff of two compiled graphs (field added / removed / changed). Useful for CI change detection. |
| [ ] | **LSP server** — text-editor integration: hover type, go-to-definition for Ref fields, inline diagnostics on save. Requires the Language Server Protocol stdio transport and a JSON-RPC layer. |
| [ ] | **HTML diagnostic report** — pretty dump for non-engineers. Input: `ValidationResult` + source files; output: self-contained HTML with syntax-highlighted excerpts and severity badges. |
| [ ] | **DSL stage** — once the model is stable, a small domain-specific syntax for writing schemas and rules (instead of raw JSON). Requires Predicate JSON serialization (see §2) as a prerequisite. |

### 6. Test Coverage

| Status | Item |
|--------|------|
| [x] | `DiagnosticTests` — factory methods, severity, code, message, suggestion, SourceLocation. |
| [x] | `CompileStateTests` — `Merge()` ordering, commutativity, `IsUsable()`, `StateName()`. |
| [x] | `TypeRegistryTests` — Primitive / Ref / List registration, `TypeName()` formatting, freeze. |
| [x] | `RawValueTests` — Integer vs Number distinction, `AsDouble()` coercion, location preservation. |
| [x] | `RangeRuleTests` — RuleId, pass/fail, boundary values, integer raw values, non-numeric fail. |
| [x] | `JsonModelLoaderTests` — missing file → Fatal, valid array → 2 instances, integer field → RawInteger, float → RawNumber, malformed JSON, missing modelId/data, LoadDefinition. |
| [x] | `ReferenceResolverTests` — valid ref → resolvedHandles, dangling ref → SDE_DANGLING_REF, same instanceId in two models (no collision), ref to wrong model, self-reference cycle → SDE_CYCLE. |
| [x] | `ModelCompilerTests` — empty compile → Clean+graph, text/integer fields in graph, AllOf/ModelIds, dangling ref → UnresolvedRefs, default inference. |
| [x] | `FullPipelineTests` — load JSON → compile → Clean+graph, cross-model Ref → CompiledInstanceRef, dangling ref → UnresolvedRefs, same id in two namespaces. |
| [ ] | **`EnumDefinitionTests`** — ContainsMember, FindMember, empty members. |
| [ ] | **`FieldDefinitionTests`** — presence defaults, default value, TypeNodeId handle stored. |
| [ ] | **`ModelDefinitionTests`** — FindField, HasField, crossFieldRules. |
| [ ] | **`RegexRuleTests`** — matching, non-matching, non-text value. |
| [ ] | **`EnumMembershipRuleTests`** — blocked until the stub is replaced with a real implementation. |
| [ ] | **`PredicateTests`** — ImplicationPredicate (P→Q), AndPredicate, OrPredicate, FieldEqualsPredicate, FieldRangePredicate with valid/invalid ModelInstances. |
| [ ] | **`ValidatorTests`** — missing required field → SDE_MISSING_FIELD, type mismatch → SDE_TYPE_MISMATCH, cross-field rule failure, no definition for model → SDE_UNKNOWN_MODEL. |

---

## Open work — design notes

This section documents the *how* for every unchecked item. Items are ordered by
implementation wave; later waves depend on earlier ones or are lower urgency.

---

### Wave 1 — Correctness gaps (fix before any new feature)

These items make the system produce wrong output silently. They block correct usage
of the pipeline in production schemas.

#### 1.1 Array / object recursive compilation

**Problem.** `ModelCompiler::BuildGraph()` allocates slab slots for arrays and
objects but fills every slot with `CompiledNull`. Nested `RawValue` elements are
never converted. Any array or map field in a compiled graph is currently garbage.

**Fix.** Add a `RawToCompiled()` helper (anonymous namespace inside
`ModelCompiler.cpp`). Replace the stub visitor arms in `BuildGraph()` with calls
to this function.

```
// Inside ModelCompiler.cpp (anonymous namespace)
CompiledValue RawToCompiled(const RawValue& raw, ValueSlab& slab,
                             const ReferenceResolver::ResolveResult& resolved,
                             const std::string& handlePrefix)
{
    return std::visit([&](const auto& v) -> CompiledValue {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, RawNull>)    return CompiledValue{CompiledNull{}};
        if constexpr (std::is_same_v<T, RawInteger>) return CompiledValue{static_cast<CompiledInteger>(v)};
        if constexpr (std::is_same_v<T, RawNumber>)  return CompiledValue{static_cast<CompiledNumber>(v)};
        if constexpr (std::is_same_v<T, RawBool>)    return CompiledValue{static_cast<CompiledBool>(v)};
        if constexpr (std::is_same_v<T, RawText>)    return CompiledValue{CompiledText{v}};
        if constexpr (std::is_same_v<T, RawArray>) {
            uint32_t  n     = static_cast<uint32_t>(v.elements.size());
            SlabRange range = slab.Reserve(n);
            for (uint32_t i = 0; i < n; ++i)
                slab.mSlots[range.offset + i] = RawToCompiled(v.elements[i], slab, resolved, "");
            return CompiledValue{CompiledArrayRef{range}};
        }
        if constexpr (std::is_same_v<T, RawObject>) {
            uint32_t  pairCount = static_cast<uint32_t>(v.fields.size());
            SlabRange range     = slab.Reserve(pairCount * 2);
            uint32_t  i         = range.offset;
            for (const auto& [k, val] : v.fields) {
                slab.mSlots[i++] = CompiledValue{CompiledText{k}};
                slab.mSlots[i++] = RawToCompiled(val, slab, resolved, "");
            }
            return CompiledValue{CompiledObjectRef{range}};
        }
        return CompiledValue{CompiledNull{}};
    }, raw.data);
}
```

Ref fields are handled before `RawToCompiled` is called (via the
`resolved.resolvedHandles` map), so the function does not need to know about
references.

---

#### 1.2 EnumMembershipRule + EnumRegistry

**Problem.** `EnumMembershipRule::Apply()` always returns `true`. The `Validator`
has no access to `EnumDefinition` objects — it only holds a `TypeRegistry` (which
stores the enumId string but not the allowed members).

**Fix.** Introduce `EnumRegistry`, parallel to `RuleRegistry`. Pass it into
`Validator`.

```
// include/SDE/Model/EnumRegistry.h  (new file)
class EnumRegistry {
public:
    void Register(EnumDefinition def);
    void Freeze();
    [[nodiscard]] bool IsFrozen() const noexcept;
    [[nodiscard]] const EnumDefinition* Find(const std::string& enumId) const noexcept;
private:
    std::map<std::string, EnumDefinition> mDefs;
    bool mFrozen = false;
};
```

`Validator` constructor gains a fourth parameter:

```
Validator(const RuleRegistry&,
          const TypeRegistry&,
          const EnumRegistry&);   // new
```

`CheckTypeMatch()` for `TypeKind::Enum` then becomes:

```
case TypeKind::Enum:
    if (!value.IsText()) { kindMismatch("Enum (text)"); break; }
    if (const EnumDefinition* ed = mEnumRegistry.Find(node.enumId))
        if (!ed->ContainsMember(std::get<RawText>(value.data)))
            out.push_back(Diagnostic::MakeError(loc, "SDE_ENUM_MEMBER",
                "Field '" + field.id + "': value '" +
                std::get<RawText>(value.data) + "' is not a member of enum '" +
                node.enumId + "'."));
    break;
```

`ModelCompiler` gains a matching `EnumRegistry const&` member. `cli/main.cpp`
constructs and freezes the registry before calling the compiler.

`JsonModelLoader::LoadDefinition()` gains an optional `EnumRegistry*` parameter.
If the schema JSON contains a top-level `"enums"` array, the loader registers each
entry:

```json
{
  "id": "Item",
  "enums": [
    { "id": "Rarity", "members": [
        { "name": "Common",    "value": "common"    },
        { "name": "Rare",      "value": "rare"      },
        { "name": "Legendary", "value": "legendary" }
    ]}
  ],
  "fields": [
    { "id": "rarity", "type": "Enum<Rarity>", "presence": "required" }
  ]
}
```

---

#### 1.3 Rule declaration in JSON schema files

**Problem.** `JsonModelLoader::ParseFieldDefinition()` reads id, type, presence,
and default but silently ignores any `"rules"` key. Per-field constraints must
currently be wired in code after loading.

**Target schema field format:**

```json
{
  "id": "damage",
  "type": "Number",
  "presence": "required",
  "rules": [
    { "kind": "SDE_RANGE",       "min": 0.0, "max": 9999.0 },
    { "kind": "SDE_REGEX",       "pattern": "^[0-9]+(\\.[0-9]+)?$" },
    { "kind": "SDE_ENUM_MEMBER", "enumId": "DamageType" }
  ]
}
```

**Fix.** Add `ParseRule()` inside `JsonModelLoader.cpp` (anonymous namespace):

```
std::shared_ptr<Rule> ParseRule(const nlohmann::json& obj,
                                 const std::string&    file,
                                 std::vector<Diagnostic>& out)
{
    if (!obj.contains("kind") || !obj["kind"].is_string()) {
        out.push_back(Diagnostic::MakeError({file,0,0}, "SDE_MISSING_RULE_KIND",
            "Rule object has no 'kind' field."));
        return nullptr;
    }
    const std::string kind = obj["kind"];

    if (kind == "SDE_RANGE") {
        double min = obj.value("min", -std::numeric_limits<double>::max());
        double max = obj.value("max",  std::numeric_limits<double>::max());
        return std::make_shared<RangeRule>(min, max);
    }
    if (kind == "SDE_REGEX")
        return std::make_shared<RegexRule>(obj.value("pattern", ""));
    if (kind == "SDE_ENUM_MEMBER")
        return std::make_shared<EnumMembershipRule>(obj.value("enumId", ""));

    out.push_back(Diagnostic::MakeError({file,0,0}, "SDE_UNKNOWN_RULE",
        "Unknown rule kind: '" + kind + "'."));
    return nullptr;
}
```

`ParseFieldDefinition()` then iterates `obj["rules"]` and appends each result to
`fd.rules`.

---

#### 1.4 RefIntegrityRule

**Problem.** A `Ref<Item>` field whose raw value is an empty string `""` or
whitespace-only passes the `Validator`'s `CheckTypeMatch` (which only checks
`IsText()`) and proceeds to `ReferenceResolver` where it produces a confusing
`SDE_DANGLING_REF` error with an empty target id. A dedicated per-field rule
catches this earlier with a clearer message.

**Design.** Small, standalone, no dependencies beyond `Rule.h`:

```
// Added to Rule.h
class RefIntegrityRule final : public Rule {
public:
    bool Apply(const RawValue& value, const FieldDefinition& field,
               const SourceLocation& loc,
               std::vector<Diagnostic>& out) const override;
    [[nodiscard]] std::string RuleId() const override { return "SDE_REF_INTEGRITY"; }
};
```

`Apply()` logic: confirm `IsText()`; confirm the string is not empty; confirm it
contains no whitespace (Ref ids are identifiers, not display strings). Emits
`SDE_REF_INTEGRITY` with a message like: `"Field 'owner': Ref id must be a
non-empty identifier string."`.

`RuleRegistry::RegisterBuiltIns()` — currently a no-op — can attach a
`RefIntegrityRule` as the default rule for every field with `TypeKind::Ref`. The
alternative is to inject it automatically in `Validator::CheckTypeMatch` without
going through `RuleRegistry`; the former is more explicit.

---

### Wave 2 — Diagnostics quality

#### 2.1 JSON source locations (line = 0, column = 0)

**Problem.** nlohmann/json's DOM API discards token positions during parse. Every
`RawValue::location.line` and `.column` is `0`. Diagnostics point at the file but
not the line.

**Two-pass fix (no new dependencies, works within current nlohmann setup):**

1. Before calling `nlohmann::json::parse()`, scan the raw text once to build a
   table mapping byte offsets to `(line, column)`:

   ```
   // OffsetMap[i] = {line, col} for byte i in the source string.
   struct LineCol { int line; int column; };
   std::vector<LineCol> BuildOffsetMap(const std::string& text);
   ```

2. After the DOM is parsed, re-scan the source text to find the byte offset of
   each field's key string. Use the offset map to look up `(line, col)`.

   This is approximate — string literals that appear multiple times in the document
   may match the wrong occurrence — but accurate enough for single-occurrence field
   keys in typical game data files.

**SAX-based fix (preferred, more accurate):**

Use `nlohmann::json::sax_parse()` with a custom `SAXHandler`:

```
struct SAXHandler {
    // Called by nlohmann for each key or value token.
    bool key(std::string& s);
    bool string(std::string& s);
    bool number_integer(int64_t v);
    bool number_float(double v, const std::string&);
    bool boolean(bool v);
    bool null();
    bool start_object(std::size_t);
    bool end_object();
    bool start_array(std::size_t);
    bool end_array();
    bool parse_error(std::size_t byte, const std::string& token,
                     const nlohmann::json::exception& ex);

    // Internal state: current path, position from byte offset.
    // Output: map<vector<string>, SourceLocation> — path → location.
};
```

The SAX handler receives byte offsets via `parse_error`'s `byte` parameter on
error; for the happy path, byte offset tracking requires wrapping
`nlohmann::basic_json<>::input_adapter` — an implementation detail. The two-pass
approach avoids this at the cost of approximate line numbers.

**Recommendation:** implement the two-pass approach first (one day of work, no
risk). Mark the result as approximate in the code comment. Revisit SAX if precise
line numbers become a blocker.

---

#### 2.2 Deterministic CompiledModelGraph output

**Problem.** `CompiledModelGraph::mInstances` is
`std::unordered_map<pair<string,string>, CompiledInstance, PairHash>`. Hash-map
iteration order is unspecified and varies across runs, platforms, and library
versions. `AllOf()`, `ModelIds()`, and `JsonModelWriter::Write()` produce output
in a non-deterministic order. Content hashing and cache invalidation are
impossible.

**Fix.** Switch `mInstances` to `std::map<pair<string,string>, CompiledInstance>`.
Remove `PairHash`. The natural lexicographic ordering on `(modelId, instanceId)`
is both deterministic and meaningful (alphabetical by model, then by id within
model).

```
// CompiledModelGraph.h — change private member:
using InstanceKey = std::pair<std::string, std::string>;
std::map<InstanceKey, CompiledInstance> mInstances;  // was unordered_map
// Remove: struct PairHash { ... };
```

`Find()` becomes `mInstances.find(key)` — still O(log n). For authoring-time
compile of schemas with < 100k instances, this is immeasurably faster than the
overhead of the JSON parse that precedes it.

Side effects of this change:
- `AllOf()` naturally returns instances in instanceId-sorted order.
- `ModelIds()` naturally returns model ids in alphabetical order.
- `JsonModelWriter` output becomes stable — the same input always produces the
  same compiled JSON.

---

### Wave 3 — Missing test coverage

These tests unblock correctness verification for already-written code. Write them
immediately after each corresponding Wave-1 fix lands.

#### 3.1 PredicateTests

Covers `FieldExistsPredicate`, `FieldEqualsPredicate`, `FieldRangePredicate`,
`AndPredicate`, `OrPredicate`, `ImplicationPredicate`. All predicates take a
`ModelInstance` argument. Key cases:

- `ImplicationPredicate`: (condition false) → true; (condition true, consequent
  true) → true; (condition true, consequent false) → false.
- `AndPredicate`: both true → true; first false → false without evaluating second.
- `FieldRangePredicate`: field absent → false; field not numeric → false; in range
  → true; at boundary → true.

#### 3.2 ValidatorTests

Covers `Validator::ValidateOne()` and `Validate()`. Key cases:

- Missing required field → `SDE_MISSING_FIELD` Error diagnostic.
- Type mismatch (Text field given Integer) → `SDE_TYPE_MISMATCH` Error.
- Cross-field rule failure → diagnostic with the CrossFieldRule's ruleId.
- Unknown model in instance → `SDE_UNKNOWN_MODEL` Error.
- Optional missing field → no diagnostic.
- All rules pass → `CompileState::Clean`.
- Warning-level diagnostic → `CompileState::WithWarnings`.

#### 3.3 RegexRuleTests

- Matching text → passes.
- Non-matching text → `SDE_REGEX` Error.
- Non-text value → `SDE_REGEX` Error.
- Empty pattern → matches anything.
- Invalid regex in constructor → `std::regex` throws `regex_error`; constructor
  must catch and store an error state (currently not handled — this is a latent bug
  to fix alongside the test).

#### 3.4 EnumMembershipRuleTests

Blocked until Wave-1 fix 1.2 (EnumRegistry) lands. Once the stub is replaced,
test:

- Value is a valid member → passes.
- Value is not a member → `SDE_ENUM_MEMBER` Error with field id and bad value.
- EnumRegistry has no entry for the enumId → currently unclear whether to error
  or pass; define policy (recommended: Warning with code `SDE_UNKNOWN_ENUM`).

#### 3.5 ModelDefinitionTests, FieldDefinitionTests, EnumDefinitionTests

Lightweight structural tests:

- `ModelDefinition::FindField()` returns pointer when present, nullptr when absent.
- `ModelDefinition::HasField()` is consistent with `FindField()`.
- `FieldDefinition` presence defaults to Required.
- `EnumDefinition::ContainsMember()` / `FindMember()` correctness.

---

### Wave 4 — Schema evolution

#### 4.1 Schema versioning and migration

**Problem.** When a FieldDefinition changes (field renamed, deleted, or a new
required field added), existing data files at the old `schemaVersion` become
invalid. There is no mechanism to bring them forward.

**Proposed design.** Add declarative migration steps to `ModelDefinition`. Each
step is a pure data record (no lambdas) so migrations can be serialized, audited,
and tool-generated.

```
// include/SDE/Model/ModelDefinition.h additions

enum class FieldMigrationKind { Rename, Delete, AddWithDefault, Copy };

struct FieldMigration {
    FieldMigrationKind kind;
    std::string        fromField; ///< source field id (for Rename, Delete, Copy)
    std::string        toField;   ///< destination field id (for Rename, Copy, AddWithDefault)
    std::string        value;     ///< literal default (for AddWithDefault)
};

struct SchemaMigration {
    int                          fromVersion; ///< applied to instances at this version
    int                          toVersion;   ///< result version after migration
    std::vector<FieldMigration>  steps;
};

// Added to ModelDefinition:
std::vector<SchemaMigration> migrations;
```

**MigrationRunner** class (new file `Compilation/MigrationRunner.h/.cpp`):

```
class MigrationRunner {
public:
    /// Migrate all instances whose schemaVersion < def.schemaVersion.
    /// Instances are modified in-place. Emits SDE_MIGRATION_ERROR on gaps.
    void Migrate(std::vector<ModelInstance>&        instances,
                 const std::vector<ModelDefinition>& definitions,
                 std::vector<Diagnostic>&             out) const;
};
```

`ModelCompiler::Compile()` calls `MigrationRunner::Migrate()` before
`InferDefaults()`. The instance's `fields["schemaVersion"]` is updated to the
current version after each step chain completes.

**JSON schema format.** The `"migrations"` array sits at the top level of the
schema file alongside `"fields"`:

```json
{
  "id": "Item",
  "schemaVersion": 3,
  "migrations": [
    {
      "fromVersion": 1, "toVersion": 2,
      "steps": [
        { "kind": "Rename", "fromField": "damage_base", "toField": "damage" }
      ]
    },
    {
      "fromVersion": 2, "toVersion": 3,
      "steps": [
        { "kind": "AddWithDefault", "toField": "rarity", "value": "Common" },
        { "kind": "Delete", "fromField": "legacyTag" }
      ]
    }
  ],
  "fields": [ ... ]
}
```

**Constraints.** Migration chains must be contiguous (no version gaps). The
`MigrationRunner` checks this and emits `SDE_MIGRATION_GAP` if a required step is
missing. Circular version references are impossible by construction
(`fromVersion < toVersion` is enforced).

---

#### 4.2 Struct field nesting

**Status.** `TypeKind::Struct` is reserved in `TypeNode.h`. `TypeRegistry::Struct()`
exists. `StructFieldSpec` is defined. No loader, writer, or compiler path handles
it.

**Design.** A struct field is an inline anonymous record — think C's anonymous
struct embedded in another struct. In JSON, it is represented as a nested object:

```json
{ "id": "sword_01", "stats": { "damage": 45, "weight": 3.2 } }
```

The schema declares the struct's sub-fields:

```json
{
  "id": "stats",
  "type": "Struct",
  "fields": [
    { "id": "damage", "type": "Number", "presence": "required" },
    { "id": "weight", "type": "Number", "presence": "required" }
  ]
}
```

`JsonModelLoader::LoadDefinition()` must recursively call `ParseFieldDefinition()`
for each struct sub-field and pass the resulting `vector<StructFieldSpec>` to
`TypeRegistry::Struct()`.

`Validator::CheckTypeMatch()` for `TypeKind::Struct` must recursively validate
each sub-field against the `StructFieldSpecs` returned by
`TypeRegistry::StructFields(id)`.

`ModelCompiler::BuildGraph()` (via `RawToCompiled`) already handles `RawObject` →
`CompiledObjectRef`, so the compiler path works once the validator path is wired.

**Dependency.** Struct nesting should be designed after schema migration is
stable, since struct field renames are a common migration scenario.

---

### Wave 5 — Tooling

#### 5.1 sde watch

**Design.** New subcommand dispatched from `cli/main.cpp`. Internally:

```
sde watch <project-dir> [--interval=<ms>]
```

Abstract watcher interface (new file `cli/FileWatcher.h`):

```
class FileWatcher {
public:
    virtual ~FileWatcher() = default;
    virtual void Watch(const std::string& dir,
                       std::function<void(const std::string& path)> onChange) = 0;
    virtual void Poll() = 0; ///< called in the event loop
};
```

Concrete implementations:
- `InotifyWatcher` (Linux) — `inotify_init` + `IN_CLOSE_WRITE` events.
- `KqueueWatcher` (macOS) — `kqueue` + `EVFILT_VNODE`.
- `PollingWatcher` (cross-platform fallback) — `stat()` mtime comparison every
  `--interval` ms (default 500 ms).

Event loop in `CmdWatch()`:

```
while (true) {
    watcher.Poll();
    if (changed) {
        // debounce: wait 200ms quiet period
        auto result = compiler.Compile(...);
        PrintDiagnostics(result.validation.diagnostics);
        if (IsUsable(result.state))
            printf("\033[32m✓ Clean\033[0m\n");
        changed = false;
    }
}
```

**Dependency.** `sde watch` is useful immediately even without incremental
recompile; it just re-runs the full pipeline on change. Incremental recompile is
an optimization added later.

---

#### 5.2 sde diff

**Design.** Compares two compiled graphs (two `compiled.json` files or two in-
memory `CompileResult`s after separate compile runs).

```
sde diff <compiled-a.json> <compiled-b.json> [--format=text|json]
```

Algorithm: walk all `(modelId, instanceId)` keys from both graphs (union), then
for each key:

| Case | Output |
|------|--------|
| In A only | `REMOVED Item/sword_01` |
| In B only | `ADDED  Item/axe_02` |
| In both, same fields | (silent) |
| In both, different field values | `CHANGED Item/sword_01 .damage: 45 → 60` |

Field comparison uses recursive slab traversal for arrays/objects.

`CompiledModelGraph` must produce deterministic output (Wave 2.2) before `sde diff`
produces stable results across runs. Otherwise the diff would show spurious
ordering changes.

---

#### 5.3 Predicate JSON serialization

**Isolation constraint.** nlohmann must not appear in `Predicate.h`.

**Solution.** New files:
- `include/SDE/IO/PredicateSerializer.h` — public header, no nlohmann.
- `src/SDE/IO/PredicateSerializer.cpp` — includes nlohmann, implements serialization.

```
// include/SDE/IO/PredicateSerializer.h
#pragma once
#include "SDE/Validation/Predicate.h"
#include <memory>
#include <string>
#include <vector>

namespace SDE {
struct Diagnostic;

/// Serialize a Predicate tree to a JSON string.
[[nodiscard]] std::string SerializePredicate(const Predicate& pred);

/// Reconstruct a Predicate tree from a JSON string.
/// Emits SDE_BAD_PREDICATE diagnostics on unknown kind or malformed structure.
[[nodiscard]] std::shared_ptr<Predicate> DeserializePredicate(
    const std::string&       json,
    std::vector<Diagnostic>& out);

} // namespace SDE
```

**Wire-in.** Once implemented, `JsonModelLoader::LoadDefinition()` can read a
top-level `"crossFieldRules"` array in schema files and call
`DeserializePredicate()` for each predicate sub-object. `CrossFieldRule` objects
are then added to `ModelDefinition::crossFieldRules`.

**Format.** Predicate nodes are objects with a `"kind"` discriminator:

```json
{
  "kind": "Implication",
  "condition": { "kind": "FieldEquals", "field": "timed", "value": "true" },
  "consequent": { "kind": "FieldRange",  "field": "duration", "min": 0.001, "max": 1e9 }
}
```

Supported `"kind"` values (must match `PredicateKind()` return values exactly):
`"FieldExists"`, `"FieldEquals"`, `"FieldRange"`, `"And"`, `"Or"`, `"Implication"`.

---

#### 5.4 HTML diagnostic report

**Design.** New subcommand or post-compile output mode:

```
sde compile <project-dir> --report=report.html
```

The report is a single self-contained HTML file (inline CSS + JS, no external
resources). Sections:

1. Summary: total errors / warnings / instances compiled.
2. Per-file table: file name, error count, warning count, first error message.
3. Diagnostic list: each `Diagnostic` rendered as a code-excerpt block with the
   offending line highlighted (requires source locations to be non-zero — Wave 2.1
   is a prerequisite for useful reports).

Implementation: string-template approach — no HTML parser needed; the report
generator writes the HTML structure directly. A single 400-line
`HtmlReportWriter.cpp` is sufficient.

---

### Wave 6 — Long-term

#### 6.1 LSP server

**Design.** `sde lsp` subcommand. Speaks the Language Server Protocol (LSP) over
stdio via JSON-RPC 2.0. Zero external dependencies — the transport layer is a
simple line-delimiter loop over `stdin`/`stdout`.

Messages handled at launch:

| Message | Action |
|---------|--------|
| `initialize` | Reply with capabilities: `textDocumentSync`, `hoverProvider`, `definitionProvider`, `diagnosticsProvider`. |
| `textDocument/didOpen` | Load + validate the opened file; push `publishDiagnostics`. |
| `textDocument/didChange` | Re-validate; push updated `publishDiagnostics`. |
| `textDocument/hover` | If cursor is on a Ref field value, look up the target instance and return its schema as hover text. |
| `textDocument/definition` | If cursor is on a Ref value, return the source location of the target instance's file. |
| `shutdown` / `exit` | Clean exit. |

The LSP layer is a pure transport wrapper around the existing pipeline. The
validation logic it calls is identical to `sde validate`.

**Dependencies.** Useful source locations (Wave 2.1) are required for go-to-
definition to return accurate positions. Without them the server works but
definition jumps land at line 0.

---

#### 6.2 BinaryModelLoader

**Design.** A compact, content-addressed format for shipped builds where JSON
parse overhead matters.

**File layout:**

```
[magic: 8 bytes "SDEBIN\0\0"]
[format_version: uint16]
[schema_hash: uint64]   ← CRC64 of the schema JSON at compile time
[string_table_offset: uint32]
[instance_count: uint32]
[instances: instance_count × InstanceHeader]
[string_table: null-terminated strings]
```

Each `InstanceHeader`:

```
[modelId_offset: uint32]    ← index into string_table
[instanceId_offset: uint32]
[field_count: uint16]
[fields: field_count × FieldEntry]
```

Each `FieldEntry`:

```
[key_offset: uint32]
[value_kind: uint8]  ← 0=Null, 1=Integer, 2=Number, 3=Bool, 4=Text, 5=Ref, 6=Array, 7=Object
[value: union depending on kind]
```

`BinaryModelLoader` implements the abstract `ModelLoader` interface; callers are
unchanged. The abstract interface already supports this — `Load()` returns
`vector<ModelInstance>` regardless of source format.

**The schema hash in the header detects format mismatches** — if the schema
changed since the binary was compiled, the loader emits `SDE_SCHEMA_MISMATCH` and
returns empty. This prevents silent data corruption when old binaries are loaded
against new schemas.

**Priority.** Low — JSON is fully sufficient for authoring-time workflows and even
for shipped builds in projects below 100k instances. Binary format becomes
relevant when JSON parse time exceeds frame budget on load screens.

---

## Dependency graph of open work

```
Wave 1 (correctness)
────────────────────
  1.1 Array/Object compilation      ← standalone, no blockers
  1.2 EnumRegistry + Rule           ← standalone
  1.3 Rule JSON declaration         ← standalone
  1.4 RefIntegrityRule              ← standalone

Wave 2 (quality)
────────────────
  2.1 JSON source locations         ← standalone; unblocks LSP (6.1)
  2.2 Deterministic output          ← standalone; unblocks sde diff (5.2)

Wave 3 (tests)
──────────────
  PredicateTests                    ← no blockers
  ValidatorTests                    ← no blockers
  RegexRuleTests                    ← no blockers
  EnumMembershipRuleTests           ← requires 1.2
  ModelDefinitionTests              ← no blockers

Wave 4 (schema evolution)
─────────────────────────
  4.1 Schema migration              ← no blockers; prerequisite for 4.2
  4.2 Struct field nesting          ← requires 4.1 to be stable

Wave 5 (tooling)
────────────────
  5.1 sde watch                     ← no blockers; benefits from incremental recompile later
  5.2 sde diff                      ← requires 2.2 (deterministic output)
  5.3 Predicate serialization       ← no blockers; unblocks DSL (6.3)
  5.4 HTML report                   ← requires 2.1 for useful line numbers

Wave 6 (long-term)
──────────────────
  6.1 LSP server                    ← requires 2.1 (source locations)
  6.2 BinaryModelLoader             ← requires 4.1 (schema hash for mismatch detection)
  6.3 DSL stage                     ← requires 5.3 (Predicate JSON round-trip)
  Incremental recompile             ← requires 2.2 (stable identity for dependency graph)
```

---

## Known technical debt

| # | Area | Issue |
|---|------|-------|
| 1 | `EnumMembershipRule` | `Apply()` always returns `true`. Needs `EnumRegistry` in `Validator`. |
| 2 | JSON source locations | All `RawValue::location.line` and `.column` are `0`. nlohmann DOM drops token positions. |
| 3 | Array/Object compilation | `BuildGraph()` fills slab slots with `CompiledNull`; nested values not recursively compiled. |
| 4 | Predicate round-trip | `PredicateKind()` exists but `ToJson()` / `FromJson()` are not implemented. |
| 5 | Rule declaration in JSON | `JsonModelLoader::ParseFieldDefinition()` silently ignores a `"rules"` JSON array. |
| 6 | `RefIntegrityRule` | Mentioned in early design, never implemented as a per-field Rule. |
| 7 | Deterministic graph output | `CompiledModelGraph::mInstances` is an `unordered_map`; iteration order is not stable. |
| 8 | Schema migration | No versioning mechanism. Old instances cannot migrate forward when a schema field changes. |

---

## Strict isolation rules

- SDE **must not** include any `SagaEditor/` or `SagaEngine/` header.
  Verified by `Tools/Scripts/check_tools_isolation.py` (CI gate).
- SDE depends only on the C++ standard library and `nlohmann_json` (conan dep).
  nlohmann is isolated to `JsonModelLoader.cpp` and `ModelWriter.cpp` — no public header exposes it.
- SDE produces a static library (`SDE`). Editor and runtime link it; SDE never links them.
- The public surface the editor / runtime need: `CompiledModelGraph`, `CompiledInstance`,
  `CompiledValue`, `Diagnostic`, `CompileState`.

## Repository extraction checklist

When the day comes to split this folder into its own repo:

1. Copy `Tools/SystemDefinitionEngine/` to `<new-repo>/`.
2. The CMakeLists.txt is already self-contained (`SDE` lib + `sde` binary + `SDETests`).
3. Run `python3 Tools/Scripts/check_tools_isolation.py` on the new repo to confirm zero
   forbidden includes.
4. Add `nlohmann_json/3.11.3` to the standalone conanfile.
5. Done. No source rewrites. No shared headers.

## Current layout

```
Tools/SystemDefinitionEngine/
├── SDE_ROADMAP.md
├── README.md
├── CMakeLists.txt
├── include/SDE/
│   ├── Model/
│   │   ├── TypeNode.h           ← TypeNodeId handles + TypeRegistry (frozen before pipeline)
│   │   ├── EnumDefinition.h
│   │   ├── FieldDefinition.h
│   │   ├── Relation.h
│   │   └── ModelDefinition.h
│   ├── Validation/
│   │   ├── Diagnostic.h
│   │   ├── CompileState.h       ← 6-state enum + relational operators
│   │   ├── ValidationResult.h
│   │   ├── Predicate.h          ← serializable AST for cross-field conditions
│   │   ├── Rule.h               ← RangeRule, RegexRule, EnumMembershipRule, CrossFieldRule
│   │   └── Validator.h          ← RuleRegistry (DI, no singleton) + Validator
│   ├── IO/
│   │   ├── ModelLoader.h        ← RawValue (RawInteger/RawNumber distinct), ModelInstance
│   │   ├── JsonModelLoader.h    ← zero nlohmann symbols in this header
│   │   └── ModelWriter.h
│   └── Compilation/
│       ├── CompiledModelGraph.h ← ValueSlab + compound-key map + CompiledValue variant
│       ├── ReferenceResolver.h  ← (modelId, instanceId) compound key, DFS cycle detection
│       └── ModelCompiler.h
├── src/SDE/                     ← implementations mirror include/
│   ├── Model/
│   ├── Validation/
│   ├── IO/
│   └── Compilation/
├── cli/
│   └── main.cpp                 ← sde validate / sde compile
└── tests/
    ├── Model/                   ← TypeRegistryTests
    ├── Validation/              ← DiagnosticTests, CompileStateTests, RangeRuleTests
    ├── IO/                      ← RawValueTests, JsonModelLoaderTests
    ├── Compilation/             ← ReferenceResolverTests, ModelCompilerTests
    └── Integration/             ← FullPipelineTests
```
