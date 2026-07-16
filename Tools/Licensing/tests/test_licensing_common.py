# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

from pathlib import Path
import sys
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from licensing_common import (  # noqa: E402
    action_severity,
    gitwildmatch,
    safe_relative_path,
    validate_spdx_expression,
)


class LicensingCommonTests(unittest.TestCase):
    def test_safe_relative_paths_reject_escape_and_windows_forms(self) -> None:
        self.assertTrue(safe_relative_path("Engine/Source/File.cpp"))
        for value in ("", "../outside", "/absolute", "Engine\\File.cpp", "A/./B"):
            with self.subTest(value=value):
                self.assertFalse(safe_relative_path(value))

    def test_gitwildmatch_has_segment_aware_double_star(self) -> None:
        self.assertTrue(gitwildmatch("Engine/File.cpp", "Engine/**"))
        self.assertTrue(gitwildmatch("Engine/A/B/File.cpp", "Engine/**/File.cpp"))
        self.assertFalse(gitwildmatch("Engine/A/B/File.cpp", "Engine/*/File.cpp"))
        self.assertFalse(
            gitwildmatch("engine/File.cpp", "Engine/**", case_sensitive=True)
        )

    def test_spdx_parser_accepts_composition_and_collects_license_refs(self) -> None:
        refs = validate_spdx_expression(
            "(MPL-2.0 OR Apache-2.0) AND LicenseRef-Saga-Proprietary"
        )
        self.assertEqual(refs, {"LicenseRef-Saga-Proprietary"})

    def test_spdx_parser_rejects_incomplete_expressions(self) -> None:
        for value in ("", "MPL-2.0 AND", "(MPL-2.0", "MPL-2.0 WITH"):
            with self.subTest(value=value):
                with self.assertRaises(ValueError):
                    validate_spdx_expression(value)

    def test_policy_actions_have_explicit_visibility(self) -> None:
        self.assertEqual(action_severity("deny"), "ERROR")
        self.assertEqual(action_severity("report"), "WARNING")
        self.assertIsNone(action_severity("ignore"))
        with self.assertRaises(ValueError):
            action_severity("allow")


if __name__ == "__main__":
    unittest.main()
