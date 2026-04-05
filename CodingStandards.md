# SagaEngine – Code Commenting Standard

This document defines the mandatory commenting style for the SagaEngine codebase. The goal is clarity, architectural intent, and long-term consistency.

---

## 1. File Header

Every `.h` and `.cpp` file must begin with:

```cpp
/// @file SDLInputBackend.h
/// @brief SDL2/SDL3 implementation of IPlatformInputBackend.
```

Rules:
- `@file` must match the exact filename.
- `@brief` must describe responsibility in one precise sentence.
- No vague descriptions.

---

## 2. Type Documentation

All structs, classes, and enums must have a short description.

```cpp
/// Per-gamepad handle storage (opaque void* to avoid SDL type in header).
struct GamepadSlot
{
    void*    handle   = nullptr;  ///< SDL_GameController* / SDL_Gamepad*
    uint32_t deviceId = Input::kInvalidDeviceId;
};
```

Rules:
- Use `///` above the type.
- Use `///<` for member fields.
- Explain intent, not the obvious.

---

## 3. Function Documentation

Non-trivial functions require a summary.

```cpp
/// Map SDL Joystick ID to internal Device ID.
[[nodiscard]] uint32_t FindDeviceId(int sdlJoystickId) const noexcept;
```

Rules:
- Start with a verb.
- Clarify return semantics if needed.
- Document ownership rules for pointers.

---

## 4. External Mapping Comments

When converting external APIs to engine types:

```cpp
// SDL_Scancode → SagaEngine::Input::KeyCode
```

Rules:
- External API first.
- Use `→` to show direction.
- Place directly above conversion logic.

---

## 5. Section Divider Style

Large source files must use a single-line structured divider format.

### Mandatory Format

```cpp
// ─── Keyboard Handling ────────────────────────────────────────────────
// ─── Mouse Handling ───────────────────────────────────────────────────
// ─── Gamepad Management ───────────────────────────────────────────────
// ─── Event Dispatch ───────────────────────────────────────────────────
// ─── Serialization ────────────────────────────────────────────────────
```

Rules:
- Format must start with `// ─── `.
- Section name comes immediately after.
- The remainder of the line must be filled with `─` to keep visual alignment.
- All dividers in a file must use identical total line width.
- Section names must describe responsibility, not just a single vague word.


---

## 6. Non-Goals

Comments exist to:
- Explain architectural intent
- Clarify non-obvious logic
- Document boundaries

Comments must NOT:
- Translate obvious code
- Replace proper naming
- Add noise

---

Consistency in documentation is part of engine quality.

