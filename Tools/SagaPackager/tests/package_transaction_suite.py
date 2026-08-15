# SPDX-License-Identifier: MPL-2.0
from __future__ import annotations

import shutil
import tempfile
from pathlib import Path
from typing import Callable


def run_tests(run_cli: Callable[..., object], load: Callable[[Path], dict],
              repo_root: Path, arena_root: Path, runtime_bridge: Path) -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        canonical = root / "ArenaRush"
        shutil.copytree(arena_root, canonical)
        staged = root / "transaction" / "staged.sagaproj"
        staged.parent.mkdir(parents=True)
        shutil.copy2(canonical / "ArenaRush.sagaproj", staged)
        manifest_before = (canonical / "ArenaRush.sagaproj").read_bytes()
        lock_before = (canonical / "SagaPackage.lock").read_bytes()
        canonical_current = canonical / "Build/Packages/manifests/current.json"
        current_before = canonical_current.read_bytes() if canonical_current.exists() else None
        candidate_lock = staged.parent / "SagaPackage.lock"
        candidate_packages = staged.parent / "Packages"
        report = staged.parent / "report.json"
        result = run_cli(
            "resolve-candidate", "--project", str(staged),
            "--project-root", str(canonical), "--engine-root", str(repo_root),
            "--lock", str(candidate_lock), "--generated-out", str(candidate_packages),
            "--runtime-bridge", str(runtime_bridge), "--out", str(report))
        assert result.returncode == 0, result.stderr + result.stdout
        assert load(report)["status"] == "Passed"
        candidate_inspection_path = staged.parent / "candidate-inspection.json"
        candidate_inspection = run_cli(
            "inspect", "--project", str(staged),
            "--project-root", str(canonical), "--engine-root", str(repo_root),
            "--lock", str(candidate_lock), "--generated-out", str(candidate_packages),
            "--runtime-bridge", str(runtime_bridge),
            "--out", str(candidate_inspection_path))
        assert candidate_inspection.returncode == 0, (
            candidate_inspection.stderr + candidate_inspection.stdout)
        assert load(candidate_inspection_path)["status"] == "Passed"
        ownership_paths = list((candidate_packages / "generations").glob(
            "*/api_ownership_v1.json"))
        assert len(ownership_paths) == 1
        ownership = load(ownership_paths[0])
        instrumentation = [symbol for symbol in ownership["symbols"]
                           if symbol["module"] == "collectibles.instrumentation"]
        assert len(instrumentation) == 23
        assert all(symbol["documentationId"] and not symbol["enabled"]
                   for symbol in instrumentation)
        lowlevel = [symbol for symbol in ownership["symbols"]
                    if symbol["module"] == "collectibles.lowlevel"]
        assert len(lowlevel) == 13
        assert any(symbol["apiId"] == "CollectiblesLowLevelBatch.TryCollect"
                   for symbol in lowlevel)
        assert all(symbol["documentationId"] and not symbol["enabled"]
                   for symbol in lowlevel)
        assert (canonical / "ArenaRush.sagaproj").read_bytes() == manifest_before
        assert (canonical / "SagaPackage.lock").read_bytes() == lock_before
        assert (canonical_current.read_bytes() if canonical_current.exists() else None) == current_before

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        canonical = root / "ArenaRush"
        shutil.copytree(arena_root, canonical)
        packages = canonical / "Build/Packages"
        lock_path = canonical / "SagaPackage.lock"
        lock_before = lock_path.read_bytes()
        report = root / "refresh.json"
        result = run_cli(
            "generation-refresh", "--project", str(canonical / "ArenaRush.sagaproj"),
            "--engine-root", str(repo_root), "--lock", str(lock_path),
            "--generated-out", str(packages), "--runtime-bridge", str(runtime_bridge),
            "--out", str(report))
        assert result.returncode == 0, result.stderr + result.stdout
        assert lock_path.read_bytes() == lock_before
        inspection_path = root / "inspection.json"
        inspected = run_cli(
            "inspect", "--project", str(canonical / "ArenaRush.sagaproj"),
            "--engine-root", str(repo_root), "--lock", str(lock_path),
            "--generated-out", str(packages), "--runtime-bridge", str(runtime_bridge),
            "--out", str(inspection_path))
        assert inspected.returncode == 0, inspected.stderr + inspected.stdout
        inspection = load(inspection_path)
        assert inspection["status"] == "Passed" and inspection["offline"] is True
        rows = {row["module"]: row for row in inspection["modules"]}
        assert rows["gameplay.highlevel"]["enabled"] is True
        assert rows["challenge.core.highlevel"]["enabled"] is True
        assert rows["collectibles.highlevel"]["enabled"] is True
        assert rows["collectibles.instrumentation"]["enabled"] is False
        assert rows["collectibles.instrumentation"]["license"] == "MPL-2.0"
        assert rows["collectibles.instrumentation"]["trust"] == "TrustedEngineDistribution"
        assert rows["collectibles.instrumentation"]["blocksSurfaces"] == []
        assert rows["collectibles.lowlevel"]["enabled"] is False
        assert rows["collectibles.lowlevel"]["runtimeReadiness"] == "RuntimeBacked"
        assert rows["collectibles.lowlevel"]["blocksSurfaces"] == []
