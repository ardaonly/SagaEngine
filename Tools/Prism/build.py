#!/usr/bin/env python3
"""
build.py — Prism bootstrap installer.

Run this script to set up Prism's two layers and stage their entry
points in `Tools/Prism/bin/`:

  * `prism-extract` — C++ binary built via CMake. Requires `cmake` AND
    a discoverable LLVM/Clang installation (LLVM 16+). When LLVM is
    missing the script prints a clean diagnostic and SKIPS this layer
    rather than aborting.
  * `prism-graph`   — pure-Python pipeline (`Tools/Prism/pipeline/`).
    Always installable; only Python 3.8+ required.

Typical usage:

    python3 Tools/Prism/build.py                    # full install
    python3 Tools/Prism/build.py --skip-extractor   # Python only
    python3 Tools/Prism/build.py --llvm-dir <path>  # explicit LLVM
    python3 Tools/Prism/build.py --debug --clean

After a successful run:

    Tools/Prism/bin/prism-extract --help     (if LLVM was present)
    Tools/Prism/bin/prism-graph --help       (always)

The dispatcher (`Tools/SagaTools/`) calls this script when the user
runs `tools install prism`.
"""

from __future__ import annotations

import argparse
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path


# ─── Constants ──────────────────────────────────────────────────────────────

HERE          = Path(__file__).resolve().parent          # Tools/Prism/
EXTRACTOR_DIR = HERE / "extractor"
PIPELINE_DIR  = HERE / "pipeline"
BUILD_DIR     = HERE / "build"
BIN_DIR       = HERE / "bin"

EXTRACTOR_BINARY = "prism-extract.exe" if platform.system() == "Windows" else "prism-extract"
GRAPH_LAUNCHER   = "prism-graph.cmd"   if platform.system() == "Windows" else "prism-graph"


# ─── CLI ────────────────────────────────────────────────────────────────────

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        prog="prism build.py",
        description="Compile + stage the Prism extractor and pipeline.",
    )
    parser.add_argument("--debug", action="store_true",
                        help="Build the extractor with CMAKE_BUILD_TYPE=Debug.")
    parser.add_argument("--clean", action="store_true",
                        help="Remove the build directory before configuring.")
    parser.add_argument("--jobs", "-j", type=int, default=0,
                        help="Parallel build jobs (default: cmake's autoselection).")
    parser.add_argument("--skip-extractor", action="store_true",
                        help="Skip the C++ extractor (only stage prism-graph).")
    parser.add_argument("--llvm-dir", default=None,
                        help="Explicit LLVM cmake config dir; passed as -DLLVM_DIR=<path>.")
    parser.add_argument("--clang-dir", default=None,
                        help="Explicit Clang cmake config dir; passed as -DClang_DIR=<path>.")
    return parser.parse_args()


# ─── Detection ──────────────────────────────────────────────────────────────

def find_cmake() -> str | None:
    return shutil.which("cmake")


def detect_llvm() -> tuple[str | None, str | None]:
    """Try to find LLVM / Clang cmake config dirs without invoking llvm-config.

    Returns (llvm_dir, clang_dir) — either may be None when not detectable.
    The extractor's CMakeLists ultimately runs `find_package(LLVM)` so the
    presence of those directories is the definitive answer; this is a
    best-effort heuristic that avoids the user having to discover paths
    on common Linux / macOS layouts.
    """
    candidates = [
        "/usr/lib/llvm-18/lib/cmake/llvm",
        "/usr/lib/llvm-17/lib/cmake/llvm",
        "/usr/lib/llvm-16/lib/cmake/llvm",
        "/opt/homebrew/opt/llvm/lib/cmake/llvm",
        "/usr/local/opt/llvm/lib/cmake/llvm",
    ]
    for cand in candidates:
        if Path(cand).is_dir():
            llvm_dir  = cand
            clang_dir = cand.replace("/llvm", "/clang", 1)
            if Path(clang_dir).is_dir():
                return llvm_dir, clang_dir
            return llvm_dir, None
    return None, None


def find_python() -> str:
    """Path to the current interpreter — used for the prism-graph launcher."""
    return sys.executable


# ─── Extractor build ────────────────────────────────────────────────────────

def build_extractor(cmake: str, args: argparse.Namespace) -> Path | None:
    """Configure and build prism-extract. Returns the staged binary path or None."""
    llvm_dir  = args.llvm_dir
    clang_dir = args.clang_dir
    if llvm_dir is None or clang_dir is None:
        det_llvm, det_clang = detect_llvm()
        llvm_dir  = llvm_dir  or det_llvm
        clang_dir = clang_dir or det_clang

    if llvm_dir is None:
        sys.stderr.write(
            "[prism build.py] WARNING: no LLVM installation detected.\n"
            "[prism build.py] Skipping prism-extract. Pass --llvm-dir <path> to force.\n"
            "[prism build.py] Install hints:\n"
            "[prism build.py]   Linux  : sudo apt install llvm-17-dev libclang-17-dev\n"
            "[prism build.py]   macOS  : brew install llvm\n"
        )
        return None

    print(f"[prism build.py] LLVM_DIR  = {llvm_dir}")
    print(f"[prism build.py] Clang_DIR = {clang_dir}")

    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    build_type = "Debug" if args.debug else "Release"

    configure = [
        cmake,
        "-S", str(EXTRACTOR_DIR),
        "-B", str(BUILD_DIR),
        f"-DCMAKE_BUILD_TYPE={build_type}",
        f"-DLLVM_DIR={llvm_dir}",
    ]
    if clang_dir:
        configure.append(f"-DClang_DIR={clang_dir}")
    print(f"[prism build.py] {' '.join(configure)}")
    try:
        subprocess.check_call(configure)
    except subprocess.CalledProcessError:
        sys.stderr.write("[prism build.py] WARNING: extractor configure failed; skipping.\n")
        return None

    build_cmd = [cmake, "--build", str(BUILD_DIR), "--config", build_type, "--target", "prism-extract"]
    if args.jobs > 0:
        build_cmd += ["-j", str(args.jobs)]
    print(f"[prism build.py] {' '.join(build_cmd)}")
    try:
        subprocess.check_call(build_cmd)
    except subprocess.CalledProcessError:
        sys.stderr.write("[prism build.py] WARNING: extractor build failed; skipping.\n")
        return None

    candidates = [
        BUILD_DIR / EXTRACTOR_BINARY,
        BUILD_DIR / "Release" / EXTRACTOR_BINARY,
        BUILD_DIR / "Debug"   / EXTRACTOR_BINARY,
    ]
    src = next((p for p in candidates if p.exists()), None)
    if src is None:
        sys.stderr.write(
            "[prism build.py] WARNING: built binary not found; skipping stage.\n"
        )
        return None

    BIN_DIR.mkdir(parents=True, exist_ok=True)
    dst = BIN_DIR / EXTRACTOR_BINARY
    shutil.copy2(src, dst)
    if platform.system() != "Windows":
        os.chmod(dst, 0o755)
    print(f"[prism build.py] staged: {dst}")
    return dst


# ─── Pipeline (Python) install ──────────────────────────────────────────────

def stage_pipeline_launcher() -> Path:
    """Drop a tiny `prism-graph` launcher into bin/ that execs the Python pipeline.

    No `pip install` — we keep the pipeline as a plain script so the bootstrap
    works on hosts that have no permission to install packages globally.
    """
    BIN_DIR.mkdir(parents=True, exist_ok=True)
    python = find_python()
    target = BIN_DIR / GRAPH_LAUNCHER
    run_py = PIPELINE_DIR / "run.py"

    if platform.system() == "Windows":
        target.write_text(
            "@echo off\r\n"
            f"\"{python}\" \"{run_py}\" %*\r\n",
            encoding="utf-8",
        )
    else:
        target.write_text(
            "#!/usr/bin/env sh\n"
            f"exec \"{python}\" \"{run_py}\" \"$@\"\n",
            encoding="utf-8",
        )
        os.chmod(target, 0o755)
    print(f"[prism build.py] staged: {target}")
    return target


# ─── Steps ──────────────────────────────────────────────────────────────────

def step_clean() -> None:
    if BUILD_DIR.exists():
        print(f"[prism build.py] removing {BUILD_DIR}")
        shutil.rmtree(BUILD_DIR)


def smoke_test(extractor: Path | None, launcher: Path) -> None:
    if extractor is not None:
        print(f"[prism build.py] smoke: {extractor} --help (best-effort)")
        subprocess.call([str(extractor), "--help"], stderr=subprocess.STDOUT)
    print(f"[prism build.py] launcher staged at {launcher} → {find_python()} {PIPELINE_DIR / 'run.py'}")


# ─── Entry point ────────────────────────────────────────────────────────────

def main() -> int:
    args = parse_args()

    print(f"[prism build.py] Prism root  : {HERE}")
    print(f"[prism build.py] Bin dir     : {BIN_DIR}")

    if args.clean:
        step_clean()

    extractor: Path | None = None
    if not args.skip_extractor:
        cmake = find_cmake()
        if cmake is None:
            sys.stderr.write(
                "[prism build.py] WARNING: `cmake` not on PATH — skipping extractor.\n"
            )
        else:
            extractor = build_extractor(cmake, args)
    else:
        print("[prism build.py] --skip-extractor set; not building C++ layer.")

    launcher = stage_pipeline_launcher()
    smoke_test(extractor, launcher)

    print(f"[prism build.py] DONE. Add to PATH: {BIN_DIR}")
    if extractor is None:
        print("[prism build.py] NOTE: extractor was not built. Re-run with --llvm-dir or"
              " install LLVM and try again.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except subprocess.CalledProcessError as ex:
        sys.stderr.write(f"[prism build.py] FAILED (exit {ex.returncode}): {' '.join(ex.cmd)}\n")
        sys.exit(ex.returncode or 1)
    except KeyboardInterrupt:
        sys.stderr.write("\n[prism build.py] interrupted.\n")
        sys.exit(130)
