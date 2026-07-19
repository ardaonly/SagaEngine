# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

from dataclasses import asdict, dataclass, field
from pathlib import Path, PurePosixPath
import argparse
import json
import re
import subprocess
import sys
import tomllib
from typing import Any

from licensing_common import (
    ACTION_TO_SEVERITY,
    action_severity,
    gitwildmatch,
    repository_root,
    safe_relative_path,
    validate_spdx_expression,
)

from .model import *


def print_report(report: Report) -> None:
    order = {"ERROR": 0, "WARNING": 1, "INFO": 2}
    for finding in sorted(
        report.findings,
        key=lambda item: (
            order.get(item.severity, 9),
            item.code,
            item.message,
        ),
    ):
        print(finding.render())

    resolved = sum(item.resolved for item in report.resolutions.values())
    unresolved = sum(not item.resolved for item in report.resolutions.values())
    errors = sum(item.severity == "ERROR" for item in report.findings)
    warnings = sum(item.severity == "WARNING" for item in report.findings)
    infos = sum(item.severity == "INFO" for item in report.findings)

    print()
    print(f"Domain-matched files: {len(report.classified)}")
    print(f"License-resolved files: {resolved}")
    print(f"License-unresolved files: {unresolved}")
    print(f"Errors: {errors}")
    print(f"Warnings: {warnings}")
    print(f"Info: {infos}")


def write_json_report(path: Path, report: Report) -> None:
    payload = {
        "findings": [asdict(item) for item in report.findings],
        "classified": report.classified,
        "resolutions": {
            key: asdict(value)
            for key, value in report.resolutions.items()
        },
        "summary": {
            "domain_matched": len(report.classified),
            "license_resolved": sum(
                item.resolved for item in report.resolutions.values()
            ),
            "license_unresolved": sum(
                not item.resolved for item in report.resolutions.values()
            ),
            "errors": sum(
                item.severity == "ERROR" for item in report.findings
            ),
            "warnings": sum(
                item.severity == "WARNING" for item in report.findings
            ),
        },
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(payload, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )
