#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Arda Koyuncu
# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

from functools import lru_cache
from pathlib import Path, PurePosixPath
import hashlib
import re
import subprocess
from typing import Iterable

ACTION_TO_SEVERITY = {
    "deny": "ERROR",
    "report": "WARNING",
    "ignore": None,
}
SPDX_ID_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9.+_-]*$")
LICENSE_REF_RE = re.compile(r"^LicenseRef-[A-Za-z0-9.+_-]+$")
DOCUMENT_REF_LICENSE_RE = re.compile(
    r"^DocumentRef-[A-Za-z0-9.+_-]+:LicenseRef-[A-Za-z0-9.+_-]+$"
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def safe_relative_path(value: str) -> bool:
    if not value or "\\" in value:
        return False
    path = PurePosixPath(value)
    return (
        not path.is_absolute()
        and ".." not in path.parts
        and "." not in path.parts
    )


def _segment_match(value: str, pattern: str, case_sensitive: bool) -> bool:
    import fnmatch

    if not case_sensitive:
        value = value.casefold()
        pattern = pattern.casefold()
    return fnmatch.fnmatchcase(value, pattern)


def gitwildmatch(path: str, pattern: str, *, case_sensitive: bool = True) -> bool:
    """Match repository-relative paths with segment-aware ** semantics.

    `*` and `?` never cross `/`; `**` matches zero or more complete segments.
    Patterns are repository-relative and do not implement negation.
    """
    if not safe_relative_path(path) or not safe_relative_path(pattern):
        return False

    path_parts = tuple(PurePosixPath(path).parts)
    pattern_parts = tuple(PurePosixPath(pattern).parts)

    @lru_cache(maxsize=None)
    def match(pi: int, gi: int) -> bool:
        if gi == len(pattern_parts):
            return pi == len(path_parts)

        token = pattern_parts[gi]
        if token == "**":
            return match(pi, gi + 1) or (
                pi < len(path_parts) and match(pi + 1, gi)
            )

        return (
            pi < len(path_parts)
            and _segment_match(path_parts[pi], token, case_sensitive)
            and match(pi + 1, gi + 1)
        )

    return match(0, 0)


def repository_root(explicit: Path | None = None) -> Path:
    if explicit is not None:
        root = explicit.resolve()
        result = subprocess.run(
            ["git", "rev-parse", "--show-toplevel"],
            cwd=root,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        if result.returncode != 0:
            raise RuntimeError(f"Not a Git repository: {root}")
        actual = Path(result.stdout.strip()).resolve()
        if actual != root:
            raise RuntimeError(
                f"--root must be the repository root: expected {actual}, got {root}"
            )
        return root

    result = subprocess.run(
        ["git", "rev-parse", "--show-toplevel"],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError("Not inside a Git repository.")
    return Path(result.stdout.strip()).resolve()


def action_severity(action: str) -> str | None:
    if action not in ACTION_TO_SEVERITY:
        raise ValueError(f"Unsupported action: {action!r}")
    return ACTION_TO_SEVERITY[action]


def _tokenize_spdx(expression: str) -> list[str]:
    token_re = re.compile(
        r"\s*(\(|\)|AND\b|OR\b|WITH\b|"
        r"DocumentRef-[A-Za-z0-9.+_-]+:LicenseRef-[A-Za-z0-9.+_-]+|"
        r"[A-Za-z0-9][A-Za-z0-9.+_-]*)"
    )
    tokens: list[str] = []
    position = 0
    while position < len(expression):
        match = token_re.match(expression, position)
        if not match:
            raise ValueError(
                f"Invalid SPDX token near {expression[position:position + 24]!r}"
            )
        tokens.append(match.group(1))
        position = match.end()
    return tokens


class _SpdxParser:
    def __init__(self, tokens: list[str]):
        self.tokens = tokens
        self.position = 0
        self.license_refs: set[str] = set()

    def peek(self) -> str | None:
        if self.position >= len(self.tokens):
            return None
        return self.tokens[self.position]

    def take(self, value: str | None = None) -> str:
        token = self.peek()
        if token is None:
            raise ValueError("Unexpected end of SPDX expression")
        if value is not None and token != value:
            raise ValueError(f"Expected {value!r}, found {token!r}")
        self.position += 1
        return token

    def parse(self) -> None:
        self.parse_or()
        if self.peek() is not None:
            raise ValueError(f"Unexpected SPDX token {self.peek()!r}")

    def parse_or(self) -> None:
        self.parse_and()
        while self.peek() == "OR":
            self.take("OR")
            self.parse_and()

    def parse_and(self) -> None:
        self.parse_with()
        while self.peek() == "AND":
            self.take("AND")
            self.parse_with()

    def parse_with(self) -> None:
        simple_license = self.parse_primary()
        if self.peek() == "WITH":
            if not simple_license:
                raise ValueError("WITH may only follow a simple license identifier")
            self.take("WITH")
            exception = self.take()
            if exception in {"AND", "OR", "WITH", "(", ")"}:
                raise ValueError("Invalid SPDX exception identifier")
            if not SPDX_ID_RE.fullmatch(exception):
                raise ValueError(f"Invalid SPDX exception identifier {exception!r}")

    def parse_primary(self) -> bool:
        token = self.peek()
        if token == "(":
            self.take("(")
            self.parse_or()
            self.take(")")
            return False
        if token is None or token in {"AND", "OR", "WITH", ")"}:
            raise ValueError(f"Expected SPDX license identifier, found {token!r}")
        identifier = self.take()
        if not (
            SPDX_ID_RE.fullmatch(identifier)
            or LICENSE_REF_RE.fullmatch(identifier)
            or DOCUMENT_REF_LICENSE_RE.fullmatch(identifier)
        ):
            raise ValueError(f"Invalid SPDX identifier {identifier!r}")
        if LICENSE_REF_RE.fullmatch(identifier):
            self.license_refs.add(identifier)
        return True


def validate_spdx_expression(expression: str) -> set[str]:
    if not isinstance(expression, str) or not expression.strip():
        raise ValueError("SPDX expression must be a non-empty string")
    parser = _SpdxParser(_tokenize_spdx(expression.strip()))
    parser.parse()
    return parser.license_refs


def ensure_license_ref_texts(
    root: Path,
    expressions: Iterable[str],
) -> list[str]:
    missing: list[str] = []
    for expression in expressions:
        for identifier in validate_spdx_expression(expression):
            path = root / "LICENSES" / f"{identifier}.txt"
            if not path.is_file():
                missing.append(path.relative_to(root).as_posix())
    return sorted(set(missing))

CANONICAL_DOCUMENT_PATHS = {
    "LICENSES/MPL-2.0.txt",
    "LICENSES/Apache-2.0.txt",
    "LICENSES/0BSD.txt",
    "LICENSES/CC-BY-4.0.txt",
    "LICENSES/CC0-1.0.txt",
    "DCO",
}

GOVERNANCE_CHECKSUM_PATHS = {
    ".reuse/dep5",
    "LICENSE",
    "LICENSE_POLICY.toml",
    "LICENSE_TEXT_SOURCES.toml",
    "Tools/Developer/BoundaryCheck/Policies/path_rules.json",
    "Tools/Developer/BoundaryCheck/Policies/path_rules.json.license",
    "SagaWiki/pages/licensing.html",
    "LICENSES/THIRD_PARTY_NOTICES.md",
    "Tools/Licensing/apply_contributing_dco.py",
    "Tools/Licensing/collect_cmake_target_graph.py",
    "Tools/Licensing/generate_legacy_path_rules.py",
    "Tools/Licensing/licensing_common.py",
    "Tools/Licensing/regenerate_license_checksums.py",
    "Tools/Licensing/validate_license_policy.py",
    "Tools/Licensing/verify_license_texts.py",
}

LEGACY_TRANSITION_PATHS = {
    "LICENSES/LICENSE",
    "LICENSES/LICENSE-EDITOR",
    "LICENSES/NOTICE",
}


def expected_checksum_paths(
    root: Path,
    *,
    include_existing_legacy: bool,
) -> set[str]:
    paths = set(CANONICAL_DOCUMENT_PATHS) | set(GOVERNANCE_CHECKSUM_PATHS)
    if include_existing_legacy:
        paths.update(
            path
            for path in LEGACY_TRANSITION_PATHS
            if (root / path).is_file()
        )
    return paths
