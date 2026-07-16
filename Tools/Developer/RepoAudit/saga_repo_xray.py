#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import re
import subprocess
import sys
from collections import Counter, defaultdict
from dataclasses import dataclass, asdict
from pathlib import Path
from typing import Any, Iterable


# ─────────────────────────────────────────────────────────────────────────────
# Config
# ─────────────────────────────────────────────────────────────────────────────

IGNORE_DIR_NAMES = {
    ".git",
    ".agents",
    ".codex",
    ".core",
    ".saga-private",
    ".idea",
    ".vscode",
    ".cache",
    ".pytest_cache",
    ".mypy_cache",
    "__pycache__",
    "node_modules",
    ".direnv",
    ".nix-profile",
    "bin",
    "obj",
    "out",
    "dist",
    ".vs",
    "Vendor",
}

DEFAULT_IGNORE_PREFIXES = {
    "build/",
    "Build/",
    "cmake-build-",
    "coverage/",
    "third_party/",
    "external/",
    "Vendor/",
}

TEXT_EXTENSIONS = {
    ".h", ".hpp", ".hh", ".hxx",
    ".c", ".cc", ".cpp", ".cxx",
    ".cs",
    ".py", ".sh", ".bash",
    ".cmake",
    ".txt", ".md",
    ".json", ".jsonl",
    ".toml", ".yml", ".yaml",
    ".xml",
    ".sagaproj",
    ".nix",
    ".in",
}

SOURCE_EXTENSIONS = {
    ".h", ".hpp", ".hh", ".hxx",
    ".c", ".cc", ".cpp", ".cxx",
    ".cs",
}

CPP_HEADER_EXTENSIONS = {".h", ".hpp", ".hh", ".hxx"}
CPP_SOURCE_EXTENSIONS = {".c", ".cc", ".cpp", ".cxx"}
DOC_EXTENSIONS = {".md", ".txt"}
JSON_EXTENSIONS = {".json", ".jsonl"}

RETIRED_OWNERSHIP_ROOTS = {
    "Apps",
    "Backends",
    "Collaboration",
    "Content",
    "Editor",
    "Runtime",
    "Server",
    "Shared",
    "core",
    "docs",
    "samples",
}

SUSPICIOUS_MARKERS = [
    "todo",
    "fixme",
    "hack",
    "temporary",
    "temp",
    "stub",
    "placeholder",
    "dummy",
    "mock",
    "fake",
    "not implemented",
    "deferred",
    "future work",
    "out of scope",
    "partiallyproven",
    "missingevidence",
    "blocked",
    "unverified",
    "not claimed",
    "not started",
    "not production",
]

POSITIVE_CLAIM_PATTERNS = [
    r"\bproduction[- ]ready\b",
    r"\brelease candidate\b",
    r"\bproduct beta\b",
    r"\benterprise[- ]ready\b",
    r"\bsecure cloud\b",
    r"\bfull editor\b",
    r"\bfull visual scripting\b",
    r"\barbitrary c# to blocks\b",
    r"\bplayable editor\b",
    r"\bclient evaluation implemented\b",
    r"\bclienthost implemented\b",
    r"\bruntime gameplay implemented\b",
    r"\bserver gameplay implemented\b",
    r"\braw full ctest passed\b",
    r"\bheavy stress passed\b",
    r"\breal transport stress passed\b",
]

NEGATION_WINDOW_WORDS = {
    "not",
    "no",
    "without",
    "deferred",
    "unclaimed",
    "forbidden",
    "non-claim",
    "does not",
    "must not",
    "is not",
    "not implemented",
    "not claimed",
}

PUBLIC_HEADER_FORBIDDEN_PATTERNS = [
    ("App/ClientHost include leak in public/header file", r"#\s*include\s*[<\"](?:.*ClientHost|.*ClientConfig|Apps/)"),
    ("Direct SDL leak in public/header file", r"#\s*include\s*[<\"].*SDL"),
    ("Direct Diligent leak in public/header file", r"#\s*include\s*[<\"].*Diligent"),
    ("Direct RmlUi leak in public/header file", r"#\s*include\s*[<\"].*Rml"),
]

QT_PUBLIC_HEADER_PATTERN = r"#\s*include\s*[<\"](?:Qt[^>/\"]+/)?Q[A-Z][^>\"]*"

LAYER_FORBIDDEN_HINTS = {
    "Engine/Source/Runtime/": ("Runtime", [
        r"#\s*include\s*[<\"].*ClientHost",
        r"#\s*include\s*[<\"].*ClientConfig",
        r"#\s*include\s*[<\"]Apps/",
        r"#\s*include\s*[<\"]SagaEditor",
        r"#\s*include\s*[<\"]Q",
    ]),
    "Engine/Source/Runtime/ServerAuthority/": ("ServerAuthority", [
        r"#\s*include\s*[<\"].*ClientHost",
        r"#\s*include\s*[<\"]SagaEditor",
        r"#\s*include\s*[<\"]Q",
    ]),
}

DOC_AREA_ORDER = [
    "architecture",
    "editor",
    "evidence",
    "governance",
    "reference",
    "runtime",
    "start",
    "toolchain",
    "policy",
]


# ─────────────────────────────────────────────────────────────────────────────
# Data classes
# ─────────────────────────────────────────────────────────────────────────────

@dataclass
class Finding:
    severity: str
    category: str
    path: str
    message: str
    evidence: str = ""


@dataclass
class FileRecord:
    path: str
    category: str
    extension: str
    sizeBytes: int
    lines: int = 0
    sha256_16: str = ""
    gitStatus: str = ""
    textKind: str = ""
    headings: list[str] | None = None
    commands: list[str] | None = None
    cmakeTargets: list[str] | None = None
    includes: list[str] | None = None
    suspiciousMarkers: list[str] | None = None
    workItemMentions: list[int] | None = None


# ─────────────────────────────────────────────────────────────────────────────
# Utility
# ─────────────────────────────────────────────────────────────────────────────

def run_cmd(args: list[str], cwd: Path) -> tuple[int, str, str]:
    try:
        p = subprocess.run(
            args,
            cwd=cwd,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=20,
        )
        return p.returncode, p.stdout, p.stderr
    except Exception as exc:
        return 999, "", str(exc)


def rel(path: Path, root: Path) -> str:
    return path.resolve().relative_to(root.resolve()).as_posix()


def normalize_rel(path: str) -> str:
    return path.replace("\\", "/")


def should_ignore_rel(path: str) -> bool:
    p = normalize_rel(path)

    parts = p.split("/")
    if any(part in IGNORE_DIR_NAMES for part in parts):
        return True

    for prefix in DEFAULT_IGNORE_PREFIXES:
        if p.startswith(prefix):
            return True

    return False


def is_text_candidate(path: Path) -> bool:
    return path.suffix.lower() in TEXT_EXTENSIONS or path.name in {
        "CMakeLists.txt",
        "LICENSE",
        "README",
        "README.md",
        "flake.nix",
        "shell.nix",
    }


def read_text_safe(path: Path, max_bytes: int) -> str:
    try:
        if path.stat().st_size > max_bytes:
            return ""
        return path.read_text(encoding="utf-8", errors="replace")
    except Exception:
        return ""


def sha256_short(path: Path) -> str:
    try:
        h = hashlib.sha256()
        with path.open("rb") as f:
            while True:
                b = f.read(1024 * 64)
                if not b:
                    break
                h.update(b)
        return h.hexdigest()[:16]
    except Exception:
        return ""


def count_lines(text: str) -> int:
    if not text:
        return 0
    return text.count("\n") + 1


def extract_headings(text: str, limit: int = 80) -> list[str]:
    out: list[str] = []
    for line in text.splitlines():
        if re.match(r"^#{1,6}\s+", line):
            out.append(line.strip())
            if len(out) >= limit:
                break
    return out


def extract_work_item_mentions(text: str) -> list[int]:
    nums = set()
    for m in re.finditer(r"\bWork item\s+(\d{1,4})\b", text, flags=re.IGNORECASE):
        nums.add(int(m.group(1)))
    return sorted(nums)


def extract_includes(text: str) -> list[str]:
    includes = []
    for line in text.splitlines():
        m = re.match(r"\s*#\s*include\s+[<\"]([^>\"]+)[>\"]", line)
        if m:
            includes.append(m.group(1))
    return includes[:200]


def extract_cmake_targets(text: str) -> list[str]:
    patterns = [
        r"\badd_executable\s*\(\s*([A-Za-z0-9_\-\.]+)",
        r"\badd_library\s*\(\s*([A-Za-z0-9_\-\.]+)",
        r"\badd_test\s*\(\s*NAME\s+([A-Za-z0-9_\-\.]+)",
        r"\bset_tests_properties\s*\(\s*([A-Za-z0-9_\-\.]+)",
    ]
    found = set()
    for pattern in patterns:
        for m in re.finditer(pattern, text, flags=re.IGNORECASE):
            found.add(m.group(1))
    return sorted(found)


def extract_cli_commands(text: str) -> list[str]:
    found = set()

    patterns = [
        r'case\s+"([a-z][a-z0-9\-]{2,})"',
        r'Command\s*=\s*"([a-z][a-z0-9\-]{2,})"',
        r'command\s*==\s*"([a-z][a-z0-9\-]{2,})"',
        r'args\[[0-9]+\]\s*==\s*"([a-z][a-z0-9\-]{2,})"',
        r'"([a-z][a-z0-9]+(?:-[a-z0-9]+)+)"',
    ]

    for pattern in patterns:
        for m in re.finditer(pattern, text):
            v = m.group(1)
            if len(v) <= 80 and not v.startswith("http"):
                found.add(v)

    ignored = {
        "report-only",
        "read-only",
        "source-truth",
        "local-only",
        "utf-8",
        "application-json",
    }
    return sorted(x for x in found if x not in ignored)


def classify_top_level(path: str) -> str:
    normalized = normalize_rel(path)
    first = normalized.split("/", 1)[0]

    if first in RETIRED_OWNERSHIP_ROOTS or normalized.startswith("Tools/scripts/"):
        return f"retired:{first}"

    prefixes = (
        ("Engine/Source/Runtime/", "runtime"),
        ("Engine/Source/Editor/", "editor"),
        ("Engine/Source/Programs/", "programs"),
        ("Engine/Source/Developer/", "developer"),
        ("Engine/Managed/", "managed"),
        ("Engine/Content/", "engine-content"),
        ("SagaWiki/", "docs"),
        ("Samples/", "samples"),
        ("Tools/", "tools"),
        ("Tests/", "tests"),
        ("cmake/", "build-system"),
    )
    for prefix, category in prefixes:
        if normalized.startswith(prefix):
            return category

    if normalized in {"CMakeLists.txt", "CMakePresets.json", "flake.nix", "shell.nix"}:
        return "build-system"
    if first == "Engine":
        return "engine"
    return "other"


def is_public_header(path: str) -> bool:
    normalized = normalize_rel(path)
    lower_path = normalized.lower()
    if Path(normalized).suffix.lower() not in CPP_HEADER_EXTENSIONS:
        return False
    if normalized.startswith("Engine/Source/"):
        return "/Public/" in normalized
    return normalized.startswith("Tools/") and "/include/" in lower_path


def wiki_area(path: str) -> str:
    parts = normalize_rel(path).split("/")
    if len(parts) >= 4 and parts[:2] == ["SagaWiki", "library"]:
        return parts[2]
    if len(parts) >= 3 and parts[0] == "SagaWiki":
        return parts[1]
    return "<wiki-root>"


def context_around(text: str, start: int, size: int = 70) -> str:
    a = max(0, start - size)
    b = min(len(text), start + size)
    return text[a:b].replace("\n", " ").strip()


def is_negated_claim(text: str, match_start: int, match_end: int) -> bool:
    sentence_start = max(
        text.rfind("\n\n", 0, match_start),
        text.rfind(".", 0, match_start),
        text.rfind("!", 0, match_start),
        text.rfind("?", 0, match_start),
    )
    sentence_ends = [
        pos for pos in (
            text.find("\n\n", match_end),
            text.find(".", match_end),
            text.find("!", match_end),
            text.find("?", match_end),
        ) if pos >= 0
    ]
    sentence_end = min(sentence_ends) if sentence_ends else len(text)
    window = text[sentence_start + 1:sentence_end].lower()
    return any(w in window for w in NEGATION_WINDOW_WORDS)


def simple_json_status(obj: Any) -> dict[str, Any]:
    if not isinstance(obj, dict):
        return {"jsonKind": type(obj).__name__}

    keys = [
        "status", "outcome", "result", "decision",
        "localOnly", "networkExposure", "mutatesSource",
        "enforcement", "clientEvaluationStatus",
        "runtimeImplementationClaim",
        "clientHostImplementationClaim",
        "clientEvaluationImplementationClaim",
        "rawFullCTestClaim",
    ]
    return {k: obj.get(k) for k in keys if k in obj}


def make_tree(paths: list[str], max_depth: int, max_entries: int) -> list[str]:
    lines = []
    sorted_paths = sorted(paths)
    count = 0
    for path in sorted_paths:
        parts = path.split("/")
        if len(parts) > max_depth:
            continue
        indent = "  " * (len(parts) - 1)
        lines.append(f"{indent}{parts[-1]}")
        count += 1
        if count >= max_entries:
            lines.append("... truncated ...")
            break
    return lines


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


# ─────────────────────────────────────────────────────────────────────────────
# Main analyzer
# ─────────────────────────────────────────────────────────────────────────────

class RepoXray:
    def __init__(
        self,
        root: Path,
        out: Path,
        max_file_bytes: int,
        include_snippets: bool,
        max_snippet_lines: int,
    ) -> None:
        self.root = root.resolve()
        self.out = out.resolve()
        self.max_file_bytes = max_file_bytes
        self.include_snippets = include_snippets
        self.max_snippet_lines = max_snippet_lines

        self.findings: list[Finding] = []
        self.file_records: list[FileRecord] = []
        self.snippets: dict[str, str] = {}

        self.git_status_by_path: dict[str, str] = {}
        self.git_tracked: set[str] = set()
        self.git_untracked: set[str] = set()
        self.git_dirty: set[str] = set()
        self.git_inventory_available = False

        self.docs: list[dict[str, Any]] = []
        self.work_item_mentions: dict[int, set[str]] = defaultdict(set)
        self.cli_commands_by_file: dict[str, list[str]] = {}
        self.cmake_targets_by_file: dict[str, list[str]] = {}
        self.json_reports: list[dict[str, Any]] = []

        self.direct_source_counts_by_dir: dict[str, Counter[str]] = defaultdict(Counter)
        self.header_basenames: dict[str, list[str]] = defaultdict(list)
        self.source_basenames: dict[str, list[str]] = defaultdict(list)

    def add_finding(
        self,
        severity: str,
        category: str,
        path: str,
        message: str,
        evidence: str = "",
    ) -> None:
        self.findings.append(Finding(severity, category, path, message, evidence))

    def load_git_state(self) -> None:
        code, stdout, _ = run_cmd(["git", "ls-files"], self.root)
        if code == 0:
            self.git_inventory_available = True
            self.git_tracked = {normalize_rel(x.strip()) for x in stdout.splitlines() if x.strip()}

        code, stdout, _ = run_cmd(["git", "ls-files", "--others", "--exclude-standard"], self.root)
        if code == 0:
            self.git_untracked = {normalize_rel(x.strip()) for x in stdout.splitlines() if x.strip()}

        code, stdout, _ = run_cmd(["git", "status", "--short"], self.root)
        if code == 0:
            for line in stdout.splitlines():
                if not line.strip():
                    continue
                status = line[:2]
                path = normalize_rel(line[3:].strip())
                if " -> " in path:
                    path = path.split(" -> ", 1)[1]
                self.git_status_by_path[path] = status
                if status.strip():
                    self.git_dirty.add(path)

    def iter_files(self) -> list[Path]:
        if self.git_inventory_available:
            paths = self.git_tracked | self.git_untracked
            return sorted(
                self.root / path
                for path in paths
                if not should_ignore_rel(path) and (self.root / path).is_file()
            )

        files: list[Path] = []
        for dirpath, dirnames, filenames in os.walk(self.root):
            current = Path(dirpath)

            rel_dir = "." if current == self.root else rel(current, self.root)
            if rel_dir != "." and should_ignore_rel(rel_dir + "/"):
                dirnames[:] = []
                continue

            dirnames[:] = [
                d for d in dirnames
                if d not in IGNORE_DIR_NAMES
                and not should_ignore_rel(normalize_rel(str(Path(rel_dir) / d)) + "/")
            ]

            for name in filenames:
                path = current / name
                r = rel(path, self.root)
                if should_ignore_rel(r):
                    continue
                files.append(path)
        return files

    def analyze(self) -> dict[str, Any]:
        self.load_git_state()
        files = self.iter_files()
        rel_files = [rel(p, self.root) for p in files]

        for path in files:
            self.analyze_file(path)

        self.analyze_layout()
        self.analyze_docs()
        self.analyze_tools()
        self.analyze_samples()
        self.analyze_build_reports()
        self.analyze_git_hygiene()

        modules = self.build_module_summary()
        summary = self.build_summary(files)

        data = {
            "metadata": {
                "root": str(self.root),
                "fileCount": len(files),
                "trackedCount": len(self.git_tracked),
                "untrackedCount": len(self.git_untracked),
                "dirtyCount": len(self.git_dirty),
            },
            "summary": summary,
            "modules": modules,
            "git": {
                "untracked": sorted(self.git_untracked),
                "dirty": sorted(self.git_dirty),
                "statusByPath": self.git_status_by_path,
            },
            "docs": {
                "documents": self.docs,
                "workItemMentions": {
                    str(k): sorted(v) for k, v in sorted(self.work_item_mentions.items())
                },
            },
            "tools": {
                "cliCommandsByFile": self.cli_commands_by_file,
            },
            "buildSystem": {
                "cmakeTargetsByFile": self.cmake_targets_by_file,
            },
            "buildReports": self.json_reports,
            "sourceLayout": {
                "directSourceCountsByDir": {
                    k: dict(v) for k, v in sorted(self.direct_source_counts_by_dir.items())
                },
                "headerBasenames": self.header_basenames,
                "sourceBasenames": self.source_basenames,
            },
            "files": [asdict(x) for x in self.file_records],
            "snippets": self.snippets,
            "findings": [asdict(x) for x in self.findings],
            "tree": make_tree(rel_files, max_depth=6, max_entries=5000),
        }
        return data

    def analyze_file(self, path: Path) -> None:
        r = rel(path, self.root)
        ext = path.suffix.lower()
        category = classify_top_level(r)
        size = path.stat().st_size
        git_status = self.git_status_by_path.get(r, "tracked" if r in self.git_tracked else "untracked" if r in self.git_untracked else "")

        record = FileRecord(
            path=r,
            category=category,
            extension=ext or "<no_ext>",
            sizeBytes=size,
            gitStatus=git_status,
        )

        if ext in SOURCE_EXTENSIONS:
            self.direct_source_counts_by_dir[rel(path.parent, self.root)][ext] += 1
            stem = path.stem.lower()
            if ext in CPP_HEADER_EXTENSIONS:
                self.header_basenames[stem].append(r)
            if ext in CPP_SOURCE_EXTENSIONS:
                self.source_basenames[stem].append(r)

        if is_text_candidate(path):
            text = read_text_safe(path, self.max_file_bytes)
            record.lines = count_lines(text)
            record.sha256_16 = sha256_short(path)
            record.textKind = "text" if text else "too_large_or_unreadable"

            if self.include_snippets and text:
                self.snippets[r] = "\n".join(text.splitlines()[:self.max_snippet_lines])

            lower = text.lower()

            markers = [m for m in SUSPICIOUS_MARKERS if m in lower]
            record.suspiciousMarkers = markers

            for marker in markers:
                if marker in {"todo", "fixme", "hack", "stub", "placeholder", "dummy", "not implemented"}:
                    self.add_finding(
                        "info",
                        "suspicious-marker",
                        r,
                        f"Contains marker `{marker}`.",
                    )

            work_item_nums = extract_work_item_mentions(text)
            record.workItemMentions = work_item_nums
            for work_item in work_item_nums:
                self.work_item_mentions[work_item].add(r)

            self.detect_claims(r, text)
            self.detect_boundary_text(r, text)

            if r.startswith("SagaWiki/") and ext in DOC_EXTENSIONS:
                headings = extract_headings(text)
                record.headings = headings
                self.docs.append({
                    "path": r,
                    "lines": record.lines,
                    "headings": headings,
                    "workItemMentions": work_item_nums,
                    "markers": markers,
                    "gitStatus": git_status,
                })

            if ext in SOURCE_EXTENSIONS:
                record.includes = extract_includes(text)

            if ext == ".cs" and r.startswith("Tools/"):
                commands = extract_cli_commands(text)
                record.commands = commands
                if commands:
                    self.cli_commands_by_file[r] = commands

            if path.name == "CMakeLists.txt" or ext == ".cmake":
                targets = extract_cmake_targets(text)
                record.cmakeTargets = targets
                if targets:
                    self.cmake_targets_by_file[r] = targets

            if ext in JSON_EXTENSIONS:
                self.inspect_json(path, r, text)

            if size > 250_000 and ext in SOURCE_EXTENSIONS:
                self.add_finding(
                    "medium",
                    "large-source-file",
                    r,
                    f"Large source file: {size} bytes. Consider splitting.",
                )

            if record.lines > 1500 and ext in SOURCE_EXTENSIONS:
                self.add_finding(
                    "medium",
                    "very-long-source-file",
                    r,
                    f"Very long source file: {record.lines} lines.",
                )

        self.file_records.append(record)

    def detect_claims(self, path: str, text: str) -> None:
        if Path(path).suffix.lower() not in DOC_EXTENSIONS | JSON_EXTENSIONS:
            return
        lower = text.lower()
        for pattern in POSITIVE_CLAIM_PATTERNS:
            for m in re.finditer(pattern, lower):
                if is_negated_claim(lower, m.start(), m.end()):
                    continue
                self.add_finding(
                    "high",
                    "possible-positive-overclaim",
                    path,
                    f"Possible positive claim matched `{pattern}`.",
                    context_around(text, m.start()),
                )

    def detect_boundary_text(self, path: str, text: str) -> None:
        if is_public_header(path):
            if not path.startswith("Engine/Source/Editor/EditorQt/") and re.search(
                QT_PUBLIC_HEADER_PATTERN, text
            ):
                self.add_finding(
                    "high",
                    "public-boundary-leak",
                    path,
                    "Qt include outside the EditorQt public owner",
                )
            for label, pattern in PUBLIC_HEADER_FORBIDDEN_PATTERNS:
                if re.search(pattern, text):
                    self.add_finding(
                        "medium",
                        "public-boundary-leak",
                        path,
                        label,
                    )

        for prefix, (layer, patterns) in LAYER_FORBIDDEN_HINTS.items():
            if path.startswith(prefix) and "/Tests/" not in path:
                for pattern in patterns:
                    if re.search(pattern, text):
                        self.add_finding(
                            "medium",
                            "layer-boundary-risk",
                            path,
                            f"{layer} file contains boundary-sensitive pattern `{pattern}`.",
                        )

    def inspect_json(self, path: Path, r: str, text: str) -> None:
        if not text:
            return

        try:
            if path.suffix.lower() == ".jsonl":
                statuses = []
                for line in text.splitlines():
                    if line.strip():
                        statuses.append(simple_json_status(json.loads(line)))
                if statuses:
                    self.json_reports.append({
                        "path": r,
                        "kind": "jsonl",
                        "records": len(statuses),
                        "sampleStatus": statuses[:5],
                    })
                return

            obj = json.loads(text)
            status = simple_json_status(obj)
            if status:
                self.json_reports.append({
                    "path": r,
                    "kind": "json",
                    "status": status,
                })

            if isinstance(obj, dict):
                if obj.get("mutatesSource") is True:
                    self.add_finding(
                        "high",
                        "json-report-mutates-source",
                        r,
                        "JSON report declares mutatesSource=true.",
                    )

                if obj.get("enforcement") not in {None, "ReportOnly"} and "Build/" in r:
                    self.add_finding(
                        "medium",
                        "json-report-enforcement",
                        r,
                        f"Generated-looking report has enforcement={obj.get('enforcement')!r}.",
                    )

        except Exception as exc:
            if r.startswith("Build/") or r.startswith("Samples/") or r.startswith("Tools/"):
                self.add_finding(
                    "low",
                    "json-parse-failed",
                    r,
                    f"Could not parse JSON: {exc}",
                )

    def analyze_layout(self) -> None:
        retired_paths = sorted(RETIRED_OWNERSHIP_ROOTS) + ["Tools/scripts"]
        for retired_path in retired_paths:
            if (self.root / retired_path).exists():
                self.add_finding(
                    "high",
                    "retired-ownership-root",
                    retired_path,
                    "Retired ownership root exists; move behavior to a current owner instead of restoring the old layout.",
                )

        for directory, counts in sorted(self.direct_source_counts_by_dir.items()):
            total = sum(counts.values())
            if total >= 30:
                self.add_finding(
                    "medium",
                    "flat-source-directory",
                    directory,
                    f"Directory contains {total} direct source/header files.",
                    str(dict(counts)),
                )

        header_only = []
        source_only = []

        for stem, paths in self.header_basenames.items():
            if stem not in self.source_basenames and len(paths) <= 5:
                header_only.extend(paths)

        for stem, paths in self.source_basenames.items():
            if stem not in self.header_basenames and len(paths) <= 5:
                source_only.extend(paths)

        for p in header_only[:200]:
            if not ("/third" in p.lower() or "/external" in p.lower()):
                self.add_finding(
                    "low",
                    "header-without-source-pair",
                    p,
                    "Header basename has no matching .cpp/.cxx file. This can be fine, but check if intentional.",
                )

        for p in source_only[:200]:
            self.add_finding(
                "low",
                "source-without-header-pair",
                p,
                "Source basename has no matching header. This can be fine, but check if intentional.",
            )

    def analyze_docs(self) -> None:
        docs_by_area: dict[str, list[str]] = defaultdict(list)

        for d in self.docs:
            path = d["path"]
            if not path.startswith("SagaWiki/"):
                continue
            parts = path.split("/")
            area = wiki_area(path)
            docs_by_area[area].append(path)

            if path == "SagaWiki/index.html":
                continue

            if len(parts) == 2:
                self.add_finding(
                    "low",
                    "docs-root-clutter",
                    path,
                    "Document is directly under SagaWiki/. Consider grouping.",
                )

            if d["lines"] > 1200:
                self.add_finding(
                    "medium",
                    "long-doc",
                    path,
                    f"Long doc: {d['lines']} lines. Consider splitting or summarizing.",
                )

            if not d["headings"]:
                self.add_finding(
                    "low",
                    "doc-no-headings",
                    path,
                    "Markdown/text doc has no headings.",
                )

        for area, paths in docs_by_area.items():
            if area not in DOC_AREA_ORDER and area != "<wiki-root>":
                self.add_finding(
                    "low",
                    "unusual-doc-area",
                    f"SagaWiki/{area}",
                    f"Wiki area `{area}` is not in the current library areas.",
                )

        if self.work_item_mentions:
            nums = sorted(self.work_item_mentions)
            for n in range(min(nums), max(nums) + 1):
                if n not in self.work_item_mentions:
                    if 0 <= n <= 300:
                        self.add_finding(
                            "info",
                            "work-item-number-gap",
                            f"Work item {n}",
                            "Work item number missing from detected work item range.",
                        )

            for work_item, paths in sorted(self.work_item_mentions.items()):
                if len(paths) >= 8:
                    self.add_finding(
                        "medium",
                        "work-item-sprawl",
                        f"Work item {work_item}",
                        f"Work item appears in {len(paths)} files. Roadmap state may be duplicated.",
                        ", ".join(sorted(paths)[:8]),
                    )

    def analyze_tools(self) -> None:
        tool_dirs = sorted(
            p for p in (self.root / "Tools").glob("*")
            if p.is_dir()
        ) if (self.root / "Tools").exists() else []

        for tool in tool_dirs:
            r = rel(tool, self.root)
            files = [p for p in tool.rglob("*") if p.is_file()]
            csproj = list(tool.glob("*.csproj"))
            tests = list(tool.rglob("tests/*.py")) + list(tool.rglob("*Tests.cpp"))
            readme = tool / "README.md"

            if not csproj and any(p.suffix == ".cs" for p in files):
                self.add_finding(
                    "medium",
                    "tool-missing-csproj",
                    r,
                    "Tool has C# files but no .csproj.",
                )

            if not tests:
                self.add_finding(
                    "medium",
                    "tool-missing-tests",
                    r,
                    "Tool has no obvious focused tests.",
                )

            if not readme.exists():
                self.add_finding(
                    "low",
                    "tool-missing-readme",
                    r,
                    "Tool has no README.md.",
                )

            if any(rel(p, self.root) in self.git_untracked for p in files):
                self.add_finding(
                    "high",
                    "untracked-tool-files",
                    r,
                    "Tool has untracked files. git diff scans may not cover it.",
                )

    def analyze_samples(self) -> None:
        samples = self.root / "Samples"
        if not samples.exists():
            self.add_finding(
                "medium",
                "samples-missing",
                "Samples",
                "No canonical Samples directory found.",
            )
            return

        for sample_dir in sorted(p for p in samples.glob("*") if p.is_dir()):
            r = rel(sample_dir, self.root)
            files = [p for p in sample_dir.rglob("*") if p.is_file()]
            sagaproj = list(sample_dir.glob("*.sagaproj"))
            generated = [p for p in files if "/Generated/" in rel(p, self.root)]
            fixtures = [p for p in files if "fixture" in p.name.lower() or "test" in p.name.lower()]
            scenes = [p for p in files if "/Scenes/" in rel(p, self.root) or p.suffix == ".scene.json"]
            readme = sample_dir / "README.md"

            if not sagaproj:
                self.add_finding(
                    "medium",
                    "sample-missing-project",
                    r,
                    "Sample has no .sagaproj.",
                )

            if not readme.exists():
                self.add_finding(
                    "low",
                    "sample-missing-readme",
                    r,
                    "Sample has no README.md.",
                )

            if not scenes:
                self.add_finding(
                    "medium",
                    "sample-missing-scene-source",
                    r,
                    "Sample has no obvious scene source truth.",
                )

            if len(generated) >= 8:
                self.add_finding(
                    "medium",
                    "sample-generated-clutter",
                    r,
                    f"Sample contains {len(generated)} generated-looking files.",
                )

            if len(fixtures) >= 10:
                self.add_finding(
                    "medium",
                    "sample-fixture-clutter",
                    r,
                    f"Sample contains {len(fixtures)} fixture/test-looking files.",
                )

            source_files = [p for p in files if p.suffix.lower() in SOURCE_EXTENSIONS]
            if len(source_files) == 0 and sagaproj:
                self.add_finding(
                    "info",
                    "sample-no-code",
                    r,
                    "Sample has project metadata but no obvious code files.",
                )

    def analyze_build_reports(self) -> None:
        for prefix in ["Build/SourceTruth", "Build/Evaluation", "Build/Enterprise", "Build/ProjectReadiness"]:
            d = self.root / prefix
            if not d.exists():
                continue

            reports = sorted(d.rglob("*.json"))
            if not reports:
                self.add_finding(
                    "info",
                    "build-report-dir-empty",
                    prefix,
                    "Build report directory exists but contains no JSON reports.",
                )
                continue

            for p in reports:
                r = rel(p, self.root)
                text = read_text_safe(p, self.max_file_bytes)
                if not text:
                    continue
                try:
                    obj = json.loads(text)
                except Exception:
                    self.add_finding(
                        "medium",
                        "generated-report-invalid-json",
                        r,
                        "Generated report is not valid JSON.",
                    )
                    continue

                status = simple_json_status(obj)
                status_text = json.dumps(status, ensure_ascii=False)

                if "MissingEvidence" in status_text or "Blocked" in status_text:
                    self.add_finding(
                        "medium",
                        "generated-report-not-clean",
                        r,
                        f"Generated report has missing/blocked-like status: {status_text}",
                    )

                if isinstance(obj, dict):
                    for key, expected in [
                        ("mutatesSource", False),
                        ("enforcement", "ReportOnly"),
                    ]:
                        if key in obj and obj[key] != expected:
                            self.add_finding(
                                "medium",
                                "report-invariant-mismatch",
                                r,
                                f"{key}={obj[key]!r}, expected {expected!r}.",
                            )

    def analyze_git_hygiene(self) -> None:
        if self.git_untracked:
            by_top = Counter(p.split("/", 1)[0] for p in self.git_untracked)
            for top, count in by_top.most_common():
                if count >= 5:
                    self.add_finding(
                        "high",
                        "many-untracked-files",
                        top,
                        f"{count} untracked files under `{top}`. Scans using git diff may miss them.",
                    )

        for p in sorted(self.git_untracked):
            if p.startswith((
                "Engine/Source/",
                "Samples/",
                "SagaWiki/",
                "Tools/",
                "Tests/",
                "cmake/",
            )):
                self.add_finding(
                    "medium",
                    "important-untracked-file",
                    p,
                    "Important implementation-looking file is untracked.",
                )

    def build_module_summary(self) -> list[dict[str, Any]]:
        by_module: dict[str, dict[str, Any]] = defaultdict(lambda: {
            "fileCount": 0,
            "sourceCount": 0,
            "docCount": 0,
            "testCount": 0,
            "jsonCount": 0,
            "untrackedCount": 0,
            "dirtyCount": 0,
            "subdirs": set(),
        })

        for rec in self.file_records:
            parts = rec.path.split("/")
            module = parts[0]
            item = by_module[module]
            item["fileCount"] += 1

            if rec.extension in SOURCE_EXTENSIONS:
                item["sourceCount"] += 1
            if rec.extension in DOC_EXTENSIONS:
                item["docCount"] += 1
            if rec.extension in JSON_EXTENSIONS:
                item["jsonCount"] += 1
            if "/tests/" in rec.path.lower() or rec.path.startswith("Tests/") or rec.path.endswith("_test.py"):
                item["testCount"] += 1
            if rec.path in self.git_untracked:
                item["untrackedCount"] += 1
            if rec.path in self.git_dirty:
                item["dirtyCount"] += 1
            if len(parts) > 1:
                item["subdirs"].add(parts[1])

        out = []
        for module, item in sorted(by_module.items()):
            item = dict(item)
            item["name"] = module
            item["subdirs"] = sorted(item["subdirs"])
            out.append(item)
        return out

    def build_summary(self, files: list[Path]) -> dict[str, Any]:
        ext = Counter(p.suffix.lower() or "<no_ext>" for p in files)
        top = Counter(classify_top_level(rel(p, self.root)) for p in files)
        severity = Counter(f.severity for f in self.findings)
        category = Counter(f.category for f in self.findings)

        return {
            "filesByExtension": dict(ext.most_common()),
            "filesByTopLevelCategory": dict(top.most_common()),
            "findingsBySeverity": dict(severity.most_common()),
            "findingsByCategory": dict(category.most_common()),
            "workItemRange": [
                min(self.work_item_mentions) if self.work_item_mentions else None,
                max(self.work_item_mentions) if self.work_item_mentions else None,
            ],
        }


# ─────────────────────────────────────────────────────────────────────────────
# Report writers
# ─────────────────────────────────────────────────────────────────────────────

def write_json(data: dict[str, Any], out: Path) -> None:
    write_text(out, json.dumps(data, indent=2, ensure_ascii=False))


def write_csv_file_index(data: dict[str, Any], out: Path) -> None:
    out.parent.mkdir(parents=True, exist_ok=True)
    fields = [
        "path", "category", "extension", "sizeBytes", "lines",
        "gitStatus", "sha256_16", "textKind",
    ]
    with out.open("w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for rec in data["files"]:
            w.writerow({k: rec.get(k, "") for k in fields})


def md_escape(s: Any) -> str:
    return str(s).replace("|", "\\|").replace("\n", " ")


def write_findings_md(data: dict[str, Any], out: Path) -> None:
    findings = data["findings"]
    lines = ["# Findings", ""]

    if not findings:
        lines.append("No findings.")
    else:
        order = {"high": 0, "medium": 1, "low": 2, "info": 3}
        findings = sorted(findings, key=lambda f: (order.get(f["severity"], 9), f["category"], f["path"]))

        for f in findings:
            lines.append(f"## {f['severity'].upper()} — {f['category']}")
            lines.append("")
            lines.append(f"- Path: `{f['path']}`")
            lines.append(f"- Message: {f['message']}")
            if f.get("evidence"):
                lines.append(f"- Evidence: `{f['evidence']}`")
            lines.append("")

    write_text(out, "\n".join(lines))


def write_docs_audit_md(data: dict[str, Any], out: Path) -> None:
    docs = data["docs"]["documents"]
    work_item_mentions = data["docs"]["workItemMentions"]

    lines = ["# Docs Audit", ""]

    by_area = defaultdict(list)
    for d in docs:
        area = wiki_area(d["path"])
        by_area[area].append(d)

    lines.append("## Docs by Area")
    lines.append("")
    lines.append("| Area | Count |")
    lines.append("|---|---:|")
    for area, items in sorted(by_area.items()):
        lines.append(f"| `{md_escape(area)}` | {len(items)} |")

    lines.append("")
    lines.append("## Long / Suspicious Docs")
    lines.append("")
    for d in sorted(docs, key=lambda x: x["lines"], reverse=True)[:80]:
        markers = ", ".join(d.get("markers", []))
        lines.append(f"- `{d['path']}` — lines={d['lines']}, work items={d.get('workItemMentions', [])}, markers=[{markers}]")

    lines.append("")
    lines.append("## Work item Mentions")
    lines.append("")
    for work_item, paths in sorted(work_item_mentions.items(), key=lambda kv: int(kv[0])):
        lines.append(f"### Work item {work_item}")
        for p in paths[:40]:
            lines.append(f"- `{p}`")
        if len(paths) > 40:
            lines.append(f"- ... {len(paths) - 40} more")
        lines.append("")

    write_text(out, "\n".join(lines))


def write_work_item_audit_md(data: dict[str, Any], out: Path) -> None:
    work_item_mentions = {int(k): v for k, v in data["docs"]["workItemMentions"].items()}
    lines = ["# Work item Audit", ""]

    if not work_item_mentions:
        lines.append("No work item mentions detected.")
        write_text(out, "\n".join(lines))
        return

    nums = sorted(work_item_mentions)
    lines.append(f"- Work item range: `{min(nums)}–{max(nums)}`")
    lines.append(f"- Work item count with mentions: `{len(nums)}`")
    lines.append("")

    missing = [n for n in range(min(nums), max(nums) + 1) if n not in work_item_mentions]
    lines.append("## Missing Work item Numbers in Detected Range")
    lines.append("")
    if missing:
        lines.append(", ".join(map(str, missing[:300])))
        if len(missing) > 300:
            lines.append(f"\n... {len(missing) - 300} more")
    else:
        lines.append("None.")

    lines.append("")
    lines.append("## Work item Sprawl")
    lines.append("")
    for work_item in nums:
        paths = work_item_mentions[work_item]
        if len(paths) >= 5:
            lines.append(f"- Work item `{work_item}` appears in `{len(paths)}` files.")
            for p in paths[:10]:
                lines.append(f"  - `{p}`")
    write_text(out, "\n".join(lines))


def write_boundary_audit_md(data: dict[str, Any], out: Path) -> None:
    findings = [
        f for f in data["findings"]
        if f["category"] in {
            "public-boundary-leak",
            "layer-boundary-risk",
            "retired-ownership-root",
            "flat-source-directory",
            "important-untracked-file",
            "many-untracked-files",
        }
    ]

    lines = ["# Boundary Audit", ""]
    if not findings:
        lines.append("No boundary findings.")
    else:
        for f in findings:
            lines.append(f"- **{f['severity']}** `{f['category']}` — `{f['path']}`: {f['message']}")
            if f.get("evidence"):
                lines.append(f"  - Evidence: `{f['evidence']}`")
    write_text(out, "\n".join(lines))


def write_tools_audit_md(data: dict[str, Any], out: Path) -> None:
    lines = ["# Tools Audit", ""]
    commands = data["tools"]["cliCommandsByFile"]

    if not commands:
        lines.append("No CLI commands detected.")
    else:
        for path, cmds in sorted(commands.items()):
            lines.append(f"## `{path}`")
            for c in cmds:
                lines.append(f"- `{c}`")
            lines.append("")

    lines.append("## Tool-related findings")
    lines.append("")
    for f in data["findings"]:
        if "tool" in f["category"] or f["path"].startswith("Tools/"):
            lines.append(f"- **{f['severity']}** `{f['category']}` — `{f['path']}`: {f['message']}")

    write_text(out, "\n".join(lines))


def write_samples_audit_md(data: dict[str, Any], out: Path) -> None:
    lines = ["# Samples Audit", ""]
    sample_files = [f for f in data["files"] if f["path"].startswith("Samples/")]

    by_sample = defaultdict(list)
    for f in sample_files:
        parts = f["path"].split("/")
        sample = parts[1] if len(parts) > 1 else "<root>"
        by_sample[sample].append(f)

    lines.append("## Samples")
    lines.append("")
    for sample, files in sorted(by_sample.items()):
        ext = Counter(x["extension"] for x in files)
        lines.append(f"### `{sample}`")
        lines.append(f"- Files: `{len(files)}`")
        lines.append(f"- Extensions: `{dict(ext.most_common())}`")
        generated = [x["path"] for x in files if "/Generated/" in x["path"]]
        scenes = [x["path"] for x in files if "/Scenes/" in x["path"] or x["path"].endswith(".scene.json")]
        lines.append(f"- Scene-like files: `{len(scenes)}`")
        lines.append(f"- Generated-like files: `{len(generated)}`")
        if scenes:
            lines.append("- Scene files:")
            for p in scenes[:20]:
                lines.append(f"  - `{p}`")
        lines.append("")

    lines.append("## Sample Findings")
    lines.append("")
    for f in data["findings"]:
        if f["path"].startswith("Samples/") or "sample" in f["category"]:
            lines.append(f"- **{f['severity']}** `{f['category']}` — `{f['path']}`: {f['message']}")

    write_text(out, "\n".join(lines))


def write_build_reports_md(data: dict[str, Any], out: Path) -> None:
    reports = data["buildReports"]
    lines = ["# Build Reports Audit", ""]

    if not reports:
        lines.append("No JSON build reports detected.")
    else:
        lines.append("| Path | Status Summary |")
        lines.append("|---|---|")
        for r in sorted(reports, key=lambda x: x["path"]):
            status = r.get("status") or r.get("sampleStatus") or {}
            lines.append(f"| `{md_escape(r['path'])}` | `{md_escape(json.dumps(status, ensure_ascii=False))}` |")

    write_text(out, "\n".join(lines))


def write_ai_context_pack_md(data: dict[str, Any], out: Path) -> None:
    lines = ["# SagaEngine AI Context Pack", ""]

    lines.append("## Repo Summary")
    lines.append("")
    meta = data["metadata"]
    lines.append(f"- Files: `{meta['fileCount']}`")
    lines.append(f"- Tracked files: `{meta['trackedCount']}`")
    lines.append(f"- Untracked files: `{meta['untrackedCount']}`")
    lines.append(f"- Dirty paths: `{meta['dirtyCount']}`")
    lines.append("")

    lines.append("## Critical Findings")
    lines.append("")
    critical = [f for f in data["findings"] if f["severity"] in {"high", "medium"}]
    if not critical:
        lines.append("No high/medium findings.")
    else:
        for f in critical[:120]:
            lines.append(f"- **{f['severity']}** `{f['category']}` — `{f['path']}`: {f['message']}")
            if f.get("evidence"):
                lines.append(f"  - Evidence: `{f['evidence']}`")
    lines.append("")

    lines.append("## Modules")
    lines.append("")
    lines.append("| Module | Files | Source | Docs | Tests | Untracked | Dirty |")
    lines.append("|---|---:|---:|---:|---:|---:|---:|")
    for m in data["modules"]:
        lines.append(
            f"| `{m['name']}` | {m['fileCount']} | {m['sourceCount']} | "
            f"{m['docCount']} | {m['testCount']} | {m['untrackedCount']} | {m['dirtyCount']} |"
        )

    lines.append("")
    lines.append("## Tool Commands")
    lines.append("")
    for path, cmds in sorted(data["tools"]["cliCommandsByFile"].items()):
        lines.append(f"### `{path}`")
        for c in cmds[:100]:
            lines.append(f"- `{c}`")
        lines.append("")

    lines.append("## Build Report Statuses")
    lines.append("")
    for r in sorted(data["buildReports"], key=lambda x: x["path"])[:200]:
        status = r.get("status") or r.get("sampleStatus") or {}
        lines.append(f"- `{r['path']}`: `{json.dumps(status, ensure_ascii=False)}`")

    lines.append("")
    lines.append("## Untracked Important Paths")
    lines.append("")
    untracked = data["git"]["untracked"]
    if not untracked:
        lines.append("No untracked files detected.")
    else:
        for p in untracked[:300]:
            if p.startswith(("Engine/Source/", "Samples/", "SagaWiki/", "Tools/", "Tests/", "cmake/")):
                lines.append(f"- `{p}`")

    lines.append("")
    lines.append("## Tree Evaluation")
    lines.append("")
    lines.append("```text")
    lines.extend(data["tree"][:1500])
    lines.append("```")
    lines.append("")

    write_text(out, "\n".join(lines))


def write_tree(data: dict[str, Any], out: Path) -> None:
    write_text(out, "\n".join(data["tree"]) + "\n")


# ─────────────────────────────────────────────────────────────────────────────
# Main
# ─────────────────────────────────────────────────────────────────────────────

def main() -> int:
    parser = argparse.ArgumentParser(
        description="SagaEngine repo xray: AI-readable architecture, docs, boundary, tools, samples, and evidence audit."
    )
    parser.add_argument("--root", default=".", help="Repository root.")
    parser.add_argument("--out", default="Build/RepoXray", help="Output directory.")
    parser.add_argument("--max-file-bytes", type=int, default=768_000, help="Max bytes to read per text file.")
    parser.add_argument("--include-snippets", action="store_true", help="Include first lines of text files in JSON output.")
    parser.add_argument("--max-snippet-lines", type=int, default=60, help="Max snippet lines per text file.")
    args = parser.parse_args()

    root = Path(args.root).resolve()
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)

    if not root.exists():
        print(f"Root does not exist: {root}", file=sys.stderr)
        return 2

    analyzer = RepoXray(
        root=root,
        out=out,
        max_file_bytes=args.max_file_bytes,
        include_snippets=args.include_snippets,
        max_snippet_lines=args.max_snippet_lines,
    )

    data = analyzer.analyze()

    write_ai_context_pack_md(data, out / "00_ai_context_pack.md")
    write_json(data, out / "01_repo_xray.json")
    write_findings_md(data, out / "02_findings.md")
    write_docs_audit_md(data, out / "03_docs_audit.md")
    write_work_item_audit_md(data, out / "04_work_item_audit.md")
    write_boundary_audit_md(data, out / "05_boundary_audit.md")
    write_tools_audit_md(data, out / "06_tools_audit.md")
    write_samples_audit_md(data, out / "07_samples_audit.md")
    write_build_reports_md(data, out / "08_build_reports_audit.md")
    write_csv_file_index(data, out / "09_file_index.csv")
    write_tree(data, out / "10_repo_tree.txt")

    summary = data["summary"]
    severity = summary["findingsBySeverity"]

    print(f"Wrote reports to: {out}")
    print(f"Files: {data['metadata']['fileCount']}")
    print(f"Tracked: {data['metadata']['trackedCount']}")
    print(f"Untracked: {data['metadata']['untrackedCount']}")
    print(f"Dirty: {data['metadata']['dirtyCount']}")
    print(f"Findings: {severity}")

    high = severity.get("high", 0)
    medium = severity.get("medium", 0)

    if high:
        print("Result: HIGH findings present. Review 02_findings.md and 05_boundary_audit.md.")
        return 1

    if medium:
        print("Result: medium findings present. Review recommended before more work items.")
        return 0

    print("Result: no high/medium findings.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
