# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


MODULE_PATH = Path(__file__).resolve().parents[1] / "validate_license_policy.py"
sys.path.insert(0, str(MODULE_PATH.parent))
SPEC = importlib.util.spec_from_file_location("validate_license_policy_risks", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
validator = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = validator
SPEC.loader.exec_module(validator)


def policy(**actions: str) -> dict[str, object]:
    validation = {
        "case_sensitive": True,
        "governance_missing_action": "deny",
        "external_symlink_action": "deny",
        "submodule_missing_action": "deny",
        "lfs_pointer_action": "deny",
        "rename_domain_change_action": "deny",
    }
    validation.update(actions)
    return {
        "validation": validation,
        "reviewed_commit": "1" * 40,
        "review_record_paths": [],
    }


class RepositoryRiskTests(unittest.TestCase):
    def test_missing_submodule_is_denied(self) -> None:
        report = validator.Report()
        with mock.patch.object(validator, "recursive_submodule_status", return_value={}):
            validator.validate_submodules(
                Path("."), {"ThirdParty/Vendor": "0" * 40}, policy(), report
            )
        self.assertEqual([item.code for item in report.errors], ["SUBMODULE_NOT_PRESENT"])

    def test_external_symlink_is_denied(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            link = root / "external-link"
            link.symlink_to(Path(directory).parent / "outside")
            report = validator.Report()
            with mock.patch.object(
                validator, "tracked_modes", return_value={"external-link": "120000"}
            ):
                validator.validate_symlinks(root, policy(), report)
        self.assertEqual([item.code for item in report.errors], ["EXTERNAL_SYMLINK"])

    def test_missing_governance_documents_and_markers_are_denied(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "CONTRIBUTING.md").write_text("no markers\n", encoding="utf-8")
            report = validator.Report()
            value = policy()
            value["governance"] = {
                "required_documents": ["LICENSE", "CONTRIBUTING.md"],
                "contributing_section_source": "LICENSE",
                "contributing_document": "CONTRIBUTING.md",
                "contributing_update_status": "required-before-foundation-commit",
                "contributing_marker_begin": "BEGIN",
                "contributing_marker_end": "END",
            }
            validator.validate_governance(root, value, report)
        self.assertEqual(
            {item.code for item in report.errors},
            {
                "GOVERNANCE_DOCUMENT_MISSING",
                "CONTRIBUTING_SECTION_SOURCE_MISSING",
                "CONTRIBUTING_DCO_SECTION_NOT_APPLIED",
            },
        )

    def test_invalid_lfs_index_pointer_is_denied(self) -> None:
        report = validator.Report()
        attr_result = subprocess.CompletedProcess(
            ["git"], 0, "asset.bin: filter: lfs\n", ""
        )
        lfs_result = subprocess.CompletedProcess(["git"], 1, "", "unavailable")
        blob_result = subprocess.CompletedProcess(["git"], 0, b"not a pointer", b"")
        with mock.patch.object(
            validator, "run", side_effect=[attr_result, lfs_result]
        ), mock.patch.object(
            validator, "run_git", return_value="asset.bin\n"
        ), mock.patch.object(
            validator, "run_bytes", return_value=blob_result
        ):
            validator.validate_lfs(Path("."), ["asset.bin"], policy(), report)
        self.assertIn("LFS_INDEX_POINTER_INVALID", {item.code for item in report.errors})
        self.assertIn("GIT_LFS_UNAVAILABLE", {item.code for item in report.findings})

    def test_cross_domain_rename_after_reviewed_baseline_is_denied_once(self) -> None:
        report = validator.Report()
        domains = [
            validator.Domain({}, "engine", 100, ["Engine/**"], True),
            validator.Domain({}, "tools", 100, ["Tools/**"], True),
        ]
        empty = subprocess.CompletedProcess(["git"], 0, "", "")
        reviewed_diff = subprocess.CompletedProcess(
            ["git"], 0, "R100\tEngine/File.cpp\tTools/File.cpp\n", ""
        )
        with mock.patch.object(
            validator,
            "run",
            side_effect=[empty, empty, reviewed_diff],
        ) as run_mock:
            validator.validate_renames(
                Path("."), "1" * 40, domains, policy(), report
            )
        self.assertEqual([item.code for item in report.errors], ["RENAME_DOMAIN_CHANGE"])
        self.assertEqual(
            run_mock.call_args_list[-1].args[0][-1],
            f"{'1' * 40}..HEAD",
        )

    def test_reviewed_baseline_does_not_reuse_event_base(self) -> None:
        report = validator.Report()
        domains = [
            validator.Domain({}, "engine", 100, ["Engine/**"], True),
            validator.Domain({}, "tools", 100, ["Tools/**"], True),
        ]
        empty = subprocess.CompletedProcess(["git"], 0, "", "")
        with mock.patch.object(
            validator,
            "run",
            side_effect=[empty, empty, empty],
        ) as run_mock:
            validator.validate_renames(
                Path("."), "1" * 40, domains, policy(), report
            )
        self.assertEqual(report.errors, [])
        commands = [call.args[0] for call in run_mock.call_args_list]
        self.assertIn(f"{'1' * 40}..HEAD", commands[-1])
        self.assertNotIn("event-base...HEAD", commands[-1])

    def test_shallow_repository_is_denied(self) -> None:
        report = validator.Report()
        shallow = subprocess.CompletedProcess(["git"], 0, "true\n", "")
        with mock.patch.object(validator, "run", return_value=shallow):
            result = validator.validate_reviewed_commit(Path("."), policy(), report)
        self.assertIsNone(result)
        self.assertEqual([item.code for item in report.errors], ["SHALLOW_REPOSITORY"])

    def test_unverifiable_shallow_state_is_denied(self) -> None:
        report = validator.Report()
        failed = subprocess.CompletedProcess(["git"], 1, "", "not a repository")
        with mock.patch.object(validator, "run", return_value=failed):
            result = validator.validate_reviewed_commit(Path("."), policy(), report)
        self.assertIsNone(result)
        self.assertEqual(
            [item.code for item in report.errors],
            ["SHALLOW_REPOSITORY_UNVERIFIED"],
        )

    def test_invalid_reviewed_commit_format_is_denied(self) -> None:
        report = validator.Report()
        value = policy()
        value["reviewed_commit"] = "not-a-full-sha"
        not_shallow = subprocess.CompletedProcess(["git"], 0, "false\n", "")
        with mock.patch.object(validator, "run", return_value=not_shallow):
            result = validator.validate_reviewed_commit(Path("."), value, report)
        self.assertIsNone(result)
        self.assertEqual([item.code for item in report.errors], ["COMMIT_FORMAT"])

    def test_missing_reviewed_commit_is_denied(self) -> None:
        report = validator.Report()
        not_shallow = subprocess.CompletedProcess(["git"], 0, "false\n", "")
        missing = subprocess.CompletedProcess(["git"], 128, "", "missing")
        with mock.patch.object(
            validator,
            "run",
            side_effect=[not_shallow, missing],
        ):
            result = validator.validate_reviewed_commit(Path("."), policy(), report)
        self.assertIsNone(result)
        self.assertEqual([item.code for item in report.errors], ["COMMIT_MISSING"])

    def test_non_commit_reviewed_object_is_denied(self) -> None:
        report = validator.Report()
        not_shallow = subprocess.CompletedProcess(["git"], 0, "false\n", "")
        blob = subprocess.CompletedProcess(["git"], 0, "blob\n", "")
        with mock.patch.object(
            validator,
            "run",
            side_effect=[not_shallow, blob],
        ):
            result = validator.validate_reviewed_commit(Path("."), policy(), report)
        self.assertIsNone(result)
        self.assertEqual(
            [item.code for item in report.errors],
            ["REVIEWED_OBJECT_NOT_COMMIT"],
        )

    def test_non_ancestor_reviewed_commit_is_denied(self) -> None:
        report = validator.Report()
        not_shallow = subprocess.CompletedProcess(["git"], 0, "false\n", "")
        commit = subprocess.CompletedProcess(["git"], 0, "commit\n", "")
        head = subprocess.CompletedProcess(["git"], 0, f"{'2' * 40}\n", "")
        diverged = subprocess.CompletedProcess(["git"], 1, "", "")
        with mock.patch.object(
            validator,
            "run",
            side_effect=[not_shallow, commit, head, diverged],
        ):
            result = validator.validate_reviewed_commit(Path("."), policy(), report)
        self.assertIsNone(result)
        self.assertEqual(
            [item.code for item in report.errors],
            ["EVIDENCE_BASELINE_NOT_ANCESTOR"],
        )


if __name__ == "__main__":
    unittest.main()
