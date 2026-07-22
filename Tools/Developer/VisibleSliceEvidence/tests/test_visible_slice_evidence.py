# SPDX-License-Identifier: MPL-2.0

import json
import pathlib
import subprocess
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[4]
RUNNER = ROOT / "Tools/Developer/VisibleSliceEvidence/visible_slice_evidence.py"


class VisibleSliceEvidenceTests(unittest.TestCase):
    def test_dry_run_is_atomic_structured_and_explicitly_unexecuted(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            evidence = pathlib.Path(temporary) / "evidence"
            result = subprocess.run([sys.executable, str(RUNNER), "--repo-root", str(ROOT),
                "--evidence-root", str(evidence), "--dry-run",
                "--command-json", json.dumps([sys.executable, "--version"])],
                text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
            self.assertEqual(result.returncode, 0, result.stderr)
            report = json.loads((evidence / "visible-slice-evidence.json").read_text())
            self.assertEqual(report["schemaVersion"], 1)
            self.assertEqual(report["status"], "unexecuted")
            self.assertEqual(report["graphical"]["status"], "unexecuted")
            self.assertEqual(report["commands"][0]["status"], "unexecuted")
            self.assertIn("does not replace", report["nonClaims"][0])

    def test_serial_command_result_records_log_hash(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            evidence = pathlib.Path(temporary) / "evidence"
            command = [sys.executable, "-c", "print('visible-slice')"]
            result = subprocess.run([sys.executable, str(RUNNER), "--repo-root", str(ROOT),
                "--evidence-root", str(evidence), "--command-json", json.dumps(command)],
                text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
            self.assertEqual(result.returncode, 0, result.stderr)
            report = json.loads((evidence / "visible-slice-evidence.json").read_text())
            self.assertEqual(report["status"], "passed")
            self.assertEqual(report["commands"][0]["exitCode"], 0)
            self.assertTrue(report["commands"][0]["logHash"].startswith("sha256:"))

    def test_runtime_result_is_normalized_into_graphical_evidence(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            evidence = root / "evidence"
            capture = root / "frame.ppm"
            capture.write_bytes(b"P6\n1 1\n255\n\x01\x02\x03")
            runtime = root / "runtime-result.json"
            runtime.write_text(json.dumps({
                "canonicalLaunchManifestHash": "sha256:" + "a" * 64,
                "framesExecuted": 3,
                "interactionActionCount": 2,
                "playSessionId": "graphical-test",
                "status": "Succeeded",
                "observedState": {"drawCount": 1, "meaningfulRendererPixels": True,
                                  "promptText": ["[E] Open Door", "[E] Close Door"]},
                "runtimeDiagnostics": [{"code": "Runtime.Render.FrameCaptureEvidence",
                                        "subject": str(capture)}],
            }), encoding="utf-8")
            result = subprocess.run([sys.executable, str(RUNNER), "--repo-root", str(ROOT),
                "--evidence-root", str(evidence), "--runtime-evidence", str(runtime),
                "--graphical", "--command-json", json.dumps([sys.executable, "--version"])],
                text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
            self.assertEqual(result.returncode, 0, result.stderr)
            report = json.loads((evidence / "visible-slice-evidence.json").read_text())
            self.assertEqual(report["graphical"]["status"], "executed")
            self.assertEqual(report["graphical"]["promptStateSequence"][1], "[E] Close Door")
            self.assertTrue(report["graphical"]["captures"][0]["hash"].startswith("sha256:"))


if __name__ == "__main__":
    unittest.main()
