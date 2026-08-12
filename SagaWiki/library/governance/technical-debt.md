---
title: Technical debt and known limitations
status: current
owner: SagaWiki
reviewed_against: 0.0.11-hibernation
---

# Technical debt and known limitations

This is the durable register for work that remains after the 0.0.11 hardening
effort. Missing aspirational features are not automatically debt.

| ID | Type | Subsystem | Problem and impact | Current containment | Severity | Future direction |
| --- | --- | --- | --- | --- | --- | --- |
| TD-CI-001 | TECHNICAL DEBT | Validation | GitHub automated test execution is disabled, so no clean-runner continuous evidence exists. | CMake-owned local suites build prerequisites before focused CTest execution. | High | Qualify independent clean runners, then add thin workflow orchestration. |
| TD-TEST-002 | TECHNICAL DEBT | Validation | ASan/LSan, UBSan, TSan, coverage, and production fuzzing are not operational gates. | Deterministic focused tests and explicit non-claims remain. | High | Implement separate measured jobs after the native graph is clean-runner proven. |
| TD-TEST-009 | TECHNICAL DEBT | Managed tests | Complete managed CLI drivers depend on mutable checked-in sample generation state and fail independently when it is absent or current. | Safe suites run freshly staged CLI startup checks and the private-runtime boundary contract; full drivers remain registered diagnostics. | High | Generate isolated package/sample fixtures per test, then promote the drivers into `managed-tools`. |
| KL-TEST-003 | KNOWN LIMITATION | Product tests | Qt product tests require XCB and a display server; GPU tests require qualified hardware/driver state. | `product-xcb` is explicit and environment failure is `UNAVAILABLE`; GPU is excluded. | Medium | Record display/GPU identity and qualify dedicated environments. |
| TD-MAINT-004 | TECHNICAL DEBT | Native ownership | Historical oversized translation units increase review and regression cost. | Reviewed entries and ceilings live in the maintainability baseline; new debt cannot be hidden. | Medium | Split one bounded owner at a time with focused behavior tests. |
| KL-DIST-005 | KNOWN LIMITATION | Distribution | Current packaging and installed-consumer evidence is platform-bounded and is not a complete signed multi-platform release process. | Linux distribution policy, inventory, license, and private-runtime checks fail closed. | High | Add clean installation and reproducibility evidence on each supported release platform. |
| TD-ARCH-006 | TECHNICAL DEBT | World/serialization | `WorldNode` does not yet serialize real ECS components for complete world snapshots. | It suppresses placeholder snapshots instead of publishing false evidence. | Medium | Introduce an owned component serialization contract and compatibility tests. |
| KL-UX-007 | KNOWN LIMITATION | Editor | Environment-sensitive product workflows and remaining editor polish are not fully automated or release-qualified. | Qt module tests and explicit product-XCB validation cover bounded paths. | Medium | Add measured lifecycle/accessibility/workflow evidence without changing Editor authority. |
| FF-CI-008 | FUTURE FEATURE | Validation | Professional PR, main, nightly, weekly, and release validation tiers do not exist. | The testing roadmap defines prerequisites and non-goals. | Medium | Workflows should only set up, invoke repository suites, and upload results. |

Known product bugs should be added only with a reproducible behavior and owner.
An unavailable environment is a limitation, not a passing test and not
automatically a product bug.
