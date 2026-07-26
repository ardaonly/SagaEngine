# SPDX-License-Identifier: MPL-2.0

import importlib.util
import json
import pathlib
import subprocess
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[4]
RUNNER = ROOT / "Tools/Developer/VisibleSliceEvidence/visible_slice_evidence.py"
SPEC = importlib.util.spec_from_file_location("visible_slice_evidence", RUNNER)
assert SPEC and SPEC.loader
EVIDENCE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(EVIDENCE)


def frame(width: int, height: int, variant: int,
          bounds: dict[str, float]) -> tuple[int, int, bytes]:
    pixels = bytearray(width * height * 3)
    if variant:
        region = EVIDENCE.physical_region(bounds, width, height)
        for y in range(region["height"]):
            for x in range(region["width"]):
                marked = (x + y) % 2 == 0 if variant == 1 else x % 2 == 0
                if marked:
                    offset = ((region["y"] + y) * width + region["x"] + x) * 3
                    pixels[offset:offset + 3] = b"\xff\xff\xff"
    return width, height, bytes(pixels)


def write_ppm(path: pathlib.Path, value: tuple[int, int, bytes]) -> None:
    width, height, pixels = value
    path.write_bytes(f"P6\n{width} {height}\n255\n".encode() + pixels)


class GraphicalFixture:
    def __init__(self, root: pathlib.Path) -> None:
        self.root = root
        self.e_session = "e-session"
        self.f_session = "f-session"
        self.e_launch = "sha256:" + "a" * 64
        self.f_launch = "sha256:" + "b" * 64
        self.e_input = "sha256:" + "c" * 64
        self.f_input = "sha256:" + "d" * 64
        self.shared = {
            "uiArtifactHash": "sha256:" + "e" * 64,
            "uiDocumentHash": "sha256:" + "4" * 64,
            "uiStylesheetHashes": ["sha256:" + "5" * 64],
            "sceneHash": "sha256:" + "1" * 64,
            "scriptAssemblyHash": "sha256:" + "2" * 64,
            "packageGenerationHash": "sha256:" + "3" * 64,
        }
        self.e_records = self._records("e", self.e_session, self.e_launch, self.e_input)
        self.f_records = self._records("f", self.f_session, self.f_launch, self.f_input)
        self.style_records = self._records(
            "e", "style-session", "sha256:" + "6" * 64, self.e_input,
            file_prefix="style", style=True)
        for record in self.style_records:
            record["uiArtifactHash"] = "sha256:" + "7" * 64
            record["uiStylesheetHashes"] = ["sha256:" + "8" * 64]
        self.e_path = self._runtime("e-result.json", self.e_session, self.e_launch, self.e_records)
        self.f_path = self._runtime("f-result.json", self.f_session, self.f_launch, self.f_records)
        self.style_path = self._runtime(
            "style-result.json", "style-session", "sha256:" + "6" * 64,
            self.style_records)

    def _capture(self, name: str, prefix: str, index: int, label: str, prompt: str,
                 state: bool, value: tuple[int, int, bytes],
                 pre_overlay: tuple[int, int, bytes],
                 previous: tuple[int, int, bytes] | None, session: str, launch: str,
                 input_hash: str, bounds: dict[str, float]) -> dict:
        path = self.root / f"{prefix}-{name}.ppm"
        pre_path = self.root / f"{prefix}-{name}-pre-overlay.ppm"
        write_ppm(path, value)
        write_ppm(pre_path, pre_overlay)
        width, height, _ = value
        region = EVIDENCE.physical_region(bounds, width, height)
        baseline_difference = EVIDENCE.difference(value, pre_overlay, region)
        semantic_difference = (0, 0) if previous is None else \
            EVIDENCE.difference(value, previous, region)
        return {"name": name, "frameIndex": index, "width": width, "height": height,
                "sessionId": session, "canonicalLaunchHash": launch,
                "inputArtifactHash": input_hash, **self.shared,
                "promptBindingLabel": label, "promptSemanticLabel": prompt,
                "doorState": state, "filePath": str(path), "sha256": EVIDENCE.sha256(path),
                "preOverlayPath": str(pre_path), "preOverlaySha256": EVIDENCE.sha256(pre_path),
                "preOverlayFrameIndex": index, "layoutBounds": bounds, "pixelRatio": 1.0,
                "uiDrawCallCount": 0 if name == "world-baseline" else 2,
                "hudRegion": region,
                "nonBackgroundPixelCount": EVIDENCE.non_background_count(value, region),
                "baselineDifferencePixelCount": baseline_difference[0],
                "baselineRgbAbsoluteDifference": baseline_difference[1],
                "priorSemanticDifferencePixelCount": semantic_difference[0],
                "priorSemanticRgbAbsoluteDifference": semantic_difference[1]}

    def _records(self, prefix: str, session: str, launch: str, input_hash: str,
                 file_prefix: str | None = None, style: bool = False) -> list[dict]:
        bounds = {"x": 3.0, "y": 12.0, "width": 14.0, "height": 6.0}
        pre_overlay = frame(20, 20, 0, bounds)
        opened = frame(20, 20, 2 if style else 1, bounds)
        closed = frame(20, 20, 1 if style else 2, bounds)
        upper = prefix.upper()
        artifact_prefix = file_prefix or prefix
        opened_record = self._capture(f"{prefix}-open", artifact_prefix, 1, upper,
            f"[{upper}] Open Door", False, opened, pre_overlay, None,
            session, launch, input_hash, bounds)
        baseline_record = dict(opened_record, name="world-baseline",
            promptBindingLabel="", promptSemanticLabel="", doorState=False,
            filePath=opened_record["preOverlayPath"], sha256=opened_record["preOverlaySha256"],
            uiDrawCallCount=0, baselineDifferencePixelCount=0,
            baselineRgbAbsoluteDifference=0, priorSemanticDifferencePixelCount=0,
            priorSemanticRgbAbsoluteDifference=0,
            nonBackgroundPixelCount=EVIDENCE.non_background_count(
                pre_overlay, opened_record["hudRegion"]))
        records = [baseline_record, opened_record]
        records.append(self._capture(f"{prefix}-close", artifact_prefix, 31, upper,
            f"[{upper}] Close Door", True, closed, pre_overlay, opened,
            session, launch, input_hash, bounds))
        if prefix == "e":
            records.append(self._capture("e-reopened", artifact_prefix, 191, "E", "[E] Open Door",
                False, opened, pre_overlay, closed, session, launch, input_hash, bounds))
            resized_bounds = {"x": 4.5, "y": 14.4, "width": 21.0, "height": 7.2}
            resized_pre = frame(30, 24, 0, resized_bounds)
            resized = frame(30, 24, 1, resized_bounds)
            records.append(self._capture("e-resized", artifact_prefix, 246, "E", "[E] Open Door",
                False, resized, resized_pre, None, session, launch, input_hash,
                resized_bounds))
        return records

    def _runtime(self, name: str, session: str, launch: str, records: list[dict]) -> pathlib.Path:
        path = self.root / name
        path.write_text(json.dumps({"schemaVersion": 1, "status": "Succeeded",
            "playSessionId": session, "canonicalLaunchManifestHash": launch,
            "observedState": {"evidenceCaptures": records}}), encoding="utf-8")
        return path

    def save(self) -> None:
        self._runtime("e-result.json", self.e_session, self.e_launch, self.e_records)
        self._runtime("f-result.json", self.f_session, self.f_launch, self.f_records)


class VisibleSliceEvidenceTests(unittest.TestCase):
    def run_runner(self, evidence: pathlib.Path, *runtime_paths: pathlib.Path,
                   graphical: bool = False) -> subprocess.CompletedProcess[str]:
        command = [sys.executable, str(RUNNER), "--repo-root", str(ROOT),
            "--evidence-root", str(evidence),
            "--command-json", json.dumps([sys.executable, "--version"])]
        for path in runtime_paths:
            command.extend(["--runtime-evidence", str(path)])
        if graphical:
            command.append("--graphical")
        return subprocess.run(command, text=True, stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, check=False)

    def test_dry_run_is_atomic_structured_and_explicitly_unexecuted(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            evidence = pathlib.Path(temporary) / "evidence"
            result = subprocess.run([sys.executable, str(RUNNER), "--repo-root", str(ROOT),
                "--evidence-root", str(evidence), "--dry-run",
                "--command-json", json.dumps([sys.executable, "--version"])],
                text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
            self.assertEqual(result.returncode, 0, result.stderr)
            report = json.loads((evidence / "visible-slice-evidence.json").read_text())
            self.assertEqual(report["schemaVersion"], 2)
            self.assertEqual(report["status"], "unexecuted")
            self.assertEqual(report["graphical"]["status"], "unexecuted")
            self.assertEqual(report["commands"][0]["status"], "unexecuted")

    def test_serial_command_result_records_log_hash(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            evidence = pathlib.Path(temporary) / "evidence"
            result = self.run_runner(evidence)
            self.assertEqual(result.returncode, 0, result.stderr)
            report = json.loads((evidence / "visible-slice-evidence.json").read_text())
            self.assertEqual(report["status"], "passed")
            self.assertTrue(report["commands"][0]["logHash"].startswith("sha256:"))

    def test_complete_e_and_f_named_capture_contract_passes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            fixture = GraphicalFixture(root)
            evidence = root / "evidence"
            result = self.run_runner(
                evidence, fixture.e_path, fixture.f_path, fixture.style_path,
                graphical=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            report = json.loads((evidence / "visible-slice-evidence.json").read_text())
            self.assertEqual(report["graphical"]["status"], "passed")
            self.assertEqual({item["name"] for item in report["graphical"]["captures"]},
                {"world-baseline", "e-open", "e-close", "e-reopened", "e-resized",
                 "f-open", "f-close", "style-e-open"})

    def test_incomplete_or_inconsistent_graphical_records_cannot_pass(self) -> None:
        mutations = {
            "one arbitrary capture": lambda value: value.e_records.__setitem__(
                slice(None), [dict(value.e_records[1], name="arbitrary")]),
            "capture without hash": lambda value: value.e_records[1].pop("sha256"),
            "single screenshot": lambda value: value.e_records[1].update(
                {"preOverlayPath": value.e_records[1]["filePath"],
                 "preOverlaySha256": value.e_records[1]["sha256"]}),
            "missing baseline": lambda value: value.e_records.__setitem__(
                slice(None), [item for item in value.e_records if item["name"] != "world-baseline"]),
            "missing E open": lambda value: value.e_records.__setitem__(
                slice(None), [item for item in value.e_records if item["name"] != "e-open"]),
            "missing E close": lambda value: value.e_records.__setitem__(
                slice(None), [item for item in value.e_records if item["name"] != "e-close"]),
            "missing F": lambda value: value.f_records.__setitem__(slice(None), []),
            "missing HUD region": lambda value: value.e_records[1].pop("hudRegion"),
            "fixed percentage ROI": lambda value: value.e_records[1].update(
                {"hudRegion": {"x": 6, "y": 15, "width": 8, "height": 4}}),
            "cross-frame baseline": lambda value: value.e_records[1].update(
                {"preOverlayFrameIndex": 999}),
            "zero ROI difference": lambda value: value.e_records[1].update(
                {"baselineDifferencePixelCount": 0, "nonBackgroundPixelCount": 0}),
            "missing RGB aggregate": lambda value: value.e_records[1].pop(
                "baselineRgbAbsoluteDifference"),
            "anchor beyond tolerance": lambda value: value.e_records[1]["layoutBounds"].update(
                {"x": 8.0}),
            "mismatched session": lambda value: value.e_records[1].update(
                {"sessionId": "stale-session"}),
            "mismatched launch": lambda value: value.e_records[1].update(
                {"canonicalLaunchHash": "sha256:" + "9" * 64}),
            "stale input artifact": lambda value: value.f_records[1].update(
                {"inputArtifactHash": value.e_input}),
            "duplicate capture name": lambda value: value.e_records.append(
                dict(value.e_records[1])),
            "semantic frame mismatch": lambda value: value.e_records[2].update(
                {"promptSemanticLabel": "[E] Open Door"}),
            "pixel frame mismatch": lambda value: value.e_records[2].update(
                {"priorSemanticDifferencePixelCount": 0}),
            "missing UI draw": lambda value: value.e_records[1].update(
                {"uiDrawCallCount": 0}),
        }
        for name, mutate in mutations.items():
            with self.subTest(name=name), tempfile.TemporaryDirectory() as temporary:
                root = pathlib.Path(temporary)
                fixture = GraphicalFixture(root)
                mutate(fixture)
                fixture.save()
                evidence = root / "evidence"
                result = self.run_runner(
                    evidence, fixture.e_path, fixture.f_path, fixture.style_path,
                    graphical=True)
                self.assertNotEqual(result.returncode, 0)
                report = json.loads((evidence / "visible-slice-evidence.json").read_text())
                self.assertIn(report["graphical"]["status"], {"incomplete", "failed"})
                self.assertNotEqual(report["graphical"]["status"], "passed")


if __name__ == "__main__":
    unittest.main()
