# SPDX-License-Identifier: MPL-2.0
from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import unittest


MODULE_PATH = Path(__file__).resolve().parents[1] / "normalize_spdx.py"
SPEC = importlib.util.spec_from_file_location("normalize_spdx", MODULE_PATH)
assert SPEC and SPEC.loader
normalize_spdx = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = normalize_spdx
SPEC.loader.exec_module(normalize_spdx)


class NormalizeSpdxTests(unittest.TestCase):
    def test_python_shebang_and_mpl_are_canonical(self) -> None:
        source = "#!/usr/bin/env python3\n# SPDX-FileCopyrightText: 2026 Arda Koyuncu\nprint('ok')\n"
        result, changes = normalize_spdx.normalize_text(
            Path("tool.py"), source, "MPL-2.0", executable_python=True
        )
        self.assertEqual(
            result,
            "#!/usr/bin/env python3\n# SPDX-License-Identifier: MPL-2.0\nprint('ok')\n",
        )
        self.assertIn("removed-personal-copyright", changes)

    def test_import_only_python_loses_shebang(self) -> None:
        result, changes = normalize_spdx.normalize_text(
            Path("support.py"), "#!/usr/bin/env python3\nVALUE = 1\n", "MPL-2.0",
            executable_python=False,
        )
        self.assertEqual(result, "# SPDX-License-Identifier: MPL-2.0\nVALUE = 1\n")
        self.assertIn("removed-import-only-shebang", changes)

    def test_sample_identifier_is_replaced_with_mit(self) -> None:
        result, changes = normalize_spdx.normalize_text(
            Path("DoorController.cs"), "// SPDX-License-Identifier: MPL-2.0\nclass Door {}\n",
            "MIT", executable_python=False,
        )
        self.assertEqual(result, "// SPDX-License-Identifier: MIT\nclass Door {}\n")
        self.assertIn("replaced-license", changes)

    def test_crlf_and_idempotence_are_preserved(self) -> None:
        source = "// SPDX-License-Identifier: MPL-2.0\r\nint main() {}\r\n"
        first, changes = normalize_spdx.normalize_text(
            Path("main.cpp"), source, "MPL-2.0", executable_python=False
        )
        second, second_changes = normalize_spdx.normalize_text(
            Path("main.cpp"), first, "MPL-2.0", executable_python=False
        )
        self.assertEqual(first, source)
        self.assertEqual(second, first)
        self.assertEqual(changes, [])
        self.assertEqual(second_changes, [])

    def test_commentless_json_is_not_corrupted(self) -> None:
        source = '{"schemaVersion":1}\n'
        result, changes = normalize_spdx.normalize_text(
            Path("manifest.json"), source, "MPL-2.0", executable_python=False
        )
        self.assertEqual(result, source)
        self.assertEqual(changes, [])


if __name__ == "__main__":
    unittest.main()
