# Getting Started

> **Owner:** Developer experience and build/tooling maintainers
> **Audience:** New contributors and repository evaluators
> **Update trigger:** Entry commands, documentation indexes, or first-use workflows change
> **Authority:** Navigation and onboarding only; current scripts and tool help define executable behavior

> Status: Conservative repository entry point

SagaEngine is currently an engine and toolchain development repository, not a
finished installable game-creation product.

## Read First

Before changing code, read:

1. [What Is SagaEngine?](WHAT_IS_SAGAENGINE.md)
2. [Current Capabilities](CURRENT_CAPABILITIES.md)
3. [What Is Not Implemented](WHAT_IS_NOT_IMPLEMENTED.md)
4. [Saga Ecosystem Map](SAGA_ECOSYSTEM_MAP.md)
5. [Saga MMO Genre Focus](SAGA_MMO_GENRE_FOCUS.md)

The product direction describes where SagaEngine is going. It does not replace
current source, build, test, package, or runtime evidence.

## First Local Checks

Start with repository-structural checks:

```sh
scripts/build-default --dry-run
scripts/test-taxonomy --check
scripts/verify-local
```

`scripts/verify-local` is structural by default. Do not treat it as a real
build or CTest run unless the relevant build or test options were explicitly
used.

For a code change:

1. identify the owning target and module;
2. run the narrowest relevant build or test;
3. inspect generated evidence and its non-claims;
4. expand verification only when the affected boundary requires it.

Prefer current commands and current indexes over commands copied from historical
milestone or candidate documents.

## What To Avoid

Do not assume that:

- a report is an enforcement gate;
- a staged package is a production distribution;
- a fixture is a finished sample;
- a focused test proves full product readiness;
- a planning document proves implementation;
- the MMO product focus means complete MMO capability already exists;
- a public-looking header is a stable installed SDK;
- every tool or executable represents a permanent product boundary.

## Where To Go Next

- Product and capability questions: this directory
- Architecture and ownership questions: `docs/architecture/`
- Test policy and suite questions: `docs/testing/`
- Tool-specific behavior: the current README beside the owning tool
- Historical context: `docs/internal/product-history/`

When current documentation and source disagree, record the conflict and verify
the actual target, consumer, runtime path, and test evidence before changing the
architecture.
