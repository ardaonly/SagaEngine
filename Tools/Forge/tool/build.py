#!/usr/bin/env python3
"""
build.py — Forge bootstrap installer.

Run this script with no arguments to build the Forge binary from source
and stage it in `Tools/Forge/tool/bin/forge` (`.exe` on Windows).

It is intentionally self-contained:

  * Standard library only — no `pip install` required.
  * Cross-platform — works on Linux, macOS, and Windows under Python 3.8+.
  * Single dependency at runtime: `cmake` on PATH (≥ 3.22). If `cmake`
    is missing the script prints a clear error and exits non-zero.

Typical usage:

    python3 Tools/Forge/tool/build.py            # release build
    python3 Tools/Forge/tool/build.py --debug    # debug build
    python3 Tools/Forge/tool/build.py --clean    # wipe the build dir first

After a successful run:

    Tools/Forge/tool/bin/forge --version

The dispatcher (`Tools/SagaTools/`) calls this script when the user runs
`tools install forge`.
"""

from __future__ import annotations

import argparse
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path

# ─── Optional shared platform helpers ───────────────────────────────────────
# This import is intentionally optional: Forge/tool/build.py is designed to
# work standalone (extractable outside the engine tree).  If platform_detect
# is unavailable the fallback messages below are used instead.
# parents[2]: Tools/Forge/tool/ → Tools/Forge/ → Tools/
try:
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    from common import platform_detect as _pd
    _PD_AVAILABLE = True
except (ImportError, IndexError):
    _pd = None          # type: ignore[assignment]
    _PD_AVAILABLE = False

# ─── Constants ──────────────────────────────────────────────────────────────

HERE         = Path(__file__).resolve().parent          # Tools/Forge/tool/
BUILD_DIR    = HERE / "build"
BIN_DIR      = HERE / "bin"
BINARY_NAME  = "forge.exe" if platform.system() == "Windows" else "forge"

REQUIRED_CMAKE_MAJOR = 3
REQUIRED_CMAKE_MINOR = 22


# ─── CLI ────────────────────────────────────────────────────────────────────

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        prog="forge build.py",
        description="Compile Forge from source and stage the binary in bin/.",
    )
    parser.add_argument(
        "--debug", action="store_true",
        help="Build with CMAKE_BUILD_TYPE=Debug instead of Release.",
    )
    parser.add_argument(
        "--clean", action="store_true",
        help="Remove the build directory before configuring.",
    )
    parser.add_argument(
        "--jobs", "-j", type=int, default=0,
        help="Parallel build jobs (default: cmake's autoselection).",
    )
    return parser.parse_args()


# ─── Tool detection ─────────────────────────────────────────────────────────

def find_cmake() -> str:
    """Return the absolute path of the `cmake` binary, or exit with help text."""
    cmake = shutil.which("cmake")
    if cmake:
        return cmake
    if _PD_AVAILABLE:
        sys.stderr.write(_pd.cmake_not_found_hint("[forge build.py] "))
    else:
        sys.stderr.write(
            "[forge build.py] ERROR: `cmake` was not found on PATH.\n"
            "[forge build.py] Forge wraps cmake — install it before running this bootstrap.\n"
            "[forge build.py]   Linux  : sudo apt install cmake   (or your distro's equivalent)\n"
            "[forge build.py]   NixOS  : nix-shell -p cmake\n"
            "[forge build.py]   macOS  : brew install cmake\n"
            "[forge build.py]   Windows: https://cmake.org/download\n"
        )
    sys.exit(2)


def check_cmake_version(cmake: str) -> None:
    """Print the detected cmake version, warn if older than the required minimum."""
    try:
        out = subprocess.check_output([cmake, "--version"], text=True).strip()
    except subprocess.CalledProcessError as ex:
        sys.stderr.write(f"[forge build.py] ERROR: `cmake --version` failed: {ex}\n")
        sys.exit(2)

    first = out.splitlines()[0] if out else ""
    print(f"[forge build.py] {first}")

    # Parse "cmake version X.Y.Z" if possible; this is best-effort.
    try:
        token = first.split()[-1]
        parts = token.split(".")
        major, minor = int(parts[0]), int(parts[1])
    except Exception:
        sys.stderr.write("[forge build.py] WARNING: could not parse cmake version string.\n")
        return

    if (major, minor) < (REQUIRED_CMAKE_MAJOR, REQUIRED_CMAKE_MINOR):
        sys.stderr.write(
            f"[forge build.py] WARNING: cmake {major}.{minor} < required "
            f"{REQUIRED_CMAKE_MAJOR}.{REQUIRED_CMAKE_MINOR}; the build may fail.\n"
        )


# ─── Build steps ────────────────────────────────────────────────────────────

def step_clean() -> None:
    if BUILD_DIR.exists():
        print(f"[forge build.py] removing {BUILD_DIR}")
        shutil.rmtree(BUILD_DIR)


def step_configure(cmake: str, build_type: str) -> None:
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    cmd = [
        cmake,
        "-S", str(HERE),
        "-B", str(BUILD_DIR),
        f"-DCMAKE_BUILD_TYPE={build_type}",
    ]
    print(f"[forge build.py] {' '.join(cmd)}")
    subprocess.check_call(cmd)


def step_build(cmake: str, build_type: str, jobs: int) -> None:
    cmd = [cmake, "--build", str(BUILD_DIR), "--config", build_type, "--target", "forge"]
    if jobs > 0:
        cmd += ["-j", str(jobs)]
    print(f"[forge build.py] {' '.join(cmd)}")
    subprocess.check_call(cmd)


def step_stage_binary() -> Path:
    """Copy the built binary into bin/. Returns the staged binary path."""
    BIN_DIR.mkdir(parents=True, exist_ok=True)

    # Multi-config generators (MSBuild, XCode) put the binary in <build>/<Config>/.
    candidates = [
        BUILD_DIR / BINARY_NAME,
        BUILD_DIR / "Release" / BINARY_NAME,
        BUILD_DIR / "Debug"   / BINARY_NAME,
    ]
    src = next((p for p in candidates if p.exists()), None)
    if src is None:
        sys.stderr.write(
            f"[forge build.py] ERROR: built binary not found in any of:\n"
            + "".join(f"[forge build.py]   {p}\n" for p in candidates)
        )
        sys.exit(3)

    dst = BIN_DIR / BINARY_NAME
    shutil.copy2(src, dst)
    if platform.system() != "Windows":
        os.chmod(dst, 0o755)
    print(f"[forge build.py] staged: {dst}")
    return dst


def step_smoke_test(staged: Path) -> None:
    print(f"[forge build.py] smoke test: {staged} --version")
    subprocess.check_call([str(staged), "--version"])


# ─── Entry point ────────────────────────────────────────────────────────────

def main() -> int:
    args = parse_args()
    build_type = "Debug" if args.debug else "Release"

    # ── NixOS: tri-state dependency validation + auto-shell ──────────────
    if _PD_AVAILABLE:
        missing = _pd.build_deps_missing(require_clang=False)
        if missing:
            sys.stderr.write(_pd.missing_deps_hint("[forge build.py] ", missing))
            if _pd.detect() == _pd.PlatformKind.NIXOS and not os.environ.get("IN_NIX_SHELL"):
                pkgs = _pd.collect_packages(missing, "[forge build.py] ")
                rc = _pd.auto_nix_shell(
                    str(Path(__file__).resolve()), sys.argv[1:],
                    pkgs, "[forge build.py] ",
                )
                if rc is not None:
                    sys.exit(rc)
            sys.exit(2)
    # ────────────────────────────────────────────────────────────────────

    print(f"[forge build.py] Forge source : {HERE}")
    print(f"[forge build.py] Build type   : {build_type}")
    print(f"[forge build.py] Build dir    : {BUILD_DIR}")
    print(f"[forge build.py] Bin dir      : {BIN_DIR}")

    cmake = find_cmake()
    check_cmake_version(cmake)

    if args.clean:
        step_clean()

    step_configure(cmake, build_type)
    step_build(cmake, build_type, args.jobs)
    staged = step_stage_binary()
    step_smoke_test(staged)

    print(f"[forge build.py] DONE. Add to PATH: {BIN_DIR}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except subprocess.CalledProcessError as ex:
        sys.stderr.write(f"[forge build.py] FAILED (exit {ex.returncode}): {' '.join(ex.cmd)}\n")
        sys.exit(ex.returncode or 1)
    except KeyboardInterrupt:
        sys.stderr.write("\n[forge build.py] interrupted.\n")
        sys.exit(130)
