# @file external_json.py
# @brief Generic external JSON manifest and diagnostics ingestion.

from __future__ import annotations

import json
from pathlib import Path
from typing  import Any, Dict

from .errors import ExternalJsonError


def load_external_json(
    kind: str,
    paths_by_key: Dict[str, Path],
) -> Dict[str, Any]:
    payloads: Dict[str, Any] = {}
    for key, path in paths_by_key.items():
        if not path.is_file():
            raise ExternalJsonError(kind, key, path, "file does not exist")
        try:
            with path.open("r", encoding="utf-8") as fh:
                payloads[key] = json.load(fh)
        except json.JSONDecodeError as exc:
            raise ExternalJsonError(kind, key, path, f"invalid JSON: {exc}") from exc
        except OSError as exc:
            raise ExternalJsonError(kind, key, path, str(exc)) from exc
    return payloads


def has_blocking_diagnostics(payloads: Dict[str, Any]) -> bool:
    return any(_payload_has_blocking_diagnostics(payload)
               for payload in payloads.values())


def _payload_has_blocking_diagnostics(payload: Any) -> bool:
    if not isinstance(payload, dict):
        return False
    if payload.get("hasBlockingDiagnostics") is True:
        return True

    summary = payload.get("summary")
    if isinstance(summary, dict):
        return summary.get("hasBlockingDiagnostics") is True
    return False
