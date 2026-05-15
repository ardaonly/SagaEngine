#!/usr/bin/env python3
"""Guard the Engine-wide Public/Private migration boundary."""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys


HEADER_SUFFIXES = {".h", ".hh", ".hpp", ".hxx"}
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", *HEADER_SUFFIXES}

PRIVATE_INCLUDE_RE = re.compile(r"#\s*include\s*[<\"]([^>\"]*Private/[^>\"]*)[>\"]")
DIAGNOSTICS_FORBIDDEN_RE = re.compile(
    r"#\s*include\s*[<\"](?:SagaServer/|SagaEditor/|SDE/|SagaEngine/Server/|SagaEngine/Editor/|SagaEngine/Core/Private/)[^>\"]*[>\"]"
)
SIMULATION_FORBIDDEN_RE = re.compile(
    r"#\s*include\s*[<\"]SagaEngine/Input/Networking/[^>\"]*[>\"]"
)
ENGINE_PUBLIC_FORBIDDEN_DIRS = (
    Path("SagaEngine/Server"),
    Path("SagaEngine/Platform/SDL"),
    Path("SagaEngine/Input/Backends/SDL"),
    Path("SagaEngine/Render/Backend/Diligent"),
)
ENGINE_PUBLIC_FORBIDDEN_FILES = (
    Path("SagaEngine/Resources/Formats/GltfMeshImporter.h"),
)
ENGINE_PUBLIC_FORBIDDEN_INCLUDE_RE = re.compile(
    r"#\s*include\s*[<\"](?:SagaEngine/Server/|SagaEngine/Platform/SDL/|SagaEngine/Input/Backends/SDL/|SagaEngine/Render/Backend/Diligent/)[^>\"]*[>\"]"
)


def relative(path: Path, root: Path) -> Path:
    return path.resolve().relative_to(root.resolve())


def iter_code_files(root: Path) -> list[Path]:
    return sorted(
        path
        for path in root.rglob("*")
        if path.is_file() and path.suffix in SOURCE_SUFFIXES
    )


def check_private_includes(repo_root: Path) -> list[str]:
    errors: list[str] = []
    for path in iter_code_files(repo_root):
        for line_no, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
            match = PRIVATE_INCLUDE_RE.search(line)
            if not match:
                continue

            include_path = match.group(1)
            errors.append(
                f"{relative(path, repo_root)}:{line_no}: illegal Private include: {include_path}"
            )
    return errors


def check_diagnostics_public_headers(repo_root: Path) -> list[str]:
    diagnostics_public = (
        repo_root / "Engine" / "Public" / "SagaEngine" / "Diagnostics"
    )
    if not diagnostics_public.exists():
        return []

    errors: list[str] = []
    for path in sorted(p for p in diagnostics_public.rglob("*") if p.suffix in HEADER_SUFFIXES):
        for line_no, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
            if DIAGNOSTICS_FORBIDDEN_RE.search(line):
                errors.append(
                    f"{relative(path, repo_root)}:{line_no}: Diagnostics public header depends upward: {line.strip()}"
                )
    return errors


def check_simulation_public_headers(repo_root: Path) -> list[str]:
    simulation_public = (
        repo_root / "Engine" / "Public" / "SagaEngine" / "Simulation"
    )
    if not simulation_public.exists():
        return []

    errors: list[str] = []
    for path in sorted(p for p in simulation_public.rglob("*") if p.suffix in HEADER_SUFFIXES):
        for line_no, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
            if SIMULATION_FORBIDDEN_RE.search(line):
                errors.append(
                    f"{relative(path, repo_root)}:{line_no}: Simulation public header depends on input networking: {line.strip()}"
                )
    return errors


def check_engine_public_header_boundaries(repo_root: Path) -> list[str]:
    engine_public = repo_root / "Engine" / "Public"
    if not engine_public.exists():
        return []

    errors: list[str] = []
    for rel_dir in ENGINE_PUBLIC_FORBIDDEN_DIRS:
        forbidden_dir = engine_public / rel_dir
        if forbidden_dir.exists():
            errors.append(
                f"{relative(forbidden_dir, repo_root)} must not exist in Engine/Public"
            )

    for rel_file in ENGINE_PUBLIC_FORBIDDEN_FILES:
        forbidden_file = engine_public / rel_file
        if forbidden_file.exists():
            errors.append(
                f"{relative(forbidden_file, repo_root)} must not exist in Engine/Public"
            )

    for path in sorted(p for p in engine_public.rglob("*") if p.suffix in HEADER_SUFFIXES):
        for line_no, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
            if ENGINE_PUBLIC_FORBIDDEN_INCLUDE_RE.search(line):
                errors.append(
                    f"{relative(path, repo_root)}:{line_no}: Engine public header exposes concrete/backend/server path: {line.strip()}"
                )
    return errors


def check_no_root_source_sagaengine(repo_root: Path) -> list[str]:
    errors: list[str] = []
    if (repo_root / "Source" / "SagaEngine").exists():
        errors.append("Source/SagaEngine must not exist; engine code lives under Engine")
    if (repo_root / "Engine" / "include").exists():
        errors.append("Engine/include must not exist; public headers live under Engine/Public")
    if (repo_root / "Engine" / "src").exists():
        errors.append("Engine/src must not exist; private sources live under Engine/Private")

    engine_root = repo_root / "Engine"
    if engine_root.exists():
        for entry in sorted(engine_root.iterdir()):
            if not entry.is_dir() or entry.name in {"Public", "Private"}:
                continue
            if (entry / "Public").exists() or (entry / "Private").exists():
                errors.append(
                    f"{relative(entry, repo_root)} must not own Public/Private; use Engine/Public and Engine/Private"
                )
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".")
    args = parser.parse_args()

    repo_root = Path(args.repo_root).resolve()
    errors = [
        *check_no_root_source_sagaengine(repo_root),
        *check_private_includes(repo_root),
        *check_engine_public_header_boundaries(repo_root),
        *check_diagnostics_public_headers(repo_root),
        *check_simulation_public_headers(repo_root),
    ]
    if errors:
        print("[public_private_boundary] ERROR")
        for error in errors:
            print(f"  {error}")
        return 1

    print("[public_private_boundary] OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
