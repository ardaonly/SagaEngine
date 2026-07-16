# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[3]
LICENSING = ROOT / "Tools" / "Licensing"
sys.path.insert(0, str(LICENSING))


def load(name: str):
    path = LICENSING / f"{name}.py"
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


generator = load("generate_legacy_path_rules")
checksums = load("regenerate_license_checksums")


class GeneratedControlTests(unittest.TestCase):
    def test_path_rules_are_priority_sorted_and_preserve_owner(self) -> None:
        payload = generator.build_payload(
            {
                "domains": [
                    {
                        "id": "broad",
                        "priority": 100,
                        "current_license_mode": "fixed-spdx",
                        "current_license_expression": "MPL-2.0",
                        "paths": ["Tools/**"],
                    },
                    {
                        "id": "specific",
                        "priority": 500,
                        "current_license_mode": "fixed-spdx",
                        "current_license_expression": "Apache-2.0",
                        "paths": ["Tools/Forge/**"],
                    },
                ]
            }
        )
        self.assertEqual(
            [rule["domain_id"] for rule in payload["rules"]],
            ["specific", "broad"],
        )
        self.assertFalse(payload["authoritative"])

    def test_checksum_render_rejects_missing_inputs(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with mock.patch.object(
                checksums,
                "expected_checksum_paths",
                return_value={"LICENSE", "missing"},
            ):
                (root / "LICENSE").write_text("license", encoding="utf-8")
                with self.assertRaisesRegex(RuntimeError, "missing"):
                    checksums.render(root, include_legacy=False)

    def test_current_generated_path_rules_match_policy_projection(self) -> None:
        import tomllib

        policy = tomllib.loads((ROOT / "LICENSE_POLICY.toml").read_text(encoding="utf-8"))
        actual = json.loads(
            (
                ROOT
                / "Tools/Developer/BoundaryCheck/Policies/path_rules.json"
            ).read_text(encoding="utf-8")
        )
        self.assertEqual(actual, generator.build_payload(policy))


if __name__ == "__main__":
    unittest.main()
