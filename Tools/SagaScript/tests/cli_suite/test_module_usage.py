# SPDX-License-Identifier: MPL-2.0
from __future__ import annotations

from .support.environment import *
from .support.worker import sha256_file


PACKAGE_ID = "com.saga.gameplay.challenge"
MODULE_ID = "collectibles.instrumentation"
CREATE_ID = (
    "M:SagaEngine.Scripting.Gameplay.DevelopmentDiagnostics."
    "CollectibleInstrumentation.Create(System.Int32)"
)
STATE_ID = (
    "T:SagaEngine.Scripting.Gameplay.DevelopmentDiagnostics."
    "CollectibleInstrumentationState"
)


def canonical_bytes(value: object) -> bytes:
    return (json.dumps(value, sort_keys=True, indent=2) + "\n").encode("utf-8")


def prepare_request(root: Path, sources: dict[str, str]) -> tuple[Path, Path]:
    source_entries = []
    for name, body in sources.items():
        source = write_source(root, name, body)
        source_entries.append({"path": str(source), "hash": sha256_file(source)})

    project_hash = "sha256:project"
    graph_hash = "sha256:graph"
    catalog = root / "api_ownership_v1.json"
    catalog.write_bytes(canonical_bytes({
        "schemaVersion": 1,
        "projectId": "module-usage-test",
        "projectHash": project_hash,
        "graphHash": graph_hash,
        "symbols": [
            {
                "documentationId": CREATE_ID,
                "apiId": "CollectibleInstrumentation.Create",
                "surface": f"{PACKAGE_ID}/{MODULE_ID}",
                "module": MODULE_ID,
                "packageId": PACKAGE_ID,
            },
            {
                "documentationId": STATE_ID,
                "apiId": "CollectibleInstrumentationState",
                "surface": f"{PACKAGE_ID}/{MODULE_ID}",
                "module": MODULE_ID,
                "packageId": PACKAGE_ID,
            },
        ],
    }))
    references = [{"path": str(RUNTIME_BRIDGE_ASSEMBLY),
                   "hash": sha256_file(RUNTIME_BRIDGE_ASSEMBLY)}]
    canonical_references = [
        {"path": item["path"].replace("\\", "/"), "hash": item["hash"]}
        for item in sorted(references, key=lambda item: item["path"])
    ]
    request = root / "request.json"
    request.write_text(json.dumps({
        "schemaVersion": 1,
        "projectId": "module-usage-test",
        "projectHash": project_hash,
        "lockHash": "sha256:lock",
        "graphHash": graph_hash,
        "generationId": "generation-test",
        "generationHash": "sha256:generation",
        "generationManifestHash": "sha256:generation-manifest",
        "referenceHash": "sha256:" + hashlib.sha256(
            canonical_bytes(canonical_references)).hexdigest(),
        "ownershipCatalogPath": str(catalog),
        "ownershipCatalogHash": sha256_file(catalog),
        "packageId": PACKAGE_ID,
        "module": MODULE_ID,
        "sources": source_entries,
        "references": references,
    }, indent=2) + "\n", encoding="utf-8")
    return request, root / "report.json"


def run_usage(request: Path, report: Path) -> subprocess.CompletedProcess[str]:
    return run_cli("module-usage", "--request", str(request), "--out", str(report))


def test_module_usage_semantic_locations_and_unused_imports() -> None:
    with tempfile.TemporaryDirectory(prefix="sagascript-module-usage-") as temp:
        root = Path(temp)
        request, report = prepare_request(root, {
            "Unused.cs": """
using SagaEngine.Scripting.Gameplay.DevelopmentDiagnostics;
public sealed class Unused { }
""",
            "Uses.cs": """
public sealed class Uses
{
    public void Run()
    {
        var state = SagaEngine.Scripting.Gameplay.DevelopmentDiagnostics.CollectibleInstrumentation.Create(1);
        SagaEngine.Scripting.Gameplay.DevelopmentDiagnostics.CollectibleInstrumentationState copy = state;
    }
}
""",
        })
        result = run_usage(request, report)
        assert result.returncode == 0, result.stderr + result.stdout
        payload = json.loads(report.read_text(encoding="utf-8"))
        assert payload["status"] == "Passed"
        assert payload["usageCount"] == 2
        assert [item["documentationId"] for item in payload["usages"]] == [
            CREATE_ID, STATE_ID,
        ]
        assert all(item["sourcePath"].endswith("Uses.cs") for item in payload["usages"])
        assert all(item["startLine"] > 0 and item["startColumn"] > 0
                   for item in payload["usages"])


def test_module_usage_rejects_stale_saved_source() -> None:
    with tempfile.TemporaryDirectory(prefix="sagascript-module-stale-") as temp:
        root = Path(temp)
        request, report = prepare_request(root, {"Uses.cs": "public sealed class Uses { }\n"})
        write_source(root, "Uses.cs", "public sealed class Changed { }\n")
        result = run_usage(request, report)
        assert result.returncode == 1, result.stderr + result.stdout
        payload = json.loads(report.read_text(encoding="utf-8"))
        assert payload["status"] == "Failed"
        assert payload["usageCount"] == 0
        assert any(item.startswith("Script.ModuleUsage.StaleInput:")
                   for item in payload["diagnostics"])


TESTS = (
    test_module_usage_semantic_locations_and_unused_imports,
    test_module_usage_rejects_stale_saved_source,
)
