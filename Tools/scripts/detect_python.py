#!/usr/bin/env python3
"""
Robust Python interpreter detection for build scripts.
Used by build.sh and build.ps1 as fallback.
"""

import sys
import subprocess
import shutil
from pathlib import Path

def detect_python():
    """Detect best available Python 3.8+ interpreter."""
    candidates = []
    
    if sys.platform == "win32":
        candidates = ["py", "python", "python3"]
    else:
        candidates = ["python3", "python", "py"]
    
    for cmd in candidates:
        try:
            result = subprocess.run(
                [cmd, "-c", "import sys; print(sys.version_info.major, sys.version_info.minor)"],
                capture_output=True,
                text=True,
                timeout=5
            )
            if result.returncode == 0:
                parts = result.stdout.strip().split()
                major, minor = int(parts[0]), int(parts[1])
                if major >= 3 and minor >= 8:
                    return cmd, (major, minor)
        except (subprocess.TimeoutExpired, FileNotFoundError, ValueError):
            continue
    
    return None, None

def detect_pip(python_cmd):
    """Detect pip for given Python interpreter."""
    candidates = [f"{python_cmd} -m pip", "pip3", "pip"]
    
    for cmd in candidates:
        try:
            result = subprocess.run(
                cmd.split() + ["--version"],
                capture_output=True,
                text=True,
                timeout=5
            )
            if result.returncode == 0:
                return cmd
        except (subprocess.TimeoutExpired, FileNotFoundError):
            continue
    
    return None

if __name__ == "__main__":
    python_cmd, version = detect_python()
    
    if python_cmd:
        print(f"PYTHON_CMD={python_cmd}")
        print(f"PYTHON_VERSION={version[0]}.{version[1]}")
        
        pip_cmd = detect_pip(python_cmd)
        if pip_cmd:
            print(f"PIP_CMD={pip_cmd}")
        else:
            print("PIP_CMD=not_found")
            sys.exit(1)
    else:
        print("PYTHON_CMD=not_found")
        sys.exit(1)