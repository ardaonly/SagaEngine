#!/usr/bin/env python3
"""
setup.py — one-shot SagaTools bootstrap.

    python3 Tools/SagaTools/setup.py            # build the dispatcher only
    python3 Tools/SagaTools/setup.py --all      # ALSO install forge + prism

Prerequisites: cmake (>= 3.22) and Python 3.8+ on PATH. Nothing else.
"""

from __future__ import annotations

import argparse
import json
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path

# `Path(__file__).resolve()` follows symlinks AND Windows directory junctions.
# That's catastrophic when the user has a junction like
#   C:\dev\SagaEngine_v0.0.7  ->  C:\Users\...\OneDrive\...\SagaEngine_v0.0.7
# because every staged path then points back to OneDrive even though the user
# explicitly cd'd to C:\dev to escape OneDrive's path-rewriting behaviour.
# `os.path.abspath(__file__)` gives the absolute path WITHOUT resolving the
# junction, preserving the user's intent.
HERE         = Path(os.path.abspath(__file__)).parent
BUILD_DIR    = HERE / "build"
BIN_DIR      = HERE / "bin"
BINARY_NAME  = "tools.exe" if platform.system() == "Windows" else "tools"

REQUIRED_CMAKE_MAJOR = 3
REQUIRED_CMAKE_MINOR = 22


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(prog="sagatools setup.py",
        description="Build SagaTools (and optionally bootstrap forge + prism).")
    p.add_argument("--debug", action="store_true",
                   help="Build with CMAKE_BUILD_TYPE=Debug.")
    p.add_argument("--clean", action="store_true",
                   help="Remove the build directory before configuring.")
    p.add_argument("--jobs", "-j", type=int, default=0,
                   help="Parallel build jobs (default: cmake's autoselection).")
    p.add_argument("--all", action="store_true",
                   help="After building tools, also run `tools install forge` "
                        "and `tools install prism`.")
    p.add_argument("--no-smoke", action="store_true",
                   help="Skip the post-build smoke test.")
    return p.parse_args()


def find_cmake() -> str:
    cmake = shutil.which("cmake")
    if cmake:
        return cmake
    sys.stderr.write(
        "[sagatools] ERROR: `cmake` was not found on PATH.\n"
        "[sagatools] Install it before running this bootstrap:\n"
        "[sagatools]   Linux  : sudo apt install cmake\n"
        "[sagatools]   macOS  : brew install cmake\n"
        "[sagatools]   Windows: https://cmake.org/download\n"
    )
    sys.exit(2)


def check_cmake_version(cmake: str) -> None:
    try:
        out = subprocess.check_output([cmake, "--version"], text=True).strip()
    except subprocess.CalledProcessError as ex:
        sys.stderr.write(f"[sagatools] ERROR: `cmake --version` failed: {ex}\n")
        sys.exit(2)
    first = out.splitlines()[0] if out else ""
    print(f"[sagatools] {first}")
    try:
        token = first.split()[-1]
        parts = token.split(".")
        major, minor = int(parts[0]), int(parts[1])
    except Exception:
        return
    if (major, minor) < (REQUIRED_CMAKE_MAJOR, REQUIRED_CMAKE_MINOR):
        sys.stderr.write(
            f"[sagatools] WARNING: cmake {major}.{minor} < required "
            f"{REQUIRED_CMAKE_MAJOR}.{REQUIRED_CMAKE_MINOR}\n"
        )


def step_clean() -> None:
    if BUILD_DIR.exists():
        print(f"[sagatools] removing {BUILD_DIR}")
        shutil.rmtree(BUILD_DIR)


def step_configure(cmake: str, build_type: str) -> None:
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    cmd = [cmake, "-S", str(HERE), "-B", str(BUILD_DIR), f"-DCMAKE_BUILD_TYPE={build_type}"]
    print(f"[sagatools] {' '.join(cmd)}")
    subprocess.check_call(cmd)


def step_build(cmake: str, build_type: str, jobs: int) -> None:
    cmd = [cmake, "--build", str(BUILD_DIR), "--config", build_type, "--target", "tools"]
    if jobs > 0:
        cmd += ["-j", str(jobs)]
    print(f"[sagatools] {' '.join(cmd)}")
    subprocess.check_call(cmd)


def stage_registry(out_path: Path) -> None:
    """Write a registry next to the binary with absolute paths for BOTH
    runtime binaries and installer scripts.

    Uses `os.path.abspath`, NOT `Path.resolve`, so Windows junctions
    (e.g. C:\\dev\\SagaEngine -> C:\\Users\\...\\OneDrive\\...\\SagaEngine)
    are NOT silently followed. The user explicitly chose a working
    directory; the staged registry should reference that exact directory.

    `ensure_ascii=False` keeps non-ASCII path characters as real UTF-8
    bytes in the JSON file. The dispatcher's parser handles both forms
    but writing UTF-8 directly is friendlier in editors and grep.
    """
    is_win = platform.system() == "Windows"
    def _abs(*parts: str) -> str:
        return os.path.abspath(str(HERE.parent.joinpath(*parts)))

    forge_exe       = _abs("Forge", "tool", "bin", "forge.exe" if is_win else "forge")
    prism_exe       = _abs("Prism", "bin", "prism-graph.cmd" if is_win else "prism-graph")
    forge_installer = _abs("Forge", "tool", "build.py")
    prism_installer = _abs("Prism", "build.py")

    payload = {
        "schema_version": "1.0",
        "tools":      {"forge": forge_exe,       "prism": prism_exe},
        "installers": {"forge": forge_installer, "prism": prism_installer},
    }
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(
        "// generated by Tools/SagaTools/setup.py -- do not hand-edit.\n"
        "// Override paths by editing this file or by setting\n"
        "// $SAGATOOLS_REGISTRY to point at a different file.\n"
        + json.dumps(payload, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )


def step_stage() -> Path:
    BIN_DIR.mkdir(parents=True, exist_ok=True)
    candidates = [
        BUILD_DIR / BINARY_NAME,
        BUILD_DIR / "Release" / BINARY_NAME,
        BUILD_DIR / "Debug" / BINARY_NAME,
    ]
    src = next((p for p in candidates if p.exists()), None)
    if src is None:
        sys.stderr.write("[sagatools] ERROR: built binary not found.\n")
        sys.exit(3)
    dst = BIN_DIR / BINARY_NAME
    shutil.copy2(src, dst)
    if platform.system() != "Windows":
        os.chmod(dst, 0o755)
    print(f"[sagatools] staged binary  : {dst}")
    cfg = BIN_DIR / "config" / "tools.registry.json"
    stage_registry(cfg)
    print(f"[sagatools] staged registry: {cfg}")
    return dst


def step_smoke(staged: Path) -> None:
    print(f"[sagatools] smoke test : {staged} --version")
    subprocess.check_call([str(staged), "--version"])
    print(f"[sagatools] smoke test : {staged} list")
    subprocess.check_call([str(staged), "list"])


def cascade_install(tools_bin: Path, name: str) -> int:
    print(f"\n[sagatools] cascade  : {tools_bin} install {name}")
    rc = subprocess.call([str(tools_bin), "install", name])
    if rc != 0:
        sys.stderr.write(
            f"[sagatools] WARNING: `tools install {name}` exited with code {rc}.\n"
            f"[sagatools] You can re-run it later with: tools install {name}\n"
        )
    return rc


def print_next_steps(tools_bin: Path) -> None:
    bin_dir = tools_bin.parent
    print()
    print("=" * 70)
    print("  SagaTools is ready.")
    print("=" * 70)
    print(f"  binary   : {tools_bin}")
    print(f"  registry : {bin_dir / 'config' / 'tools.registry.json'}")
    print()
    print("  Add this directory to your PATH for the current shell:")
    print()
    if platform.system() == "Windows":
        print(f"    set PATH={bin_dir};%PATH%               (cmd)")
        print(f'    $env:PATH = "{bin_dir};$env:PATH"      (PowerShell)')
    else:
        print(f'    export PATH="{bin_dir}:$PATH"')
    print()
    print("  Then verify:")
    print()
    print("    tools list")
    print("    tools forge --help")
    print("    tools prism --version")
    print()
    print("=" * 70)


def main() -> int:
    args = parse_args()
    build_type = "Debug" if args.debug else "Release"
    print(f"[sagatools] source     : {HERE}")
    print(f"[sagatools] build type : {build_type}")
    print(f"[sagatools] build dir  : {BUILD_DIR}")
    print(f"[sagatools] bin dir    : {BIN_DIR}")
    cmake = find_cmake()
    check_cmake_version(cmake)
    if args.clean:
        step_clean()
    step_configure(cmake, build_type)
    step_build(cmake, build_type, args.jobs)
    staged = step_stage()
    if not args.no_smoke:
        step_smoke(staged)
    if args.all:
        cascade_install(staged, "forge")
        cascade_install(staged, "prism")
    print_next_steps(staged)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except subprocess.CalledProcessError as ex:
        cmd = ex.cmd if isinstance(ex.cmd, str) else " ".join(str(c) for c in ex.cmd)
        sys.stderr.write(f"[sagatools] FAILED (exit {ex.returncode}): {cmd}\n")
        sys.exit(ex.returncode or 1)
    except KeyboardInterrupt:
        sys.stderr.write("\n[sagatools] interrupted.\n")
        sys.exit(130)
