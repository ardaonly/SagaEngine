#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Configure, build, and run a CMake-owned SagaEngine test suite."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import time
from datetime import datetime, timezone
import xml.etree.ElementTree as ET


SCHEMA_VERSION = 1
DEFAULT_PRESET = "linux-gcc"
DEFAULT_CONFIGURATION = "RelWithDebInfo"
CATALOG_RELATIVE_PATH = Path("generated/SagaTestSuites.json")


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def run_command(command: list[str], cwd: Path, manifest: dict[str, object],
                env: dict[str, str] | None = None) -> int:
    started = time.monotonic()
    record: dict[str, object] = {
        "command": command,
        "workingDirectory": str(cwd),
        "startedAt": utc_now(),
    }
    manifest["commands"].append(record)  # type: ignore[union-attr]
    try:
        result = subprocess.run(command, cwd=cwd, env=env, check=False)
        code = result.returncode
    except OSError as error:
        print(f"unable to execute {command[0]}: {error}", file=sys.stderr)
        record["error"] = str(error)
        code = 127
    record["durationSeconds"] = round(time.monotonic() - started, 3)
    record["exitCode"] = code
    return code


def capture(command: list[str], cwd: Path) -> str | None:
    try:
        result = subprocess.run(
            command, cwd=cwd, text=True, stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL, check=False)
    except OSError:
        return None
    return result.stdout.strip() if result.returncode == 0 else None


def cmake_cache_value(build_dir: Path, key: str) -> str | None:
    cache = build_dir / "CMakeCache.txt"
    if not cache.is_file():
        return None
    prefix = key + ":"
    for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith(prefix):
            return line.partition("=")[2]
    return None


def compiler_identity(build_dir: Path) -> dict[str, str | None]:
    identity: dict[str, str | None] = {
        "id": None,
        "version": None,
        "path": cmake_cache_value(build_dir, "CMAKE_CXX_COMPILER"),
    }
    compiler_files = sorted(
        (build_dir / "CMakeFiles").glob("*/CMakeCXXCompiler.cmake"))
    if compiler_files:
        source = compiler_files[-1].read_text(encoding="utf-8", errors="replace")
        for field, variable in (("id", "ID"), ("version", "VERSION")):
            match = re.search(
                rf'^set\(CMAKE_CXX_COMPILER_{variable} "([^"]*)"\)',
                source, re.MULTILINE)
            if match:
                identity[field] = match.group(1)
    return identity


def load_catalog(build_dir: Path) -> dict[str, object]:
    path = build_dir / CATALOG_RELATIVE_PATH
    return json.loads(path.read_text(encoding="utf-8"))


def suite_map(catalog: dict[str, object]) -> dict[str, dict[str, object]]:
    return {item["name"]: item for item in catalog["suites"]}  # type: ignore[index]


def ctest_selection(build_dir: Path, suite_name: str,
                    manifest: dict[str, object]) -> tuple[list[str], str]:
    command = ["ctest", "--test-dir", str(build_dir), "--show-only=json-v1",
               "-L", f"^suite-{suite_name}$"]
    started = time.monotonic()
    record: dict[str, object] = {
        "command": command,
        "workingDirectory": str(build_dir),
        "startedAt": utc_now(),
    }
    manifest["commands"].append(record)  # type: ignore[union-attr]
    result = subprocess.run(
        command,
        text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
    record["durationSeconds"] = round(time.monotonic() - started, 3)
    record["exitCode"] = result.returncode
    if result.returncode != 0:
        raise RuntimeError(result.stderr.strip() or "CTest metadata query failed")
    payload = json.loads(result.stdout)
    names = [test["name"] for test in payload.get("tests", [])]
    digest = hashlib.sha256(
        json.dumps(payload, sort_keys=True, separators=(",", ":")).encode("utf-8")
    ).hexdigest()
    return names, digest


def junit_counts(path: Path) -> dict[str, int] | None:
    if not path.is_file():
        return None
    root = ET.parse(path).getroot()
    suites = [root] if root.tag == "testsuite" else list(root.findall("testsuite"))
    if not suites:
        return None
    total = sum(int(item.attrib.get("tests", "0")) for item in suites)
    failed = sum(
        int(item.attrib.get("failures", "0")) + int(item.attrib.get("errors", "0"))
        for item in suites)
    skipped = sum(int(item.attrib.get("skipped", "0")) for item in suites)
    return {"passed": total - failed - skipped, "failed": failed, "skipped": skipped}


class Xvfb:
    def __init__(self) -> None:
        self.process: subprocess.Popen[bytes] | None = None
        self.display: str | None = None

    def start(self) -> str | None:
        executable = shutil.which("Xvfb")
        if not executable:
            return None
        for number in range(91, 111):
            socket = Path(f"/tmp/.X11-unix/X{number}")
            if socket.exists():
                continue
            process = subprocess.Popen(
                [executable, f":{number}", "-screen", "0", "1280x720x24",
                 "-nolisten", "tcp"], stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL, start_new_session=True)
            for _ in range(50):
                if process.poll() is not None:
                    break
                if socket.exists():
                    self.process = process
                    self.display = f":{number}"
                    return self.display
                time.sleep(0.1)
            if process.poll() is None:
                os.killpg(process.pid, signal.SIGTERM)
                process.wait(timeout=5)
        return None

    def stop(self) -> None:
        if self.process and self.process.poll() is None:
            os.killpg(self.process.pid, signal.SIGTERM)
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(self.process.pid, signal.SIGKILL)
                self.process.wait(timeout=5)


def default_build_dir(root: Path) -> Path:
    version = (root / "VERSION").read_text(encoding="utf-8").strip()
    return root / "build" / f"RelWithDebInfo-{version}"


def configure_command(args: argparse.Namespace, build_dir: Path) -> list[str]:
    # C++ dependency scanning and sccache have produced missing depfile races in
    # focused parallel builds. Suites favor reproducibility over a host cache.
    return ["cmake", "--preset", args.preset, "-B", str(build_dir),
            "-DCMAKE_C_COMPILER_LAUNCHER=",
            "-DCMAKE_CXX_COMPILER_LAUNCHER="]


def write_manifest(path: Path, manifest: dict[str, object], started: float) -> None:
    manifest["endedAt"] = utc_now()
    manifest["durationSeconds"] = round(time.monotonic() - started, 3)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    temporary.replace(path)
    print(f"Execution manifest: {path}")


def parse_args(argv: list[str], root: Path) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("suite", nargs="?", help="CMake-owned suite name")
    parser.add_argument("--list", action="store_true", help="list configured suites")
    parser.add_argument("--preset", default=DEFAULT_PRESET)
    parser.add_argument("--build-dir", type=Path, default=default_build_dir(root))
    parser.add_argument("--config", default=DEFAULT_CONFIGURATION)
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--manifest", type=Path)
    args = parser.parse_args(argv)
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    if not args.list and not args.suite:
        parser.error("provide a suite name or --list")
    return args


def main(argv: list[str] | None = None) -> int:
    root = Path(__file__).resolve().parents[3]
    args = parse_args(sys.argv[1:] if argv is None else argv, root)
    build_dir = args.build_dir.resolve()

    if not (build_dir / CATALOG_RELATIVE_PATH).is_file():
        configure = configure_command(args, build_dir)
        result = subprocess.run(configure, cwd=root, check=False)
        if result.returncode != 0:
            return result.returncode

    catalog = load_catalog(build_dir)
    suites = suite_map(catalog)
    if args.list:
        for name, suite in suites.items():
            print(f"{name:14} {suite['description']} [{suite['environment']}]")
        return 0
    if args.suite not in suites:
        print(f"unknown suite: {args.suite}", file=sys.stderr)
        print("available: " + ", ".join(suites), file=sys.stderr)
        return 2

    suite = suites[args.suite]
    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    manifest_path = (args.manifest.resolve() if args.manifest else
        build_dir / "reports" / "test-suites" / f"{args.suite}-{timestamp}.json")
    started = time.monotonic()
    manifest: dict[str, object] = {
        "schemaVersion": SCHEMA_VERSION,
        "repositoryHead": capture(["git", "rev-parse", "HEAD"], root),
        "suite": args.suite,
        "preset": args.preset,
        "configureBuildDirectory": str(build_dir),
        "configuration": args.config,
        "startedAt": utc_now(),
        "requiredEnvironment": suite["environment"],
        "aggregateTarget": suite["aggregateTarget"],
        "buildTargets": suite["buildTargets"],
        "compiler": compiler_identity(build_dir),
        "cmakeVersion": capture(["cmake", "--version"], root),
        "commands": [],
        "status": "FAIL",
        "exitCode": 1,
    }
    display = Xvfb()
    try:
        configure = configure_command(args, build_dir)
        code = run_command(configure, root, manifest)
        if code:
            manifest["exitCode"] = code
            return code

        refreshed = suite_map(load_catalog(build_dir))
        suite = refreshed[args.suite]
        selected, digest = ctest_selection(build_dir, args.suite, manifest)
        manifest["selectedTests"] = selected
        manifest["selectedLabel"] = f"suite-{args.suite}"
        manifest["ctestGraphSha256"] = digest
        if not selected:
            print(f"suite {args.suite} selected no CTest tests", file=sys.stderr)
            manifest["exitCode"] = 3
            return 3

        environment = os.environ.copy()
        if args.suite == "product-xcb" and not environment.get("DISPLAY"):
            chosen = display.start()
            if not chosen:
                print("UNAVAILABLE: product-xcb requires DISPLAY or Xvfb", file=sys.stderr)
                manifest["status"] = "UNAVAILABLE"
                manifest["exitCode"] = 4
                manifest["unavailableReason"] = "No usable DISPLAY or Xvfb"
                return 4
            environment["DISPLAY"] = chosen
            manifest["display"] = chosen

        build = ["cmake", "--build", str(build_dir), "--config", args.config,
                 "--target", str(suite["aggregateTarget"]), "--parallel", str(args.jobs)]
        code = run_command(build, root, manifest, environment)
        if code:
            manifest["exitCode"] = code
            return code

        junit = manifest_path.with_suffix(".junit.xml")
        test = ["ctest", "--test-dir", str(build_dir), "-C", args.config,
                "--output-on-failure", "--no-tests=error", "--schedule-random",
                "-L", f"^suite-{args.suite}$", "--output-junit", str(junit)]
        if args.suite == "product-xcb":
            test.extend(["--parallel", "1"])
        else:
            test.extend(["--parallel", str(args.jobs)])
        code = run_command(test, root, manifest, environment)
        counts = junit_counts(junit)
        if counts is not None:
            manifest["counts"] = counts
        manifest["junitPath"] = str(junit)
        manifest["exitCode"] = code
        manifest["status"] = "PASS" if code == 0 else "FAIL"
        return code
    except (KeyError, ValueError, json.JSONDecodeError, RuntimeError) as error:
        print(f"suite runner failed: {error}", file=sys.stderr)
        manifest["error"] = str(error)
        manifest["exitCode"] = 2
        return 2
    finally:
        display.stop()
        write_manifest(manifest_path, manifest, started)


if __name__ == "__main__":
    raise SystemExit(main())
