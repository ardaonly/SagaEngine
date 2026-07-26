# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

import hashlib
from pathlib import Path
import subprocess
import sys
import unittest


ROOT = Path(__file__).resolve().parents[3]
LICENSING_ROOT = ROOT / "Tools/Licensing"
if str(LICENSING_ROOT) not in sys.path:
    sys.path.insert(0, str(LICENSING_ROOT))

from licensing_policy import PolicyResolver, load_policy_context  # noqa: E402


FONT_ROOT = Path("Samples/StarterArena/Content/UI/ThirdParty/DejaVu")
FONT_SHA256 = "7da195a74c55bef988d0d48f9508bd5d849425c1770dba5d7bfc6ce9ed848954"
LICENSE_SHA256 = "f88d5294af5a772f9114eb385009e3478d62b21f3e5bbc34d159d328047c8867"
REQUIRED = ("DejaVuSans.ttf", "LICENSE", "PROVENANCE.json", "PROVENANCE.md", "SHA256SUMS")


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


class StarterArenaFontProvenanceTests(unittest.TestCase):
    def test_complete_resource_record_is_tracked_and_checksummed(self) -> None:
        for name in REQUIRED:
            relative = (FONT_ROOT / name).as_posix()
            with self.subTest(path=relative):
                result = subprocess.run(
                    ["git", "ls-files", "--error-unmatch", relative], cwd=ROOT,
                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
                self.assertEqual(result.returncode, 0, result.stderr.decode())
                self.assertTrue((ROOT / relative).is_file())
        sums = (ROOT / FONT_ROOT / "SHA256SUMS").read_text(encoding="utf-8").splitlines()
        expected = {line.split()[1]: line.split()[0] for line in sums}
        self.assertEqual(expected["DejaVuSans.ttf"], FONT_SHA256)
        self.assertEqual(expected["LICENSE"], LICENSE_SHA256)
        self.assertEqual(digest(ROOT / FONT_ROOT / "DejaVuSans.ttf"), FONT_SHA256)
        self.assertEqual(digest(ROOT / FONT_ROOT / "LICENSE"), LICENSE_SHA256)

    def test_provenance_is_complete_and_policy_prevents_saga_relicensing(self) -> None:
        provenance = (ROOT / FONT_ROOT / "PROVENANCE.md").read_text(encoding="utf-8")
        for value in ("DejaVu Fonts", "2.37", "version_2_37", "ttf/DejaVuSans.ttf",
                      "Bitstream Vera", FONT_SHA256, "unmodified", "2026-07-22"):
            self.assertIn(value, provenance)
        resolver = PolicyResolver(load_policy_context(ROOT))
        for name in REQUIRED:
            relative = (FONT_ROOT / name).as_posix()
            with self.subTest(path=relative):
                decision = resolver.decide(relative)
                self.assertTrue(decision.resolution.resolved)
                self.assertEqual(decision.resolution.expression, "UPSTREAM")
                self.assertEqual(decision.resolution.source, "bounded-upstream-license")
                self.assertEqual(
                    decision.domain.raw["current_license_expression"], "Bitstream-Vera")
                self.assertFalse(decision.modification_allowed)


if __name__ == "__main__":
    unittest.main()
