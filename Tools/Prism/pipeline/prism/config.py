# @file config.py
# @brief PipelineConfig — single source of truth for all pipeline defaults.
#
# All path fields are resolved to absolute Paths during construction.
# The config object is intentionally immutable after from_args() returns.

from __future__ import annotations

from dataclasses import dataclass
from pathlib     import Path
from typing      import Optional

from .errors import ConfigError

# ─── Defaults ─────────────────────────────────────────────────────────────────

DEFAULT_RAW_JSON: str = "prism.raw.json"
DEFAULT_OUT_JSON: str = "prism.graph.json"
DEFAULT_OUT_TXT:  str = "prism.txt"


# ─── Config ───────────────────────────────────────────────────────────────────

@dataclass(frozen=True)
class PipelineConfig:
    """
    Resolved, validated configuration for one prism-graph invocation.
    Construct via PipelineConfig.from_args() — do not instantiate directly.
    """

    raw_json:  Path         # Absolute path to prism-extract output.
    out_dir:   Path         # Absolute output directory.
    json_name: str          # Graph JSON filename.
    txt_name:  str          # Plain-text summary filename.
    emit_txt:  bool         # False when --no-txt is supplied.
    verbose:   bool
    quiet:     bool

    # ── Derived paths ─────────────────────────────────────────────────────────

    @property
    def graph_json_path(self) -> Path:
        """Absolute path to the output graph JSON."""
        return self.out_dir / self.json_name

    @property
    def txt_path(self) -> Path:
        """Absolute path to the plain-text summary."""
        return self.out_dir / self.txt_name

    # ── Factory ───────────────────────────────────────────────────────────────

    @staticmethod
    def from_args(
        raw_json:  Optional[Path],
        out_dir:   Optional[Path],
        json_name: str = DEFAULT_OUT_JSON,
        txt_name:  str = DEFAULT_OUT_TXT,
        emit_txt:  bool = True,
        verbose:   bool = False,
        quiet:     bool = False,
    ) -> "PipelineConfig":
        """
        Resolve paths and validate all fields.
        Raises ConfigError on any invalid combination.
        """
        resolved_raw = (
            Path(DEFAULT_RAW_JSON) if raw_json is None else raw_json
        ).resolve()

        resolved_out = (
            resolved_raw.parent if out_dir is None else out_dir
        ).resolve()

        cfg = PipelineConfig(
            raw_json  = resolved_raw,
            out_dir   = resolved_out,
            json_name = json_name,
            txt_name  = txt_name,
            emit_txt  = emit_txt,
            verbose   = verbose,
            quiet     = quiet,
        )
        cfg._validate()
        return cfg

    def _validate(self) -> None:
        if not self.json_name.strip():
            raise ConfigError("json_name", "must not be empty")
        if not self.txt_name.strip():
            raise ConfigError("txt_name",  "must not be empty")
        if self.verbose and self.quiet:
            raise ConfigError("verbose/quiet", "mutually exclusive flags")
