# SagaEngine — Tools Roadmap (Index + Isolation Policy)







---

## Anchor tools



---

## Strict Isolation Policy

**Tools may NOT live inside `Editor/`, `Engine/`, `Server/`, `Backends/`, or `Apps/`.** Every tool folder is self-contained:

```
Tools/<ToolName>/
├── include/<ToolHeader>/      ← public headers
├── src/<ToolHeader>/          ← implementation
├── README.md                  ← stand-alone repo readme (lives WITH the tool)
├── <TOOL>_ROADMAP.md          ← travels with the tool when split off
└── tests/                     ← optional self-contained test suite
```

**Edge rules** (a single violation is a code-review block):



---

## Why two tools, why now



These tools also do *not* belong inside the editor or the engine. They
have separate concerns, separate consumers (CI, content authors, future
third-party studios), and a real chance of being independently useful
to projects that do not use the rest of the engine. Building them
inside `Tools/` from day one is what keeps that possibility open.

---

## Where to look next

