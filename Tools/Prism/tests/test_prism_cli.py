#!/usr/bin/env python3
"""
Tests for Prism's unified CLI wrapper.
"""

from __future__ import annotations

import contextlib
import io
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import prism


class PrismCliTests(unittest.TestCase):
    def test_empty_invocation_prints_help(self) -> None:
        out = io.StringIO()

        with contextlib.redirect_stdout(out):
            rc = prism.main([])

        self.assertEqual(rc, 0)
        self.assertIn("Unified CLI", out.getvalue())

    def test_extract_passes_arguments_to_extractor(self) -> None:
        calls: list[list[str]] = []

        with patch.object(prism, "_tool_path", return_value="/tmp/prism-extract"):
            with patch.object(prism, "_run", side_effect=lambda cmd: calls.append(cmd) or 0):
                rc = prism.main(["extract", "--help"])

        self.assertEqual(rc, 0)
        self.assertEqual(calls, [["/tmp/prism-extract", "--help"]])

    def test_graph_passes_arguments_to_graph_builder(self) -> None:
        calls: list[list[str]] = []

        with patch.object(prism, "_tool_path", return_value="/tmp/prism-graph"):
            with patch.object(prism, "_run", side_effect=lambda cmd: calls.append(cmd) or 0):
                rc = prism.main(["graph", "raw.json", "--no-txt"])

        self.assertEqual(rc, 0)
        self.assertEqual(calls, [["/tmp/prism-graph", "raw.json", "--no-txt"]])

    def test_run_uses_low_friction_defaults(self) -> None:
        calls: list[list[str]] = []

        def fake_tool_path(path: Path, name: str) -> str:
            return f"/tmp/{name}"

        with patch.object(prism, "_tool_path", side_effect=fake_tool_path):
            with patch.object(prism, "_run", side_effect=lambda cmd: calls.append(cmd) or 0):
                rc = prism.main(["run"])

        self.assertEqual(rc, 0)
        self.assertEqual(
            calls,
            [
                [
                    "/tmp/prism-extract",
                    "--repo-root", ".",
                    "-p", "build",
                    "-o", "prism.raw.json",
                ],
                ["/tmp/prism-graph", "prism.raw.json"],
            ],
        )

    def test_run_expands_sources_from_compile_database(self) -> None:
        calls: list[list[str]] = []

        def fake_tool_path(path: Path, name: str) -> str:
            return f"/tmp/{name}"

        with tempfile.TemporaryDirectory() as tmp:
            build_dir = Path(tmp) / "build"
            build_dir.mkdir()
            (build_dir / "compile_commands.json").write_text(
                json.dumps([
                    {"file": "/repo/src/A.cpp"},
                    {"file": "/repo/src/B.cpp"},
                    {"file": "/repo/src/A.cpp"},
                ]),
                encoding="utf-8",
            )

            with patch.object(prism, "_tool_path", side_effect=fake_tool_path):
                with patch.object(prism, "_run", side_effect=lambda cmd: calls.append(cmd) or 0):
                    rc = prism.main(["run", "-p", str(build_dir)])

        self.assertEqual(rc, 0)
        self.assertEqual(calls[0][-2:], ["/repo/src/A.cpp", "/repo/src/B.cpp"])

    def test_run_forwards_depth_options_to_the_right_stage(self) -> None:
        calls: list[list[str]] = []

        def fake_tool_path(path: Path, name: str) -> str:
            return f"/tmp/{name}"

        with patch.object(prism, "_tool_path", side_effect=fake_tool_path):
            with patch.object(prism, "_run", side_effect=lambda cmd: calls.append(cmd) or 0):
                rc = prism.main([
                    "run",
                    "--repo-root", "/repo",
                    "-p", "/repo/build",
                    "-o", "out/raw.json",
                    "--out-dir", "out",
                    "--json-name", "ai.graph.json",
                    "--txt-name", "ai.txt",
                    "--no-txt",
                    "--verbose",
                    "--extra-arg=-DSAGA_PRISM=1",
                    "Engine/Renderer/RenderGraph.cpp",
                ])

        self.assertEqual(rc, 0)
        self.assertEqual(
            calls,
            [
                [
                    "/tmp/prism-extract",
                    "--repo-root", "/repo",
                    "-p", "/repo/build",
                    "-o", "out/raw.json",
                    "--extra-arg", "-DSAGA_PRISM=1",
                    "Engine/Renderer/RenderGraph.cpp",
                ],
                [
                    "/tmp/prism-graph",
                    "out/raw.json",
                    "--out-dir", "out",
                    "--json-name", "ai.graph.json",
                    "--txt-name", "ai.txt",
                    "--no-txt",
                    "--verbose",
                ],
            ],
        )

    def test_run_stops_when_extractor_fails(self) -> None:
        calls: list[list[str]] = []

        def fake_tool_path(path: Path, name: str) -> str:
            return f"/tmp/{name}"

        def fake_run(cmd: list[str]) -> int:
            calls.append(cmd)
            return 2

        with patch.object(prism, "_tool_path", side_effect=fake_tool_path):
            with patch.object(prism, "_run", side_effect=fake_run):
                rc = prism.main(["run"])

        self.assertEqual(rc, 2)
        self.assertEqual(len(calls), 1)
        self.assertEqual(calls[0][0], "/tmp/prism-extract")


if __name__ == "__main__":
    unittest.main()
