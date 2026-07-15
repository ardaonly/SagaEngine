#!/usr/bin/env python3
"""Focused tests for SagaSync core models."""

from __future__ import annotations

import json
from datetime import datetime, timezone
from pathlib import Path
import tempfile
import unittest

import sys

ROOT = Path(__file__).resolve().parents[5]
sys.path.insert(0, str(ROOT / "Tools" / "Developer" / "Environment" / "SagaSync"))

from core.commit_queue import build_commit_queue
from core.commit_plan import (
    GROUP_DOCS,
    GROUP_SOURCE,
    GROUP_TOOL,
    build_commit_plan,
    parse_status_line,
    summarize_files,
)
from core.actions import default_toolbar_actions, verification_actions
from core.export_health import HEALTH_BLOCKED, HEALTH_READY, HEALTH_STALE, evaluate_export_health
from core.export_status import load_export_manifest, load_export_states
from core.git_status import GitStatus, normalize_status_lines
from core.run_history import (
    DISPLAY_BLOCKED_BY_VERIFICATION,
    DISPLAY_READY_UNVERIFIED,
    RUN_FAILED,
    RUN_NOT_RUN,
    RUN_PASSED,
    health_display_reason,
    health_display_status,
    make_run_result,
    profile_display_status,
)
from core.smoke import build_smoke_payload
from core.verification import verification_profiles


class SagaSyncCoreTests(unittest.TestCase):
    def test_normalize_status_lines_filters_empty_lines(self) -> None:
        self.assertEqual(normalize_status_lines([" M file.txt", "", "?? new.py"]), (" M file.txt", "?? new.py"))

    def test_load_export_manifest_and_states(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            manifest_dir = root / "Tools" / "Developer" / "RepoAudit"
            state_dir = root / ".core" / "export" / "state"
            manifest_dir.mkdir(parents=True)
            state_dir.mkdir(parents=True)
            (manifest_dir / "export-manifest.json").write_text(
                json.dumps(
                    {
                        "target_branch": "main",
                        "tools": {
                            "Forge": {"path": "Tools/Forge", "repo": "ardaonly/forge"},
                        },
                    }
                ),
                encoding="utf-8",
            )
            (state_dir / "FORGE.json").write_text(
                json.dumps(
                    {
                        "tool": "Forge",
                        "path": "Tools/Forge",
                        "repo": "ardaonly/forge",
                        "target_branch": "main",
                        "source_commit": "abc",
                        "source_short": "abc",
                        "subtree_commit": "def",
                        "remote_head_before": "old",
                        "remote_head_after": "def",
                        "exported_at": "2026-05-19T00:00:00+00:00",
                    }
                ),
                encoding="utf-8",
            )

            manifest = load_export_manifest(root)
            states = load_export_states(root, manifest)

        self.assertEqual(manifest.target_branch, "main")
        self.assertEqual(manifest.tools[0].name, "Forge")
        self.assertTrue(states["Forge"].synced)

    def test_export_health_ready_when_state_matches_head(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            manifest_dir = root / "Tools" / "Developer" / "RepoAudit"
            state_dir = root / ".core" / "export" / "state"
            manifest_dir.mkdir(parents=True)
            state_dir.mkdir(parents=True)
            (manifest_dir / "export-manifest.json").write_text(
                json.dumps(
                    {
                        "target_branch": "main",
                        "tools": {
                            "Forge": {"path": "Tools/Forge", "repo": "ardaonly/forge"},
                        },
                    }
                ),
                encoding="utf-8",
            )
            (state_dir / "FORGE.json").write_text(
                json.dumps(
                    {
                        "tool": "Forge",
                        "path": "Tools/Forge",
                        "repo": "ardaonly/forge",
                        "target_branch": "main",
                        "source_commit": "abc123",
                        "source_short": "abc123",
                        "subtree_commit": "def456789000",
                        "remote_head_after": "def456789000",
                    }
                ),
                encoding="utf-8",
            )
            manifest = load_export_manifest(root)
            states = load_export_states(root, manifest)
            status = GitStatus(root, "main", "abc123", "", 0, 0, ())

            health = evaluate_export_health(manifest, states, status)

        self.assertEqual(health[0].status, HEALTH_READY)

    def test_export_health_blocks_dirty_tool_path(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            manifest_dir = root / "Tools" / "Developer" / "RepoAudit"
            state_dir = root / ".core" / "export" / "state"
            manifest_dir.mkdir(parents=True)
            state_dir.mkdir(parents=True)
            (manifest_dir / "export-manifest.json").write_text(
                json.dumps(
                    {
                        "target_branch": "main",
                        "tools": {
                            "Forge": {"path": "Tools/Forge", "repo": "ardaonly/forge"},
                        },
                    }
                ),
                encoding="utf-8",
            )
            (state_dir / "FORGE.json").write_text(
                json.dumps(
                    {
                        "tool": "Forge",
                        "path": "Tools/Forge",
                        "repo": "ardaonly/forge",
                        "target_branch": "main",
                        "source_commit": "abc123",
                        "source_short": "abc123",
                        "subtree_commit": "def456789000",
                        "remote_head_after": "def456789000",
                    }
                ),
                encoding="utf-8",
            )
            manifest = load_export_manifest(root)
            states = load_export_states(root, manifest)
            status = GitStatus(root, "main", "abc123", "", 0, 0, (" M Tools/Forge/src/main.cpp",))

            health = evaluate_export_health(manifest, states, status)

        self.assertEqual(health[0].status, HEALTH_BLOCKED)

    def test_export_health_marks_stale_source(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            manifest_dir = root / "Tools" / "Developer" / "RepoAudit"
            state_dir = root / ".core" / "export" / "state"
            manifest_dir.mkdir(parents=True)
            state_dir.mkdir(parents=True)
            (manifest_dir / "export-manifest.json").write_text(
                json.dumps(
                    {
                        "target_branch": "main",
                        "tools": {
                            "Forge": {"path": "Tools/Forge", "repo": "ardaonly/forge"},
                        },
                    }
                ),
                encoding="utf-8",
            )
            (state_dir / "FORGE.json").write_text(
                json.dumps(
                    {
                        "tool": "Forge",
                        "path": "Tools/Forge",
                        "repo": "ardaonly/forge",
                        "target_branch": "main",
                        "source_commit": "old",
                        "source_short": "old",
                        "subtree_commit": "def456789000",
                        "remote_head_after": "def456789000",
                    }
                ),
                encoding="utf-8",
            )
            manifest = load_export_manifest(root)
            states = load_export_states(root, manifest)
            status = GitStatus(root, "main", "new", "", 0, 0, ())

            health = evaluate_export_health(manifest, states, status)

        self.assertEqual(health[0].status, HEALTH_STALE)

    def test_commit_queue_blocks_export_until_source_commit(self) -> None:
        status = GitStatus(
            repo_root=Path("."),
            branch="main",
            head="abc",
            upstream="origin/main",
            ahead=0,
            behind=0,
            status_lines=(" M Tools/Forge/src/main.cpp", " M SagaWiki/dev/ITERATION_NOTES.md"),
        )
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            manifest_dir = root / "Tools" / "Developer" / "RepoAudit"
            state_dir = root / ".core" / "export" / "state"
            manifest_dir.mkdir(parents=True)
            state_dir.mkdir(parents=True)
            (manifest_dir / "export-manifest.json").write_text(
                json.dumps(
                    {
                        "target_branch": "main",
                        "tools": {
                            "Forge": {"path": "Tools/Forge", "repo": "ardaonly/forge"},
                        },
                    }
                ),
                encoding="utf-8",
            )
            manifest = load_export_manifest(root)

        queue = build_commit_queue(status, manifest)
        self.assertEqual(queue[0].title, "Prepare SagaEngine source change")
        self.assertEqual(queue[1].title, "Export Forge mirror")
        self.assertTrue(queue[1].blocked)

    def test_commit_plan_groups_source_tool_and_docs_changes(self) -> None:
        status = GitStatus(
            repo_root=Path("."),
            branch="main",
            head="abc",
            upstream="origin/main",
            ahead=0,
            behind=0,
            status_lines=(
                " M src/runtime.cpp",
                " M Tools/Forge/src/main.cpp",
                "?? Tools/Developer/Environment/SagaSync/core/commit_plan.py",
                " M SagaWiki/dev/ITERATION_NOTES.md",
            ),
        )
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            manifest_dir = root / "Tools" / "Developer" / "RepoAudit"
            manifest_dir.mkdir(parents=True)
            (manifest_dir / "export-manifest.json").write_text(
                json.dumps(
                    {
                        "target_branch": "main",
                        "tools": {
                            "Forge": {"path": "Tools/Forge", "repo": "ardaonly/forge"},
                        },
                    }
                ),
                encoding="utf-8",
            )
            manifest = load_export_manifest(root)

        plan = build_commit_plan(status, manifest)

        self.assertEqual([group.kind for group in plan.groups], [GROUP_SOURCE, GROUP_TOOL, GROUP_TOOL, GROUP_DOCS])
        self.assertEqual([group.label for group in plan.groups], ["SagaEngine", "Forge", "SagaSync", "Documentation"])
        self.assertEqual(plan.groups[0].suggested_message, "feat: update SagaEngine source changes")
        self.assertEqual(plan.groups[1].suggested_message, "feat(forge): sync Forge changes")
        self.assertEqual(plan.groups[2].suggested_message, "feat(sagasync): update SagaSync changes")
        self.assertEqual(plan.groups[3].suggested_message, "docs: update development notes")
        self.assertFalse(plan.groups[0].blocked)
        self.assertTrue(plan.groups[1].blocked)

    def test_commit_plan_parses_renames_and_summarizes_files(self) -> None:
        parsed = parse_status_line("R  Tools/Forge/old.cpp -> Tools/Forge/new.cpp")
        self.assertIsNotNone(parsed)
        self.assertEqual(parsed.path, "Tools/Forge/new.cpp")
        self.assertEqual(parsed.status, "R")

        files = tuple(parse_status_line(f" M file{i}.txt") for i in range(6))
        complete = tuple(item for item in files if item is not None)
        self.assertEqual(
            summarize_files(complete, limit=3),
            "file0.txt, file1.txt, file2.txt, +3 more",
        )

    def test_safe_actions_are_dry_run_or_read_only(self) -> None:
        actions = default_toolbar_actions() + verification_actions()
        commands = [" ".join(action.command) for action in actions]
        self.assertIn("scripts/export forge --dry-run", commands)
        self.assertIn("git diff --check", commands)
        forbidden = (" commit", " push", " checkout", " switch", " branch")
        for command in commands:
            self.assertFalse(any(token in command for token in forbidden), command)

    def test_verification_profiles_are_named_and_safe(self) -> None:
        profiles = verification_profiles()
        self.assertEqual([profile.name for profile in profiles], ["quick", "forge", "export-safety"])
        commands = [action.command for profile in profiles for action in profile.actions]
        self.assertIn(("python3", "Tools/Forge/build.py", "-j", "1"), commands)
        for command in commands:
            joined = " ".join(command)
            self.assertNotIn(" commit", joined)
            self.assertNotIn(" push", joined)

    def test_run_history_formats_result_status(self) -> None:
        result = make_run_result(
            profile="quick",
            started_at=datetime(2026, 5, 19, 12, 0, 0, tzinfo=timezone.utc),
            duration_ms=42,
            exit_code=0,
            command_summary="git diff --check",
        )

        self.assertEqual(result.status, RUN_PASSED)
        self.assertEqual(profile_display_status(result), RUN_PASSED)
        self.assertEqual(profile_display_status(None), RUN_NOT_RUN)

        failed = make_run_result(
            profile="quick",
            started_at=datetime(2026, 5, 19, 12, 0, 0, tzinfo=timezone.utc),
            duration_ms=-1,
            exit_code=1,
            command_summary="git diff --check",
        )
        self.assertEqual(failed.status, RUN_FAILED)
        self.assertEqual(failed.duration_ms, 0)

    def test_health_display_tracks_unverified_and_failed_verification(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            manifest_dir = root / "Tools" / "Developer" / "RepoAudit"
            state_dir = root / ".core" / "export" / "state"
            manifest_dir.mkdir(parents=True)
            state_dir.mkdir(parents=True)
            (manifest_dir / "export-manifest.json").write_text(
                json.dumps(
                    {
                        "target_branch": "main",
                        "tools": {
                            "Forge": {"path": "Tools/Forge", "repo": "ardaonly/forge"},
                        },
                    }
                ),
                encoding="utf-8",
            )
            (state_dir / "FORGE.json").write_text(
                json.dumps(
                    {
                        "tool": "Forge",
                        "path": "Tools/Forge",
                        "repo": "ardaonly/forge",
                        "target_branch": "main",
                        "source_commit": "abc123",
                        "source_short": "abc123",
                        "subtree_commit": "def456789000",
                        "remote_head_after": "def456789000",
                    }
                ),
                encoding="utf-8",
            )
            manifest = load_export_manifest(root)
            states = load_export_states(root, manifest)
            status = GitStatus(root, "main", "abc123", "", 0, 0, ())
            health = evaluate_export_health(manifest, states, status)[0]

        self.assertEqual(health_display_status(health, None), DISPLAY_READY_UNVERIFIED)
        self.assertIn("verification profile has not run", health_display_reason(health, None))
        failed = make_run_result(
            profile="export-safety",
            started_at=datetime(2026, 5, 19, 12, 0, 0, tzinfo=timezone.utc),
            duration_ms=1,
            exit_code=1,
            command_summary="scripts/export plan",
        )
        self.assertEqual(health_display_status(health, failed), DISPLAY_BLOCKED_BY_VERIFICATION)
        self.assertIn("verification profile failed", health_display_reason(health, failed))

    def test_smoke_payload_uses_core_models(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            manifest_dir = root / "Tools" / "Developer" / "RepoAudit"
            state_dir = root / ".core" / "export" / "state"
            manifest_dir.mkdir(parents=True)
            state_dir.mkdir(parents=True)
            (manifest_dir / "export-manifest.json").write_text(
                json.dumps(
                    {
                        "target_branch": "main",
                        "tools": {
                            "Forge": {"path": "Tools/Forge", "repo": "ardaonly/forge"},
                        },
                    }
                ),
                encoding="utf-8",
            )
            (state_dir / "FORGE.json").write_text(
                json.dumps(
                    {
                        "tool": "Forge",
                        "path": "Tools/Forge",
                        "repo": "ardaonly/forge",
                        "target_branch": "main",
                        "source_commit": "unknown",
                        "source_short": "unknown",
                        "subtree_commit": "def456789000",
                        "remote_head_after": "def456789000",
                    }
                ),
                encoding="utf-8",
            )
            # Initialize a minimal git repository for get_git_status.
            import subprocess

            subprocess.run(["git", "init"], cwd=root, check=True, capture_output=True)

            payload = build_smoke_payload(root)

        self.assertEqual(payload["tools"], ["Forge"])
        self.assertEqual(payload["queueItems"], 1)
        self.assertEqual(payload["commitPlan"]["groups"], 1)
        self.assertEqual(payload["commitPlan"]["blocked"], 0)
        self.assertEqual(payload["commitPlan"]["messages"], ["feat: update SagaEngine source changes"])
        self.assertEqual(payload["exportHealthDisplay"]["Forge"], DISPLAY_READY_UNVERIFIED)
        self.assertEqual(payload["verificationProfiles"], ["quick", "forge", "export-safety"])
        self.assertEqual(payload["verificationResults"], {})


if __name__ == "__main__":
    unittest.main()
