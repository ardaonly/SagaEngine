# SPDX-License-Identifier: MPL-2.0
"""Architecture check for SagaScript's runtime-only product contract."""

from __future__ import annotations

from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2].parent


def test_sagascript_has_no_sdk_or_global_host_probe() -> None:
    source = "\n".join(
        path.read_text(encoding="utf-8")
        for path in sorted((REPO_ROOT / "Tools" / "SagaScript" / "src").glob("*.cs"))
    )
    wrapper = (REPO_ROOT / "Tools" / "SagaScript" / "sagascript").read_text(
        encoding="utf-8"
    )

    assert "DotNetSdkProbe" not in source
    assert "--list-sdks" not in source
    assert 'FileName = "dotnet"' not in source
    assert "dotnet run" not in wrapper
    assert "SAGA_DOTNET" not in wrapper
    assert "runtime/dotnet/dotnet" in wrapper
    assert 'tools/SagaScript/sagascript.dll' in wrapper


if __name__ == "__main__":
    test_sagascript_has_no_sdk_or_global_host_probe()
    print("1 SagaScript private-runtime contract test passed")
