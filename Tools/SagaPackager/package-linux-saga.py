#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Compatibility entry point for Linux Saga distribution packaging."""

from pathlib import Path
import sys

SCRIPT_ROOT = Path(__file__).resolve().parent
if str(SCRIPT_ROOT) not in sys.path:
    sys.path.insert(0, str(SCRIPT_ROOT))

from package_linux.cli import main


if __name__ == "__main__":
    raise SystemExit(main())
