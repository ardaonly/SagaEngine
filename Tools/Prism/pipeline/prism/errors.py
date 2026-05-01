# @file errors.py
# @brief Prism pipeline exception hierarchy.
#
# All exceptions raised by the pipeline derive from PrismError so callers
# can catch the full family with a single except clause if needed, while
# still being able to distinguish specific failure modes.

from __future__ import annotations
from pathlib import Path


class PrismError(Exception):
    """Root of the Prism pipeline exception hierarchy."""


class RawJsonNotFoundError(PrismError):
    """Raised when the prism-extract output JSON file does not exist."""

    def __init__(self, path: Path) -> None:
        super().__init__(f"Raw extraction JSON not found: {path}")
        self.path = path


class RawJsonSchemaError(PrismError):
    """Raised when the raw JSON does not conform to the expected schema."""

    def __init__(self, reason: str) -> None:
        super().__init__(f"Raw JSON schema error: {reason}")


class ExportError(PrismError):
    """Raised when an exporter fails to write its output file."""

    def __init__(self, path: Path, reason: str) -> None:
        super().__init__(f"Export to '{path}' failed: {reason}")
        self.path   = path
        self.reason = reason


class ConfigError(PrismError):
    """Raised when a resolved PipelineConfig contains invalid values."""

    def __init__(self, field: str, reason: str) -> None:
        super().__init__(f"Config error — {field}: {reason}")
        self.field  = field
        self.reason = reason
