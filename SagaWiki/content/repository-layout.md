---
title: Repository layout
status: current
owner: SagaWiki
generated_html: pages/repository-layout.html
---

# Repository layout

| Path | Owner |
| --- | --- |
| `Engine/Source/Runtime` | Runtime modules, each with Public, Private, Tests, and CMake ownership. |
| `Engine/Source/Editor` | Editor modules; Qt types live only under EditorQt. |
| `Engine/Source/Programs` | Thin program entry points and program-specific integration. |
| `Tools` | Build, project, scripting, packaging, licensing, and developer tooling. |
| `Tests` | Unit, Architecture, Integration, GPU, Contract, Support, Evidence, and fixtures. |
| `Samples` | Sample projects and sample-owned runtime/headless source. |
| `SagaWiki` | Canonical current documentation. |
| `LICENSES` | SPDX texts and authoritative third-party notices. |

Legacy top-level source roots are retired. New code must use the owners in this table rather than recreating generic source dumps or an unowned tool-script directory.
