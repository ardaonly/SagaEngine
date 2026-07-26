#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Inventory first-party files and enforce the reviewed oversized-file baseline."""

from __future__ import annotations

import argparse
import ast
from dataclasses import asdict, dataclass
import json
from pathlib import Path
import re
import sys
from typing import Iterable, Sequence


REPOSITORY_ROOT = Path(__file__).resolve().parents[3]
LICENSING_ROOT = REPOSITORY_ROOT / "Tools" / "Licensing"
if str(LICENSING_ROOT) not in sys.path:
    sys.path.insert(0, str(LICENSING_ROOT))

from licensing_policy import (  # noqa: E402
    PolicyResolver,
    canonical_first_party_files,
    embedded_spdx,
    is_generated_file,
    load_policy_context,
)


SCHEMA_VERSION = 1
FUNCTION_RE = re.compile(
    r"^\s*(?:template\s*<[^>]+>\s*)?(?:[\w:&*<>\[\],~]+\s+)+([\w:~]+)\s*\([^;]*\)\s*(?:const\s*)?(?:noexcept\s*)?\{\s*$"
)
LANGUAGES = {
    ".c": "C", ".cc": "C++", ".cpp": "C++", ".cxx": "C++",
    ".h": "C/C++ header", ".hh": "C++ header", ".hpp": "C++ header", ".hxx": "C++ header",
    ".cs": "C#", ".py": "Python", ".cmake": "CMake", ".md": "Markdown",
    ".json": "JSON", ".toml": "TOML", ".yml": "YAML", ".yaml": "YAML",
    ".sh": "Shell", ".bash": "Shell", ".ps1": "PowerShell", ".xml": "XML",
}
CODE_LANGUAGES = {
    "C", "C++", "C/C++ header", "C++ header", "C#", "Python", "CMake",
    "Shell", "PowerShell",
}


@dataclass(frozen=True)
class FileRecord:
    path: str
    language: str
    physicalLines: int
    nonEmptyLines: int
    largestFunction: str | None
    largestFunctionLines: int | None
    majorResponsibilities: int
    generated: bool
    role: str
    embeddedSpdx: str | None
    resolvedLicense: str
    sizeClass: str
    recommendedAction: str


@dataclass(frozen=True)
class Diagnostic:
    code: str
    path: str
    message: str


def tracked_files(root: Path) -> list[str]:
    return canonical_first_party_files(root)


def _language(path: Path) -> str:
    if path.name == "CMakeLists.txt":
        return "CMake"
    return LANGUAGES.get(path.suffix.lower(), "Text")


def _role(relative: str) -> str:
    if "/tests/" in relative.lower() or relative.startswith("Tests/") or Path(relative).name.startswith("test_"):
        return "test"
    if any(token in relative.lower() for token in ("legacy", "experimental", "deprecated")):
        return "legacy"
    return "production"


def _generated(relative: str, text: str) -> bool:
    return is_generated_file(relative, text)


def _python_structure(text: str) -> tuple[str | None, int | None, int]:
    try:
        tree = ast.parse(text)
    except SyntaxError:
        return None, None, 0
    nodes = [node for node in tree.body if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef))]
    functions = [
        (node.name, (node.end_lineno or node.lineno) - node.lineno + 1)
        for node in ast.walk(tree)
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef))
    ]
    largest = max(functions, key=lambda item: (item[1], item[0]), default=None)
    return (largest[0], largest[1], len(nodes)) if largest else (None, None, len(nodes))


def _text_structure(text: str) -> tuple[str | None, int | None, int]:
    lines = text.splitlines()
    starts: list[tuple[int, str]] = []
    classes = 0
    for index, line in enumerate(lines, 1):
        if re.match(r"^\s*(?:class|struct|interface|record)\s+\w+", line):
            classes += 1
        match = FUNCTION_RE.match(line)
        if match:
            starts.append((index, match.group(1)))
    spans = [
        (name, (starts[index + 1][0] if index + 1 < len(starts) else len(lines) + 1) - line)
        for index, (line, name) in enumerate(starts)
    ]
    largest = max(spans, key=lambda item: (item[1], item[0]), default=None)
    return (largest[0], largest[1], classes + len(starts)) if largest else (None, None, classes)


def _size_class(lines: int) -> str:
    if lines > 1500:
        return "critical-monolith"
    if lines > 1000:
        return "high-maintenance"
    if lines > 750:
        return "review"
    return "normal"


def inventory(root: Path) -> list[FileRecord]:
    context = load_policy_context(root)
    resolver = PolicyResolver(context)
    records: list[FileRecord] = []
    for relative in tracked_files(root):
        path = root / relative
        try:
            data = path.read_bytes()
        except OSError:
            continue
        if b"\0" in data:
            continue
        try:
            text = data.decode("utf-8")
        except UnicodeDecodeError:
            continue
        generated = _generated(relative, text)
        if generated:
            continue
        physical = len(text.splitlines())
        non_empty = sum(bool(line.strip()) for line in text.splitlines())
        language = _language(path)
        structure = _python_structure(text) if language == "Python" else _text_structure(text)
        decision = resolver.decide(relative, text=text)
        resolved = (
            decision.resolution.expression
            if decision.resolution.resolved
            else "AMBIGUOUS" if decision.diagnostics else "UNRESOLVED"
        )
        embedded = embedded_spdx(path)
        size_class = _size_class(physical)
        action = "review ownership split" if physical > 1000 else "review cohesion" if physical > 750 else "none"
        records.append(FileRecord(
            path=relative,
            language=language,
            physicalLines=physical,
            nonEmptyLines=non_empty,
            largestFunction=structure[0],
            largestFunctionLines=structure[1],
            majorResponsibilities=structure[2],
            generated=False,
            role=_role(relative),
            embeddedSpdx=embedded or None,
            resolvedLicense=resolved,
            sizeClass=size_class,
            recommendedAction=action,
        ))
    return sorted(records, key=lambda item: item.path)


def evaluate_baseline(records: Iterable[FileRecord], baseline: dict) -> list[Diagnostic]:
    current = {record.path: record for record in records if record.physicalLines > 1000}
    entries = {entry["path"]: entry for entry in baseline.get("entries", [])}
    diagnostics: list[Diagnostic] = []
    for path, record in sorted(current.items()):
        entry = entries.get(path)
        if entry is None:
            diagnostics.append(Diagnostic("Maintainability.NewOversizedFile", path, f"{record.physicalLines} lines exceeds 1000"))
            continue
        limit = int(entry.get("maximumPhysicalLines", 0))
        if record.physicalLines > limit:
            diagnostics.append(Diagnostic("Maintainability.BaselineGrowth", path, f"{record.physicalLines} lines exceeds reviewed baseline {limit}"))
        if (
            record.physicalLines > 1500
            and entry.get("disposition") == "declarative-data-exception"
            and record.language in CODE_LANGUAGES
        ):
            diagnostics.append(Diagnostic(
                "Maintainability.InvalidCriticalException",
                path,
                "critical executable code cannot use a declarative-data exception",
            ))
        for field in ("reason", "owner", "disposition"):
            if not str(entry.get(field, "")).strip():
                diagnostics.append(Diagnostic("Maintainability.BaselineIncomplete", path, f"missing {field}"))
    for path in sorted(entries.keys() - current.keys()):
        diagnostics.append(Diagnostic("Maintainability.BaselineStale", path, "file is absent or no longer exceeds 1000 lines"))
    return sorted(diagnostics, key=lambda item: (item.path, item.code))


def _summary(records: Sequence[FileRecord]) -> dict[str, int]:
    return {
        "fileCount": len(records),
        "reviewCount": sum(record.physicalLines > 750 for record in records),
        "highMaintenanceCount": sum(record.physicalLines > 1000 for record in records),
        "criticalMonolithCount": sum(record.physicalLines > 1500 for record in records),
    }


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("--repo-root", type=Path, default=REPOSITORY_ROOT)
    result.add_argument("--baseline", type=Path, default=Path("Tools/Developer/RepoAudit/maintainability_baseline.json"))
    result.add_argument("--report", type=Path)
    result.add_argument("--check", action="store_true")
    return result


def main(argv: Sequence[str] | None = None) -> int:
    args = parser().parse_args(argv)
    root = args.repo_root.resolve()
    baseline_path = args.baseline if args.baseline.is_absolute() else root / args.baseline
    records = inventory(root)
    diagnostics: list[Diagnostic] = []
    if args.check:
        try:
            baseline = json.loads(baseline_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as exc:
            diagnostics.append(Diagnostic("Maintainability.BaselineInvalid", baseline_path.as_posix(), str(exc)))
        else:
            if baseline.get("schemaVersion") != SCHEMA_VERSION:
                diagnostics.append(Diagnostic("Maintainability.BaselineSchemaUnsupported", baseline_path.as_posix(), "expected schemaVersion 1"))
            else:
                diagnostics.extend(evaluate_baseline(records, baseline))
    payload = {
        "schemaVersion": SCHEMA_VERSION,
        "summary": _summary(records),
        "files": [asdict(record) for record in records],
        "diagnostics": [asdict(item) for item in diagnostics],
    }
    serialized = json.dumps(payload, indent=2, sort_keys=True) + "\n"
    if args.report:
        output = args.report if args.report.is_absolute() else root / args.report
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(serialized, encoding="utf-8")
    print(json.dumps({"summary": payload["summary"], "diagnostics": payload["diagnostics"]}, sort_keys=True))
    return 1 if diagnostics else 0


if __name__ == "__main__":
    raise SystemExit(main())
