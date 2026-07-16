#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Arda Koyuncu
# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest


MODULE_PATH = Path(__file__).resolve().parents[1] / "saga_repo_xray.py"
SPEC = importlib.util.spec_from_file_location("saga_repo_xray", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
XRAY = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = XRAY
SPEC.loader.exec_module(XRAY)


class RepoXrayFixtureTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary_directory = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary_directory.name)

    def tearDown(self) -> None:
        self.temporary_directory.cleanup()

    def write(self, relative: str, text: str) -> None:
        destination = self.root / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text(text, encoding="utf-8")

    def analyze(self):
        analyzer = XRAY.RepoXray(
            root=self.root,
            out=self.root / "Build/RepoXray",
            max_file_bytes=100_000,
            include_snippets=False,
            max_snippet_lines=10,
        )
        return analyzer.analyze()

    def test_current_owners_samples_and_wiki_are_classified(self) -> None:
        self.write(
            "Engine/Source/Runtime/Render/Private/Backend.h",
            '#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>\n',
        )
        self.write(
            "Engine/Source/Runtime/Render/Public/SagaEngine/Render/PublicApi.h",
            "#pragma once\n",
        )
        self.write(
            "Engine/Source/Editor/EditorCore/Public/SagaEditor/Core.h",
            "#pragma once\n",
        )
        self.write(
            "Engine/Source/Programs/SagaRuntime/Private/Main.cpp",
            "int main() { return 0; }\n",
        )
        self.write(
            "SagaWiki/library/start/repository-map.md",
            "# Repository map\n\nCurrent owner map.\n",
        )
        self.write("Samples/Demo/Demo.sagaproj", '{"schemaVersion": 0}\n')
        self.write("Samples/Demo/README.md", "# Demo\n")
        self.write("Samples/Demo/Scenes/demo.scene.json", "{}\n")

        data = self.analyze()
        categories = {record["path"]: record["category"] for record in data["files"]}
        findings = data["findings"]

        self.assertEqual(
            categories["Engine/Source/Runtime/Render/Private/Backend.h"],
            "runtime",
        )
        self.assertEqual(
            categories["Engine/Source/Editor/EditorCore/Public/SagaEditor/Core.h"],
            "editor",
        )
        self.assertEqual(
            categories["Engine/Source/Programs/SagaRuntime/Private/Main.cpp"],
            "programs",
        )
        self.assertEqual(categories["Samples/Demo/Demo.sagaproj"], "samples")
        self.assertNotIn(
            "samples-missing", {finding["category"] for finding in findings}
        )
        self.assertNotIn(
            "unusual-doc-area", {finding["category"] for finding in findings}
        )
        self.assertFalse(
            any(
                finding["category"] == "public-boundary-leak"
                and finding["path"].endswith("Private/Backend.h")
                for finding in findings
            )
        )

    def test_public_vendor_header_leak_is_reported(self) -> None:
        self.write(
            "Engine/Source/Runtime/RHI/Public/SagaEngine/RHI/LeakyApi.h",
            '#include <SDL2/SDL.h>\n#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>\n',
        )
        self.write("Samples/Demo/Demo.sagaproj", '{"schemaVersion": 0}\n')
        self.write("Samples/Demo/README.md", "# Demo\n")
        self.write("Samples/Demo/Scenes/demo.scene.json", "{}\n")

        findings = self.analyze()["findings"]
        public_leaks = [
            finding
            for finding in findings
            if finding["category"] == "public-boundary-leak"
        ]
        self.assertEqual(len(public_leaks), 2)
        self.assertTrue(
            all(finding["path"].endswith("Public/SagaEngine/RHI/LeakyApi.h")
                for finding in public_leaks)
        )

    def test_retired_ownership_roots_are_rejected(self) -> None:
        self.write("Apps/Legacy/Main.cpp", "int main() { return 0; }\n")
        self.write("Runtime/LegacyRuntime.h", "#pragma once\n")

        findings = self.analyze()["findings"]
        retired = {
            finding["path"]
            for finding in findings
            if finding["category"] == "retired-ownership-root"
        }
        self.assertEqual(retired, {"Apps", "Runtime"})

    def test_private_and_vendor_metadata_are_not_scanned(self) -> None:
        self.write(".saga-private/notes/plan.md", "# Local plan\n")
        self.write("Vendor/Library/include/vendor.h", "#pragma once\n")
        self.write("Engine/Source/Runtime/Core/Public/SagaEngine/Core.h", "#pragma once\n")

        paths = {record["path"] for record in self.analyze()["files"]}
        self.assertIn(
            "Engine/Source/Runtime/Core/Public/SagaEngine/Core.h", paths
        )
        self.assertFalse(any(path.startswith(".saga-private/") for path in paths))
        self.assertFalse(any(path.startswith("Vendor/") for path in paths))


class ClassificationTests(unittest.TestCase):
    def test_current_and_retired_paths_have_distinct_categories(self) -> None:
        self.assertEqual(
            XRAY.classify_top_level("Engine/Source/Developer/SagaDev/Bridge.cpp"),
            "developer",
        )
        self.assertEqual(
            XRAY.classify_top_level("Engine/Managed/RuntimeBridge/Bridge.cs"),
            "managed",
        )
        self.assertEqual(
            XRAY.classify_top_level("SagaWiki/library/runtime/runtime.md"),
            "docs",
        )
        self.assertEqual(
            XRAY.classify_top_level("Apps/Legacy/Main.cpp"),
            "retired:Apps",
        )


if __name__ == "__main__":
    unittest.main()
