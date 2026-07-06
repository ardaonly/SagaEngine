<!--
SPDX-FileCopyrightText: 2026 Arda Koyuncu
SPDX-License-Identifier: CC-BY-4.0
-->

# SagaEngine License Foundation

This document describes the canonical license and contribution texts installed
for SagaEngine's transitional licensing foundation.

## Repository files

- `LICENSES/MPL-2.0.txt`
- `LICENSES/Apache-2.0.txt`
- `LICENSES/0BSD.txt`
- `LICENSES/CC-BY-4.0.txt`
- `LICENSES/CC0-1.0.txt`
- `LICENSES/LicenseRef-Saga-Editor-Restricted.txt`
- `DCO`
- `LICENSE_TEXT_SOURCES.toml`
- `SHA256SUMS`
- `Tools/Licensing/verify_license_texts.py`
- `.reuse/dep5`

The restricted Editor license preserves the current effective treatment of the
Editor paths until those files are validly migrated or removed from that
licensing domain.

Run `python3 Tools/Licensing/verify_license_texts.py` from the repository root
to validate the license manifest, hashes and repository placement.

These files do not by themselves relicense SagaEngine source files. Effective
relicensing requires verified authority, applicable file metadata and recorded
migration commits.
