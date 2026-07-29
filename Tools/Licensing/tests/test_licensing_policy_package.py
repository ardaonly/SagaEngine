# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

from pathlib import Path
import tempfile
import unittest
from unittest import mock

from licensing_policy import (
    Domain,
    LoadedPolicyContext,
    PolicyResolver,
    is_generated_file,
    is_third_party_path,
    list_repository_files,
    load_policy_context,
)


REPOSITORY_ROOT = Path(__file__).resolve().parents[3]


def context(root: Path, domains: list[Domain] | None = None) -> LoadedPolicyContext:
    selected = domains or [Domain({
        "current_license_mode": "fixed-spdx",
        "current_license_expression": "MPL-2.0",
        "resolution_requirement": "metadata-or-policy-default",
    }, "first-party", 100, ["**"], True)]
    policy = {
        "validation": {
            "case_sensitive": True,
            "metadata_conflict_action": "deny",
            "unresolved_license_action": "deny",
        },
        "metadata": {"precedence": ["embedded-spdx", "sidecar", "reuse-dep5"]},
        "submodules": [],
    }
    (root / "LICENSE_TEXT_SOURCES.toml").write_text("files = []\n", encoding="utf-8")
    return LoadedPolicyContext(
        root=root,
        policy_path=root / "LICENSE_POLICY.toml",
        policy=policy,
        rights_holders=set(),
        domains=selected,
        case_sensitive=True,
    )


class PolicyLoadingTests(unittest.TestCase):
    def test_malformed_toml_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "LICENSE_POLICY.toml").write_text("invalid = [\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "invalid canonical licensing policy"):
                load_policy_context(root)

    def test_repository_domains_resolve_samples_and_first_party(self) -> None:
        resolver = PolicyResolver(load_policy_context(REPOSITORY_ROOT))
        expected = {
            "Samples/StarterArena/README.md": "MIT",
            "Samples/ArenaRush/README.md": "MIT",
            "Engine/Source/Runtime/Scripting/CMakeLists.txt": "MPL-2.0",
            "Tools/Licensing/normalize_spdx.py": "MPL-2.0",
            "Tests/README.md": "MPL-2.0",
        }
        for path, expression in expected.items():
            with self.subTest(path=path):
                decision = resolver.decide(path)
                self.assertTrue(decision.resolution.resolved)
                self.assertEqual(decision.resolution.expression, expression)


class InventoryTests(unittest.TestCase):
    def test_inventory_order_is_deterministic(self) -> None:
        outputs = {
            ("ls-files", "--cached"): "z.cpp\na.cpp\n",
            ("ls-files", "--deleted"): "",
            ("ls-files", "--others", "--exclude-standard"): "m.cpp\n",
        }
        with mock.patch(
            "licensing_policy.inventory.run_git",
            side_effect=lambda _root, *args: outputs[args],
        ):
            result = list_repository_files(Path("."), True, [], case_sensitive=True)
        self.assertEqual(result, ["a.cpp", "m.cpp", "z.cpp"])

    def test_generated_and_third_party_paths_are_shared_exclusions(self) -> None:
        self.assertTrue(is_generated_file("Engine/Generated/File.cs", "class File {}"))
        self.assertTrue(is_generated_file("Engine/File.cs", "// generated file; do not edit"))
        self.assertTrue(is_third_party_path("Vendor/Library/source.cpp"))
        self.assertTrue(is_third_party_path("Engine/ThirdParty/source.cpp"))


class ResolverTests(unittest.TestCase):
    def test_metadata_precedence_and_policy_default(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "embedded.py").write_text(
                "# SPDX-License-Identifier: MPL-2.0\n", encoding="utf-8")
            (root / "sidecar.json").write_text("{}\n", encoding="utf-8")
            (root / "sidecar.json.license").write_text(
                "SPDX-License-Identifier: MPL-2.0\n", encoding="utf-8")
            (root / "dep5.json").write_text("{}\n", encoding="utf-8")
            (root / "default.py").write_text("print('ok')\n", encoding="utf-8")
            (root / ".reuse").mkdir()
            (root / ".reuse" / "dep5").write_text(
                "Files: dep5.json\nLicense: MPL-2.0\n", encoding="utf-8")
            resolver = PolicyResolver(context(root))

            self.assertEqual(resolver.decide("embedded.py").resolution.source, "embedded-spdx")
            sidecar = resolver.decide("sidecar.json")
            self.assertEqual(sidecar.resolution.source, "sidecar")
            self.assertFalse(sidecar.modification_allowed)
            self.assertEqual(resolver.decide("dep5.json").resolution.source, "reuse-dep5")
            self.assertEqual(
                resolver.decide("default.py").resolution.source,
                "policy-current-default",
            )

    def test_metadata_conflict_is_unresolved(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "conflict.py").write_text(
                "# SPDX-License-Identifier: MPL-2.0\n", encoding="utf-8")
            (root / "conflict.py.license").write_text(
                "SPDX-License-Identifier: MIT\n", encoding="utf-8")
            decision = PolicyResolver(context(root)).decide("conflict.py")
            self.assertFalse(decision.resolution.resolved)
            self.assertIn("LICENSE_METADATA_CONFLICT", {
                item.code for item in decision.diagnostics
            })

    def test_ambiguous_ownership_has_no_modification_permission(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            raw = {
                "current_license_mode": "fixed-spdx",
                "current_license_expression": "MPL-2.0",
                "resolution_requirement": "metadata-or-policy-default",
            }
            domains = [
                Domain(raw, "one", 100, ["Engine/**"], True),
                Domain(raw, "two", 100, ["Engine/**"], True),
            ]
            decision = PolicyResolver(context(root, domains)).decide("Engine/File.cpp")
            self.assertIsNone(decision.domain)
            self.assertFalse(decision.modification_allowed)


if __name__ == "__main__":
    unittest.main()
