"""SagaSync PySide6 application bootstrap."""

from __future__ import annotations

from pathlib import Path
import sys


def run_gui(repo_root: Path) -> int:
    try:
        from PySide6.QtWidgets import QApplication
    except ModuleNotFoundError as exc:
        missing = exc.name or "PySide6"
        print(
            "[sagasync] PySide6 is required for the GUI.\n"
            f"[sagasync] missing module: {missing}\n"
            "[sagasync] install it with: python3 -m pip install PySide6\n"
            "[sagasync] or use: cd Tools/SagaSync && nix-shell && python3 sagasync.py\n"
            "[sagasync] non-GUI check: python3 Tools/SagaSync/sagasync.py --smoke",
            file=sys.stderr,
        )
        return 2

    from .main_window import SagaSyncWindow

    app = QApplication(sys.argv)
    window = SagaSyncWindow(repo_root)
    window.show()
    return app.exec()
