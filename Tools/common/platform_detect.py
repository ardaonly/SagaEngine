#!/usr/bin/env python3
"""
platform_detect.py — shared platform-detection helpers for SagaEngine build tooling.

Consumed by:
  Tools/Prism/build.py          (from common import platform_detect as _pd)
  Tools/SagaTools/setup.py      (from common import platform_detect as _pd)
  Tools/Forge/tool/build.py     (optional import with fallback)

Stdlib only — no pip install.  Python 3.8+.
"""

# ─── Imports ────────────────────────────────────────────────────────────────

from __future__ import annotations

import enum
import glob
import os
import platform
import re
import shlex
import shutil
import subprocess
import sys
from pathlib import Path
from typing import List, Optional, Tuple


# ─── Platform enumeration ───────────────────────────────────────────────────

class PlatformKind(enum.Enum):
    WINDOWS   = "windows"
    MACOS     = "macos"
    NIXOS     = "nixos"
    LINUX_FHS = "linux_fhs"


# ─── Detection ──────────────────────────────────────────────────────────────

def detect() -> PlatformKind:
    """Detect the current platform, distinguishing NixOS from other Linux distros."""
    system = platform.system()
    if system == "Windows":
        return PlatformKind.WINDOWS
    if system == "Darwin":
        return PlatformKind.MACOS
    if _is_nixos():
        return PlatformKind.NIXOS
    return PlatformKind.LINUX_FHS


def _is_nixos() -> bool:
    if Path("/etc/NIXOS").exists():
        return True
    try:
        text = Path("/etc/os-release").read_text(encoding="utf-8", errors="replace")
        for line in text.splitlines():
            if line.strip() == "ID=nixos":
                return True
    except OSError:
        pass
    return False


# ─── NixOS capability abstraction ───────────────────────────────────────────

class NixCapability(enum.Enum):
    """Runtime capability probes for NixOS nixpkgs packages.

    Replaces static allowlists. Each capability maps to a nixpkgs
    attribute that is probed on first use and cached for the lifetime
    of the process.
    """
    HAS_CMAKE       = ("cmake",       "cmake.version or \"ok\"")
    HAS_GNUMAKE     = ("gnumake",     "gnumake.version or \"ok\"")
    HAS_GCC         = ("gcc",         "gcc.cc.version or \"ok\"")
    HAS_CLANG       = ("clang",       "clang.version or \"ok\"")
    HAS_LLVM        = ("llvm",        "llvm.version or \"ok\"")
    HAS_LLVM_CLANG  = ("clang",       "clang.version or \"ok\"")

    def __init__(self, attr: str, eval_expr: str):
        self.attr = attr
        self.eval_expr = eval_expr


class ValidationState(enum.Enum):
    """Tri-state result for NixOS package validation."""
    VALID   = "valid"
    UNKNOWN = "unknown"
    INVALID = "invalid"


_capability_cache: dict[NixCapability, ValidationState] = {}


def _probe_capability(cap: NixCapability) -> ValidationState:
    """Probe a single NixOS capability and cache the result."""
    if cap in _capability_cache:
        return _capability_cache[cap]
    nix_instantiate = shutil.which("nix-instantiate")
    if not nix_instantiate:
        _capability_cache[cap] = ValidationState.UNKNOWN
        return ValidationState.UNKNOWN
    try:
        result = subprocess.run(
            [nix_instantiate, "--eval", "-E",
             f"with import <nixpkgs> {{}}; {cap.eval_expr}"],
            capture_output=True, text=True, timeout=30,
        )
        state = ValidationState.VALID if result.returncode == 0 else ValidationState.INVALID
    except subprocess.TimeoutExpired:
        state = ValidationState.UNKNOWN
    except (OSError, FileNotFoundError):
        state = ValidationState.UNKNOWN
    _capability_cache[cap] = state
    return state


def validate_capability(cap: NixCapability, prefix: str = "") -> ValidationState:
    """Public wrapper that prints warnings for UNKNOWN state.

    Callers should handle INVALID with a hard fail.
    """
    state = _probe_capability(cap)
    if state == ValidationState.UNKNOWN:
        sys.stderr.write(
            f"{prefix}WARNING: Could not validate nixpkgs capability '{cap.attr}'.\n"
            f"{prefix}         Proceeding — nix-shell will report errors if the package is missing.\n"
        )
    elif state == ValidationState.INVALID:
        sys.stderr.write(
            f"{prefix}ERROR: NixOS package '{cap.attr}' is not available in nixpkgs.\n"
            f"{prefix}       This may indicate an outdated channel or incorrect attribute mapping.\n"
        )
    return state


# ─── Build dependency types ─────────────────────────────────────────────────

class BuildDep:
    """Defines a required build tool with platform binaries and NixOS capability mapping."""
    __slots__ = ("name", "binaries", "nix_pkgs", "fhs_pkgs",
                 "version_flags", "category", "nix_capabilities")

    def __init__(
        self,
        name: str,
        binaries: dict[str, list[str]],
        nix_pkgs: list[str],
        fhs_pkgs: list[str],
        version_flags: list[list[str]] | None = None,
        category: str = "build-system",
        nix_capabilities: list[NixCapability] | None = None,
    ):
        self.name = name
        self.binaries = binaries
        self.nix_pkgs = nix_pkgs
        self.fhs_pkgs = fhs_pkgs
        self.version_flags = version_flags or [["--version"]]
        self.category = category
        self.nix_capabilities = nix_capabilities or []


# ─── Build dependency registry ──────────────────────────────────────────────

_MINIMAL_BUILD_DEPS: list[BuildDep] = [
    BuildDep(
        name="cmake",
        binaries={"unix": ["cmake"], "windows": ["cmake", "cmake3"]},
        nix_pkgs=["cmake"],
        fhs_pkgs=["cmake"],
        version_flags=[["--version"], ["-version"]],
        category="build-system",
        nix_capabilities=[NixCapability.HAS_CMAKE],
    ),
    BuildDep(
        name="make",
        binaries={"unix": ["gmake", "make", "mingw32-make"], "windows": ["nmake", "jom"]},
        nix_pkgs=["gnumake"],
        fhs_pkgs=["make"],
        version_flags=[["--version"], ["-version"]],
        category="build-system",
        nix_capabilities=[NixCapability.HAS_GNUMAKE],
    ),
    BuildDep(
        name="gcc",
        binaries={"unix": ["g++", "gcc"], "windows": ["g++", "gcc"]},
        nix_pkgs=["gcc"],
        fhs_pkgs=["gcc", "g++"],
        version_flags=[["--version"]],
        category="compiler",
        nix_capabilities=[NixCapability.HAS_GCC],
    ),
    BuildDep(
        name="clang",
        binaries={"unix": ["clang++", "clang"], "windows": ["clang++", "clang"]},
        nix_pkgs=["clang"],
        fhs_pkgs=["clang"],
        version_flags=[["--version"], ["-v"]],
        category="compiler",
        nix_capabilities=[NixCapability.HAS_CLANG],
    ),
    BuildDep(
        name="msvc",
        binaries={"unix": [], "windows": ["cl", "cl.exe"]},
        nix_pkgs=[],
        fhs_pkgs=[],
        version_flags=[["/?"], ["/help"]],
        category="compiler",
        nix_capabilities=[],
    ),
]

_LLVM_BUILD_DEP = BuildDep(
    name="llvm+clang",
    binaries={"unix": ["llvm-config", "clang"], "windows": []},
    nix_pkgs=["llvm", "clang", "zlib", "libxml2"],
    fhs_pkgs=["llvm-dev", "libclang-dev", "zlib1g-dev", "libxml2-dev"],
    version_flags=[["--version"], ["-v"]],
    category="library",
    nix_capabilities=[NixCapability.HAS_LLVM, NixCapability.HAS_LLVM_CLANG],
)

_RUST_BUILD_DEP = BuildDep(
    name="rust",
    binaries={"unix": ["cargo", "rustc"], "windows": ["cargo", "cargo.exe"]},
    nix_pkgs=["cargo"],
    fhs_pkgs=["cargo", "rustc"],
    version_flags=[["--version"]],
    category="build-system",
    nix_capabilities=[],
)


# ─── Tool availability testing ──────────────────────────────────────────────

def _is_tool_available(dep: BuildDep) -> bool:
    """Test if a build tool is functional by running version probes."""
    is_win = platform.system() == "Windows"
    candidates = dep.binaries.get("windows" if is_win else "unix", [])
    for binary in candidates:
        path = shutil.which(binary)
        if path is None:
            continue
        for flags in dep.version_flags:
            try:
                result = subprocess.run(
                    [path] + flags, capture_output=True, text=True, timeout=5,
                )
                if result.returncode == 0 or result.stdout.strip() or result.stderr.strip():
                    return True
            except (subprocess.SubprocessError, OSError):
                continue
    return False


# ─── Cross-compilation detection ────────────────────────────────────────────

def _emit_cross_compile_warning() -> None:
    """Print a warning to stderr if cross-compilation env vars are detected."""
    toolchain = os.environ.get("CMAKE_TOOLCHAIN_FILE")
    target = os.environ.get("TARGET_ARCH")
    cross = os.environ.get("CROSS_COMPILE")
    if not (toolchain or target or cross):
        return
    parts = [
        "[deps] WARNING: Cross-compilation detected.",
        "[deps]   Dependency checks validate the HOST toolchain only.",
        f"[deps]   Host architecture: {platform.machine()}",
    ]
    if target:
        parts.append(f"[deps]   Target architecture: {target}")
    if toolchain:
        parts.append(f"[deps]   Toolchain file: {toolchain}")
    parts.append("[deps]   Ensure the TARGET toolchain is provided separately.")
    sys.stderr.write(" ".join(parts) + "\n")


# ─── Build dependency detection ─────────────────────────────────────────────

def build_deps_missing(
    require_clang: bool = True,
    include_llvm: bool = False,
    require_rust: bool = False,
) -> list[BuildDep]:
    """Return missing build dependencies for the current platform.

    When require_rust=True only the Rust toolchain (cargo/rustc) is checked;
    C++ deps are skipped entirely since the caller is a Rust-only project.

    Cross-compilation warning is emitted if relevant env vars are set.
    Platform-appropriate binaries selected automatically.
    """
    _emit_cross_compile_warning()
    if require_rust:
        return [] if _is_tool_available(_RUST_BUILD_DEP) else [_RUST_BUILD_DEP]
    is_win = platform.system() == "Windows"
    plat = detect()
    skip: set[str] = set()
    if not require_clang:
        skip.add("clang")
    if is_win:
        skip.add("gcc")
        skip.add("make")
    else:
        skip.add("msvc")
    missing: list[BuildDep] = []
    for dep in _MINIMAL_BUILD_DEPS:
        if dep.name in skip:
            continue
        bins = dep.binaries.get("windows" if is_win else "unix", [])
        if not bins:
            continue
        if not _is_tool_available(dep):
            missing.append(dep)
    if include_llvm and plat != PlatformKind.WINDOWS:
        llvm_dir, _ = find_llvm_cmake_dirs()
        if llvm_dir is None:
            missing.append(_LLVM_BUILD_DEP)
    return missing


# ─── Package collection ─────────────────────────────────────────────────────

def collect_packages(
    missing: list[BuildDep],
    prefix: str = "",
) -> list[str]:
    """Collect NixOS package names from missing deps with stable ordering.

    Ordering: build-system → compiler → library (deterministic, reproducible).
    Validates capabilities with tri-state policy before returning packages.
    Returns empty list if any capability is INVALID (caller should hard-fail).
    """
    category_order = {"build-system": 0, "compiler": 1, "library": 2}
    seen: set[str] = set()
    collected: list[tuple[int, str]] = []
    all_valid = True
    for dep in missing:
        rank = category_order.get(dep.category, 99)
        for pkg in dep.nix_pkgs:
            if pkg not in seen:
                seen.add(pkg)
                collected.append((rank, pkg))
        if detect() == PlatformKind.NIXOS:
            for cap in dep.nix_capabilities:
                state = validate_capability(cap, prefix)
                if state == ValidationState.INVALID:
                    all_valid = False
    if not all_valid:
        return []
    collected.sort(key=lambda x: x[0])
    return [pkg for _, pkg in collected]


# ─── Missing dependency hints ───────────────────────────────────────────────

def missing_deps_hint(
    prefix: str = "",
    missing: list[BuildDep] | None = None,
) -> str:
    """Return formatted error string for missing build dependencies."""
    if missing is None:
        missing = build_deps_missing()
    if not missing:
        return ""
    kind = detect()
    is_win = platform.system() == "Windows"
    lines = [f"{prefix}ERROR: Missing required build dependencies:\n"]
    for dep in missing:
        bins = dep.binaries.get("windows" if is_win else "unix", [])
        bin_str = ", ".join(bins) if bins else "N/A"
        lines.append(f"{prefix}  - {dep.name} (needs: {bin_str})\n")
    lines.append(f"{prefix}Install them before building:\n")
    if kind == PlatformKind.NIXOS:
        pkgs = collect_packages(missing, prefix)
        if pkgs:
            pkg_str = " ".join(pkgs)
            lines.append(f"{prefix}  NixOS  : nix-shell -p {pkg_str}\n")
            lines.append(f"{prefix}           or add [{pkg_str}] to shell.nix buildInputs\n")
        else:
            lines.append(f"{prefix}  NixOS  : Some packages are not available in your nixpkgs channel.\n")
            lines.append(f"{prefix}           Run 'nix-channel --update' or check your flake inputs.\n")
    elif kind == PlatformKind.MACOS:
        fhs = []
        for dep in missing:
            fhs.extend(dep.fhs_pkgs)
        lines.append(f"{prefix}  macOS  : brew install {' '.join(dict.fromkeys(fhs))}\n")
    elif kind == PlatformKind.WINDOWS:
        lines.append(f"{prefix}  Windows: Install Visual Studio with 'Desktop development with C++'\n")
        lines.append(f"{prefix}           and ensure 'C++ CMake tools for Windows' is selected\n")
    else:
        fhs = []
        for dep in missing:
            fhs.extend(dep.fhs_pkgs)
        lines.append(f"{prefix}  Linux  : sudo apt install {' '.join(dict.fromkeys(fhs))}\n")
    return "".join(lines)


# ─── Install hints (backward compat) ────────────────────────────────────────

def cmake_not_found_hint(prefix: str = "") -> str:
    kind = detect()
    lines = [
        f"{prefix}ERROR: `cmake` was not found on PATH.\n",
        f"{prefix}Install it before running this bootstrap:\n",
    ]
    if kind == PlatformKind.NIXOS:
        lines += [
            f"{prefix}  NixOS  : nix-shell -p cmake\n",
            f"{prefix}           or add pkgs.cmake to your shell.nix / flake devShell\n",
        ]
    elif kind == PlatformKind.MACOS:
        lines += [f"{prefix}  macOS  : brew install cmake\n"]
    elif kind == PlatformKind.WINDOWS:
        lines += [f"{prefix}  Windows: https://cmake.org/download\n"]
    else:
        lines += [
            f"{prefix}  Linux  : sudo apt install cmake\n",
            f"{prefix}           sudo dnf install cmake\n",
            f"{prefix}           sudo pacman -S cmake\n",
        ]
    return "".join(lines)


def llvm_not_found_hint(prefix: str = "") -> str:
    kind = detect()
    lines = [
        f"{prefix}WARNING: no LLVM installation detected.\n",
        f"{prefix}Skipping prism-extract. Pass --llvm-dir <path> to force.\n",
        f"{prefix}Install hints:\n",
    ]
    if kind == PlatformKind.NIXOS:
        pkgs = collect_packages([_LLVM_BUILD_DEP], prefix)
        pkg_str = " ".join(pkgs) if pkgs else "llvm clang"
        lines += [
            f"{prefix}  NixOS  : nix-shell -p {pkg_str}\n",
            f"{prefix}           or add {pkg_str} to your shell.nix buildInputs\n",
            f"{prefix}           then re-run; llvm-config on PATH will be auto-discovered.\n",
        ]
    elif kind == PlatformKind.MACOS:
        lines += [f"{prefix}  macOS  : brew install llvm\n"]
    else:
        lines += [
            f"{prefix}  Linux  : sudo apt install llvm-dev libclang-dev\n",
            f"{prefix}           sudo dnf install llvm-devel clang-devel\n",
        ]
    return "".join(lines)


# ─── LLVM discovery ─────────────────────────────────────────────────────────

def find_llvm_cmake_dirs(
    llvm_dir: Optional[str] = None,
    clang_dir: Optional[str] = None,
) -> Tuple[Optional[str], Optional[str]]:
    llvm_dir  = llvm_dir  or os.environ.get("LLVM_DIR")  or None
    clang_dir = clang_dir or os.environ.get("CLANG_DIR") or None
    if llvm_dir and Path(llvm_dir).is_dir():
        if not clang_dir:
            clang_dir = _derive_clang_dir(llvm_dir)
        return llvm_dir, _ensure_clang_dir(llvm_dir, clang_dir)
    probed = _probe_llvm_config()
    if probed[0]:
        llvm_dir  = llvm_dir  or probed[0]
        clang_dir = clang_dir or probed[1]
        if llvm_dir:
            return llvm_dir, _ensure_clang_dir(llvm_dir, clang_dir)
    fhs = _scan_fhs()
    if fhs[0]:
        return fhs[0], _ensure_clang_dir(fhs[0], fhs[1])
    if _is_nixos():
        nix = _scan_nix_store()
        if nix[0]:
            return nix
    return None, None


def _is_native_elf(path: str) -> bool:
    """Return True if the ELF binary at path matches the host word size."""
    try:
        with open(path, "rb") as f:
            header = f.read(5)
        if header[:4] != b"\x7fELF":
            return False
        ei_class = header[4]          # 1 = 32-bit, 2 = 64-bit
        host_64 = platform.machine() in ("x86_64", "aarch64", "riscv64", "ppc64le")
        return (ei_class == 2) == host_64
    except (OSError, IndexError):
        return False


def find_nix_dep_prefixes(dep_names: List[str]) -> List[str]:
    """Return nix store paths usable as CMAKE_PREFIX_PATH entries.

    On NixOS cmake cannot find headers/libs in the standard FHS locations because
    they live in the nix store.  For each name in dep_names this function returns
    both the -dev package (headers/cmake configs) and the runtime package
    (shared/static libraries) so that cmake's find_package can locate both
    INCLUDE_DIR and LIBRARY for each dependency.

    Only native-architecture (ELF class matches host) runtime packages are
    included to avoid accidentally adding 32-bit libs on 64-bit hosts.

    Only meaningful on NixOS; returns an empty list on other platforms.
    """
    if not _is_nixos():
        return []
    prefixes: List[str] = []
    for name in dep_names:
        dev_hits = sorted(glob.glob(f"/nix/store/*-{name}-*-dev"), reverse=True)
        for hit in dev_hits:
            p = Path(hit)
            if (p / "include").is_dir() or (p / "lib" / "cmake").is_dir():
                prefixes.append(hit)
                break
        lib_hits = sorted(glob.glob(f"/nix/store/*-{name}-[0-9]*"), reverse=True)
        for hit in lib_hits:
            if hit.endswith("-dev"):
                continue
            p = Path(hit)
            lib_dir = p / "lib"
            if not lib_dir.is_dir():
                continue
            for f in lib_dir.iterdir():
                if f.suffix in (".so", ".a", ".dylib") and (f.is_file() or f.is_symlink()):
                    target = str(f.resolve()) if f.is_symlink() else str(f)
                    if _is_native_elf(target):
                        prefixes.append(hit)
                        break
            else:
                continue
            break
    return prefixes


def _ensure_clang_dir(llvm_dir: Optional[str], clang_dir: Optional[str]) -> Optional[str]:
    """Return clang_dir, falling back to a nix store scan when on NixOS and still None."""
    if clang_dir is not None or llvm_dir is None:
        return clang_dir
    if _is_nixos():
        return _scan_nix_clang_dir(llvm_dir)
    return None


def _derive_clang_dir(llvm_dir: str) -> Optional[str]:
    cand = llvm_dir.replace("/llvm", "/clang", 1)
    return cand if Path(cand).is_dir() else None


def _scan_nix_clang_dir(llvm_dir: str) -> Optional[str]:
    """Scan the nix store for a Clang cmake directory.

    On NixOS, LLVM and Clang live in separate store paths with different
    hashes, so _derive_clang_dir (which rewrites the LLVM hash path) always
    fails.  This function globs /nix/store independently, preferring a clang
    package whose version matches the LLVM version extracted from llvm_dir.
    """
    m = re.search(r"-llvm-(\d+\.\d+)", llvm_dir)
    if m:
        version_prefix = m.group(1)
        hits = sorted(
            glob.glob(f"/nix/store/*-clang-{version_prefix}*/lib/cmake/clang"),
            reverse=True,
        )
        for hit in hits:
            if Path(hit).is_dir():
                return hit
    hits = sorted(glob.glob("/nix/store/*-clang-*/lib/cmake/clang"), reverse=True)
    for hit in hits:
        if Path(hit).is_dir():
            return hit
    return None


def _probe_llvm_config() -> Tuple[Optional[str], Optional[str]]:
    for tool in ("llvm-config", "llvm-config-18", "llvm-config-17", "llvm-config-16"):
        binary = shutil.which(tool)
        if binary is None:
            continue
        try:
            prefix = subprocess.check_output(
                [binary, "--prefix"], text=True, stderr=subprocess.DEVNULL, timeout=5,
            ).strip()
        except Exception:
            continue
        if not prefix:
            continue
        cand_llvm  = str(Path(prefix) / "lib" / "cmake" / "llvm")
        cand_clang = str(Path(prefix) / "lib" / "cmake" / "clang")
        if Path(cand_llvm).is_dir():
            return cand_llvm, (cand_clang if Path(cand_clang).is_dir() else None)
    return None, None


def _scan_fhs() -> Tuple[Optional[str], Optional[str]]:
    candidates = [
        "/usr/lib/llvm-18/lib/cmake/llvm",
        "/usr/lib/llvm-17/lib/cmake/llvm",
        "/usr/lib/llvm-16/lib/cmake/llvm",
        "/opt/homebrew/opt/llvm/lib/cmake/llvm",
        "/usr/local/opt/llvm/lib/cmake/llvm",
    ]
    for cand in candidates:
        if Path(cand).is_dir():
            return cand, _derive_clang_dir(cand)
    return None, None


def _scan_nix_store() -> Tuple[Optional[str], Optional[str]]:
    store = Path("/nix/store")
    if not store.is_dir():
        return None, None
    hits = sorted(glob.glob("/nix/store/*-llvm-*/lib/cmake/llvm"), reverse=True)
    for hit in hits:
        if Path(hit).is_dir():
            clang_dir = _derive_clang_dir(hit) or _scan_nix_clang_dir(hit)
            return hit, clang_dir
    return None, None


# ─── NixOS auto-shell ────────────────────────────────────────────────────────

def auto_nix_shell(
    script_path: str,
    argv: list[str],
    packages: list[str],
    prefix: str = "",
) -> int | None:
    """Re-execute *script_path* inside a temporary nix-shell with *packages*.

    Tri-state validation:
    - VALID   → packages passed to nix-shell
    - UNKNOWN → warning printed, packages still passed
    - INVALID → returns None (caller must exit)
    """
    if detect() != PlatformKind.NIXOS:
        return None
    if os.environ.get("IN_NIX_SHELL"):
        return None
    nix_shell = shutil.which("nix-shell")
    if not nix_shell:
        return None
    if not packages:
        return None
    if "python3" not in packages:
        packages = packages + ["python3"]
    run_cmd = "python3 " + shlex.quote(script_path)
    if argv:
        run_cmd += " " + " ".join(shlex.quote(a) for a in argv)
    cmd = [nix_shell, "-p"] + packages + ["--run", run_cmd]
    print(f"{prefix}NixOS: required tools not found on PATH.")
    print(f"{prefix}Launching temporary nix-shell with: {' '.join(packages)}")
    print()
    return subprocess.call(cmd)


# ─── Launcher helper ─────────────────────────────────────────────────────────

def write_unix_launcher(target: Path, run_py: Path) -> None:
    target.write_text(
        "#!/usr/bin/env sh\n"
        f"exec python3 \"{run_py}\" \"$@\"\n",
        encoding="utf-8",
    )
    os.chmod(target, 0o755)
