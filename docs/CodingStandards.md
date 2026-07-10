# SagaEngine Coding Standards

This document defines repository-facing commenting, source documentation, and code hygiene requirements for SagaEngine-owned code. The goal is clarity, architectural intent, maintainability, and long-term consistency.

---

## 1. Scope

This standard applies to SagaEngine-owned, hand-maintained source code and repository-facing documentation.

It does not automatically apply to:

* third-party or vendor code,
* generated files,
* imported upstream sources,
* external submodules,
* files whose authoritative format is controlled by another tool.

Generated output must be changed through its authoritative source or generator.

---

## 2. File Headers

Every SagaEngine-owned, hand-maintained `.h` and `.cpp` file must begin with:

```cpp
/// @file SDLInputBackend.h
/// @brief Implements the SDL2 and SDL3 platform input backend.
```

Rules:

* `@file` must match the exact filename.
* `@brief` must describe the file's responsibility in one precise sentence.
* Do not use vague descriptions.
* Do not describe historical context, implementation attempts, or temporary work state.

---

## 3. Type Documentation

Public types and architecturally meaningful internal types must be documented.

```cpp
/// Stores the native handle and Saga device identity for one connected gamepad.
struct GamepadSlot
{
    void* handle = nullptr;  ///< Native SDL_GameController* or SDL_Gamepad* handle.
    uint32_t deviceId = Input::kInvalidDeviceId;
};
```

Private or local types require documentation only when their responsibility, invariants, ownership, lifetime, or relationship to another subsystem is not clear from naming and surrounding code.

Rules:

* Use `///` above documented structs, classes, and enums.
* Use `///<` for member fields when ownership, lifetime, units, sentinel values, threading behavior, or external representation is not obvious.
* Explain intent, constraints, or contracts rather than restating declarations.
* Do not document obvious fields solely to satisfy formatting.
* Documentation comments must not use emojis.

---

## 4. Function Documentation

Public API functions must document their observable contract when the declaration does not express it completely.

Internal functions require documentation when they contain non-obvious policy, ownership, lifetime, synchronization, failure handling, coordinate conversion, unit conversion, protocol behavior, or architectural intent.

```cpp
/// Returns the Saga device ID associated with an SDL joystick instance ID.
///
/// Returns Input::kInvalidDeviceId when no active mapping exists.
[[nodiscard]] uint32_t FindDeviceId(int sdlJoystickId) const noexcept;
```

Rules:

* Describe behavior precisely.
* Clarify return semantics when sentinel values, nullable results, or partial results are possible.
* Document pointer, handle, and resource ownership.
* Document preconditions, postconditions, thread-affinity requirements, and failure behavior when relevant.
* Do not repeat the function name in prose without adding semantic information.

---

## 5. External Mapping Comments

Add an external mapping comment when a conversion is non-obvious, lossy, policy-driven, incomplete, or dependent on different value domains.

```cpp
// SDL_Scancode → SagaEngine::Input::KeyCode
```

Rules:

* Place the comment directly above the relevant conversion logic.
* Write the external representation first and the SagaEngine representation second.
* Use `→` to show conversion direction.
* Do not add a mapping comment when function names and types already express the conversion completely.

---

## 6. Section Dividers

Section dividers may be used when a source file contains several distinct implementation regions that remain under one coherent owner.

```cpp
// ─── Keyboard Handling ───
// ─── Mouse Handling ───
// ─── Gamepad Management ───
```

Use section names that describe a concrete responsibility. Do not pad divider lines to a fixed width. Do not use dividers to justify files that contain unrelated responsibilities. When sections have different dependencies, lifecycles, owners, or reasons to change, split the implementation instead.

---

## 7. Comment Purpose

Comments exist to explain architectural intent, clarify non-obvious behavior, document boundaries, state invariants, and preserve contracts that cannot be expressed clearly through code alone.

Comments must not translate obvious code into prose, compensate for weak naming, preserve obsolete implementation history, describe temporary execution state, or add visual noise.

---

## 8. Comment Maintenance

Comments are part of the maintained implementation.

When behavior, ownership, lifetime, invariants, failure semantics, or architectural responsibility changes, update or remove the affected comments in the same change.

A stale or misleading comment is a defect. Prefer no comment over a comment that restates outdated implementation details.

---

## 9. Repository Hygiene

The repository must remain product-focused and reproducible.

Rules:

* Emojis are not allowed in source code, comments, repository-facing documentation, commit-facing examples, generator templates, or project metadata.
* Local editor, tool, assistant, cache, and workspace state must not be committed unless the repository explicitly owns and documents that configuration.
* Tooling that runs in CI must resolve the repository root through Git, an explicit `--repo-root` argument, or an existing authoritative repository manifest.
* Do not introduce tool-specific marker files solely for repository discovery.
* Shared Python modules belong under `Tools/common/`.
* Executable maintenance scripts belong under `Tools/scripts/`.
* Do not hand-edit generated output to satisfy repository standards. Change the authoritative source, template, or generator instead.

---

Consistency in documentation is part of engine quality. Comments must improve understanding without becoming a substitute for clear ownership, naming, structure, or design.
