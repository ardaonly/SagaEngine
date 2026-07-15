---
title: Engineering conventions and contract review
status: guide
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Engineering conventions and contract review

Coding conventions serve ownership, review, and maintenance. They are not a substitute for formatters or a demand for decorative comments. The durable standard is that a reader can identify a file's owner, a public type's contract, a complex function's invariants, and an external mapping without reconstructing intent from Git history.

## Scope

These conventions apply to Saga-owned source, tests, tools, scripts, CMake, and maintained documentation. Vendor/imported code follows upstream policy and is not mechanically rewritten to match local style.

Owner-specific language formatting remains authoritative. C++ uses configured compiler/format/static checks; C# and Python follow their current tool conventions. This page focuses on semantic documentation and repository hygiene.

## File and owner clarity

A file path should reveal semantic owner. Runtime/editor modules use their `Public`, `Private`, and `Tests` roots. Programs contain composition-specific entry work. Tools and developer checks remain distinct. Do not create a generic `Common`, `Shared`, `Utils`, or old root as a convenient dumping ground without a real owner contract.

New files carry the repository's applicable SPDX metadata according to license policy. Do not copy a header blindly across licensing domains.

## Public types

A public type comment explains:

- what domain concept it represents;
- who owns its lifetime or copied value;
- valid/invalid/default states;
- thread/transaction requirements where relevant;
- failure/result semantics;
- what it deliberately does not own or guarantee.

Avoid comments that repeat the class name. For a handle, document invalid/stale/generation and destruction behavior. For a snapshot, document consistency and lifetime. For a factory/interface, document selected implementation and ownership transfer.

Names such as `Manager`, `Host`, `Backend`, `Impl`, `Session`, or `Runtime` require particular scrutiny in Public. They may be valid contracts, but their comments and consumer evidence must justify that surface.

## Functions

Document non-obvious preconditions, mutation, ownership transfer, units, coordinate/time space, ordering, complexity/bounds, and failure. A trivial accessor needs no essay. A replication apply, path resolver, source patch, package gate, or native lifecycle method does.

Boolean returns are insufficient when callers need to distinguish missing, invalid, stale, unsupported, denied, cancelled, or internal failure. Prefer typed result/status plus bounded diagnostics.

## External mappings

When engine values map to a vendor/protocol/file-schema enum, state the external contract and fallback behavior near the mapping implementation. Do not leak vendor constants into public values to avoid writing a mapping.

Mappings handle unknown/new external values conservatively. Tests cover every supported branch and the fallback/rejection branch. Coordinate conversions, error codes, pixel formats, packet categories, and license/tool report schemas particularly benefit from explicit mapping notes.

## Invariants and section structure

Use small comments to mark invariant groups in a large implementation: state transition, synchronization, lifetime, validation, publication, cleanup. Section dividers help navigation only when the file is genuinely large; they should not create visual noise around every function.

Important invariants belong in tests and durable Wiki contracts as well as comments. Comments can drift; an architecture/unit test makes a boundary executable.

## Comment purpose

Good comments explain why a boundary exists, why an apparently simpler action is unsafe, what correctness rule a sequence preserves, or what evidence limits a claim. Bad comments narrate syntax, preserve retired milestone language, or promise unimplemented future behavior.

TODOs identify owner, concrete missing contract, and tracking context where used. A bare “fix later” becomes archaeology. Remove/update comments when code ownership or behavior changes.

## Error messages and diagnostics

Human messages name the failed operation and actionable input. Stable diagnostic ids support tests/tooling. Do not put secrets, full payloads, or unbounded source in logs.

Assertions protect internal impossible states; operational invalid input returns a diagnostic result. Do not use an assertion as the only validation for network, project, package, or user-controlled data.

## Includes and dependencies

Include the public header you use, not another module's private path. Public headers include only dependencies needed by their declarations. Implementation includes vendor/backends privately. Forward declarations or opaque handles can reduce coupling when they preserve clarity and lifetime safety.

Qt stays in EditorQt, Diligent in private RHI/Render implementation, SDL in private platform/input implementation, RmlUi in private UI backend, and pqxx/Redis/hiredis in persistence implementation unless a deliberate reviewed contract says otherwise.

## Tests near changes

Every behavior change identifies the evidence category it needs. Parser/validation changes add accepted and rejected fixtures. State machines add transition/failure tests. Concurrency adds deterministic synchronization tests. Public headers add installed-consumer/architecture checks. Native graphics adds CPU and GPU evidence at the appropriate layer.

Do not inflate coverage by testing a private helper through a public-sounding suite name. Label white-box evidence honestly.

## Generated and local output

Generated artifacts record producer, input fingerprint, schema/tool version, and freshness where applicable. They are not edited as source. Build/cache/report/audit/repomix output stays untracked unless deliberately selected as a fixture.

Do not commit `.saga-private` state. Do not reintroduce `docs/` or old top-level source roots for compatibility. History remains in Git.

## Review questions

Before accepting a change:

1. Is the semantic owner correct?
2. Is each public type intentional, vendor-neutral, and used by a real consumer?
3. Are dependencies public/private for the right reason?
4. Are source truth and derived artifacts distinguished?
5. Are path, freshness, authority, lifetime, thread, and failure rules explicit?
6. Does the test category prove the exact claim?
7. Do docs describe current implementation and retain non-claims?
8. Are generated/local/private artifacts excluded from the change?
9. Is licensing/provenance valid for every affected path?
10. Can the next maintainer understand the constraint without old milestone documents?

## Refactoring rule

A mechanical move preserves behavior first, then a later focused change narrows contracts. Do not keep an old layout alive with compatibility aliases solely to avoid updating internal paths. Do not mix a broad target split, API redesign, runtime behavior change, documentation migration, and license policy change in one unreviewable commit.

## Documentation rule

SagaWiki records current durable contracts. Markdown is the maintainable source and `scripts/wiki build` produces the offline reader. Detailed reference pages retain design knowledge; concise topic pages provide entry points. Historical source paths can appear in the migration/selection map but must not become current instructions.
