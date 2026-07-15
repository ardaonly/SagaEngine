---
title: Source of truth
status: current
owner: SagaWiki
generated_html: pages/source-of-truth.html
---

# Source of truth

| Domain | Authority |
| --- | --- |
| Project identity | Source-controlled `.sagaproj` manifest. |
| Behavior | C# source where managed behavior is used. |
| Visual Blocks | Projection and editing metadata over compatible C# source spans, not an independent behavior graph. |
| Build ownership | Module `CMakeLists.txt` plus aggregate target registration. |
| Packages/assets | Authored source manifests; generated package/runtime manifests carry derived consumption data with freshness and provenance checks. |
| Runtime authority | Server state is authoritative for server-owned gameplay decisions; client prediction and presentation are execution state, not authored truth. |
| Evidence | Reports and test results prove only their declared inputs, configuration, and assertions. |
| Licensing | `LICENSE`, `LICENSES/`, policy TOML, REUSE metadata, and generated checksums. |
| Documentation | SagaWiki for current facts; code and verified evidence prevail when more specific. |

## Project manifest contract

The active `.sagaproj` contract is schemaVersion 0. SagaProjectKit validates the manifest version, required references, and project-relative path boundaries; SagaPackager consumes the same manifest contract when resolving package profiles and staging inputs. Sample manifests demonstrate and test the contract, but they are examples and evidence rather than a separate schema authority.

Generated reports are observations. A report does not mutate source truth and must identify its inputs, scope, and limitations. Missing or stale evidence remains missing or stale; it must not be inferred from filenames or historical claims.

## Many views, one behavior

Visual Blocks, inspectors, source maps, patch previews, and editor read models may expose different views of one behavior, but they do not gain independent authority. Generated artifacts must not silently replace missing source, and package manifests must not be used as authoring truth for missing scenes, scripts, prefabs, or source assets. Samples demonstrate supported contracts and provide evidence; they do not define a second schema or capability authority.
