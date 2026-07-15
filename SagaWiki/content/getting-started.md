---
title: Getting started
status: current
owner: SagaWiki
generated_html: pages/getting-started.html
---

# Getting started

## Prerequisites

Use Python 3, CMake, Conan 2, Ninja, a matching C++ compiler, and the profiles under `profiles/`. The Nix shell is the preferred reproducible Linux environment but is not a product platform requirement.

## Linux

```sh
nix-shell --run "Tools/Forge/bin/forge install --profile linux-gcc"
nix-shell --run "cmake --preset linux-gcc"
nix-shell --run "cmake --build --preset linux-gcc"
ctest --test-dir build/RelWithDebInfo-0.0.11 --output-on-failure
```

The stable root wrappers are `scripts/build`, `scripts/test`, `scripts/verify`, `scripts/package`, and `scripts/wiki`. The wiki wrapper owns standard-library-only documentation generation and verification; the other wrappers delegate to their owning tools and contain no product logic.

## Project inputs

A `.sagaproj` manifest is the canonical project input. Use `Tools/SagaProjectKit/sagaproject validate --project <path>` before assuming a sample or package is valid.
