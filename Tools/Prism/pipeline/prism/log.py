# @file log.py
# @brief Structured logger with a live progress bar.
#
# Diagnostic messages go to stderr; final result lines go to stdout.
# This separation keeps CI log parsers from confusing progress noise with output.

from __future__ import annotations

import sys
import time
from enum    import IntEnum
from typing  import Optional, TextIO


# ─── Severity ─────────────────────────────────────────────────────────────────

class Level(IntEnum):
    DEBUG  = 0
    INFO   = 1
    WARN   = 2
    ERROR  = 3
    SILENT = 99  # suppress all stderr output


# ─── Logger ───────────────────────────────────────────────────────────────────

class PrismLogger:
    """
    Lightweight structured logger used throughout the pipeline.

    Thread-safety: not thread-safe. The pipeline is single-threaded on the
    Python side; the C++ extractor has its own stdout/stderr.
    """

    _BAR_WIDTH: int = 32
    _TAGS: dict[Level, str] = {
        Level.DEBUG: "DEBUG",
        Level.INFO:  "INFO ",
        Level.WARN:  "WARN ",
        Level.ERROR: "ERROR",
    }

    def __init__(
        self,
        level: Level   = Level.INFO,
        out:   TextIO  = sys.stdout,
        err:   TextIO  = sys.stderr,
    ) -> None:
        self._level = level
        self._out   = out
        self._err   = err

        # Progress bar state.
        self._total:  int = 0
        self._done:   int = 0
        self._t0:     Optional[float] = None
        self._in_progress: bool = False

    # ── Logging ───────────────────────────────────────────────────────────────

    def debug(self, msg: str) -> None:
        self._emit(Level.DEBUG, msg)

    def info(self, msg: str) -> None:
        self._emit(Level.INFO, msg)

    def warn(self, msg: str) -> None:
        self._emit(Level.WARN, msg)

    def error(self, msg: str) -> None:
        self._emit(Level.ERROR, msg)

    def result(self, msg: str) -> None:
        """Write a final result line to stdout regardless of log level."""
        print(msg, file=self._out, flush=True)

    # ── Progress ──────────────────────────────────────────────────────────────

    def progress_start(self, total: int, label: str = "Working") -> None:
        """Begin a new progress sequence with *total* steps."""
        if self._level >= Level.SILENT:
            return
        self._total       = max(total, 1)
        self._done        = 0
        self._t0          = time.monotonic()
        self._in_progress = True
        self._draw(label)

    def progress_step(self, note: str = "") -> None:
        """Advance the progress bar by one step."""
        if not self._in_progress:
            return
        self._done = min(self._done + 1, self._total)
        self._draw(note[:48])

    def progress_done(self) -> None:
        """Complete the progress bar and move to a new line."""
        if not self._in_progress:
            return
        elapsed = time.monotonic() - (self._t0 or time.monotonic())
        bar     = "█" * self._BAR_WIDTH
        print(
            f"\r[{bar}] 100%  {self._total}/{self._total}  {elapsed:.1f}s",
            file=self._err, flush=True,
        )
        print("", file=self._err)
        self._in_progress = False

    # ── Internals ─────────────────────────────────────────────────────────────

    def _emit(self, level: Level, msg: str) -> None:
        if level < self._level:
            return
        # Clear the progress bar line before writing a log message.
        if self._in_progress:
            print("\r" + " " * 80 + "\r", end="", file=self._err)
        tag = self._TAGS.get(level, "?????")
        print(f"[{tag}] {msg}", file=self._err, flush=True)

    def _draw(self, suffix: str) -> None:
        ratio  = self._done / self._total
        filled = int(ratio * self._BAR_WIDTH)
        bar    = "█" * filled + "░" * (self._BAR_WIDTH - filled)
        pct    = int(ratio * 100)
        line   = (
            f"\r[{bar}] {pct:>3}%  "
            f"{self._done:>{len(str(self._total))}}/{self._total}  "
            f"{suffix:<48}"
        )
        print(line, end="", file=self._err, flush=True)


# ─── Module-level singleton ───────────────────────────────────────────────────

_active: Optional[PrismLogger] = None


def init(level: Level = Level.INFO) -> PrismLogger:
    """Initialise and return the module-level logger."""
    global _active
    _active = PrismLogger(level=level)
    return _active


def get() -> PrismLogger:
    """Return the active logger, creating a default instance if needed."""
    global _active
    if _active is None:
        _active = PrismLogger()
    return _active
