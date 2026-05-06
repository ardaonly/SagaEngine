#!/usr/bin/env python3
"""
setup.py — one-shot SagaTools Rust bootstrap.

    python3 Tools/SagaTools/setup.py            # build the Rust dispatcher only
    python3 Tools/SagaTools/setup.py --all      # ALSO install forge + prism

Prerequisites: cargo (Rust toolchain) and Python 3.8+ on PATH.
Forge and Prism remain in C++ (as per roadmap).
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

# ─── Optional shared platform helpers ───────────────────────────────────────
sys.path.insert(0, str(Path(os.path.abspath(__file__)).parent.parent))
try:
    from common import platform_detect as _pd
    _PD_AVAILABLE = True
except ImportError:
    _pd = None
    _PD_AVAILABLE = False

HERE         = Path(os.path.abspath(__file__)).parent
BUILD_DIR    = HERE / "build"
BIN_DIR      = HERE / "bin"
BINARY_NAME  = "sagatools.exe" if platform.system() == "Windows" else "sagatools"

REQUIRED_CARGO_MAJOR = 1
REQUIRED_CARGO_MINOR = 0


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(prog="sagatools setup.py",
        description="Build SagaTools Rust dispatcher (and optionally bootstrap forge + prism).")
    p.add_argument("--debug", action="store_true",
                   help="Build with debug mode.")
    p.add_argument("--clean", action="store_true",
                   help="Remove the build directory before configuring.")
    p.add_argument("--jobs", "-j", type=int, default=0,
                   help="Parallel build jobs (default: cargo's autoselection).")
    p.add_argument("--all", action="store_true",
                   help="After building tools, also run `tools install forge` and `tools install prism`.")
    p.add_argument("--no-smoke", action="store_true",
                   help="Skip the post-build smoke test.")
    p.add_argument("--no-symlink", action="store_true",
                   help="Disable creating symlink in user bin directory.")
    return p.parse_args()


def find_cargo() -> str:
    cargo = shutil.which("cargo")
    if cargo:
        return cargo
    if _PD_AVAILABLE:
        sys.stderr.write(_pd.cargo_not_found_hint("[sagatools] "))
    else:
        sys.stderr.write(
            "[sagatools] ERROR: `cargo` was not found on PATH.\n"
            "[sagatools] Install Rust toolchain before running this bootstrap:\n"
            "[sagatools]   Linux/macOS: curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh\n"
            "[sagatools]   Windows: https://www.rust-lang.org/tools/install\n"
        )
    sys.exit(2)


def check_cargo_version(cargo: str) -> None:
    try:
        out = subprocess.check_output([cargo, "--version"], text=True).strip()
    except subprocess.CalledProcessError as ex:
        sys.stderr.write(f"[sagatools] ERROR: `cargo --version` failed: {ex}\n")
        sys.exit(2)
    first = out.splitlines()[0] if out else ""
    print(f"[sagatools] {first}")
    try:
        token = first.split()[-1]
        parts = token.split(".")
        major, minor = int(parts[0]), int(parts[1])
    except Exception:
        return
    if (major, minor) < (REQUIRED_CARGO_MAJOR, REQUIRED_CARGO_MINOR):
        sys.stderr.write(
            f"[sagatools] WARNING: cargo {major}.{minor} < required "
            f"{REQUIRED_CARGO_MAJOR}.{REQUIRED_CARGO_MINOR}\n"
        )


def step_clean() -> None:
    if BUILD_DIR.exists():
        print(f"[sagatools] removing {BUILD_DIR}")
        shutil.rmtree(BUILD_DIR)


def step_build_rust(cargo: str, build_type: str) -> None:
    """Build Rust dispatcher using cargo."""
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    cmd = [cargo, "build", "--target-dir", str(BUILD_DIR)]
    if build_type == "Release":
        cmd.append("--release")
    if args.jobs > 0:
        cmd.extend(["-j", str(args.jobs)])
    
    print(f"[sagatools] {' '.join(cmd)}")
    subprocess.check_call(cmd, cwd=str(HERE))


def step_stage() -> Path:
    """Stage binary to bin/ directory."""
    BIN_DIR.mkdir(parents=True, exist_ok=True)
    
    # Find built binary
    source_binary = BUILD_DIR / "debug" / BINARY_NAME
    if build_type == "Release":
        source_binary = BUILD_DIR / "release" / BINARY_NAME
    
    if not source_binary.exists():
        # Try alternative locations
        for candidate in [BUILD_DIR / "debug" / BINARY_NAME,
                      BUILD_DIR / "release" / BINARY_NAME]:
            if candidate.exists():
                source_binary = candidate
                break
    
    if not source_binary.exists():
        sys.stderr.write("[sagatools] ERROR: built binary not found.\n")
        sys.exit(3)
    
    dst = BIN_DIR / BINARY_NAME
    shutil.copy2(source_binary, dst)
    if platform.system() != "Windows":
        os.chmod(dst, 0o755)
    print(f"[sagatools] staged binary  : {dst}")
    
    # Stage registry
    cfg = BIN_DIR / "config" / "tools.registry.json"
    stage_registry(cfg)
    print(f"[sagatools] staged registry: {cfg}")
    
    # Create symlink (unless --no-symlink)
    if not args.no_symlink:
        create_symlink(dst)
    
    return dst


def stage_registry(out_path: Path) -> None:
    """Write a registry next to the binary with absolute paths."""
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
        "// Generated by Tools/SagaTools/setup.py -- do not hand-edit.\n"
        "// Override paths by editing this file or by setting\n"
        "// $SAGATOOLS_REGISTRY to point at a different file.\n"
        + json.dumps(payload, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )


def create_symlink(binary_path: Path) -> None:
    """Create symlink in ~/.local/bin (or SAGATOOLS_SYMLINK_DIR)."""
    env_dir = os.environ.get("SAGATOOLS_SYMLINK_DIR")
    if env_dir:
        target_dir = Path(env_dir).expanduser()
        print(f"[sagatools] using custom symlink dir from ENV: {target_dir}")
    else:
        target_dir = Path.home() / ".local" / "bin"
    
    try:
        target_dir.mkdir(parents=True, exist_ok=True)
    except PermissionError:
        sys.stderr.write(f"[sagatools] WARNING: Cannot create {target_dir} (permission denied)\n")
        return
    
    if not os.access(target_dir, os.W_OK):
        sys.stderr.write(f"[sagatools] WARNING: {target_dir} is not writable\n")
        return
    
    symlink_path = target_dir / "tools"
    
    # Check for existing binary (not symlink)
    if symlink_path.exists() and not symlink_path.is_symlink():
        sys.stderr.write(f"[sagatools] WARNING: {symlink_path} exists and is not a symlink\n")
        sys.stderr.write(f"[sagatools]   Not overwriting existing binary\n")
        return
    
    # Verify binary works before creating symlink
    try:
        subprocess.check_output([str(binary_path), "--version"], stderr=subprocess.STDOUT, timeout=5)
    except (subprocess.CalledProcessError, subprocess.TimeoutExpired, FileNotFoundError) as ex:
        sys.stderr.write(f"[sagatools] WARNING: Binary verification failed: {ex}\n")
        sys.stderr.write(f"[sagatools]   Skipping symlink creation\n")
        return
    
    # Atomic symlink update
    temp_symlink = target_dir / f"tools.tmp.{os.getpid()}"
    try:
        if temp_symlink.exists() or temp_symlink.is_symlink():
            temp_symlink.unlink()
        
        temp_symlink.symlink_to(binary_path)
        
        if symlink_path.exists() or symlink_path.is_symlink():
            symlink_path.unlink()
        
        temp_symlink.rename(symlink_path)
        print(f"[sagatools] symlink created : {symlink_path} -> {binary_path}")
        
        # Check if target directory is in PATH
        path_dirs = os.environ.get("PATH", "").split(os.pathsep)
        if str(target_dir) not in path_dirs:
            sys.stderr.write(f"[sagatools] WARNING: {target_dir} is not in your PATH\n")
            sys.stderr.write(f"[sagatools]   Add it with: export PATH=\"{target_dir}:$PATH\"\n")
    
    except OSError as ex:
        sys.stderr.write(f"[sagatools] WARNING: Failed to create symlink: {ex}\n")
        if temp_symlink.exists() or temp_symlink.is_symlink():
            try:
                temp_symlink.unlink()
            except OSError:
                pass


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
    print("  SagaTools (Rust) is ready.")
    print("=" * 70)
    print(f"  binary   : {tools_bin}")
    print(f"  registry : {bin_dir / 'config' / 'tools.registry.json'}")
    
    # Check if symlink was created
    local_bin = Path.home() / ".local" / "bin"
    symlink_path = local_bin / "tools"
    env_dir = os.environ.get("SAGATOOLS_SYMLINK_DIR")
    if env_dir:
        symlink_path = Path(env_dir).expanduser() / "tools"
    
    if symlink_path.exists() or symlink_path.is_symlink():
        print(f"  symlink  : {symlink_path}")
        print()
        print("  The 'tools' command should now be available if the symlink")
        print("  directory is in your PATH.")
        print()
        print("  Verify with:")
        print("    which tools")
        print()
        print("  If not found, add the directory to PATH:")
        target_dir = symlink_path.parent
        if platform.system() == "Windows":
            print(f'    set PATH={target_dir};%PATH%               (cmd)')
            print(f'    $env:PATH = "{target_dir};$env:PATH"      (PowerShell)')
        else:
            print(f'    export PATH="{target_dir}:$PATH"')
    else:
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
    global args, build_type
    args = parse_args()
    build_type = "Debug" if args.debug else "Release"
    
    # ── NixOS: tri-state dependency validation + auto-shell ──────────────
    if _PD_AVAILABLE:
        missing = _pd.build_deps_missing(require_rust=True)
        if missing:
            sys.stderr.write(_pd.missing_deps_hint("[sagatools] ", missing))
            if _pd.detect() == _pd.PlatformKind.NIXOS and not os.environ.get("IN_NIX_SHELL"):
                pkgs = _pd.collect_packages(missing, "[sagatools] ")
                rc = _pd.auto_nix_shell(
                    str(Path(os.path.abspath(__file__))), sys.argv[1:],
                    pkgs, "[sagatools] ",
                )
                if rc is not None:
                    sys.exit(rc)
            sys.exit(2)
    
    print(f"[sagatools] source     : {HERE}")
    print(f"[sagatools] build type : {build_type}")
    print(f"[sagatools] build dir  : {BUILD_DIR}")
    print(f"[sagatools] bin dir    : {BIN_DIR}")
    
    cargo = find_cargo()
    check_cargo_version(cargo)
    
    if args.clean:
        step_clean()
    
    step_build_rust(cargo, build_type)
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
