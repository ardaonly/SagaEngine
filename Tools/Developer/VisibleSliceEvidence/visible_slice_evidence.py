# SPDX-License-Identifier: MPL-2.0
"""Serial visible-slice evidence orchestration; it complements, never replaces, CTest."""

from __future__ import annotations

import argparse
import collections
import datetime as dt
import hashlib
import json
import math
import os
import pathlib
import platform
import subprocess
import time
from typing import Any


MIN_BASELINE_DIFFERENCE_PIXELS = 32
MIN_SEMANTIC_DIFFERENCE_PIXELS = 16
MIN_BASELINE_RGB_ABSOLUTE_DIFFERENCE = 2048
MIN_SEMANTIC_RGB_ABSOLUTE_DIFFERENCE = 1024
MAX_ANCHOR_ERROR_PIXELS = 2.0
REQUIRED_CAPTURE_NAMES = ("world-baseline", "e-open", "e-close", "e-resized", "f-open")


def sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return "sha256:" + digest.hexdigest()


def atomic_json(path: pathlib.Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + f".{os.getpid()}.tmp")
    temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    os.replace(temporary, path)


def defaults(build_root: pathlib.Path) -> list[list[str]]:
    binary = build_root / "bin"
    return [
        [str(binary / "SagaRuntimeVisibleSliceTests"), "--gtest_brief=1"],
        [str(binary / "SagaUIModuleTests"),
         "--gtest_filter=RmlUiRenderInterfaceTests.*", "--gtest_brief=1"],
        [str(binary / "SagaVisualBlocksEditorModuleTests"),
         "--gtest_filter=ReadOnlyScriptProjectionTests.*", "--gtest_brief=1"],
        [str(binary / "SagaEditorAuthoringModuleTests"),
         "--gtest_filter=ScriptProjectionSessionTests.*", "--gtest_brief=1"],
        [str(binary / "SagaEditorAuthoringModuleTests"),
         "--gtest_filter=PlayPreparationPipelineTests.WorkspaceAndSavedSnapshotPublishIdenticalCanonicalArtifacts:PlayPreparationPipelineTests.InputDisplayLabelChangeLeavesUnrelatedArtifactsStable",
         "--gtest_brief=1"],
        [str(binary / "SagaEditorCoreModuleTests"),
         "--gtest_filter=DocumentRecoveryTests.*", "--gtest_brief=1"],
    ]


def parse_command(value: str) -> list[str]:
    command = json.loads(value)
    if not isinstance(command, list) or not command or not all(isinstance(item, str) for item in command):
        raise argparse.ArgumentTypeError("--command-json must be a non-empty JSON string array")
    return command


def read_ppm(path: pathlib.Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    cursor = 0

    def token() -> bytes:
        nonlocal cursor
        while cursor < len(data):
            if data[cursor] == ord("#"):
                cursor = data.find(b"\n", cursor)
                if cursor < 0:
                    raise ValueError("unterminated PPM comment")
            elif chr(data[cursor]).isspace():
                cursor += 1
            else:
                break
        start = cursor
        while cursor < len(data) and not chr(data[cursor]).isspace():
            cursor += 1
        if start == cursor:
            raise ValueError("missing PPM token")
        return data[start:cursor]

    if token() != b"P6":
        raise ValueError("capture is not binary PPM")
    width = int(token())
    height = int(token())
    if int(token()) != 255 or width <= 0 or height <= 0:
        raise ValueError("capture has invalid PPM dimensions or range")
    if cursor >= len(data) or not chr(data[cursor]).isspace():
        raise ValueError("capture has no PPM pixel separator")
    cursor += 1
    pixels = data[cursor:]
    if len(pixels) != width * height * 3:
        raise ValueError("capture PPM payload size is invalid")
    return width, height, pixels


def physical_region(bounds: Any, width: int, height: int) -> dict[str, int]:
    if not isinstance(bounds, dict) or set(bounds) != {"x", "y", "width", "height"}:
        raise ValueError("layout bounds are missing or malformed")
    values = [bounds[key] for key in ("x", "y", "width", "height")]
    if any(not isinstance(value, (int, float)) or isinstance(value, bool) or
           not math.isfinite(value) for value in values):
        raise ValueError("layout bounds are non-finite")
    x, y, box_width, box_height = (float(value) for value in values)
    right, bottom = x + box_width, y + box_height
    if box_width <= 0 or box_height <= 0 or x < 0 or y < 0 or right > width or bottom > height:
        raise ValueError("layout bounds are empty or outside the framebuffer")
    left_pixel, top_pixel = math.floor(x), math.floor(y)
    right_pixel, bottom_pixel = min(width, math.ceil(right)), min(height, math.ceil(bottom))
    if right_pixel <= left_pixel or bottom_pixel <= top_pixel:
        raise ValueError("layout bounds produce an empty physical ROI")
    return {"x": left_pixel, "y": top_pixel,
            "width": right_pixel - left_pixel, "height": bottom_pixel - top_pixel}


def pixel(pixels: bytes, width: int, x: int, y: int) -> bytes:
    offset = (y * width + x) * 3
    return pixels[offset:offset + 3]


def non_background_count(frame: tuple[int, int, bytes], region: dict[str, int]) -> int:
    width, _, pixels = frame
    colors = collections.Counter(
        pixel(pixels, width, x, y)
        for y in range(region["y"], region["y"] + region["height"])
        for x in range(region["x"], region["x"] + region["width"])
    )
    return region["width"] * region["height"] - max(colors.values())


def union_region(left: dict[str, int], right: dict[str, int]) -> dict[str, int]:
    x, y = min(left["x"], right["x"]), min(left["y"], right["y"])
    right_edge = max(left["x"] + left["width"], right["x"] + right["width"])
    bottom_edge = max(left["y"] + left["height"], right["y"] + right["height"])
    return {"x": x, "y": y, "width": right_edge - x, "height": bottom_edge - y}


def difference(current: tuple[int, int, bytes], reference: tuple[int, int, bytes],
               region: dict[str, int]) -> tuple[int, int]:
    current_width, current_height, current_pixels = current
    reference_width, reference_height, reference_pixels = reference
    if (current_width, current_height) != (reference_width, reference_height):
        raise ValueError("compared captures have different framebuffer dimensions")
    changed = 0
    rgb_absolute = 0
    for y in range(region["y"], region["y"] + region["height"]):
        for x in range(region["x"], region["x"] + region["width"]):
            current_rgb = pixel(current_pixels, current_width, x, y)
            reference_rgb = pixel(reference_pixels, reference_width, x, y)
            changed += current_rgb != reference_rgb
            rgb_absolute += sum(abs(left - right)
                                for left, right in zip(current_rgb, reference_rgb))
    return changed, rgb_absolute


def load_runtime(path: pathlib.Path, repo_root: pathlib.Path) -> dict[str, Any]:
    resolved = path if path.is_absolute() else repo_root / path
    runtime = json.loads(resolved.read_text(encoding="utf-8"))
    if not isinstance(runtime, dict):
        raise ValueError("runtime evidence must be a JSON object")
    runtime["sourcePath"] = str(resolved)
    runtime["sourceHash"] = sha256(resolved)
    return runtime


def capture_path(record: dict[str, Any], repo_root: pathlib.Path) -> pathlib.Path:
    path = pathlib.Path(str(record.get("filePath", "")))
    return path if path.is_absolute() else repo_root / path


def validate_runtime_captures(runtime: dict[str, Any], repo_root: pathlib.Path,
                              errors: list[str]) -> dict[str, dict[str, Any]]:
    observed = runtime.get("observedState", {})
    records = observed.get("evidenceCaptures", []) if isinstance(observed, dict) else []
    if not isinstance(records, list):
        errors.append("evidenceCaptures is not an array")
        return {}
    by_name: dict[str, dict[str, Any]] = {}
    frames: dict[str, tuple[int, int, bytes]] = {}
    pre_frames: dict[str, tuple[int, int, bytes]] = {}
    regions: dict[str, dict[str, int]] = {}
    frame_indices: dict[int, str] = {}
    for raw in records:
        if not isinstance(raw, dict):
            errors.append("capture record is not an object")
            continue
        name = raw.get("name")
        if not isinstance(name, str) or not name:
            errors.append("unnamed capture record")
            continue
        if name in by_name:
            errors.append(f"duplicate capture name: {name}")
            continue
        frame_index = raw.get("frameIndex")
        if not isinstance(frame_index, int) or frame_index < 0:
            errors.append(f"{name}: missing frame identity")
        elif frame_index in frame_indices and name != "world-baseline" and \
                frame_indices[frame_index] != "world-baseline":
            errors.append(f"{name}: duplicate frame identity")
        else:
            frame_indices[frame_index] = name
        path = capture_path(raw, repo_root)
        if not path.is_file():
            errors.append(f"{name}: capture file is missing")
            continue
        actual_hash = sha256(path)
        if raw.get("sha256") != actual_hash:
            errors.append(f"{name}: capture hash mismatch")
        try:
            frame = read_ppm(path)
        except (OSError, ValueError) as error:
            errors.append(f"{name}: {error}")
            continue
        width, height, _ = frame
        if raw.get("width") != width or raw.get("height") != height:
            errors.append(f"{name}: framebuffer dimensions do not match the capture")
        pixel_ratio = raw.get("pixelRatio")
        if not isinstance(pixel_ratio, (int, float)) or isinstance(pixel_ratio, bool) or \
                not math.isfinite(pixel_ratio) or pixel_ratio <= 0:
            errors.append(f"{name}: pixel ratio is missing or invalid")
        try:
            expected_region = physical_region(raw.get("layoutBounds"), width, height)
        except ValueError as error:
            errors.append(f"{name}: {error}")
            continue
        if raw.get("hudRegion") != expected_region:
            errors.append(f"{name}: ROI does not match floor/ceil layout bounds")
        measured_non_background = non_background_count(frame, expected_region)
        if raw.get("nonBackgroundPixelCount") != measured_non_background:
            errors.append(f"{name}: non-background pixel count mismatch")
        pre_path_value = pathlib.Path(str(raw.get("preOverlayPath", "")))
        pre_path = pre_path_value if pre_path_value.is_absolute() else repo_root / pre_path_value
        if not pre_path.is_file():
            errors.append(f"{name}: pre-overlay capture file is missing")
            continue
        if raw.get("preOverlaySha256") != sha256(pre_path):
            errors.append(f"{name}: pre-overlay capture hash mismatch")
        try:
            pre_frame = read_ppm(pre_path)
        except (OSError, ValueError) as error:
            errors.append(f"{name}: pre-overlay {error}")
            continue
        if pre_frame[:2] != frame[:2]:
            errors.append(f"{name}: pre/post framebuffer dimensions differ")
        if raw.get("preOverlayFrameIndex") != frame_index:
            errors.append(f"{name}: pre/post frame identity mismatch")
        if name != "world-baseline" and pre_path.resolve() == path.resolve():
            errors.append(f"{name}: one screenshot cannot prove pre/post overlay")
        by_name[name] = raw
        frames[name] = frame
        pre_frames[name] = pre_frame
        regions[name] = expected_region

    if not by_name:
        return {}
    reference = next(iter(by_name.values()))
    expected_session = runtime.get("playSessionId")
    expected_launch = runtime.get("canonicalLaunchManifestHash")
    identity_fields = ("inputArtifactHash", "uiArtifactHash", "sceneHash",
                       "scriptAssemblyHash", "packageGenerationHash")
    for name, record in by_name.items():
        if record.get("sessionId") != expected_session or not expected_session:
            errors.append(f"{name}: session identity mismatch")
        if record.get("canonicalLaunchHash") != expected_launch or not expected_launch:
            errors.append(f"{name}: canonical launch hash mismatch")
        for field in identity_fields:
            if not record.get(field) or record.get(field) != reference.get(field):
                errors.append(f"{name}: {field} mismatch within runtime run")
    for name, record in by_name.items():
        if name == "world-baseline":
            measured = (0, 0)
        else:
            try:
                measured = difference(frames[name], pre_frames[name], regions[name])
            except ValueError as error:
                errors.append(f"{name}: {error}")
                continue
        if record.get("baselineDifferencePixelCount") != measured[0]:
            errors.append(f"{name}: pre/post changed-pixel mismatch")
        if record.get("baselineRgbAbsoluteDifference") != measured[1]:
            errors.append(f"{name}: pre/post RGB aggregate mismatch")
    prior_frames = {
        "e-close": "e-open", "e-reopened": "e-close",
        "f-close": "f-open", "f-reopened": "f-close",
    }
    for name, prior_name in prior_frames.items():
        if name not in frames or prior_name not in frames:
            continue
        try:
            measured = difference(
                frames[name], frames[prior_name],
                union_region(regions[name], regions[prior_name]))
        except ValueError as error:
            errors.append(f"{name}: {error}")
            continue
        if by_name[name].get("priorSemanticDifferencePixelCount") != measured[0]:
            errors.append(f"{name}: prior semantic-frame changed-pixel mismatch")
        if by_name[name].get("priorSemanticRgbAbsoluteDifference") != measured[1]:
            errors.append(f"{name}: prior semantic-frame RGB aggregate mismatch")

    baseline = by_name.get("world-baseline")
    e_open = by_name.get("e-open")
    f_open = by_name.get("f-open")
    first_open = e_open or f_open
    if baseline and first_open:
        if (baseline.get("filePath") != first_open.get("preOverlayPath") or
                baseline.get("sha256") != first_open.get("preOverlaySha256") or
                baseline.get("frameIndex") != first_open.get("frameIndex")):
            errors.append("world-baseline is not the first open frame's pre-overlay capture")
    return by_name


def validate_semantics(records: dict[str, dict[str, Any]], errors: list[str]) -> None:
    expected = {
        "e-open": ("E", "[E] Open Door", False),
        "e-close": ("E", "[E] Close Door", True),
        "e-reopened": ("E", "[E] Open Door", False),
        "e-resized": ("E", "[E] Open Door", False),
        "f-open": ("F", "[F] Open Door", False),
        "f-close": ("F", "[F] Close Door", True),
    }
    for name, values in expected.items():
        if name not in records:
            continue
        label, prompt, state = values
        record = records[name]
        if (record.get("promptBindingLabel"), record.get("promptSemanticLabel"),
                record.get("doorState")) != (label, prompt, state):
            errors.append(f"{name}: semantic data does not match the named frame")
        if record.get("uiDrawCallCount", 0) <= 0:
            errors.append(f"{name}: no UI draw call is bound to the semantic frame")


def graphical_evidence(runtimes: list[dict[str, Any]], repo_root: pathlib.Path,
                       requested: bool) -> dict[str, Any]:
    thresholds = {
        "baselineDifferencePixels": MIN_BASELINE_DIFFERENCE_PIXELS,
        "semanticDifferencePixels": MIN_SEMANTIC_DIFFERENCE_PIXELS,
        "baselineRgbAbsoluteDifference": MIN_BASELINE_RGB_ABSOLUTE_DIFFERENCE,
        "semanticRgbAbsoluteDifference": MIN_SEMANTIC_RGB_ABSOLUTE_DIFFERENCE,
        "maximumAnchorErrorPixels": MAX_ANCHOR_ERROR_PIXELS}
    if not requested:
        return {"status": "unexecuted", "captures": [], "diagnostics": [],
                "thresholds": thresholds}
    if not runtimes:
        return {"status": "incomplete", "captures": [],
                "diagnostics": ["no runtime evidence was supplied"], "thresholds": thresholds}

    errors: list[str] = []
    runs = [(runtime, validate_runtime_captures(runtime, repo_root, errors))
            for runtime in runtimes]
    e_run = next(((runtime, records) for runtime, records in runs
                  if "e-open" in records and
                  "style" not in str(runtime.get("playSessionId", "")).lower()), None)
    f_run = next(((runtime, records) for runtime, records in runs if "f-open" in records), None)
    style_run = next(((runtime, records) for runtime, records in runs
                      if "e-open" in records and
                      "style" in str(runtime.get("playSessionId", "")).lower()), None)
    selected: dict[str, dict[str, Any]] = {}
    if e_run:
        selected.update({name: e_run[1][name] for name in
                         ("world-baseline", "e-open", "e-close", "e-reopened", "e-resized")
                         if name in e_run[1]})
    if f_run:
        selected.update({name: f_run[1][name] for name in ("f-open", "f-close")
                         if name in f_run[1]})
    if style_run and "e-open" in style_run[1]:
        selected["style-e-open"] = dict(style_run[1]["e-open"], name="style-e-open")
    missing = [name for name in REQUIRED_CAPTURE_NAMES if name not in selected]
    if missing:
        errors.append("missing required named captures: " + ", ".join(missing))
    validate_semantics(selected, errors)

    if e_run:
        e_records = e_run[1]
        for name in ("e-open", "e-close", "e-reopened", "e-resized"):
            record = e_records.get(name, {})
            if record.get("baselineDifferencePixelCount", 0) < MIN_BASELINE_DIFFERENCE_PIXELS:
                errors.append(f"{name}: pre/post changed-pixel threshold was not met")
            if record.get("baselineRgbAbsoluteDifference", 0) < \
                    MIN_BASELINE_RGB_ABSOLUTE_DIFFERENCE:
                errors.append(f"{name}: pre/post RGB aggregate threshold was not met")
        for name in ("e-close", "e-reopened"):
            record = e_records.get(name, {})
            if record.get("priorSemanticDifferencePixelCount", 0) < \
                    MIN_SEMANTIC_DIFFERENCE_PIXELS:
                errors.append(f"{name}: semantic changed-pixel threshold was not met")
            if record.get("priorSemanticRgbAbsoluteDifference", 0) < \
                    MIN_SEMANTIC_RGB_ABSOLUTE_DIFFERENCE:
                errors.append(f"{name}: semantic RGB aggregate threshold was not met")
        baseline = e_records.get("e-open", {})
        resized = e_records.get("e-resized", {})
        if resized.get("width") == baseline.get("width") and resized.get("height") == baseline.get("height"):
            errors.append("e-resized: framebuffer size did not change")
        for name in ("e-open", "e-close", "e-reopened", "e-resized"):
            record = e_records.get(name, {})
            bounds = record.get("layoutBounds", {})
            if isinstance(bounds, dict):
                center = float(bounds.get("x", math.inf)) + \
                    float(bounds.get("width", math.inf)) * 0.5
                if abs(center - float(record.get("width", 0)) * 0.5) > \
                        MAX_ANCHOR_ERROR_PIXELS:
                    errors.append(f"{name}: horizontal anchor exceeds 2 physical pixels")
        if baseline and resized:
            baseline_bounds = baseline.get("layoutBounds", {})
            resized_bounds = resized.get("layoutBounds", {})
            if isinstance(baseline_bounds, dict) and isinstance(resized_bounds, dict):
                baseline_gap = baseline["height"] - (
                    baseline_bounds["y"] + baseline_bounds["height"])
                resized_gap = resized["height"] - (
                    resized_bounds["y"] + resized_bounds["height"])
                expected_gap = baseline_gap * resized["height"] / baseline["height"]
                if abs(resized_gap - expected_gap) > MAX_ANCHOR_ERROR_PIXELS:
                    errors.append("e-resized: bottom anchor exceeds 2 physical pixels")
    if f_run:
        f_open = f_run[1].get("f-open", {})
        if f_open.get("baselineDifferencePixelCount", 0) < MIN_BASELINE_DIFFERENCE_PIXELS:
            errors.append("f-open: pre/post changed-pixel threshold was not met")
        if f_open.get("baselineRgbAbsoluteDifference", 0) < \
                MIN_BASELINE_RGB_ABSOLUTE_DIFFERENCE:
            errors.append("f-open: pre/post RGB aggregate threshold was not met")
    if e_run and f_run:
        e_open, f_open = e_run[1]["e-open"], f_run[1]["f-open"]
        for field in ("uiArtifactHash", "sceneHash", "scriptAssemblyHash", "packageGenerationHash"):
            if e_open.get(field) != f_open.get(field):
                errors.append(f"E/F invariant changed unexpectedly: {field}")
        for field in ("inputArtifactHash", "canonicalLaunchHash"):
            if e_open.get(field) == f_open.get(field):
                errors.append(f"E/F variant did not change: {field}")
        if "[E]" in str(f_open.get("promptSemanticLabel", "")):
            errors.append("f-open contains stale E semantic evidence")
    if not style_run:
        errors.append("missing required RCSS style-authority runtime")
    if e_run and style_run:
        e_open, style_open = e_run[1]["e-open"], style_run[1]["e-open"]
        for field in ("inputArtifactHash", "sceneHash", "scriptAssemblyHash",
                      "packageGenerationHash", "uiDocumentHash", "preOverlaySha256"):
            if e_open.get(field) != style_open.get(field):
                errors.append(f"RCSS invariant changed unexpectedly: {field}")
        for field in ("canonicalLaunchHash", "uiArtifactHash", "uiStylesheetHashes",
                      "sha256"):
            if e_open.get(field) == style_open.get(field):
                errors.append(f"RCSS authority variant did not change: {field}")

    supplied_but_incomplete = bool(missing) and not any(
        error for error in errors if "mismatch" in error or "stale" in error or "duplicate" in error)
    status = "incomplete" if supplied_but_incomplete else "failed" if errors else "passed"
    return {"status": status, "captures": list(selected.values()),
            "diagnostics": errors, "thresholds": thresholds,
            "runtimeSources": [runtime.get("sourcePath") for runtime in runtimes]}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=pathlib.Path, default=pathlib.Path.cwd())
    parser.add_argument("--build-root", type=pathlib.Path,
                        default=pathlib.Path("build/RelWithDebInfo-0.0.11"))
    parser.add_argument("--evidence-root", type=pathlib.Path,
                        default=pathlib.Path(".saga-private/evidence/visible-slice"))
    parser.add_argument("--command-json", action="append", type=parse_command, default=[])
    parser.add_argument("--artifact", action="append", type=pathlib.Path, default=[])
    parser.add_argument("--runtime-evidence", action="append", type=pathlib.Path, default=[])
    parser.add_argument("--graphical", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()
    build_root = args.build_root if args.build_root.is_absolute() else repo_root / args.build_root
    evidence_root = args.evidence_root if args.evidence_root.is_absolute() else repo_root / args.evidence_root
    commands = args.command_json or defaults(build_root)
    started = dt.datetime.now(dt.timezone.utc)
    records: list[dict[str, Any]] = []
    overall = 0
    for index, command in enumerate(commands):
        record: dict[str, Any] = {"command": command, "index": index}
        executable = pathlib.Path(command[0])
        if executable.is_file():
            record["executable"] = {"path": str(executable), "hash": sha256(executable)}
        if args.dry_run:
            record.update({"durationMs": 0, "exitCode": None, "status": "unexecuted"})
            records.append(record)
            continue
        command_start = time.monotonic()
        try:
            completed = subprocess.run(command, cwd=repo_root, text=True,
                                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                       check=False, timeout=180)
            record["durationMs"] = round((time.monotonic() - command_start) * 1000)
            record["exitCode"] = completed.returncode
            record["status"] = "passed" if completed.returncode == 0 else "failed"
            log_path = evidence_root / "logs" / f"{index:02d}.log"
            log_path.parent.mkdir(parents=True, exist_ok=True)
            log_path.write_text(completed.stdout, encoding="utf-8")
            record["logPath"] = str(log_path)
            record["logHash"] = sha256(log_path)
            if completed.returncode != 0:
                overall = 1
        except (OSError, subprocess.TimeoutExpired) as error:
            record.update({"durationMs": round((time.monotonic() - command_start) * 1000),
                           "exitCode": None, "status": "failed", "diagnostic": str(error)})
            overall = 1
        records.append(record)

    runtimes: list[dict[str, Any]] = []
    runtime_errors: list[str] = []
    for path in args.runtime_evidence:
        try:
            runtimes.append(load_runtime(path, repo_root))
        except (OSError, ValueError, json.JSONDecodeError) as error:
            runtime_errors.append(f"{path}: {error}")
    graphical = graphical_evidence(runtimes, repo_root, args.graphical)
    graphical["diagnostics"] = runtime_errors + graphical["diagnostics"]
    if runtime_errors and args.graphical:
        graphical["status"] = "failed"
    if args.graphical and graphical["status"] != "passed":
        overall = 1
    artifacts = []
    for path in args.artifact:
        resolved = path if path.is_absolute() else repo_root / path
        artifacts.append({"path": str(resolved), "hash": sha256(resolved) if resolved.is_file() else None})
    report = {
        "schemaVersion": 2,
        "producer": "visible-slice-evidence",
        "scope": "visible-slice",
        "startedUtc": started.isoformat(),
        "finishedUtc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "repository": {"root": str(repo_root)},
        "environment": {"platform": platform.platform(), "python": platform.python_version(),
                        "backend": os.environ.get("SAGA_GRAPHICS_BACKEND", "unreported"),
                        "gpu": os.environ.get("SAGA_GPU_IDENTITY", "unreported"),
                        "driver": os.environ.get("SAGA_GPU_DRIVER", "unreported"),
                        "display": os.environ.get("DISPLAY", "")},
        "commands": records,
        "artifacts": artifacts,
        "runtimeRuns": runtimes,
        "graphical": graphical,
        "status": "unexecuted" if args.dry_run else "passed" if overall == 0 else "failed",
        "nonClaims": [
            "This serial runner does not replace the normal CTest graph.",
            "Graphical proof is absent unless graphical.status is passed.",
            "A focused run is not a full CTest result.",
        ],
    }
    report_path = evidence_root / "visible-slice-evidence.json"
    atomic_json(report_path, report)
    print(report_path)
    return overall


if __name__ == "__main__":
    raise SystemExit(main())
