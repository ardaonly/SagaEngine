# SPDX-License-Identifier: MPL-2.0
from __future__ import annotations

from .support.environment import *
from .support.evidence import *
from .support.templates import *

def test_valid_project_report_passed() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        manifest = create_project(Path(tmp) / "Project")
        out = Path(tmp) / "project_validation_report.json"

        result = run_cli("validate", "--project", str(manifest), "--out", str(out))

        assert result.returncode == 0, result.stderr + result.stdout
        report = load_report(out)
        assert report["schemaVersion"] == 1
        assert report["tool"] == "sagaproject"
        assert report["command"] == "validate"
        assert report["status"] == "Passed"
        assert report["diagnostics"] == []
        assert report["project"]["projectId"] == "fixture-project"

def test_missing_required_field_fails() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        manifest_payload = valid_manifest()
        del manifest_payload["projectId"]
        manifest = create_project(Path(tmp) / "Project", manifest_payload)
        out = Path(tmp) / "report.json"

        result = run_cli("validate", "--project", str(manifest), "--out", str(out))

        assert result.returncode == 1
        report = load_report(out)
        assert report["status"] == "Failed"
        assert any(d["code"] == "Project.Manifest.MissingField" for d in report["diagnostics"])

def test_unknown_schema_version_fails() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        manifest_payload = valid_manifest()
        manifest_payload["schemaVersion"] = 99
        manifest = create_project(Path(tmp) / "Project", manifest_payload)
        out = Path(tmp) / "report.json"

        result = run_cli("validate", "--project", str(manifest), "--out", str(out))

        assert result.returncode == 1
        report = load_report(out)
        assert any(
            d["code"] == "Project.Manifest.SchemaVersionUnsupported"
            for d in report["diagnostics"]
        )

def test_non_sagaproj_manifest_is_rejected() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp) / "Project"
        manifest = root / ("saga.project" + ".json")
        write_json(manifest, valid_manifest())
        out = Path(tmp) / "report.json"

        result = run_cli("validate", "--project", str(manifest), "--out", str(out))

        assert result.returncode == 1
        assert load_report(out)["diagnostics"][0]["code"] == "Project.Input.UnsupportedFileType"

def test_project_relative_path_rules_are_enforced() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        manifest_payload = valid_manifest()
        manifest_payload["assets"] = [
            {"id": "absolute", "path": "/tmp/not-project", "kind": "directory"},
            {"id": "escape", "path": "../escape", "kind": "directory"},
        ]
        manifest = create_project(Path(tmp) / "Project", manifest_payload)
        out = Path(tmp) / "report.json"

        result = run_cli("validate", "--project", str(manifest), "--out", str(out))

        assert result.returncode == 1
        codes = {d["code"] for d in load_report(out)["diagnostics"]}
        assert "Project.Path.AbsoluteNotAllowed" in codes
        assert "Project.Path.ParentTraversalNotAllowed" in codes

def test_missing_referenced_profile_fails() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        manifest = create_project(Path(tmp) / "Project")
        (manifest.parent / "launch_profiles.json").unlink()
        out = Path(tmp) / "report.json"

        result = run_cli("validate", "--project", str(manifest), "--out", str(out))

        assert result.returncode == 1
        report = load_report(out)
        assert any(d["code"] == "Project.Reference.Missing" for d in report["diagnostics"])

def test_missing_input_uses_deterministic_exit_code() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "report.json"

        result = run_cli("validate", "--project", str(Path(tmp) / "missing.sagaproj"), "--out", str(out))

        assert result.returncode == 3
        assert load_report(out)["diagnostics"][0]["code"] == "Project.Input.Missing"

def test_invalid_usage_uses_deterministic_exit_code() -> None:
    result = run_cli("validate", "--project")

    assert result.returncode == 2

def test_project_open_does_not_mutate_manifest() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        manifest = create_project(Path(tmp) / "Project")
        before = manifest.read_text(encoding="utf-8")
        out = Path(tmp) / "report.json"

        result = run_cli("validate", "--project", str(manifest), "--out", str(out))

        assert result.returncode == 0, result.stderr + result.stdout
        assert manifest.read_text(encoding="utf-8") == before

def test_resolve_output_is_stable_and_ordered() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        manifest = create_project(Path(tmp) / "Project")
        first = Path(tmp) / "first.json"
        second = Path(tmp) / "second.json"

        first_result = run_cli("resolve", "--project", str(manifest), "--out", str(first))
        second_result = run_cli("resolve", "--project", str(manifest), "--out", str(second))

        assert first_result.returncode == 0, first_result.stderr + first_result.stdout
        assert second_result.returncode == 0, second_result.stderr + second_result.stdout
        assert first.read_text(encoding="utf-8") == second.read_text(encoding="utf-8")
        paths = load_report(first)["resolvedPaths"]
        keys = [(p["category"], p["id"], p["relativePath"]) for p in paths]
        assert keys == sorted(keys)

def test_resolve_slice_reports_visibility_and_report_only_mode() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp) / "Project"
        manifest = create_project(root)
        slice_path = root / ".saga" / "slices" / "designer-local.slice.json"
        out = Path(tmp) / "project_slice_resolution_report.json"
        write_json(slice_path, valid_slice())

        result = run_cli("resolve-slice", "--project", str(manifest), "--slice", str(slice_path), "--out", str(out))

        assert result.returncode == 0, result.stderr + result.stdout
        report = load_report(out)
        assert report["command"] == "resolve-slice"
        assert report["status"] == "Passed"
        assert report["mutatesSource"] is False
        assert report["enforcement"] == "ReportOnly"
        visibility = {(item["kind"], item["id"]): item for item in report["resourceVisibility"]}
        assert visibility[("ScriptFile", "door-logic")]["visibility"] == "FullSource"
        assert visibility[("ScriptFile", "advanced")]["visibility"] == "SummaryOnly"
        assert visibility[("ReportArtifact", "raw-full-ctest")]["status"] == "Excluded"

def test_resolve_slice_rejects_path_traversal_and_unknown_kind() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp) / "Project"
        manifest = create_project(root)
        slice_path = root / ".saga" / "slices" / "bad.slice.json"
        out = Path(tmp) / "project_slice_resolution_report.json"
        write_json(
            slice_path,
            valid_slice(
                includedResources=[
                    {
                        "kind": "ScriptFile",
                        "id": "escape",
                        "path": "../Outside.cs",
                        "visibility": "FullSource",
                    },
                    {
                        "kind": "Impossible",
                        "id": "unknown",
                        "visibility": "SummaryOnly",
                    },
                ],
                excludedResources=[],
            ),
        )

        result = run_cli("resolve-slice", "--project", str(manifest), "--slice", str(slice_path), "--out", str(out))

        assert result.returncode == 1
        codes = {d["code"] for d in load_report(out)["diagnostics"]}
        assert "ProjectSlice.Path.ParentTraversalNotAllowed" in codes
        assert "ProjectSlice.Resource.UnknownKind" in codes

def test_restricted_resolve_records_hidden_resources_without_changing_validate() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp) / "Project"
        manifest = create_project(root)
        slice_path = root / ".saga" / "slices" / "restricted.slice.json"
        restricted_out = Path(tmp) / "restricted_project_resolution_report.json"
        validate_out = Path(tmp) / "project_validation_report.json"
        write_json(
            slice_path,
            valid_slice(
                includedResources=[
                    {
                        "kind": "ScriptFile",
                        "id": "advanced",
                        "path": "Scripts/AdvancedUnsupported.cs",
                        "visibility": "Hidden",
                    },
                    {
                        "kind": "ScriptFile",
                        "id": "door-logic",
                        "path": "Scripts/DoorLogic.High.cs",
                        "visibility": "FullSource",
                    },
                ],
                excludedResources=[],
            ),
        )
        before = (root / "Scripts" / "AdvancedUnsupported.cs").read_text(encoding="utf-8")

        restricted = run_cli(
            "restricted-resolve",
            "--project",
            str(manifest),
            "--slice",
            str(slice_path),
            "--out",
            str(restricted_out),
        )
        validate = run_cli("validate", "--project", str(manifest), "--out", str(validate_out))

        assert restricted.returncode == 0, restricted.stderr + restricted.stdout
        assert validate.returncode == 0, validate.stderr + validate.stdout
        report = load_report(restricted_out)
        assert report["command"] == "restricted-resolve"
        assert report["mutatesSource"] is False
        assert report["enforcement"] == "ReportOnly"
        assert any(item["id"] == "advanced" and item["relativePath"] == "" for item in report["restrictedResources"])
        assert load_report(validate_out)["command"] == "validate"
        assert (root / "Scripts" / "AdvancedUnsupported.cs").read_text(encoding="utf-8") == before

def test_resolve_slice_missing_resource_fails_deterministically() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp) / "Project"
        manifest = create_project(root)
        slice_path = root / ".saga" / "slices" / "missing.slice.json"
        first = Path(tmp) / "first.json"
        second = Path(tmp) / "second.json"
        write_json(
            slice_path,
            valid_slice(
                includedResources=[
                    {
                        "kind": "ScriptFile",
                        "id": "missing",
                        "path": "Scripts/Missing.cs",
                        "visibility": "FullSource",
                    }
                ],
                excludedResources=[],
            ),
        )

        first_result = run_cli("resolve-slice", "--project", str(manifest), "--slice", str(slice_path), "--out", str(first))
        second_result = run_cli("resolve-slice", "--project", str(manifest), "--slice", str(slice_path), "--out", str(second))

        assert first_result.returncode == 1
        assert second_result.returncode == 1
        assert first.read_text(encoding="utf-8") == second.read_text(encoding="utf-8")
        report = load_report(first)
        assert any(d["code"] == "ProjectSlice.Resource.Missing" for d in report["diagnostics"])
        assert report["missingArtifacts"][0]["status"] == "Missing"

def test_doctor_detects_missing_scripts_folder() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        manifest = create_project(Path(tmp) / "Project")
        shutil.rmtree(manifest.parent / "Scripts")
        out = Path(tmp) / "doctor.json"

        result = run_cli("doctor", "--project", str(manifest), "--out", str(out))

        assert result.returncode == 1
        report = load_report(out)
        assert report["command"] == "doctor"
        assert any(d["code"] == "Project.Reference.Missing" for d in report["diagnostics"])

def test_doctor_detects_invalid_profile_json() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        manifest = create_project(Path(tmp) / "Project")
        (manifest.parent / "package_profiles.json").write_text("{ invalid json", encoding="utf-8")
        out = Path(tmp) / "doctor.json"

        result = run_cli("doctor", "--project", str(manifest), "--out", str(out))

        assert result.returncode == 1
        assert any(
            d["code"] == "Project.Profile.ParseFailed"
            for d in load_report(out)["diagnostics"]
        )

def test_multiplayer_sandbox_validate_resolve_doctor_pass() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        validate_out = Path(tmp) / "validate.json"
        resolve_out = Path(tmp) / "resolve.json"
        doctor_out = Path(tmp) / "doctor.json"

        validate = run_cli("validate", "--project", str(SAMPLE_PROJECT), "--out", str(validate_out))
        resolve = run_cli("resolve", "--project", str(SAMPLE_PROJECT), "--out", str(resolve_out))
        doctor = run_cli("doctor", "--project", str(SAMPLE_PROJECT), "--out", str(doctor_out))

        assert validate.returncode == 0, validate.stderr + validate.stdout
        assert resolve.returncode == 0, resolve.stderr + resolve.stdout
        assert doctor.returncode == 0, doctor.stderr + doctor.stdout
        assert load_report(validate_out)["status"] == "Passed"
        assert load_report(resolve_out)["status"] == "Passed"
        assert load_report(doctor_out)["status"] == "Passed"
        assert load_report(doctor_out)["diagnostics"] == []

def test_multiplayer_sandbox_sample_slice_resolves() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "project_slice_resolution_report.json"

        result = run_cli("resolve-slice", "--project", str(SAMPLE_PROJECT), "--slice", str(SAMPLE_SLICE), "--out", str(out))

        assert result.returncode == 0, result.stderr + result.stdout
        report = load_report(out)
        assert report["slice"]["sliceId"] == "designer-local"
        assert report["status"] == "Passed"

TESTS = (
    test_valid_project_report_passed,
    test_missing_required_field_fails,
    test_unknown_schema_version_fails,
    test_non_sagaproj_manifest_is_rejected,
    test_project_relative_path_rules_are_enforced,
    test_missing_referenced_profile_fails,
    test_missing_input_uses_deterministic_exit_code,
    test_invalid_usage_uses_deterministic_exit_code,
    test_project_open_does_not_mutate_manifest,
    test_resolve_output_is_stable_and_ordered,
    test_resolve_slice_reports_visibility_and_report_only_mode,
    test_resolve_slice_rejects_path_traversal_and_unknown_kind,
    test_restricted_resolve_records_hidden_resources_without_changing_validate,
    test_resolve_slice_missing_resource_fails_deterministically,
    test_doctor_detects_missing_scripts_folder,
    test_doctor_detects_invalid_profile_json,
    test_multiplayer_sandbox_validate_resolve_doctor_pass,
    test_multiplayer_sandbox_sample_slice_resolves,
)
