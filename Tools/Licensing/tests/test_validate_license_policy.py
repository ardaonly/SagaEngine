# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

import copy
import importlib.util
from pathlib import Path
import sys
import tempfile
import tomllib
import unittest


MODULE_PATH = Path(__file__).resolve().parents[1] / "validate_license_policy.py"
POLICY_PATH = Path(__file__).resolve().parents[3] / "LICENSE_POLICY.toml"
sys.path.insert(0, str(MODULE_PATH.parent))
SPEC = importlib.util.spec_from_file_location("validate_license_policy", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
validator = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = validator
SPEC.loader.exec_module(validator)


class PolicyArrayUniquenessTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.policy = tomllib.loads(POLICY_PATH.read_text(encoding="utf-8"))

    def duplicate_findings(self, policy: dict[str, object]) -> list[validator.Finding]:
        report = validator.Report()
        validator.validate_policy_tables(policy, report)
        return [
            finding
            for finding in report.errors
            if finding.code == "DUPLICATE_STRING_ARRAY_ENTRY"
        ]

    def test_rejects_duplicate_review_record_path(self) -> None:
        policy = copy.deepcopy(self.policy)
        paths = policy["review_record_paths"]
        self.assertIsInstance(paths, list)
        paths.append(paths[0])

        findings = self.duplicate_findings(policy)

        self.assertEqual(len(findings), 1)
        self.assertIn("policy.review_record_paths", findings[0].message)

    def test_rejects_duplicate_governance_document(self) -> None:
        policy = copy.deepcopy(self.policy)
        governance = policy["governance"]
        self.assertIsInstance(governance, dict)
        documents = governance["required_documents"]
        self.assertIsInstance(documents, list)
        documents.append(documents[0])

        findings = self.duplicate_findings(policy)

        self.assertEqual(len(findings), 1)
        self.assertIn("governance.required_documents", findings[0].message)


class ComposedObjectSourceTests(unittest.TestCase):
    def test_recovers_runtime_owner_from_unix_object_path(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "Engine/Source/Runtime/Core/Private/Core.cpp"
            source.parent.mkdir(parents=True)
            source.write_text("// source\n", encoding="utf-8")

            result = validator.composed_object_source(
                root,
                "CMakeFiles/SagaCoreModule.dir/"
                "Engine/Source/Runtime/Core/Private/Core.cpp.o",
            )

            self.assertEqual(
                result, "Engine/Source/Runtime/Core/Private/Core.cpp"
            )

    def test_recovers_editor_owner_from_windows_object_path(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "Engine/Source/Editor/EditorCore/Private/Editor.cpp"
            source.parent.mkdir(parents=True)
            source.write_text("// source\n", encoding="utf-8")

            result = validator.composed_object_source(
                root,
                "CMakeFiles/SagaEditorCoreModule.dir/"
                "Engine/Source/Editor/EditorCore/Private/Editor.cpp.obj",
            )

            self.assertEqual(
                result, "Engine/Source/Editor/EditorCore/Private/Editor.cpp"
            )

    def test_rejects_missing_unsafe_and_non_object_sources(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.assertIsNone(
                validator.composed_object_source(
                    root, "CMakeFiles/Owner.dir/../../outside.cpp.o"
                )
            )
            self.assertIsNone(
                validator.composed_object_source(
                    root, "CMakeFiles/Owner.dir/Engine/Missing.cpp.o"
                )
            )
            self.assertIsNone(
                validator.composed_object_source(
                    root, "CMakeFiles/Owner.dir/Engine/Source.cpp"
                )
            )


if __name__ == "__main__":
    unittest.main()
