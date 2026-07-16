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

    def test_rejects_duplicate_job_display_names_across_workflows(self) -> None:
        shared = workflow().replace(
            "  fixture:\n",
            "  fixture:\n    name: Shared check\n",
        )
        texts = {
            "one.yml": shared,
            "two.yml": shared.replace("name: Fixture", "name: Other Workflow", 1),
        }
        findings = checker.check_repository_job_contracts(texts)
        self.assertEqual(
            [finding.rule for finding in findings],
            ["check-name-unique"],
        )

    def test_requires_full_history_for_both_licensing_jobs(self) -> None:
        texts = {
            "licensing.yml": """name: Licensing

jobs:
  licensing-policy:
    name: Policy
    timeout-minutes: 10
    steps:
      - uses: actions/checkout@fixture
        with:
          fetch-depth: 0
  configured-graph:
    name: Graph
    timeout-minutes: 10
    steps:
      - uses: actions/checkout@fixture
  licensing-gate:
    name: Gate
    timeout-minutes: 5
    steps:
      - run: true
""",
        }
        findings = checker.check_repository_job_contracts(texts)
        self.assertEqual(
            [finding.message for finding in findings],
            ["job configured-graph must checkout with fetch-depth: 0"],
        )

    def test_required_checks_may_not_use_a_matrix(self) -> None:
        texts = {
            "repository-contracts.yml": """name: Repository Contracts

jobs:
  repository-contracts:
    name: Repository contracts
    strategy:
      matrix:
        mode: [debug, release]
    timeout-minutes: 10
""",
        }
        findings = checker.check_repository_job_contracts(texts)
        self.assertEqual(
            [finding.rule for finding in findings],
            ["required-check-matrix"],
        )


if __name__ == "__main__":
    unittest.main()
