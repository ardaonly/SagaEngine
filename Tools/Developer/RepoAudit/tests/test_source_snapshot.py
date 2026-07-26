# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import unittest


MODULE_PATH = Path(__file__).resolve().parents[1] / "source_snapshot.py"
SPEC = importlib.util.spec_from_file_location("source_snapshot", MODULE_PATH)
assert SPEC and SPEC.loader
SOURCE_SNAPSHOT = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = SOURCE_SNAPSHOT
SPEC.loader.exec_module(SOURCE_SNAPSHOT)


class SourceSnapshotTests(unittest.TestCase):
    def test_source_selection_keeps_provenance_and_excludes_payloads(self) -> None:
        selected = SOURCE_SNAPSHOT.selected_source_paths([
            "Engine/Source/runtime.cpp",
            "Tools/tool.py",
            "Tests/test.cpp",
            "cmake/modules/Saga.cmake",
            "Samples/StarterArena/Content/UI/ThirdParty/DejaVu/DejaVuSans.ttf",
            "Samples/StarterArena/Content/UI/ThirdParty/DejaVu/LICENSE",
            "Samples/StarterArena/Content/UI/ThirdParty/DejaVu/PROVENANCE.md",
            "Samples/StarterArena/Content/UI/ThirdParty/DejaVu/SHA256SUMS",
            "SagaWiki/library/evidence/testing.md",
            "SagaWiki/index.html",
            ".github/MAINTENANCE.md",
            "scripts/wiki",
            "Vendor/library.cpp",
            "build/generated.cpp",
            ".saga-private/evidence/frame.png",
        ])
        self.assertIn("Samples/StarterArena/Content/UI/ThirdParty/DejaVu/LICENSE", selected)
        self.assertIn("Samples/StarterArena/Content/UI/ThirdParty/DejaVu/PROVENANCE.md", selected)
        self.assertIn("Samples/StarterArena/Content/UI/ThirdParty/DejaVu/SHA256SUMS", selected)
        self.assertIn(".github/MAINTENANCE.md", selected)
        self.assertNotIn("Samples/StarterArena/Content/UI/ThirdParty/DejaVu/DejaVuSans.ttf", selected)
        self.assertNotIn("SagaWiki/index.html", selected)
        self.assertFalse(any(path.startswith(("Vendor/", "build/", ".saga-private/"))
                             for path in selected))


if __name__ == "__main__":
    unittest.main()
