# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import unittest


MODULE_PATH = Path(__file__).resolve().parents[1] / "maintainability_audit.py"
SPEC = importlib.util.spec_from_file_location("maintainability_audit", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
AUDIT = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = AUDIT
SPEC.loader.exec_module(AUDIT)


def record(path: str, lines: int):
    return AUDIT.FileRecord(
        path=path, language="Python", physicalLines=lines, nonEmptyLines=lines,
        largestFunction=None, largestFunctionLines=None, majorResponsibilities=1,
        generated=False, role="production", embeddedSpdx="MPL-2.0",
        resolvedLicense="MPL-2.0", sizeClass="high-maintenance",
        recommendedAction="review ownership split",
    )


class MaintainabilityBaselineTests(unittest.TestCase):
    def test_new_oversized_file_fails(self) -> None:
        diagnostics = AUDIT.evaluate_baseline([record("Tools/New.py", 1001)], {"entries": []})
        self.assertEqual([item.code for item in diagnostics], ["Maintainability.NewOversizedFile"])

    def test_existing_file_cannot_grow(self) -> None:
        baseline = {"entries": [{
            "path": "Tools/Old.py", "maximumPhysicalLines": 1100,
            "reason": "cohesive", "owner": "Tools", "disposition": "reviewed-exception",
        }]}
        diagnostics = AUDIT.evaluate_baseline([record("Tools/Old.py", 1101)], baseline)
        self.assertEqual([item.code for item in diagnostics], ["Maintainability.BaselineGrowth"])

    def test_removed_debt_requires_baseline_cleanup(self) -> None:
        baseline = {"entries": [{
            "path": "Tools/Removed.py", "maximumPhysicalLines": 1100,
            "reason": "debt", "owner": "Tools", "disposition": "follow-up",
        }]}
        diagnostics = AUDIT.evaluate_baseline([], baseline)
        self.assertEqual([item.code for item in diagnostics], ["Maintainability.BaselineStale"])

    def test_complete_unchanged_baseline_passes(self) -> None:
        baseline = {"entries": [{
            "path": "Tools/Cohesive.py", "maximumPhysicalLines": 1200,
            "reason": "single parser", "owner": "Tools", "disposition": "reviewed-exception",
        }]}
        self.assertEqual(AUDIT.evaluate_baseline([record("Tools/Cohesive.py", 1150)], baseline), [])


if __name__ == "__main__":
    unittest.main()
