# @file base.py
# @brief Abstract base class for all Prism snapshot exporters.

from __future__ import annotations

from abc  import ABC, abstractmethod
from pathlib import Path

from ..schema import SnapshotData


# ─── Abstract Exporter ────────────────────────────────────────────────────────

class Exporter(ABC):
    """
    Common interface for all output serializers.
    Subclasses implement write() and must set `extension` to the file suffix
    they produce (without the leading dot).
    """

    extension: str = ""  # Override in each concrete subclass (e.g. "json", "txt").

    @abstractmethod
    def write(self, snapshot: SnapshotData, out_path: Path) -> None:
        """Serialize *snapshot* to *out_path*."""
        ...

    def __repr__(self) -> str:
        return f"{self.__class__.__name__}(extension='{self.extension}')"
