#!/usr/bin/env python3
"""
Tests for Prism's generic external JSON ingestion.
"""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]
GRAPH_RUNNER = REPO_ROOT / "Tools" / "Prism" / "pipeline" / "run.py"


def write_json(path: Path, payload: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload), encoding="utf-8")


def write_raw_graph_input(path: Path, repo_root: Path) -> None:
    write_json(path, {
        "schema_version": "1.0",
        "generated_by": "test",
        "generated_at": "2026-01-01T00:00:00Z",
        "repo_root": str(repo_root),
        "files": [],
        "symbols": [],
    })


def run_graph(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(GRAPH_RUNNER), *args],
        cwd=REPO_ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


class ExternalJsonIngestionTests(unittest.TestCase):
    def test_no_external_inputs_preserves_existing_output_shape(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            raw = root / "raw.json"
            out = root / "out"
            write_raw_graph_input(raw, root)

            result = run_graph(str(raw), "--out-dir", str(out), "--no-txt", "--quiet")

            self.assertEqual(result.returncode, 0, result.stderr)
            payload = json.loads((out / "prism.graph.json").read_text(encoding="utf-8"))
            self.assertNotIn("external_manifests", payload)
            self.assertNotIn("external_diagnostics", payload)
            self.assertNotIn("total_external_manifests", payload["stats"])
            self.assertNotIn("total_external_diagnostics", payload["stats"])
            self.assertNotIn("has_blocking_external_diagnostics", payload["stats"])

    def test_external_json_payloads_are_preserved_under_keys(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            raw = root / "raw.json"
            manifest = root / "manifest.json"
            diagnostics = root / "diagnostics.json"
            out = root / "out"
            write_raw_graph_input(raw, root)
            write_json(manifest, {"items": [{"id": "alpha"}]})
            write_json(diagnostics, {
                "summary": {"hasBlockingDiagnostics": True},
                "messages": [{"code": "E001"}],
            })

            result = run_graph(
                str(raw),
                "--out-dir", str(out),
                "--no-txt",
                "--quiet",
                "--external-manifest", f"analysis.index={manifest}",
                "--external-diagnostics", f"analysis={diagnostics}",
            )

            self.assertEqual(result.returncode, 0, result.stderr)
            payload = json.loads((out / "prism.graph.json").read_text(encoding="utf-8"))
            self.assertEqual(
                payload["external_manifests"]["analysis.index"],
                {"items": [{"id": "alpha"}]},
            )
            self.assertEqual(
                payload["external_diagnostics"]["analysis"]["messages"],
                [{"code": "E001"}],
            )
            self.assertEqual(payload["stats"]["total_external_manifests"], 1)
            self.assertEqual(payload["stats"]["total_external_diagnostics"], 1)
            self.assertTrue(payload["stats"]["has_blocking_external_diagnostics"])

    def test_root_blocking_diagnostics_flag_is_detected(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            raw = root / "raw.json"
            diagnostics = root / "diagnostics.json"
            out = root / "out"
            write_raw_graph_input(raw, root)
            write_json(diagnostics, {"hasBlockingDiagnostics": True})

            result = run_graph(
                str(raw),
                "--out-dir", str(out),
                "--no-txt",
                "--quiet",
                "--external-diagnostics", f"lint={diagnostics}",
            )

            self.assertEqual(result.returncode, 0, result.stderr)
            payload = json.loads((out / "prism.graph.json").read_text(encoding="utf-8"))
            self.assertTrue(payload["stats"]["has_blocking_external_diagnostics"])

    def test_missing_external_file_fails(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            raw = root / "raw.json"
            out = root / "out"
            missing = root / "missing.json"
            write_raw_graph_input(raw, root)

            result = run_graph(
                str(raw),
                "--out-dir", str(out),
                "--no-txt",
                "--external-manifest", f"analysis.index={missing}",
            )

            self.assertNotEqual(result.returncode, 0)
            self.assertIn("External manifest 'analysis.index'", result.stderr)
            self.assertIn("file does not exist", result.stderr)

    def test_invalid_external_json_fails(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            raw = root / "raw.json"
            invalid = root / "invalid.json"
            out = root / "out"
            write_raw_graph_input(raw, root)
            invalid.write_text("{", encoding="utf-8")

            result = run_graph(
                str(raw),
                "--out-dir", str(out),
                "--no-txt",
                "--external-diagnostics", f"lint={invalid}",
            )

            self.assertNotEqual(result.returncode, 0)
            self.assertIn("External diagnostics 'lint'", result.stderr)
            self.assertIn("invalid JSON", result.stderr)

    def test_invalid_external_argument_syntax_fails(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            raw = root / "raw.json"
            write_raw_graph_input(raw, root)

            result = run_graph(
                str(raw),
                "--external-manifest", "missing-separator",
            )

            self.assertNotEqual(result.returncode, 0)
            self.assertIn("external_manifest", result.stderr)
            self.assertIn("<key>=<path>", result.stderr)


if __name__ == "__main__":
    unittest.main()
