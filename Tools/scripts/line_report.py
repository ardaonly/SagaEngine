#!/usr/bin/env python3
"""
Count repository lines by top-level directory and file kind.

Intended location:
    Tools/scripts/line_report.py
"""

from __future__ import annotations

import argparse
import fnmatch
import json
import os
import subprocess
import sys
from collections import defaultdict
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable, Iterator, Sequence


CHUNK_SIZE = 1024 * 1024
DEFAULT_MAX_FILE_MIB = 64

DEFAULT_EXTS = {
    ".c", ".cc", ".cpp", ".cxx",
    ".h", ".hh", ".hpp", ".hxx", ".inl", ".ipp",
    ".py", ".cmake", ".md", ".txt", ".rst",
    ".json", ".yaml", ".yml", ".toml", ".ini", ".cfg", ".xml",
    ".glsl", ".vert", ".frag", ".geom", ".tesc", ".tese", ".comp",
    ".hlsl", ".slang", ".metal", ".natvis",
}

SPECIAL_NAMES = {
    "CMakeLists.txt",
    "Makefile",
    "Dockerfile",
    "LICENSE",
    ".clang-format",
    ".clang-tidy",
    ".gitignore",
}

DEFAULT_EXCLUDE_NAMES = {
    ".git", ".hg", ".svn",
    ".idea", ".vscode",
    ".cache", ".pytest_cache", ".mypy_cache", ".ruff_cache",
    "__pycache__",
    ".venv", "venv",
    "node_modules",
    "build", "dist", "out", "bin", "obj", "Build",
    "_deps", "CMakeFiles", "Vendor", "docs"
}

DEFAULT_EXCLUDE_GLOBS = {
    "cmake-build-*",
    "build-*",
}


@dataclass(frozen=True, slots=True)
class FileInfo:
    path: str
    top: str
    kind: str
    lines: int
    bytes: int


@dataclass(frozen=True, slots=True)
class SkippedFile:
    path: str
    reason: str


@dataclass(frozen=True, slots=True)
class ScanConfig:
    root: Path
    allowed_kinds: set[str] | None
    all_text: bool
    exclude_names: set[str]
    exclude_globs: set[str]
    max_bytes: int


class ConfigError(RuntimeError):
    pass


def git_root(start: Path) -> Path | None:
    try:
        result = subprocess.run(
            ["git", "-C", str(start), "rev-parse", "--show-toplevel"],
            text=True,
            capture_output=True,
            check=False,
        )
    except FileNotFoundError:
        return None

    if result.returncode != 0:
        return None

    value = result.stdout.strip()
    return Path(value).resolve() if value else None


def git_metadata_root(start: Path) -> Path | None:
    current = start.resolve()

    for candidate in (current, *current.parents):
        if (candidate / ".git").exists():
            return candidate

    return None


def resolve_root(explicit: str | None) -> Path:
    if explicit:
        root = Path(explicit).expanduser().resolve()
        if not root.is_dir():
            raise ConfigError(f"invalid --repo-root: {root}")
        return root

    starts = [Path.cwd(), Path(__file__).resolve().parent]

    for start in starts:
        root = git_root(start)
        if root is not None:
            return root

    for start in starts:
        root = git_metadata_root(start)
        if root is not None:
            return root

    raise ConfigError("repository root could not be resolved; pass --repo-root")


def normalize_kind(raw: str) -> str:
    value = raw.strip()

    if not value:
        raise ConfigError("empty extension in --extensions")

    if value in SPECIAL_NAMES:
        return value

    value = value.lower()
    return value if value.startswith(".") else f".{value}"


def parse_kinds(raw: str | None, all_text: bool) -> set[str] | None:
    if all_text:
        return None

    if raw is None:
        return set(DEFAULT_EXTS) | set(SPECIAL_NAMES)

    return {normalize_kind(item) for item in raw.split(",") if item.strip()}


def split_excludes(values: Sequence[str], use_defaults: bool) -> tuple[set[str], set[str]]:
    names = set(DEFAULT_EXCLUDE_NAMES) if use_defaults else set()
    globs = set(DEFAULT_EXCLUDE_GLOBS) if use_defaults else set()

    for raw in values:
        value = raw.strip()

        if not value:
            continue

        if any(ch in value for ch in "*?[]"):
            globs.add(value)
        else:
            names.add(value)

    return names, globs


def file_kind(path: Path) -> str:
    if path.name in SPECIAL_NAMES:
        return path.name

    suffix = path.suffix.lower()
    return suffix if suffix else "<no-extension>"


def top_level(root: Path, path: Path) -> str:
    rel = path.relative_to(root)
    return "(root)" if len(rel.parts) <= 1 else rel.parts[0]


def excluded_dir(name: str, names: set[str], globs: set[str]) -> bool:
    return name in names or any(fnmatch.fnmatchcase(name, pattern) for pattern in globs)


def walk_files(config: ScanConfig) -> Iterator[Path]:
    for dirpath_raw, dirnames, filenames in os.walk(config.root, topdown=True):
        dirpath = Path(dirpath_raw)

        dirnames[:] = [
            name
            for name in dirnames
            if not (dirpath / name).is_symlink()
            and not excluded_dir(name, config.exclude_names, config.exclude_globs)
        ]

        for filename in filenames:
            path = dirpath / filename

            if not path.is_symlink():
                yield path


def is_binary(path: Path) -> bool:
    with path.open("rb") as file:
        sample = file.read(4096)

    return b"\0" in sample


def count_lines(path: Path) -> int:
    newline_count = 0
    last_byte = b""

    with path.open("rb") as file:
        while chunk := file.read(CHUNK_SIZE):
            newline_count += chunk.count(b"\n")
            last_byte = chunk[-1:]

    if not last_byte:
        return 0

    return newline_count if last_byte == b"\n" else newline_count + 1


def scan(config: ScanConfig) -> tuple[list[FileInfo], list[SkippedFile]]:
    files: list[FileInfo] = []
    skipped: list[SkippedFile] = []

    for path in walk_files(config):
        rel = path.relative_to(config.root).as_posix()
        kind = file_kind(path)

        if config.allowed_kinds is not None and kind not in config.allowed_kinds:
            continue

        try:
            stat = path.stat()

            if stat.st_size > config.max_bytes:
                skipped.append(SkippedFile(rel, f"larger than {config.max_bytes} bytes"))
                continue

            if config.all_text and is_binary(path):
                continue

            lines = count_lines(path)

        except OSError as exc:
            skipped.append(SkippedFile(rel, str(exc)))
            continue

        files.append(
            FileInfo(
                path=rel,
                top=top_level(config.root, path),
                kind=kind,
                lines=lines,
                bytes=stat.st_size,
            )
        )

    return files, skipped


def aggregate(files: Iterable[FileInfo], *attrs: str) -> list[dict[str, int | str]]:
    buckets: dict[tuple[str, ...], dict[str, int]] = defaultdict(
        lambda: {"files": 0, "lines": 0, "bytes": 0}
    )

    for file in files:
        key = tuple(str(getattr(file, attr)) for attr in attrs)
        buckets[key]["files"] += 1
        buckets[key]["lines"] += file.lines
        buckets[key]["bytes"] += file.bytes

    rows: list[dict[str, int | str]] = []

    for key, values in buckets.items():
        row: dict[str, int | str] = {
            attr: key[index]
            for index, attr in enumerate(attrs)
        }

        row.update(values)
        rows.append(row)

    return sorted(
        rows,
        key=lambda row: (
            -int(row["lines"]),
            *(str(row[attr]).lower() for attr in attrs),
        ),
    )


def totals(files: Sequence[FileInfo]) -> dict[str, int]:
    return {
        "files": len(files),
        "lines": sum(file.lines for file in files),
        "bytes": sum(file.bytes for file in files),
    }


def fmt(value: int | str) -> str:
    return f"{value:,}" if isinstance(value, int) else value


def table(
    headers: Sequence[str],
    rows: Sequence[Sequence[int | str]],
    right: set[int],
) -> str:
    if not rows:
        return "(empty)"

    string_rows = [[fmt(cell) for cell in row] for row in rows]

    widths = [
        max(len(headers[index]), *(len(row[index]) for row in string_rows))
        for index in range(len(headers))
    ]

    def line(cells: Sequence[str]) -> str:
        out: list[str] = []

        for index, cell in enumerate(cells):
            if index in right:
                out.append(cell.rjust(widths[index]))
            else:
                out.append(cell.ljust(widths[index]))

        return "  ".join(out)

    return "\n".join(
        [
            line(headers),
            "  ".join("-" * width for width in widths),
            *(line(row) for row in string_rows),
        ]
    )


def text_report(
    root: Path,
    files: Sequence[FileInfo],
    skipped: Sequence[SkippedFile],
    largest: int,
) -> str:
    total = totals(files)

    by_top = aggregate(files, "top")
    by_kind = aggregate(files, "kind")
    by_top_kind = aggregate(files, "top", "kind")
    largest_files = sorted(files, key=lambda file: (-file.lines, file.path))[:largest]

    sections = [
        f"Repository: {root}",
        "",
        "Total",
        table(
            ["Files", "Lines", "Bytes"],
            [[total["files"], total["lines"], total["bytes"]]],
            {0, 1, 2},
        ),
        "",
        "By top-level directory",
        table(
            ["Directory", "Files", "Lines"],
            [[row["top"], row["files"], row["lines"]] for row in by_top],
            {1, 2},
        ),
        "",
        "By file kind",
        table(
            ["Kind", "Files", "Lines"],
            [[row["kind"], row["files"], row["lines"]] for row in by_kind],
            {1, 2},
        ),
        "",
        "By top-level directory and file kind",
        table(
            ["Directory", "Kind", "Files", "Lines"],
            [
                [row["top"], row["kind"], row["files"], row["lines"]]
                for row in by_top_kind
            ],
            {2, 3},
        ),
    ]

    if largest > 0:
        sections += [
            "",
            f"Largest files, top {largest}",
            table(
                ["Path", "Kind", "Lines"],
                [[file.path, file.kind, file.lines] for file in largest_files],
                {2},
            ),
        ]

    if skipped:
        sections += [
            "",
            "Skipped files",
            table(
                ["Path", "Reason"],
                [[item.path, item.reason] for item in skipped],
                set(),
            ),
        ]

    return "\n".join(sections)


def json_report(
    root: Path,
    files: Sequence[FileInfo],
    skipped: Sequence[SkippedFile],
    largest: int,
) -> str:
    payload = {
        "repository": str(root),
        "totals": totals(files),
        "by_top_level_directory": aggregate(files, "top"),
        "by_file_kind": aggregate(files, "kind"),
        "by_top_level_directory_and_file_kind": aggregate(files, "top", "kind"),
        "largest_files": [
            asdict(file)
            for file in sorted(files, key=lambda file: (-file.lines, file.path))[:largest]
        ],
        "skipped_files": [asdict(item) for item in skipped],
    }

    return json.dumps(payload, indent=2, ensure_ascii=False)


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(
        description="Count repository lines by directory and file kind."
    )

    result.add_argument(
        "--repo-root",
        help="Repository root. Defaults to Git root.",
    )

    result.add_argument(
        "--extensions",
        help="Comma-separated kinds: .cpp,.h,.hpp,.md,CMakeLists.txt",
    )

    result.add_argument(
        "--all-text",
        action="store_true",
        help="Count all non-binary files.",
    )

    result.add_argument(
        "--exclude-dir",
        action="append",
        default=[],
        help="Extra directory name/glob to exclude.",
    )

    result.add_argument(
        "--no-default-excludes",
        action="store_true",
        help="Disable default ignored directories.",
    )

    result.add_argument(
        "--max-file-mib",
        type=int,
        default=DEFAULT_MAX_FILE_MIB,
    )

    result.add_argument(
        "--largest",
        type=int,
        default=20,
    )

    result.add_argument(
        "--format",
        choices=("text", "json"),
        default="text",
    )

    result.add_argument(
        "--strict",
        action="store_true",
        help="Exit 1 when files were skipped.",
    )

    return result


def build_config(args: argparse.Namespace) -> ScanConfig:
    if args.all_text and args.extensions:
        raise ConfigError("--all-text and --extensions cannot be used together")

    if args.max_file_mib <= 0:
        raise ConfigError("--max-file-mib must be positive")

    if args.largest < 0:
        raise ConfigError("--largest must be zero or positive")

    exclude_names, exclude_globs = split_excludes(
        args.exclude_dir,
        use_defaults=not args.no_default_excludes,
    )

    return ScanConfig(
        root=resolve_root(args.repo_root),
        allowed_kinds=parse_kinds(args.extensions, args.all_text),
        all_text=args.all_text,
        exclude_names=exclude_names,
        exclude_globs=exclude_globs,
        max_bytes=args.max_file_mib * 1024 * 1024,
    )


def main(argv: Sequence[str] | None = None) -> int:
    args = parser().parse_args(argv)

    try:
        config = build_config(args)
        files, skipped = scan(config)
    except ConfigError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    if args.format == "json":
        print(json_report(config.root, files, skipped, args.largest))
    else:
        print(text_report(config.root, files, skipped, args.largest))

    return 1 if args.strict and skipped else 0


if __name__ == "__main__":
    raise SystemExit(main())
