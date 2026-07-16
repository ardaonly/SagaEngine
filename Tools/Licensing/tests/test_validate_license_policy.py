# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

import copy
import importlib.util
from pathlib import Path
import sys
import tempfile
import tomllib
import unittest
import json


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


class PolicyFailureModeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.policy = tomllib.loads(POLICY_PATH.read_text(encoding="utf-8"))

    def test_unknown_policy_field_is_denied(self) -> None:
        policy = copy.deepcopy(self.policy)
        policy["unexpected_contract"] = True
        report = validator.Report()
        validator.validate_schema(policy, report)
        self.assertIn("UNKNOWN_POLICY_FIELD", {item.code for item in report.errors})

    def test_invalid_action_and_unsafe_exclude_are_denied(self) -> None:
        validation = copy.deepcopy(self.policy["validation"])
        validation["unclassified_action"] = "allow"
        validation["untracked_exclude_patterns"].append("../outside/**")
        report = validator.Report()
        validator.validate_action_table(report, validation)
        self.assertEqual(
            {item.code for item in report.errors},
            {"INVALID_VALIDATION_ACTION", "UNTRACKED_EXCLUDE_UNSAFE"},
        )

    def test_classification_rejects_equal_priority_overlap(self) -> None:
        domains = [
            validator.Domain({}, "one", 100, ["Engine/**"], True),
            validator.Domain({}, "two", 100, ["Engine/Source/**"], True),
        ]
        selected, winners = validator.classify(
            "Engine/Source/File.cpp", domains, case_sensitive=True
        )
        self.assertIsNone(selected)
        self.assertEqual({item.id for item in winners}, {"one", "two"})

    def test_classification_uses_highest_priority_owner(self) -> None:
        domains = [
            validator.Domain({}, "broad", 100, ["Tools/**"], True),
            validator.Domain({}, "specific", 500, ["Tools/Licensing/**"], True),
        ]
        selected, winners = validator.classify(
            "Tools/Licensing/check_dco.py", domains, case_sensitive=True
        )
        self.assertEqual(selected.id, "specific")
        self.assertEqual(winners, [selected])

    def test_json_report_has_machine_visible_counts_and_resolutions(self) -> None:
        report = validator.Report()
        report.add("ERROR", "BROKEN", "broken fixture")
        report.add("WARNING", "REVIEW", "review fixture")
        report.classified["Engine/File.cpp"] = "saga-engine"
        report.resolutions["Engine/File.cpp"] = validator.Resolution(
            domain_id="saga-engine",
            matched=True,
            resolved=True,
            expression="MPL-2.0",
            source="policy",
        )
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "report.json"
            validator.write_json_report(output, report)
            payload = json.loads(output.read_text(encoding="utf-8"))
        self.assertEqual(payload["summary"]["errors"], 1)
        self.assertEqual(payload["summary"]["warnings"], 1)
        self.assertEqual(payload["summary"]["license_resolved"], 1)
        self.assertEqual(
            payload["resolutions"]["Engine/File.cpp"]["expression"], "MPL-2.0"
        )


class CMakeFileApiTests(unittest.TestCase):
    def write_json(self, path: Path, payload: object) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(payload), encoding="utf-8")

    def test_loads_sources_dependencies_and_install_surface(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            reply = build / ".cmake/api/v1/reply"
            self.write_json(
                reply / "index-fixture.json",
                {
                    "reply": {
                        "codemodel-v2": {
                            "kind": "codemodel",
                            "version": {"major": 2, "minor": 7},
                            "jsonFile": "codemodel-v2-fixture.json",
                        }
                    }
                },
            )
            self.write_json(
                reply / "codemodel-v2-fixture.json",
                {
                    "configurations": [
                        {
                            "name": "RelWithDebInfo",
                            "targets": [
                                {"id": "dep::@1", "name": "Dependency", "jsonFile": "dep.json"},
                                {"id": "owner::@1", "name": "Owner", "jsonFile": "owner.json"},
                            ],
                        }
                    ]
                },
            )
            self.write_json(
                reply / "dep.json",
                {"name": "Dependency", "type": "STATIC_LIBRARY", "paths": {}},
            )
            self.write_json(
                reply / "owner.json",
                {
                    "name": "Owner",
                    "type": "STATIC_LIBRARY",
                    "paths": {"source": ".", "build": "."},
                    "sources": [
                        {"path": "Engine/Owner.cpp", "isGenerated": False},
                        {"path": "generated.cpp", "isGenerated": True},
                    ],
                    "dependencies": [{"id": "dep::@1"}],
                    "install": {"destinations": [{"path": "lib"}]},
                },
            )

            graph = validator.load_cmake_graph(build, "RelWithDebInfo")

        owner = graph["RelWithDebInfo"]["Owner"]
        self.assertEqual(owner["dependencies"], {"Dependency"})
        self.assertEqual(owner["installs"], ["lib"])
        self.assertEqual(owner["sources"][1], {"path": "generated.cpp", "generated": True})

    def test_rejects_unavailable_selected_configuration(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            reply = build / ".cmake/api/v1/reply"
            self.write_json(
                reply / "codemodel-v2-fixture.json",
                {"configurations": [{"name": "Debug", "targets": []}]},
            )
            with self.assertRaisesRegex(ValueError, "configuration not found"):
                validator.load_cmake_graph(build, "Release")


if __name__ == "__main__":
    unittest.main()
