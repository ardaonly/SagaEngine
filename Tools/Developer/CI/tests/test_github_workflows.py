# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import unittest


MODULE_PATH = Path(__file__).resolve().parents[1] / "check_github_workflows.py"
SPEC = importlib.util.spec_from_file_location("check_github_workflows", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
checker = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = checker
SPEC.loader.exec_module(checker)


PIN = "0123456789abcdef0123456789abcdef01234567"


def workflow(extra: str = "") -> str:
    return f"""name: Fixture

on: workflow_dispatch

permissions:
  contents: read

concurrency:
  group: fixture
  cancel-in-progress: true

jobs:
  fixture:
    runs-on: ubuntu-24.04
    timeout-minutes: 10
    steps:
      - uses: actions/checkout@{PIN}
{extra}
"""


class WorkflowContractTests(unittest.TestCase):
    def test_accepts_minimal_secure_workflow(self) -> None:
        self.assertEqual(checker.check_workflow("fixture.yml", workflow()), [])

    def test_rejects_mutable_action_reference(self) -> None:
        findings = checker.check_workflow(
            "fixture.yml", workflow().replace(f"@{PIN}", "@v5")
        )
        self.assertIn("action-pin", {finding.rule for finding in findings})

    def test_rejects_missing_timeout_and_concurrency(self) -> None:
        text = workflow().replace("\nconcurrency:\n  group: fixture\n  cancel-in-progress: true\n", "")
        text = text.replace("    timeout-minutes: 10\n", "")
        rules = {finding.rule for finding in checker.check_workflow("fixture.yml", text)}
        self.assertEqual(rules, {"concurrency", "timeout"})

    def test_rejects_hidden_failures(self) -> None:
        findings = checker.check_workflow(
            "fixture.yml", workflow("      - run: false\n        continue-on-error: ON # hidden\n")
        )
        self.assertIn("failure-visibility", {finding.rule for finding in findings})

    def test_rejects_job_level_repository_write_permission(self) -> None:
        text = workflow().replace(
            "    runs-on: ubuntu-24.04\n",
            "    permissions:\n      pull-requests: write\n    runs-on: ubuntu-24.04\n",
        )
        findings = checker.check_workflow("fixture.yml", text)
        self.assertIn("permissions", {finding.rule for finding in findings})

    def test_finds_job_bodies_independently(self) -> None:
        text = workflow() + """  second:
    runs-on: windows-2022
    steps:
      - run: echo missing-timeout
"""
        findings = checker.check_workflow("fixture.yml", text)
        self.assertEqual(
            [finding.message for finding in findings if finding.rule == "timeout"],
            ["job second has no timeout"],
        )


if __name__ == "__main__":
    unittest.main()
