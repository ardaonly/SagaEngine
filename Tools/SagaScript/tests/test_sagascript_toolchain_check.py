#!/usr/bin/env python3
"""Focused tests for SagaScript toolchain availability diagnostics."""

from __future__ import annotations

import json
import os
import subprocess
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2].parent
SAGASCRIPT = REPO_ROOT / "Tools" / "SagaScript" / "sagascript"


def run_cli(*args: str) -> subprocess.CompletedProcess[str]:
    env = os.environ.copy()
    env.setdefault("DOTNET_CLI_HOME", "/tmp/sagascript-dotnet-home")
    env.setdefault("NUGET_PACKAGES", "/tmp/sagascript-nuget")
    env.setdefault("DOTNET_CLI_TELEMETRY_OPTOUT", "1")
    return subprocess.run(
        [str(SAGASCRIPT), *args],
        cwd=REPO_ROOT,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


def has_dotnet_10_sdk() -> bool:
    result = subprocess.run(
        ["dotnet", "--list-sdks"],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if result.returncode != 0:
        return False
    return any(line.startswith("10.") for line in result.stdout.splitlines())


def test_compile_reports_missing_dotnet_10_sdk() -> None:
    if has_dotnet_10_sdk():
        return

    with tempfile.TemporaryDirectory() as tmp:
        diagnostics_path = Path(tmp) / "reports" / "sagascript_diagnostics.json"
        result = run_cli(
            "compile",
            "--source",
            str(Path(tmp) / "Scripts"),
            "--out",
            str(Path(tmp) / "Build" / "Manifests"),
            "--diagnostics",
            str(diagnostics_path),
            "--json",
        )

        assert result.returncode == 1
        assert "NETSDK1045" not in result.stdout
        assert "NETSDK1045" not in result.stderr
        assert diagnostics_path.exists()
        payload = json.loads(diagnostics_path.read_text(encoding="utf-8"))
        stdout_payload = json.loads(result.stdout)
        assert payload == stdout_payload
        assert payload["summary"]["hasBlockingDiagnostics"] is True
        assert payload["diagnostics"][0]["code"] == "Script.Toolchain.DotNetSdkMissing"
        assert "will not downgrade" in payload["diagnostics"][0]["message"]


if __name__ == "__main__":
    test_compile_reports_missing_dotnet_10_sdk()
    print("1 SagaScript toolchain check test passed")
