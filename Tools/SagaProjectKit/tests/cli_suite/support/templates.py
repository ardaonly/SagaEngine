# SPDX-License-Identifier: MPL-2.0
from __future__ import annotations

from .environment import *

def prepare_tracked_sample_fixture(root: Path) -> tuple[Path, Path]:
    """Copy the tracked sample and materialize its ignored generated evidence."""
    fixture_root = root / "MultiplayerSandbox"
    shutil.copytree(
        SAMPLE_PROJECT.parent,
        fixture_root,
        ignore=shutil.ignore_patterns("Build", "Generated"),
    )
    (fixture_root / "Build" / "Reports").mkdir(parents=True)
    write_json(
        fixture_root / "Generated" / "SourceTruth" / "source_truth_foundation.generated.json",
        {
            "schemaVersion": 1,
            "artifactId": "source-truth-foundation-projection",
            "artifactKind": "GeneratedProjection",
            "canonical": False,
            "sourceSceneId": "source-truth-foundation",
            "sourceHash": "scene-source-hash-v0",
            "summary": "Generated projection fixture used only as non-canonical evidence.",
        },
    )
    return (
        fixture_root / "MultiplayerSandbox.sagaproj",
        fixture_root / ".saga" / "slices" / "designer-local.slice.json",
    )

def write_fake_packager(path: Path, body: str) -> None:
    path.write_text("#!/bin/sh\nset -eu\n" + body, encoding="utf-8")
    path.chmod(0o755)

def template_plan_for(root: Path, destination_name: str) -> tuple[Path, Path, Path]:
    destination = root / destination_name
    plan = root / f"{destination_name}.plan.json"
    report = root / f"{destination_name}.report.json"
    planned = run_cli("template-plan", "--destination", str(destination), "--template",
                      str(REPO_ROOT / "Samples" / "StarterArena"), "--out", str(plan))
    assert planned.returncode == 0, planned.stderr + planned.stdout
    return destination, plan, report
