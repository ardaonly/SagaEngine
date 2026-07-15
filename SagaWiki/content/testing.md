---
title: Testing and evidence
status: current
owner: SagaWiki
generated_html: pages/testing.html
---

# Testing and evidence

| Area | Purpose |
| --- | --- |
| Unit | Focused behavior retained during module purification. |
| Architecture | Repository layout, public/private, target, and dependency rules. |
| Integration | Editor workflows and client/server interactions. |
| GPU/Diligent | Backend-specific render integration evidence. |
| Contract/InstalledConsumer | Exported package and link-consumer contracts. |
| Support/Stress | Reusable stress infrastructure, not a product claim. |
| Evidence/FirstPlayable | Bounded executable and evidence scenarios. |

`Tests/Unit` is intentionally transitional during the hard cutover. Remaining behavior tests stay there until module-local target purity is completed in a later change; this does not make Unit the permanent owner.

A passing focused test proves only its declared inputs, configuration, and assertions. It does not imply release readiness, cross-platform parity, production scale, security certification, or full-suite health.

## Local scope

Use the narrowest owning target and focused test expression first. Raw full CTest health is not assumed, and stress, soak, load, benchmark, real-transport, slow, and long-running tests are opt-in rather than part of a default local gate. Local resource limits and the maintainer’s requested scope govern whether broader execution is appropriate.

Architecture tests prove repository ownership, public/private, vendor, target, and dependency invariants. Contract and installed-consumer tests prove only the exported or linked consumer shape they exercise. Runtime, integration, GPU, and evidence tests prove their named scenarios rather than all-platform or product readiness.

```sh
cmake --preset linux-gcc
cmake --build --preset linux-gcc --target <target> -j2
ctest --test-dir <build-dir> -R '<focused-test-regex>' --output-on-failure -j1
```
