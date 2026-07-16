# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import unittest


MODULE_PATH = Path(__file__).resolve().parents[1] / "check_dco.py"
sys.path.insert(0, str(MODULE_PATH.parent))
SPEC = importlib.util.spec_from_file_location("check_dco", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
dco = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = dco
SPEC.loader.exec_module(dco)


class DcoValidationTests(unittest.TestCase):
    def test_accepts_valid_signoff_case_insensitively(self) -> None:
        self.assertTrue(
            dco.valid_signoff(
                "Subject\n\nsigned-off-by: Arda Koyuncu <arda@example.com>\n"
            )
        )

    def test_rejects_missing_or_malformed_signoff(self) -> None:
        self.assertFalse(dco.valid_signoff("Subject only\n"))
        self.assertFalse(dco.valid_signoff("Signed-off-by: Arda <not-an-email>\n"))

    def test_evaluates_contributions_and_skips_merges(self) -> None:
        result = dco.evaluate_commits(
            [
                dco.CommitEvidence("good", 1, "Signed-off-by: A <a@example.com>"),
                dco.CommitEvidence("bad", 1, "No trailer"),
                dco.CommitEvidence("merge", 2, "No trailer"),
            ]
        )
        self.assertEqual(result.checked, ("good", "bad"))
        self.assertEqual(result.failures, ("bad",))
        self.assertEqual(result.skipped_merges, ("merge",))


if __name__ == "__main__":
    unittest.main()
