#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Arda Koyuncu
# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

from pathlib import Path, PurePosixPath
import argparse
import re
import sys
import tomllib

from licensing_common import (
    CANONICAL_DOCUMENT_PATHS,
    LEGACY_TRANSITION_PATHS,
    expected_checksum_paths,
    repository_root,
    safe_relative_path,
    sha256,
)

TOP_LEVEL_FIELDS = {
    "schema_version",
    "retrieved_at",
    "hash_algorithm",
    "strict_validation",
    "files",
}
REQUIRED_RECORD_FIELDS = {
    "path",
    "identifier",
    "title",
    "publisher",
    "document_type",
    "content_integrity",
    "source_kind",
    "retrieval_method",
    "content_type",
    "bytes",
    "sha256",
}
OPTIONAL_RECORD_FIELDS = {
    "source_url",
    "source_path",
    "reference_url",
    "source_revision",
}
ALLOWED_DOCUMENT_TYPES = {
    "license-text",
    "custom-license-text",
    "contribution-certificate",
}
ALLOWED_INTEGRITY = {
    "verbatim",
    "verbatim-from-current-repository-license",
}
ALLOWED_SOURCE_KINDS = {
    "authoritative-url",
    "immutable-versioned-url",
    "repository-current-license",
}
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")

METADATA_REQUIREMENTS = {
    "LICENSE": (
        "SPDX-FileCopyrightText: 2026 Arda Koyuncu",
        "SPDX-License-Identifier: CC-BY-4.0",
    ),
    "LICENSE_POLICY.toml": (
        "SPDX-FileCopyrightText: 2026 Arda Koyuncu",
        "SPDX-License-Identifier: MPL-2.0",
    ),
    "LICENSE_TEXT_SOURCES.toml": (
        "SPDX-FileCopyrightText: 2026 Arda Koyuncu",
        "SPDX-License-Identifier: 0BSD",
    ),
    "Tools/Developer/BoundaryCheck/Policies/path_rules.json.license": (
        "SPDX-FileCopyrightText: 2026 Arda Koyuncu",
        "SPDX-License-Identifier: 0BSD",
    ),
    "SagaWiki/pages/licensing.html": (
        "SPDX-FileCopyrightText: 2026 Arda Koyuncu",
        "SPDX-License-Identifier: Apache-2.0 OR CC-BY-4.0",
    ),
    "LICENSES/THIRD_PARTY_NOTICES.md": (
        "SPDX-License-Identifier: CC-BY-4.0",
    ),
    "Tools/Licensing/apply_contributing_dco.py": (
        "SPDX-License-Identifier: MPL-2.0",
    ),
    "Tools/Licensing/collect_cmake_target_graph.py": (
        "SPDX-License-Identifier: MPL-2.0",
    ),
    "Tools/Licensing/generate_legacy_path_rules.py": (
        "SPDX-License-Identifier: MPL-2.0",
    ),
    "Tools/Licensing/licensing_common.py": (
        "SPDX-License-Identifier: MPL-2.0",
    ),
    "Tools/Licensing/regenerate_license_checksums.py": (
        "SPDX-License-Identifier: MPL-2.0",
    ),
    "Tools/Licensing/validate_license_policy.py": (
        "SPDX-License-Identifier: MPL-2.0",
    ),
    "Tools/Licensing/verify_license_texts.py": (
        "SPDX-License-Identifier: MPL-2.0",
    ),
}


def parse_checksums(path: Path) -> dict[str, str]:
    entries: dict[str, str] = {}
    for number, line in enumerate(
        path.read_text(encoding="utf-8").splitlines(),
        1,
    ):
        match = re.fullmatch(r"([0-9a-f]{64})  ([^\r\n]+)", line)
        if not match:
            raise ValueError(f"Invalid SHA256SUMS line {number}: {line!r}")
        digest, relative = match.groups()
        if not safe_relative_path(relative):
            raise ValueError(f"Unsafe SHA256SUMS path: {relative}")
        if relative in entries:
            raise ValueError(f"Duplicate SHA256SUMS path: {relative}")
        entries[relative] = digest
    return entries


def validate_manifest(root: Path, failures: list[str]) -> tuple[dict, set[str]]:
    manifest_path = root / "LICENSE_TEXT_SOURCES.toml"
    try:
        data = tomllib.loads(manifest_path.read_text(encoding="utf-8"))
    except Exception as exc:
        failures.append(f"Manifest parse failure: {exc}")
        return {}, set()

    unknown_top = sorted(set(data) - TOP_LEVEL_FIELDS)
    missing_top = sorted(TOP_LEVEL_FIELDS - set(data))
    if unknown_top:
        failures.append(f"Unknown manifest fields: {unknown_top}")
    if missing_top:
        failures.append(f"Missing manifest fields: {missing_top}")

    if data.get("schema_version") != 2:
        failures.append(f"Unsupported manifest schema: {data.get('schema_version')!r}")
    if data.get("hash_algorithm") != "SHA-256":
        failures.append(f"Unsupported hash algorithm: {data.get('hash_algorithm')!r}")
    if data.get("strict_validation") is not True:
        failures.append("Manifest strict_validation must be true")

    seen_paths: set[str] = set()
    seen_ids: set[str] = set()
    by_id: dict[str, dict] = {}

    for index, record in enumerate(data.get("files", []), 1):
        if not isinstance(record, dict):
            failures.append(f"Manifest record {index} is not a table")
            continue
        fields = set(record)
        unknown = sorted(fields - REQUIRED_RECORD_FIELDS - OPTIONAL_RECORD_FIELDS)
        missing = sorted(REQUIRED_RECORD_FIELDS - fields)
        if unknown:
            failures.append(f"Record {index} unknown fields: {unknown}")
        if missing:
            failures.append(f"Record {index} missing fields: {missing}")
            continue

        relative = record["path"]
        identifier = record["identifier"]
        if not isinstance(relative, str) or not safe_relative_path(relative):
            failures.append(f"Record {index} unsafe path: {relative!r}")
            continue
        if relative in seen_paths:
            failures.append(f"Duplicate manifest path: {relative}")
        seen_paths.add(relative)

        if not isinstance(identifier, str) or not identifier:
            failures.append(f"Record {index} invalid identifier")
        elif identifier in seen_ids:
            failures.append(f"Duplicate manifest identifier: {identifier}")
        seen_ids.add(identifier)
        by_id[identifier] = record

        if record["document_type"] not in ALLOWED_DOCUMENT_TYPES:
            failures.append(f"{relative}: invalid document_type")
        if record["content_integrity"] not in ALLOWED_INTEGRITY:
            failures.append(f"{relative}: invalid content_integrity")
        if record["source_kind"] not in ALLOWED_SOURCE_KINDS:
            failures.append(f"{relative}: invalid source_kind")
        if not isinstance(record["bytes"], int) or record["bytes"] < 0:
            failures.append(f"{relative}: invalid bytes")
        if not isinstance(record["sha256"], str) or not SHA256_RE.fullmatch(
            record["sha256"]
        ):
            failures.append(f"{relative}: invalid SHA-256")

        source_kind = record["source_kind"]
        if source_kind in {"authoritative-url", "immutable-versioned-url"}:
            if not str(record.get("source_url", "")).startswith("https://"):
                failures.append(f"{relative}: HTTPS source_url required")
            if not str(record.get("reference_url", "")).startswith("https://"):
                failures.append(f"{relative}: HTTPS reference_url required")
        if source_kind == "repository-current-license":
            if not record.get("source_path"):
                failures.append(f"{relative}: source_path required")
            if record.get("source_url"):
                failures.append(f"{relative}: repository source must not have source_url")

        path = root / relative
        if not path.is_file():
            failures.append(f"MISSING {relative}")
            continue
        actual_hash = sha256(path)
        actual_bytes = path.stat().st_size
        if actual_hash != record["sha256"]:
            failures.append(
                f"HASH {relative}: expected {record['sha256']}, got {actual_hash}"
            )
        if actual_bytes != record["bytes"]:
            failures.append(
                f"SIZE {relative}: expected {record['bytes']}, got {actual_bytes}"
            )

    if seen_paths != CANONICAL_DOCUMENT_PATHS:
        failures.append(
            "Manifest document set mismatch: "
            f"missing={sorted(CANONICAL_DOCUMENT_PATHS - seen_paths)}, "
            f"unexpected={sorted(seen_paths - CANONICAL_DOCUMENT_PATHS)}"
        )

    zero_bsd = by_id.get("0BSD", {})
    expected_url = (
        "https://raw.githubusercontent.com/spdx/"
        "license-list-data/v3.28.0/text/0BSD.txt"
    )
    if zero_bsd.get("source_revision") != "v3.28.0":
        failures.append("0BSD source_revision must be v3.28.0")
    if zero_bsd.get("source_url") != expected_url:
        failures.append("0BSD source_url must be pinned to v3.28.0")

    return data, seen_paths


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path)
    parser.add_argument("--strict-cleanup", action="store_true")
    args = parser.parse_args()

    try:
        root = repository_root(args.root)
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2

    failures: list[str] = []
    validate_manifest(root, failures)

    try:
        policy = tomllib.loads(
            (root / "LICENSE_POLICY.toml").read_text(encoding="utf-8")
        )
    except Exception as exc:
        failures.append(f"Policy parse failure: {exc}")
        policy = {"legacy": {"cleanup_status": "pending"}}

    cleanup_status = policy.get("legacy", {}).get("cleanup_status", "pending")
    include_legacy = cleanup_status != "complete"

    legacy_existing = {
        path
        for path in LEGACY_TRANSITION_PATHS
        if (root / path).is_file()
    }
    if include_legacy:
        missing_legacy = sorted(LEGACY_TRANSITION_PATHS - legacy_existing)
        if missing_legacy:
            failures.append(
                f"Transitional legacy files missing before atomic cleanup: {missing_legacy}"
            )
    else:
        remaining = sorted(legacy_existing)
        if remaining:
            failures.append(f"Legacy files remain after cleanup: {remaining}")

    if args.strict_cleanup and legacy_existing:
        failures.append(
            f"--strict-cleanup requires legacy removal: {sorted(legacy_existing)}"
        )

    for relative in sorted(expected_checksum_paths(
        root,
        include_existing_legacy=include_legacy,
    )):
        path = root / relative
        if not path.is_file():
            failures.append(f"Missing governance/checksum input: {relative}")
        elif path.is_symlink():
            failures.append(f"Symlink not permitted in licensing foundation: {relative}")

    try:
        entries = parse_checksums(root / "SHA256SUMS")
        expected = expected_checksum_paths(
            root,
            include_existing_legacy=include_legacy,
        )
        if set(entries) != expected:
            failures.append(
                "SHA256SUMS set mismatch: "
                f"missing={sorted(expected - set(entries))}, "
                f"unexpected={sorted(set(entries) - expected)}"
            )
        for relative, digest in entries.items():
            path = root / relative
            if path.is_file():
                actual = sha256(path)
                if actual != digest:
                    failures.append(
                        f"SHA256SUMS mismatch {relative}: expected {digest}, got {actual}"
                    )
    except Exception as exc:
        failures.append(f"SHA256SUMS validation failure: {exc}")

    for relative, required_strings in METADATA_REQUIREMENTS.items():
        path = root / relative
        if not path.is_file():
            continue
        content = path.read_text(encoding="utf-8", errors="replace")
        for required in required_strings:
            if required not in content:
                failures.append(f"{relative}: missing metadata {required!r}")

    dep5_path = root / ".reuse/dep5"
    if dep5_path.is_file():
        dep5 = dep5_path.read_text(encoding="utf-8", errors="replace")
        for required in (
            "Files: SHA256SUMS",
            "Files: .reuse/dep5",
            "Copyright: 2026 Arda Koyuncu",
            "License: 0BSD",
        ):
            if required not in dep5:
                failures.append(f".reuse/dep5: missing {required!r}")

    if failures:
        print("License foundation verification failed:", file=sys.stderr)
        for failure in failures:
            print(f"  {failure}", file=sys.stderr)
        return 1

    checksum_count = len(
        parse_checksums(root / "SHA256SUMS")
    )
    print("License foundation verification passed.")
    print(f"Verified normative documents: {len(CANONICAL_DOCUMENT_PATHS)}")
    print(f"Verified checksum files: {checksum_count}")
    print(f"Legacy transition files: {len(legacy_existing)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
