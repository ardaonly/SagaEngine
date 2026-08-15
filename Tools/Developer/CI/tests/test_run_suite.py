# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


MODULE_PATH = Path(__file__).resolve().parents[1] / "run_suite.py"
SPEC = importlib.util.spec_from_file_location("run_suite", MODULE_PATH)
runner = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(runner)


class RunSuiteTests(unittest.TestCase):
    def test_catalog_is_cmake_owned_and_contains_no_python_inventory(self) -> None:
        source = MODULE_PATH.read_text(encoding="utf-8")
        self.assertNotIn("SagaLauncherUiTests", source)
        self.assertNotIn("CoreModuleTests", source)
        self.assertEqual(runner.CATALOG_RELATIVE_PATH.name, "SagaTestSuites.json")

    def test_suite_map_preserves_catalog_names(self) -> None:
        catalog = {"suites": [{"name": "quick"}, {"name": "headless"}]}
        self.assertEqual(list(runner.suite_map(catalog)), ["quick", "headless"])

    def test_junit_counts_are_reported_only_from_a_real_document(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.assertIsNone(runner.junit_counts(root / "missing.xml"))
            report = root / "report.xml"
            report.write_text(
                '<testsuites><testsuite tests="5" failures="1" errors="1" skipped="1"/></testsuites>',
                encoding="utf-8")
            self.assertEqual(
                runner.junit_counts(report),
                {"passed": 2, "failed": 2, "skipped": 1})

    def test_manifest_is_preserved_with_failure_state(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "manifest.json"
            manifest = {"status": "FAIL", "exitCode": 7}
            runner.write_manifest(path, manifest, runner.time.monotonic())
            loaded = json.loads(path.read_text(encoding="utf-8"))
            self.assertEqual(loaded["exitCode"], 7)
            self.assertIn("endedAt", loaded)

    def test_product_evidence_is_owned_beside_the_manifest(self) -> None:
        manifest = Path("reports/product-xcb.json")
        self.assertEqual(
            runner.evidence_directory(manifest),
            Path("reports/product-xcb.artifacts"))


if __name__ == "__main__":
    unittest.main()
