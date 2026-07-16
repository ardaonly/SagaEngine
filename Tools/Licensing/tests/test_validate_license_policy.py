# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest


MODULE_PATH = Path(__file__).resolve().parents[1] / "validate_license_policy.py"
sys.path.insert(0, str(MODULE_PATH.parent))
SPEC = importlib.util.spec_from_file_location("validate_license_policy", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
validator = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = validator
SPEC.loader.exec_module(validator)


class ComposedObjectSourceTests(unittest.TestCase):
    def test_recovers_runtime_owner_from_unix_object_path(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "Engine/Source/Runtime/Core/Private/Core.cpp"
            source.parent.mkdir(parents=True)
            source.write_text("// source\n", encoding="utf-8")

            result = validator.composed_object_source(
                root,
                "CMakeFiles/SagaCoreModule.dir/"
                "Engine/Source/Runtime/Core/Private/Core.cpp.o",
            )

            self.assertEqual(
                result, "Engine/Source/Runtime/Core/Private/Core.cpp"
            )

    def test_recovers_editor_owner_from_windows_object_path(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "Engine/Source/Editor/EditorCore/Private/Editor.cpp"
            source.parent.mkdir(parents=True)
            source.write_text("// source\n", encoding="utf-8")

            result = validator.composed_object_source(
                root,
                "CMakeFiles/SagaEditorCoreModule.dir/"
                "Engine/Source/Editor/EditorCore/Private/Editor.cpp.obj",
            )

            self.assertEqual(
                result, "Engine/Source/Editor/EditorCore/Private/Editor.cpp"
            )

    def test_rejects_missing_unsafe_and_non_object_sources(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.assertIsNone(
                validator.composed_object_source(
                    root, "CMakeFiles/Owner.dir/../../outside.cpp.o"
                )
            )
            self.assertIsNone(
                validator.composed_object_source(
                    root, "CMakeFiles/Owner.dir/Engine/Missing.cpp.o"
                )
            )
            self.assertIsNone(
                validator.composed_object_source(
                    root, "CMakeFiles/Owner.dir/Engine/Source.cpp"
                )
            )


if __name__ == "__main__":
    unittest.main()
