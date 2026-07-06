<!--
SPDX-FileCopyrightText: 2026 Arda Koyuncu
SPDX-License-Identifier: CC-BY-4.0
-->

# SagaEngine License Foundation Files

This package contains the license and contribution texts required to establish
SagaEngine's transitional repository licensing foundation.

## Contents

```text
LICENSES/MPL-2.0.txt
LICENSES/Apache-2.0.txt
LICENSES/0BSD.txt
LICENSES/CC-BY-4.0.txt
LICENSES/CC0-1.0.txt
LICENSES/LicenseRef-Saga-Editor-Restricted.txt
DCO
LICENSE_TEXT_SOURCES.toml
SHA256SUMS
verify_license_texts.py
.reuse/dep5
```

The restricted Editor text preserves the current effective repository license.
It must remain available until the affected paths are validly migrated or
removed from that licensing domain.

`LICENSE_TEXT_SOURCES.toml` records each normative text's type, origin,
integrity status, size and SHA-256 digest. The 0BSD text is pinned to the
immutable SPDX License List Data tag `v3.28.0`.

Run:

```bash
python3 verify_license_texts.py
```

The verifier fails closed on:

- missing or unexpected package files;
- unexpected files under `LICENSES/`;
- unsafe or duplicate manifest paths;
- duplicate identifiers;
- unknown manifest fields;
- invalid SHA-256 or byte counts;
- manifest/file mismatches;
- `SHA256SUMS` drift;
- missing auxiliary-file license metadata;
- an unpinned 0BSD source record.

These files do not by themselves relicense SagaEngine source files. Effective
relicensing still requires verified authority, applicable policy metadata and
recorded migration commits.

## Auxiliary-file licensing

- `README.md`: CC-BY-4.0
- `verify_license_texts.py`: MPL-2.0
- `LICENSE_TEXT_SOURCES.toml`: 0BSD
- `SHA256SUMS`: 0BSD through `.reuse/dep5`
- `.reuse/dep5`: 0BSD through its own stanza
