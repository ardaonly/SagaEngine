---
title: Ownership and dependency direction
status: current
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Ownership and dependency direction

Filesystem ownership, CMake target ownership, and API ownership should tell the same story. A module owns its public contracts, private implementation, tests, and build declaration. Programs compose modules; they do not become a home for reusable engine behavior.

## Dependency direction

Runtime foundations should not depend on editor modules or programs. Editor modules may consume runtime contracts. Programs may consume both runtime and editor targets as appropriate. Developer evidence may inspect any layer without becoming a runtime dependency.

Dependencies should be the narrowest visibility that public headers require:

- `PUBLIC` only when consumers must see the dependency through the installed contract;
- `PRIVATE` for implementation dependencies;
- test-only dependencies for white-box or integration evidence.

Aggregate targets can remain as transitional composition surfaces after the cutover, but they are not permission to compile unrelated owners into one permanent target.

## Artifact flow

Project manifests and authored sources flow through validators and builders into generated or packaged artifacts. Generated artifacts may be checked for freshness and consumed by runtime code, but do not replace their declared source. Evidence reports describe a run; they do not redefine the product contract.

## Review questions

When adding or moving a type, ask:

1. Which module is responsible for its semantics?
2. Is another module required to name it in a public header?
3. Does the target expose a dependency only because the implementation includes it?
4. Is this an installed consumer contract, a private adapter, or developer evidence?
5. Would the chosen owner still make sense if the concrete backend changed?

See [Ownership and dependency contracts](../reference/ownership-and-dependency-contracts.md) for the full module, target, program, install, and test boundaries, and [Engineering conventions and contract review](../reference/engineering-conventions-and-review.md) for the maintenance checklist.
