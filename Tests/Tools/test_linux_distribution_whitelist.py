#!/usr/bin/env python3
"""Focused synthetic tests for the default Linux distribution whitelist."""

from __future__ import annotations

import contextlib
from importlib.machinery import SourceFileLoader
import importlib.util
import io
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))

from saga_linux_distribution_policy import (  # noqa: E402
    ALLOWED_EXECUTABLE_PATHS,
    ALLOWED_ROOT_FILES,
    APPROVED_DOCUMENTS,
    EXCLUDED_DEV_TOOLS,
    EXCLUDED_RETIRED_TOOLS,
    EXCLUDED_SOURCE_SURFACES,
    FORBIDDEN_EXECUTABLE_NAMES,
    REQUIRED_LICENSE_PATHS,
    included_applications,
    included_public_tools,
    staged_file_inventory,
    validate_archive_entries,
    validate_layout,
    validate_metadata_sets,
)


def load_packager():
    path = ROOT / "scripts" / "package-linux-saga"
    spec = importlib.util.spec_from_loader(
        "package_linux_saga", SourceFileLoader("package_linux_saga", str(path))
    )
    if spec is None or spec.loader is None:
        raise RuntimeError("could not load package-linux-saga")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def write_file(root: Path, rel: str, *, executable: bool = False) -> None:
    path = root / rel
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(f"fixture:{rel}\n", encoding="utf-8")
    if executable:
        path.chmod(0o755)


def exact_layout(root: Path) -> None:
    for rel in sorted(ALLOWED_ROOT_FILES | APPROVED_DOCUMENTS | REQUIRED_LICENSE_PATHS):
        write_file(root, rel)
    for rel in sorted(ALLOWED_EXECUTABLE_PATHS):
        write_file(root, rel, executable=True)
    write_file(root, "licenses/Apache-2.0.txt")
    write_file(root, "samples/StarterArena/StarterArena.sagaproj")


def exact_metadata() -> dict[str, object]:
    return {
        "includedTargets": ["Saga", "SagaEditor", "SagaRuntime"],
        "publicTools": ["sagapack", "sagaproject", "sagascript"],
        "includedApplications": included_applications(),
        "includedPublicTools": included_public_tools(),
        "excludedDevTools": list(EXCLUDED_DEV_TOOLS),
        "excludedRetiredTools": list(EXCLUDED_RETIRED_TOOLS),
        "excludedSourceSurfaces": list(EXCLUDED_SOURCE_SURFACES),
    }


class LinuxDistributionWhitelistTests(unittest.TestCase):
    def test_exact_allowed_layout_passes_and_inventory_is_deterministic(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "Saga"
            exact_layout(root)
            self.assertEqual([], validate_layout(root))
            first = staged_file_inventory(root)
            second = staged_file_inventory(root)
            self.assertEqual(first, second)
            self.assertEqual(sorted(item["path"] for item in first), [item["path"] for item in first])

    def test_missing_product_application_fails(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "Saga"
            exact_layout(root)
            (root / "bin" / "SagaEditor").unlink()
            self.assertTrue(any("bin/SagaEditor" in error for error in validate_layout(root)))

    def test_extra_application_and_unknown_executable_fail(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "Saga"
            exact_layout(root)
            write_file(root, "bin/UnexpectedProduct", executable=True)
            errors = validate_layout(root)
            self.assertTrue(any("unapproved package file" in error for error in errors))
            self.assertTrue(any("unrecognized executable-bit" in error for error in errors))

    def test_every_forbidden_executable_name_fails(self) -> None:
        for name in sorted(FORBIDDEN_EXECUTABLE_NAMES):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as directory:
                root = Path(directory) / "Saga"
                exact_layout(root)
                write_file(root, f"samples/StarterArena/{name}", executable=True)
                self.assertTrue(
                    any("forbidden executable basename" in error for error in validate_layout(root))
                )

    def test_test_and_unknown_payload_executables_fail(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "Saga"
            exact_layout(root)
            write_file(root, "samples/StarterArena/UnitTests", executable=True)
            (root / "samples" / "StarterArena" / "Tests").mkdir()
            write_file(root, "tools/.saga-tools/sagapack/helper", executable=True)
            errors = validate_layout(root)
            self.assertTrue(any("test executable" in error for error in errors))
            self.assertTrue(any("test payload directory" in error for error in errors))
            self.assertTrue(any("unrecognized executable-bit" in error for error in errors))

    def test_public_tool_payload_only_passes_under_approved_root(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "Saga"
            exact_layout(root)
            write_file(root, "tools/.saga-tools/unknown/helper.dll")
            self.assertTrue(any("unapproved package file" in error for error in validate_layout(root)))

    def test_metadata_sets_are_exact(self) -> None:
        metadata = exact_metadata()
        self.assertEqual([], validate_metadata_sets(metadata))
        metadata["publicTools"] = ["sagapack", "sagaproject", "sagascript", "extra"]
        self.assertTrue(validate_metadata_sets(metadata))

    def test_archive_rejects_forbidden_and_unknown_entries(self) -> None:
        errors = validate_archive_entries(
            ["Saga/", "Saga/bin/", "Saga/bin/Saga", "Saga/bin/SagaClient"]
        )
        self.assertTrue(any("SagaClient" in error for error in errors))
        self.assertTrue(validate_archive_entries(["Saga/", "Saga/bin/ExtraProduct"]))

    def test_failed_preflight_removes_stale_layout_archive_and_checksum(self) -> None:
        packager = load_packager()
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            output = temp / "out" / "Saga"
            write_file(output, "bin/Stale", executable=True)
            archive = output.parent / "Saga.tar.zst"
            checksum = output.parent / "Saga.sha256"
            archive.write_bytes(b"stale")
            checksum.write_text("stale\n", encoding="utf-8")
            report = temp / "report.json"
            old_cwd = Path.cwd()
            old_argv = sys.argv
            try:
                os.chdir(temp)
                sys.argv = [
                    "package-linux-saga",
                    "--build-dir",
                    str(temp / "missing-build"),
                    "--output",
                    str(output),
                    "--report-out",
                    str(report),
                ]
                with contextlib.redirect_stdout(io.StringIO()):
                    self.assertEqual(1, packager.main())
            finally:
                sys.argv = old_argv
                os.chdir(old_cwd)
            self.assertFalse(output.exists())
            self.assertFalse(archive.exists())
            self.assertFalse(checksum.exists())
            self.assertEqual("blocked", json.loads(report.read_text(encoding="utf-8"))["status"])


if __name__ == "__main__":
    unittest.main()
