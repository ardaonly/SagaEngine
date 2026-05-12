#!/usr/bin/env python3
# ─── SDE bootstrap installer ─────────────────────────────────────────────────
#
# Build the SDE static library (and optionally the `sde` CLI) from source and
# stage the artefacts under ``Tools/SystemDefinitionEngine/build/`` and
# ``Tools/SystemDefinitionEngine/bin/``.
#
# Two flows are supported:
#
#   1. Conan flow (default)
#         python3 build.py
#      Calls ``conan install`` for nlohmann_json (and optionally GTest), then
#      runs the resulting CMake preset.
#
#   2. Plain CMake flow
#         python3 build.py --no-conan --cmake-prefix=/path/to/prefix
#      Skips Conan and trusts the caller's CMAKE_PREFIX_PATH/find_package
#      machinery.
#
# Common flags:
#
#   --debug           Build with CMAKE_BUILD_TYPE=Debug
#   --tests           Build SDETests   (default: off)
#   --no-cli          Skip the sde CLI binary
#   --clean           Wipe the build directory before configuring
#   --jobs <n> | -j   Parallel build jobs
#   --install-prefix  Run cmake --install with the given prefix afterwards
#   --conan-create    Run `conan create .` instead of build/install (publishes
#                     sde/0.1.0 to the local Conan cache)
#
# The script depends only on the Python 3 standard library; cmake (and conan,
# for the default flow) must be on PATH.
#
from __future__ import annotations

import argparse
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path

HERE       = Path(__file__).resolve().parent          # Tools/SystemDefinitionEngine/
BUILD_DIR  = HERE / "build"
BIN_DIR    = HERE / "bin"
CLI_NAME   = "sde.exe" if platform.system() == "Windows" else "sde"

REQUIRED_CMAKE = (3, 22)


# ─── argument parsing ───────────────────────────────────────────────────────

def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        prog="sde build.py",
        description="Compile SDE from source and stage the artefacts under build/ and bin/.",
    )
    p.add_argument("--debug",           action="store_true", help="Build Debug instead of Release.")
    p.add_argument("--tests",           action="store_true", help="Build the SDETests suite.")
    p.add_argument("--no-cli",          action="store_true", help="Skip the sde CLI binary.")
    p.add_argument("--clean",           action="store_true", help="Wipe the build directory first.")
    p.add_argument("--jobs", "-j",      type=int, default=0, help="Parallel build jobs.")
    p.add_argument("--no-conan",        action="store_true",
                   help="Skip Conan install; rely on system find_package paths.")
    p.add_argument("--cmake-prefix",    default="",
                   help="CMAKE_PREFIX_PATH to inject (only meaningful with --no-conan).")
    p.add_argument("--install-prefix",  default="",
                   help="If set, run `cmake --install` with this prefix after building.")
    p.add_argument("--conan-create",    action="store_true",
                   help="Run `conan create .` and exit (publishes the package locally).")
    return p.parse_args()


# ─── tool detection ─────────────────────────────────────────────────────────

def _which_or_exit(binary: str, hint: str) -> str:
    found = shutil.which(binary)
    if not found:
        sys.stderr.write(f"[sde build.py] ERROR: `{binary}` was not found on PATH.\n")
        sys.stderr.write(f"[sde build.py] {hint}\n")
        sys.exit(2)
    return found


def find_cmake() -> str:
    return _which_or_exit("cmake", "Install CMake >= 3.22 before running this bootstrap.")


def find_conan() -> str:
    return _which_or_exit("conan", "Install Conan 2.x or pass --no-conan.")


def check_cmake_version(cmake: str) -> None:
    try:
        out = subprocess.check_output([cmake, "--version"], text=True).strip()
    except subprocess.CalledProcessError as ex:
        sys.stderr.write(f"[sde build.py] ERROR: `cmake --version` failed: {ex}\n")
        sys.exit(2)
    first = out.splitlines()[0] if out else ""
    print(f"[sde build.py] {first}")
    try:
        token = first.split()[-1]
        major, minor = (int(x) for x in token.split(".")[:2])
    except Exception:
        return
    if (major, minor) < REQUIRED_CMAKE:
        sys.stderr.write(
            f"[sde build.py] WARNING: cmake {major}.{minor} < required "
            f"{REQUIRED_CMAKE[0]}.{REQUIRED_CMAKE[1]}; the build may fail.\n"
        )


# ─── steps ──────────────────────────────────────────────────────────────────

def step_clean() -> None:
    if BUILD_DIR.exists():
        print(f"[sde build.py] removing {BUILD_DIR}")
        shutil.rmtree(BUILD_DIR)


def step_conan_install(conan: str, build_type: str, with_tests: bool, with_cli: bool) -> None:
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    cmd = [
        conan, "install", str(HERE),
        "--output-folder", str(BUILD_DIR),
        "--build=missing",
        "-s", f"build_type={build_type}",
        "-o", f"&:build_tests={'True' if with_tests else 'False'}",
        "-o", f"&:build_cli={'True' if with_cli else 'False'}",
    ]
    print(f"[sde build.py] {' '.join(cmd)}")
    subprocess.check_call(cmd)


def step_conan_create(conan: str, build_type: str) -> None:
    cmd = [conan, "create", str(HERE),
           "--build=missing",
           "-s", f"build_type={build_type}"]
    print(f"[sde build.py] {' '.join(cmd)}")
    subprocess.check_call(cmd)


def step_configure(cmake: str, build_type: str, with_tests: bool, with_cli: bool,
                   use_conan: bool, cmake_prefix: str) -> None:
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    cmd = [cmake, "-S", str(HERE), "-B", str(BUILD_DIR),
           f"-DCMAKE_BUILD_TYPE={build_type}",
           f"-DSDE_BUILD_TESTS={'ON' if with_tests else 'OFF'}",
           f"-DSDE_BUILD_CLI={'ON' if with_cli else 'OFF'}",
           "-DSDE_INSTALL=ON"]
    if use_conan:
        toolchain = BUILD_DIR / "build" / build_type / "generators" / "conan_toolchain.cmake"
        if not toolchain.exists():
            # conan layout puts it under build/generators on multi-config / single-config
            for candidate in BUILD_DIR.rglob("conan_toolchain.cmake"):
                toolchain = candidate
                break
        if toolchain.exists():
            cmd.append(f"-DCMAKE_TOOLCHAIN_FILE={toolchain}")
        else:
            sys.stderr.write("[sde build.py] WARNING: conan_toolchain.cmake not found; "
                             "configure will rely on environment.\n")
    if cmake_prefix:
        cmd.append(f"-DCMAKE_PREFIX_PATH={cmake_prefix}")
    print(f"[sde build.py] {' '.join(cmd)}")
    subprocess.check_call(cmd)


def step_build(cmake: str, build_type: str, jobs: int) -> None:
    cmd = [cmake, "--build", str(BUILD_DIR), "--config", build_type]
    if jobs > 0:
        cmd += ["-j", str(jobs)]
    print(f"[sde build.py] {' '.join(cmd)}")
    subprocess.check_call(cmd)


def step_install(cmake: str, build_type: str, prefix: str) -> None:
    cmd = [cmake, "--install", str(BUILD_DIR), "--config", build_type, "--prefix", prefix]
    print(f"[sde build.py] {' '.join(cmd)}")
    subprocess.check_call(cmd)


def step_stage_cli(build_type: str) -> Path | None:
    candidates = [
        BUILD_DIR / CLI_NAME,
        BUILD_DIR / build_type / CLI_NAME,
        BUILD_DIR / "Release" / CLI_NAME,
        BUILD_DIR / "Debug"   / CLI_NAME,
    ]
    src = next((p for p in candidates if p.exists()), None)
    if src is None:
        return None
    BIN_DIR.mkdir(parents=True, exist_ok=True)
    dst = BIN_DIR / CLI_NAME
    shutil.copy2(src, dst)
    if platform.system() != "Windows":
        os.chmod(dst, 0o755)
    print(f"[sde build.py] staged: {dst}")
    return dst


# ─── entry ──────────────────────────────────────────────────────────────────

def main() -> int:
    args = parse_args()
    build_type = "Debug" if args.debug else "Release"
    with_tests = args.tests
    with_cli   = not args.no_cli

    print(f"[sde build.py] source     : {HERE}")
    print(f"[sde build.py] build      : {BUILD_DIR}")
    print(f"[sde build.py] build type : {build_type}")
    print(f"[sde build.py] tests      : {with_tests}")
    print(f"[sde build.py] cli        : {with_cli}")

    cmake = find_cmake()
    check_cmake_version(cmake)

    if args.clean:
        step_clean()

    if args.conan_create:
        conan = find_conan()
        step_conan_create(conan, build_type)
        return 0

    if not args.no_conan:
        conan = find_conan()
        step_conan_install(conan, build_type, with_tests, with_cli)

    step_configure(cmake, build_type, with_tests, with_cli,
                   use_conan=not args.no_conan,
                   cmake_prefix=args.cmake_prefix)
    step_build(cmake, build_type, args.jobs)

    if with_cli:
        step_stage_cli(build_type)

    if args.install_prefix:
        step_install(cmake, build_type, args.install_prefix)

    print("[sde build.py] DONE.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except subprocess.CalledProcessError as ex:
        sys.stderr.write(f"[sde build.py] FAILED (exit {ex.returncode}): {' '.join(ex.cmd)}\n")
        sys.exit(ex.returncode or 1)
    except KeyboardInterrupt:
        sys.stderr.write("\n[sde build.py] interrupted.\n")
        sys.exit(130)
