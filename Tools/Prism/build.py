#!/usr/bin/env python3
"""
build.py — Prism bootstrap installer.

Builds/stages:
  - prism-extract: C++/Clang LibTooling extractor
  - prism-graph: Python graph pipeline launcher

This script distinguishes dependency detection errors from compiler/build
errors. If LLVM is found but the extractor does not compile, it reports that
as a source/API/build failure, not as "LLVM missing".
"""

from __future__ import annotations

import argparse
import os
import platform
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


# ─── Paths ───────────────────────────────────────────────────────────────────

HERE = Path(__file__).resolve().parent

EXTRACTOR_DIR = HERE / "extractor"
PIPELINE_DIR = HERE / "pipeline"

BUILD_ROOT = HERE / "build"
EXTRACTOR_BUILD_DIR = BUILD_ROOT / "extractor"

BIN_DIR = HERE / "bin"

EXTRACTOR_BINARY = "prism-extract.exe" if platform.system() == "Windows" else "prism-extract"
GRAPH_LAUNCHER = "prism-graph.cmd" if platform.system() == "Windows" else "prism-graph"
PRISM_LAUNCHER = "prism.cmd" if platform.system() == "Windows" else "prism"


# ─── Optional shared platform helpers ────────────────────────────────────────

sys.path.insert(0, str(HERE.parent))

try:
    from common import platform_detect as _pd
    _PD_AVAILABLE = True
except ImportError:
    _pd = None  # type: ignore[assignment]
    _PD_AVAILABLE = False


# ─── Models ──────────────────────────────────────────────────────────────────

@dataclass(frozen=True)
class LLVMDetection:
    llvm_dir: str | None
    clang_dir: str | None
    source: str
    llvm_config: str | None = None
    version: str | None = None


@dataclass(frozen=True)
class ExtractorBuildResult:
    built: bool
    staged_path: Path | None
    reason: str


# ─── CLI ─────────────────────────────────────────────────────────────────────

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        prog="prism build.py",
        description="Compile + stage the Prism extractor and Python pipeline.",
    )

    parser.add_argument("--debug", action="store_true",
                        help="Build prism-extract with CMAKE_BUILD_TYPE=Debug.")
    parser.add_argument("--clean", action="store_true",
                        help="Remove Prism build directory before configuring.")
    parser.add_argument("--jobs", "-j", type=int, default=0,
                        help="Parallel CMake build jobs.")
    parser.add_argument("--skip-extractor", action="store_true",
                        help="Skip C++ extractor and stage only prism-graph.")
    parser.add_argument("--strict-extractor", action="store_true",
                        help="Compatibility option; full extractor builds already fail on extractor errors.")
    parser.add_argument("--llvm-dir", default=None,
                        help="Explicit LLVM CMake config dir, e.g. .../lib/cmake/llvm.")
    parser.add_argument("--clang-dir", default=None,
                        help="Explicit Clang CMake config dir, e.g. .../lib/cmake/clang.")
    parser.add_argument("--print-detection", action="store_true",
                        help="Print detected CMake/LLVM/Clang paths and exit.")
    parser.add_argument("--run-tests", action="store_true",
                        help="Run Prism's lightweight Python tests after staging.")

    return parser.parse_args()


# ─── Small process helpers ───────────────────────────────────────────────────

def run_capture(cmd: list[str]) -> tuple[int, str, str]:
    try:
        p = subprocess.run(
            cmd,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        return p.returncode, p.stdout.strip(), p.stderr.strip()
    except OSError as exc:
        return 127, "", str(exc)


def find_cmake() -> str | None:
    return shutil.which("cmake")


def find_python() -> str:
    return sys.executable


# ─── LLVM / Clang detection ──────────────────────────────────────────────────

def _clang_dir_from_llvm_dir(llvm_dir: str) -> str | None:
    p = Path(llvm_dir)

    # Usually:
    #   .../lib/cmake/llvm
    #   .../lib/cmake/clang
    candidate = p.parent / "clang"
    if candidate.is_dir():
        return str(candidate)

    # Defensive fallback.
    text = str(p)
    if text.endswith("/llvm"):
        candidate = Path(text[:-len("/llvm")] + "/clang")
        if candidate.is_dir():
            return str(candidate)

    match = re.search(r"-llvm-(\d+\.\d+)", text)
    if match:
        clang_dir = _clang_dir_from_nix_store(match.group(1))
        if clang_dir:
            return clang_dir

    return None


def _clang_dir_from_nix_store(version_prefix: str) -> str | None:
    nix_store = Path("/nix/store")
    if not nix_store.is_dir():
        return None

    candidates = sorted(
        nix_store.glob(f"*-clang-{version_prefix}*/lib/cmake/clang"),
        key=lambda p: str(p),
        reverse=True,
    )

    for candidate in candidates:
        if candidate.is_dir():
            return str(candidate)

    return None


def _detect_from_explicit(llvm_dir: str | None, clang_dir: str | None) -> LLVMDetection | None:
    if llvm_dir:
        llvm_path = Path(llvm_dir)
        if llvm_path.is_dir():
            final_clang = clang_dir if clang_dir and Path(clang_dir).is_dir() else _clang_dir_from_llvm_dir(llvm_dir)
            return LLVMDetection(str(llvm_path), final_clang, "explicit --llvm-dir/--clang-dir")
    return None


def _detect_from_env() -> LLVMDetection | None:
    llvm_dir = os.environ.get("LLVM_DIR")
    clang_dir = os.environ.get("Clang_DIR") or os.environ.get("CLANG_DIR")

    if llvm_dir and Path(llvm_dir).is_dir():
        final_clang = clang_dir if clang_dir and Path(clang_dir).is_dir() else _clang_dir_from_llvm_dir(llvm_dir)
        return LLVMDetection(llvm_dir, final_clang, "environment LLVM_DIR/Clang_DIR")

    return None


def _detect_from_llvm_config() -> LLVMDetection | None:
    llvm_config = shutil.which("llvm-config")
    if not llvm_config:
        return None

    rc, cmakedir, _ = run_capture([llvm_config, "--cmakedir"])
    if rc != 0 or not cmakedir:
        return None

    llvm_dir = Path(cmakedir)
    if not llvm_dir.is_dir():
        return None

    version = None
    rc, out, _ = run_capture([llvm_config, "--version"])
    if rc == 0 and out:
        version = out

    clang_dir = _clang_dir_from_llvm_dir(str(llvm_dir))
    if clang_dir is None and version:
        parts = version.split(".")
        if len(parts) >= 2:
            clang_dir = _clang_dir_from_nix_store(f"{parts[0]}.{parts[1]}")

    return LLVMDetection(
        llvm_dir=str(llvm_dir),
        clang_dir=clang_dir,
        source="llvm-config --cmakedir",
        llvm_config=llvm_config,
        version=version,
    )


def _detect_from_common_helper(llvm_dir: str | None, clang_dir: str | None) -> LLVMDetection | None:
    if not _PD_AVAILABLE:
        return None

    found_llvm, found_clang = _pd.find_llvm_cmake_dirs(llvm_dir=llvm_dir, clang_dir=clang_dir)
    if found_llvm:
        return LLVMDetection(found_llvm, found_clang, "Tools/common/platform_detect.py")

    return None


def _detect_from_fhs_paths() -> LLVMDetection | None:
    candidates = [
        "/usr/lib/llvm-20/lib/cmake/llvm",
        "/usr/lib/llvm-19/lib/cmake/llvm",
        "/usr/lib/llvm-18/lib/cmake/llvm",
        "/usr/lib/llvm-17/lib/cmake/llvm",
        "/usr/lib/llvm-16/lib/cmake/llvm",
        "/opt/homebrew/opt/llvm/lib/cmake/llvm",
        "/usr/local/opt/llvm/lib/cmake/llvm",
    ]

    for candidate in candidates:
        p = Path(candidate)
        if p.is_dir():
            return LLVMDetection(
                llvm_dir=str(p),
                clang_dir=_clang_dir_from_llvm_dir(str(p)),
                source="known FHS/Homebrew paths",
            )

    return None


def _detect_from_nix_store() -> LLVMDetection | None:
    nix_store = Path("/nix/store")
    if not nix_store.is_dir():
        return None

    llvm_candidates = sorted(
        nix_store.glob("*llvm*-dev/lib/cmake/llvm"),
        key=lambda p: str(p),
        reverse=True,
    )

    for llvm_dir in llvm_candidates:
        if llvm_dir.is_dir():
            return LLVMDetection(
                llvm_dir=str(llvm_dir),
                clang_dir=_clang_dir_from_llvm_dir(str(llvm_dir)),
                source="/nix/store glob",
            )

    return None


def detect_llvm(llvm_dir: str | None, clang_dir: str | None) -> LLVMDetection:
    detectors = [
        lambda: _detect_from_explicit(llvm_dir, clang_dir),
        _detect_from_env,
        _detect_from_llvm_config,
        lambda: _detect_from_common_helper(llvm_dir, clang_dir),
        _detect_from_fhs_paths,
        _detect_from_nix_store,
    ]

    for detector in detectors:
        found = detector()
        if found and found.llvm_dir:
            return found

    return LLVMDetection(None, None, "not found")


def print_detection(cmake: str | None, llvm: LLVMDetection) -> None:
    print("[prism build.py] Detection")
    print(f"  platform     : {platform.system()}")
    print(f"  in_nix_shell : {bool(os.environ.get('IN_NIX_SHELL'))}")
    print(f"  cmake        : {cmake or '<not found>'}")
    print(f"  llvm_dir     : {llvm.llvm_dir or '<not found>'}")
    print(f"  clang_dir    : {llvm.clang_dir or '<not found>'}")
    print(f"  source       : {llvm.source}")
    print(f"  llvm-config  : {llvm.llvm_config or '<not used>'}")
    print(f"  llvm version : {llvm.version or '<unknown>'}")


def llvm_missing_hint(prefix: str = "[prism build.py] ") -> str:
    return (
        f"{prefix}ERROR: LLVM/Clang CMake package directories were not found.\n"
        f"{prefix}Prism extractor needs LLVM + Clang development packages.\n"
        f"{prefix}Ways to fix:\n"
        f"{prefix}  1. Enter a dev shell that provides llvmPackages.clang, llvmPackages.llvm, cmake.\n"
        f"{prefix}  2. Or pass --llvm-dir .../lib/cmake/llvm --clang-dir .../lib/cmake/clang.\n"
        f"{prefix}  3. Or set LLVM_DIR and Clang_DIR environment variables.\n"
        f"{prefix}For NixOS, prefer a flake/devShell instead of relying on global paths.\n"
    )


# ─── Extractor build ─────────────────────────────────────────────────────────

def configure_extractor(cmake: str, args: argparse.Namespace, llvm: LLVMDetection) -> bool:
    build_type = "Debug" if args.debug else "Release"

    EXTRACTOR_BUILD_DIR.mkdir(parents=True, exist_ok=True)

    configure = [
        cmake,
        "-S", str(EXTRACTOR_DIR),
        "-B", str(EXTRACTOR_BUILD_DIR),
        f"-DCMAKE_BUILD_TYPE={build_type}",
        f"-DLLVM_DIR={llvm.llvm_dir}",
    ]

    if llvm.clang_dir:
        configure.append(f"-DClang_DIR={llvm.clang_dir}")

    if _PD_AVAILABLE:
        extra_prefixes = _pd.find_nix_dep_prefixes(["zlib", "libxml2", "libffi"])
        if extra_prefixes:
            configure.append(f"-DCMAKE_PREFIX_PATH={';'.join(extra_prefixes)}")

    print(f"[prism build.py] configure: {' '.join(configure)}")

    try:
        subprocess.check_call(configure)
        return True
    except subprocess.CalledProcessError as exc:
        sys.stderr.write(f"[prism build.py] ERROR: extractor configure failed with exit {exc.returncode}.\n")
        return False


def compile_extractor(cmake: str, args: argparse.Namespace) -> bool:
    build_type = "Debug" if args.debug else "Release"

    build_cmd = [
        cmake,
        "--build", str(EXTRACTOR_BUILD_DIR),
        "--config", build_type,
        "--target", "prism-extract",
    ]

    if args.jobs > 0:
        build_cmd += ["-j", str(args.jobs)]

    print(f"[prism build.py] build: {' '.join(build_cmd)}")

    try:
        subprocess.check_call(build_cmd)
        return True
    except subprocess.CalledProcessError as exc:
        sys.stderr.write(f"[prism build.py] ERROR: extractor compile/link failed with exit {exc.returncode}.\n")
        sys.stderr.write("[prism build.py] This means LLVM was found, but Prism source/build files failed to compile.\n")
        return False


def stage_extractor_binary(args: argparse.Namespace) -> Path | None:
    build_type = "Debug" if args.debug else "Release"

    candidates = [
        EXTRACTOR_BUILD_DIR / EXTRACTOR_BINARY,
        EXTRACTOR_BUILD_DIR / build_type / EXTRACTOR_BINARY,
        EXTRACTOR_BUILD_DIR / "Release" / EXTRACTOR_BINARY,
        EXTRACTOR_BUILD_DIR / "Debug" / EXTRACTOR_BINARY,
    ]

    src = next((p for p in candidates if p.exists()), None)
    if src is None:
        sys.stderr.write("[prism build.py] ERROR: extractor binary was not found after build.\n")
        return None

    BIN_DIR.mkdir(parents=True, exist_ok=True)
    dst = BIN_DIR / EXTRACTOR_BINARY
    shutil.copy2(src, dst)

    if platform.system() != "Windows":
        os.chmod(dst, 0o755)

    print(f"[prism build.py] staged extractor: {dst}")
    return dst


def build_extractor(cmake: str, args: argparse.Namespace, llvm: LLVMDetection) -> ExtractorBuildResult:
    if llvm.llvm_dir is None:
        sys.stderr.write(llvm_missing_hint())
        return ExtractorBuildResult(False, None, "llvm_not_found")

    print(f"[prism build.py] LLVM_DIR  = {llvm.llvm_dir}")
    print(f"[prism build.py] Clang_DIR = {llvm.clang_dir or '<not found>'}")
    print(f"[prism build.py] LLVM source = {llvm.source}")

    if not configure_extractor(cmake, args, llvm):
        return ExtractorBuildResult(False, None, "configure_failed")

    if not compile_extractor(cmake, args):
        return ExtractorBuildResult(False, None, "compile_failed")

    staged = stage_extractor_binary(args)
    if staged is None:
        return ExtractorBuildResult(False, None, "stage_failed")

    return ExtractorBuildResult(True, staged, "ok")


# ─── Pipeline launcher ───────────────────────────────────────────────────────

def stage_pipeline_launcher() -> Path:
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
            f"exec python3 \"{run_py}\" \"$@\"\n",
            encoding="utf-8",
        )
        os.chmod(target, 0o755)

    print(f"[prism build.py] staged graph launcher: {target}")
    return target


def stage_prism_launcher() -> Path:
    BIN_DIR.mkdir(parents=True, exist_ok=True)

    python = find_python()
    target = BIN_DIR / PRISM_LAUNCHER
    prism_py = HERE / "prism.py"

    if platform.system() == "Windows":
        target.write_text(
            "@echo off\r\n"
            f"\"{python}\" \"{prism_py}\" %*\r\n",
            encoding="utf-8",
        )
    else:
        target.write_text(
            "#!/usr/bin/env sh\n"
            f"exec python3 \"{prism_py}\" \"$@\"\n",
            encoding="utf-8",
        )
        os.chmod(target, 0o755)

    print(f"[prism build.py] staged prism launcher: {target}")
    return target


# ─── Steps ───────────────────────────────────────────────────────────────────

def step_clean() -> None:
    if BUILD_ROOT.exists():
        print(f"[prism build.py] removing {BUILD_ROOT}")
        shutil.rmtree(BUILD_ROOT)


def smoke_test(extractor: Path | None, graph_launcher: Path, prism_launcher: Path) -> bool:
    if extractor is not None:
        print(f"[prism build.py] smoke: {extractor} --help")
        rc = subprocess.call([str(extractor), "--help"], stderr=subprocess.STDOUT)
        if rc != 0:
            sys.stderr.write(f"[prism build.py] ERROR: extractor smoke test failed with exit {rc}.\n")
            return False

    print(f"[prism build.py] smoke: {prism_launcher} --help")
    rc = subprocess.call([str(prism_launcher), "--help"], stderr=subprocess.STDOUT)
    if rc != 0:
        sys.stderr.write(f"[prism build.py] ERROR: prism smoke test failed with exit {rc}.\n")
        return False

    print(f"[prism build.py] launcher: {graph_launcher}")
    print(f"[prism build.py]   -> exec python3 \"{PIPELINE_DIR / 'run.py'}\"")
    print(f"[prism build.py] launcher: {prism_launcher}")
    print(f"[prism build.py]   -> exec python3 \"{HERE / 'prism.py'}\"")
    return True


def run_tests() -> bool:
    test_runner = HERE / "tests" / "run.py"
    print(f"[prism build.py] tests: {test_runner}")

    rc = subprocess.call([find_python(), str(test_runner)])
    if rc != 0:
        sys.stderr.write(f"[prism build.py] ERROR: tests failed with exit {rc}.\n")
        return False

    return True


# ─── Main ────────────────────────────────────────────────────────────────────

def main() -> int:
    args = parse_args()

    cmake = find_cmake()
    llvm = detect_llvm(args.llvm_dir, args.clang_dir)

    if args.print_detection:
        print_detection(cmake, llvm)
        return 0

    print(f"[prism build.py] Prism root : {HERE}")
    print(f"[prism build.py] Bin dir    : {BIN_DIR}")

    if args.clean:
        step_clean()

    extractor_result = ExtractorBuildResult(False, None, "skipped")

    if args.skip_extractor:
        print("[prism build.py] --skip-extractor set; not building C++ extractor.")
    elif cmake is None:
        sys.stderr.write("[prism build.py] ERROR: cmake was not found on PATH.\n")
        extractor_result = ExtractorBuildResult(False, None, "cmake_not_found")
    elif cmake is not None:
        extractor_result = build_extractor(cmake, args, llvm)

    graph_launcher = stage_pipeline_launcher()
    prism_launcher = stage_prism_launcher()
    smoke_ok = smoke_test(extractor_result.staged_path, graph_launcher, prism_launcher)

    if not args.skip_extractor and not extractor_result.built:
        print(f"[prism build.py] NOTE: prism-extract was not built. Reason: {extractor_result.reason}")
        if extractor_result.reason == "compile_failed":
            print("[prism build.py] LLVM was detected. Fix the C++ source/CMake error, not LLVM installation.")

    if not args.skip_extractor and not extractor_result.built:
        print("[prism build.py] FAILED.")
        print(f"[prism build.py] prism launcher is available at: {prism_launcher}")
        print(f"[prism build.py] prism-graph launcher is available at: {graph_launcher}")
        return 1

    if not args.skip_extractor and not smoke_ok:
        print("[prism build.py] FAILED.")
        print(f"[prism build.py] prism launcher is available at: {prism_launcher}")
        print(f"[prism build.py] prism-graph launcher is available at: {graph_launcher}")
        return 1

    if args.run_tests and not run_tests():
        print("[prism build.py] FAILED.")
        return 1

    print("[prism build.py] DONE.")
    print(f"[prism build.py] Add to PATH: {BIN_DIR}")

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        sys.stderr.write("\n[prism build.py] interrupted.\n")
        raise SystemExit(130)
