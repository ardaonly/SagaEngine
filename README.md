## Why this project exists
SagaEngine is an intentional architecture-first effort: to design a clean, modular, platform-abstracted foundation for a game engine. The goal is to get the core boundaries and abstractions right before committing to large feature sets or production toolchains.

This README complements the project descriptor in `ENGINE.md`.

---

## Build system note
`CMakeLists.txt` is intentionally minimal. Why:
- The architecture is still evolving and will likely be refactored.
- A heavyweight build system now would create churn and friction during frequent structural changes.
- A production-grade build and CI will be added once core design stabilizes.

Simplicity in the build is a conscious choice to preserve flexibility.

---

## Short, honest status (quick metrics)
- Architectural maturity: **~70%** (concepts and separations defined).  
- Platform coverage: **~10%** (Windows-focused today).  
- Production readiness: **0–3%** (not production-ready).

---

## Contribution & expectation
- Open to discussion and small, architecture-focused contributions.  
- Major design changes: open an issue or RFC first — don't submit large, breaking PRs without prior discussion.  
- Expect refactors and breaking changes while the foundation is established.

---

## TL;DR
This is not a feature-first engine — it is a foundation-first architecture project. If the core proves solid, features and tooling will follow.
