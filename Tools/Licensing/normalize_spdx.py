#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Normalize SPDX metadata for tracked, hand-written first-party files."""

from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass
import json
from pathlib import Path
import re
import sys

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from licensing_policy import (  # noqa: E402
    FIRST_PARTY_SOURCE_ROOTS,
    INVENTORY_EXCLUDED_PARTS,
    PolicyResolver,
    comment_prefix,
    is_generated_file,
    load_policy_context,
    tracked_modes,
)


COPYRIGHT_RE = re.compile(r"^\s*(?://|#|;|/\*+|\*|<!--)?\s*SPDX-FileCopyrightText:\s*2026 Arda Koyuncu\s*(?:\*/|-->)?\s*$")
LICENSE_RE = re.compile(r"^\s*(?://|#|;|/\*+|\*|<!--)?\s*SPDX-License-Identifier:\s*([^\s*]+)\s*(?:\*/|-->)?\s*$")


@dataclass(frozen=True)
class Diagnostic:
    code: str
    path: str
    message: str
    changed: bool = False


def normalize_text(
    path: Path,
    text: str,
    expected_license: str,
    *,
    executable_python: bool,
) -> tuple[str, list[str]]:
    """Return normalized text and stable change reasons."""
    newline = "\r\n" if "\r\n" in text else "\n"
    had_final_newline = text.endswith(("\n", "\r"))
    lines = text.splitlines()
    changes: list[str] = []

    filtered: list[str] = []
    for line in lines:
        if COPYRIGHT_RE.fullmatch(line):
            changes.append("removed-personal-copyright")
            continue
        filtered.append(line)
    lines = filtered

    if path.suffix.lower() == ".py" and lines and lines[0].startswith("#!") and not executable_python:
        lines.pop(0)
        changes.append("removed-import-only-shebang")

    prefix = comment_prefix(path)
    if prefix is not None:
        expected_line = f"{prefix}SPDX-License-Identifier: {expected_license}"
        matches = [index for index, line in enumerate(lines[:20]) if LICENSE_RE.fullmatch(line)]
        if matches:
            first = matches[0]
            if lines[first] != expected_line:
                lines[first] = expected_line
                changes.append("replaced-license")
            for index in reversed(matches[1:]):
                lines.pop(index)
                changes.append("removed-duplicate-license")
        else:
            insert_at = 1 if lines and lines[0].startswith("#!") else 0
            lines.insert(insert_at, expected_line)
            changes.append("added-license")

    result = newline.join(lines)
    if had_final_newline or result:
        result += newline
    return result, sorted(set(changes))


def run(root: Path, *, fix: bool) -> tuple[list[Diagnostic], int]:
    context = load_policy_context(root)
    resolver = PolicyResolver(context)
    modes = tracked_modes(root)
    diagnostics: list[Diagnostic] = []
    changed_count = 0

    for relative in sorted(modes):
        if not relative.startswith(FIRST_PARTY_SOURCE_ROOTS):
            continue
        if any(part in INVENTORY_EXCLUDED_PARTS for part in Path(relative).parts):
            continue
        path = root / relative
        if not path.is_file():
            continue
        data = path.read_bytes()
        if b"\0" in data:
            continue
        try:
            text = data.decode("utf-8")
        except UnicodeDecodeError:
            diagnostics.append(Diagnostic("SPDX.Encoding.Unsupported", relative, "file is not UTF-8"))
            continue
        if is_generated_file(relative, text):
            continue
        decision = resolver.decide(relative, text=text)
        if decision.domain is None:
            diagnostics.append(Diagnostic(
                "SPDX.Ownership.Ambiguous",
                relative,
                "no unique canonical policy domain",
            ))
            continue
        if not decision.resolution.resolved:
            diagnostics.append(Diagnostic("SPDX.License.Unresolved", relative,
                f"domain {decision.domain.id} has no resolved SPDX license"))
            continue
        if not decision.modification_allowed:
            continue
        expected = decision.resolution.expression
        normalized, reasons = normalize_text(
            path, text, expected,
            executable_python=(path.suffix.lower() == ".py" and modes[relative] == "100755"),
        )
        if normalized != text:
            changed_count += 1
            diagnostics.append(Diagnostic("SPDX.Metadata.ChangeRequired", relative, ",".join(reasons), True))
            if fix:
                path.write_bytes(normalized.encode("utf-8"))

    return sorted(diagnostics, key=lambda item: (item.path, item.code, item.message)), changed_count


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    mode = result.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--fix", action="store_true")
    mode.add_argument("--report", action="store_true")
    result.add_argument("--root", type=Path)
    result.add_argument("--output", type=Path)
    return result


def main() -> int:
    args = parser().parse_args()
    root = (args.root or Path(__file__).resolve().parents[2]).resolve()
    diagnostics, changed = run(root, fix=args.fix)
    payload = {
        "schemaVersion": 1,
        "mode": "fix" if args.fix else "check" if args.check else "report",
        "changedFileCount": changed,
        "diagnostics": [asdict(item) for item in diagnostics],
    }
    if args.output:
        output = args.output if args.output.is_absolute() else root / args.output
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(payload, sort_keys=True))
    if any(item.code != "SPDX.Metadata.ChangeRequired" for item in diagnostics):
        return 2
    if args.check and changed:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
