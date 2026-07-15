---
title: Licensing
status: current
owner: SagaWiki
generated_html: pages/licensing.html
---

# Licensing

`LICENSE` is the repository’s authoritative primary license text. Saga-owned engine and editor source follows the current MPL-2.0-centered policy. SPDX license texts and the authoritative third-party notice are under `LICENSES/`. The former Editor-restricted license is historical and is not part of current policy or the canonical license bundle.

`LICENSE_POLICY.toml` is the machine-readable authority for repository path domains and license resolution. Forge, vendor, imported, sample, documentation, and other scoped areas follow the license domains recorded there; vendor and imported code retain their applicable upstream notices. `Tools/Developer/BoundaryCheck/Policies/path_rules.json` is a derived compatibility projection, and `SHA256SUMS` is a derived integrity control. Neither replaces the policy, source license texts, `LICENSE_TEXT_SOURCES.toml`, or REUSE metadata.

Generated output inherits licensing only through explicit policy. Product direction does not authorize proprietary, leaked, tainted, or license-unclear source.

## Stability rule

A path move or generated copy does not silently change licensing. Historical source snapshots retain the license state recorded in those snapshots, while current files resolve through current policy, provenance, SPDX/REUSE metadata, and authoritative texts. Vendor and imported material retain their upstream terms, and unresolved rights fail closed rather than inheriting a convenient repository default.

```sh
python3 Tools/Licensing/regenerate_license_checksums.py --check
python3 Tools/Licensing/verify_license_texts.py
```
