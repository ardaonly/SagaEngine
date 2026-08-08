# SPDX-License-Identifier: MPL-2.0
"""Real subprocess coverage for the long-lived language-service protocol."""

from __future__ import annotations

import json
import hashlib
import os
import queue
import shutil
import subprocess
import tempfile
import threading
import time
from pathlib import Path

from .support.environment import CLI_ASSEMBLY, REPO_ROOT, ensure_cli_built


def _sha256_text(value: str) -> str:
    return "sha256:" + hashlib.sha256(value.encode("utf-8")).hexdigest()


def _request(request_id: int, method: str, root: Path, **extra: object) -> dict[str, object]:
    value: dict[str, object] = {
        "schemaVersion": 1,
        "protocolVersion": 1,
        "messageType": "request",
        "requestId": request_id,
        "projectSessionId": "language-service-test",
        "method": method,
        "projectSnapshotIdentity": "snapshot-1",
        "payload": {},
    }
    value.update(extra)
    if method == "project/open" and "payload" not in extra:
        value["payload"] = {"canonicalRoot": str(root)}
    return value


def _run(messages: list[dict[str, object]]) -> tuple[list[dict[str, object]], str, int]:
    env = os.environ.copy()
    env.setdefault("DOTNET_CLI_HOME", "/tmp/sagascript-language-service-home")
    env.setdefault("DOTNET_CLI_TELEMETRY_OPTOUT", "1")
    ensure_cli_built(env)
    process = subprocess.Popen(
        ["dotnet", str(CLI_ASSEMBLY), "language-service", "--stdio"],
        cwd=REPO_ROOT,
        env=env,
        text=True,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    stdout, stderr = process.communicate(
        "".join(json.dumps(message, ensure_ascii=True) + "\n" for message in messages),
        timeout=20,
    )
    return [json.loads(line) for line in stdout.splitlines()], stderr, process.returncode


def _package_snapshot(root: Path) -> tuple[dict[str, object], Path]:
    packages = REPO_ROOT / "Samples/ArenaRush/Build/Packages"
    current = json.loads((packages / "manifests/current.json").read_text())
    generation = packages / "generations" / current["generationId"]
    ownership_source = generation / "api_ownership_v1.json"
    ownership = json.loads(ownership_source.read_text())
    references = json.loads((generation / "csharp_references_v1.json").read_text())
    reference_entry = next(item for item in references["references"]
                           if item["artifactId"] ==
                           "com.saga.gameplay.foundation.managed-reference")
    reference_source = generation / reference_entry["resolvedAssemblyLogicalPath"]
    reference = root / "packages" / reference_entry[
        "resolvedAssemblyLogicalPath"]
    manifest = root / "packages/api_ownership_v1.json"
    reference_manifest = root / "packages/csharp_references_v1.json"
    generation_manifest = root / "packages/generation.json"
    reference.parent.mkdir(parents=True)
    shutil.copy2(reference_source, reference)
    shutil.copy2(ownership_source, manifest)
    source_references = generation / "csharp_references_v1.json"
    source_generation = generation / "generation.json"
    shutil.copy2(source_references, reference_manifest)
    shutil.copy2(source_generation, generation_manifest)
    reference_hash = "sha256:" + hashlib.sha256(reference.read_bytes()).hexdigest()
    manifest_hash = "sha256:" + hashlib.sha256(manifest.read_bytes()).hexdigest()
    reference_manifest_hash = "sha256:" + hashlib.sha256(
        reference_manifest.read_bytes()).hexdigest()
    generation_manifest_hash = "sha256:" + hashlib.sha256(
        generation_manifest.read_bytes()).hexdigest()
    return {
        "canonicalRoot": str(root),
        "projectId": ownership["projectId"],
        "packageGenerationId": current["generationId"],
        "packageGeneration": current["generationHash"],
        "packageGraphHash": current["graphHash"],
        "generationManifest": {
            "path": str(generation_manifest), "hash": generation_manifest_hash},
        "referenceManifestHash": reference_manifest_hash,
        "referenceManifest": {
            "path": str(reference_manifest), "hash": reference_manifest_hash},
        "ownershipManifestHash": manifest_hash,
        "ownershipManifest": {"path": str(manifest), "hash": manifest_hash},
        "metadataReferences": [{"path": str(reference), "hash": reference_hash}],
    }, manifest


def test_reuses_workspace_and_rejects_stale_versions() -> None:
    with tempfile.TemporaryDirectory(prefix="sagascript-language-") as temporary:
        root = Path(temporary).resolve()
        document = root / "PlayerRules.cs"
        identity = {"documentId": "player-rules", "documentVersion": 1}
        source = "class PlayerRules { void Update() { System.Console.WriteLine(1); } }"
        messages = [
            _request(1, "initialize", root),
            _request(2, "project/open", root),
            _request(3, "document/open", root, **identity,
                     payload={"path": str(document), "text": source}),
            _request(4, "symbols", root, **identity, payload={}),
            _request(5, "document/change", root, **identity,
                     payload={"path": str(document), "text": source}),
            _request(6, "shutdown", root),
        ]
        responses, stderr, code = _run(messages)
        assert code == 0, stderr
        assert stderr == ""
        symbols = next(item for item in responses if item["requestId"] == 4)
        assert any(symbol["name"] == "PlayerRules" for symbol in symbols["payload"])
        stale = next(item for item in responses if item["requestId"] == 5)
        assert stale["messageType"] == "error"
        assert stale["payload"]["code"] == "LanguageService.StaleDocument"


def test_utf16_positions_and_path_escape_fail_closed() -> None:
    with tempfile.TemporaryDirectory(prefix="sagascript-language-") as temporary:
        root = Path(temporary).resolve()
        document = root / "Utf16Rules.cs"
        non_ascii_identifier = "T" + chr(0x00FC) + "rkce"
        supplementary = chr(0x1F600)
        source = f'class {non_ascii_identifier} {{ string value = "{supplementary}"; }}'
        messages = [
            _request(1, "initialize", root),
            _request(2, "project/open", root),
            _request(3, "document/open", root, documentId="utf16", documentVersion=1,
                     payload={"path": str(document), "text": source}),
            _request(4, "hover", root, documentId="utf16", documentVersion=1,
                     payload={"position": {"line": 0, "character": 7}}),
            _request(5, "document/open", root, documentId="escape", documentVersion=1,
                     payload={"path": str(root.parent / "Escape.cs"), "text": "class Escape {}"}),
            _request(6, "shutdown", root),
        ]
        responses, stderr, code = _run(messages)
        assert code == 0, stderr
        hover = next(item for item in responses if item["requestId"] == 4)
        assert hover["messageType"] == "response", hover
        escape = next(item for item in responses if item["requestId"] == 5)
        assert escape["messageType"] == "error"
        assert escape["payload"]["code"] == "LanguageService.PathEscape"


def test_all_interactive_features_and_preflight() -> None:
    with tempfile.TemporaryDirectory(prefix="sagascript-language-") as temporary:
        root = Path(temporary).resolve()
        document = root / "FeatureRules.cs"
        source = ("namespace Sample;\r\npublic sealed class FeatureRules {\n"
                  "\tpublic int Value;\r\n"
                  "\tpublic int Read() { System.Console.WriteLine(Value); return Value; }\n}\n")
        identity = {"documentId": "feature-rules", "documentVersion": 1}
        messages = [
            _request(1, "initialize", root),
            _request(2, "project/open", root),
            _request(3, "document/open", root, **identity,
                     payload={"path": str(document), "text": source}),
            _request(4, "diagnostics", root, **identity, payload={}),
            _request(5, "classifications", root, **identity, payload={}),
            _request(6, "completion", root, **identity,
                     payload={"position": {"line": 3, "character": 37}}),
            _request(7, "hover", root, **identity,
                     payload={"position": {"line": 3, "character": 47}}),
            _request(8, "definition", root, **identity,
                     payload={"position": {"line": 3, "character": 47}}),
            _request(9, "references", root, **identity,
                     payload={"position": {"line": 3, "character": 47}}),
            _request(10, "symbols", root, **identity, payload={}),
            _request(11, "preflight", root, **identity,
                     payload={"candidateText": "public sealed class Broken {"}),
            _request(12, "heartbeat", root),
            _request(13, "document/close", root, **identity, payload={}),
            _request(14, "project/close", root),
            _request(15, "shutdown", root),
        ]
        responses, stderr, code = _run(messages)
        assert code == 0, stderr
        by_id = {item["requestId"]: item for item in responses
                 if item["messageType"] != "notification"}
        for request_id in range(4, 13):
            assert by_id[request_id]["messageType"] == "response", by_id[request_id]
        assert isinstance(by_id[4]["payload"], list)
        assert by_id[5]["payload"]
        assert isinstance(by_id[6]["payload"], list)
        assert isinstance(by_id[7]["payload"], dict)
        assert by_id[8]["payload"]
        assert len(by_id[9]["payload"]) >= 2
        assert any(item["name"] == "FeatureRules" for item in by_id[10]["payload"])
        assert by_id[11]["payload"]["accepted"] is False
        assert by_id[12]["payload"]["alive"] is True


def test_malformed_envelope_is_fatal() -> None:
    responses, stderr, code = _run([{
        "schemaVersion": 2,
        "protocolVersion": 1,
        "messageType": "request",
        "requestId": 1,
        "projectSessionId": "invalid-version",
        "method": "initialize",
        "projectSnapshotIdentity": "snapshot",
        "payload": {},
    }])
    assert code == 2, stderr
    assert responses[0]["messageType"] == "fatal"
    assert responses[0]["payload"]["code"] == "LanguageService.MalformedEnvelope"


def test_controlled_edit_plans_exact_constant_initializer() -> None:
    with tempfile.TemporaryDirectory(prefix="sagascript-language-") as temporary:
        root = Path(temporary).resolve()
        document = root / "ArenaRushGameMode.cs"
        source = ("namespace ArenaRush; public sealed class ArenaRushGameMode { "
                  "private const int EnergyCoreScore = 10; "
                  "private const double EnergyCoreRespawnSeconds = 2.0; }")
        identity = {"documentId": "arena-rules", "documentVersion": 1}
        score_field = "field://ArenaRush.ArenaRushGameMode/EnergyCoreScore/initializer"
        respawn_field = "field://ArenaRush.ArenaRushGameMode/EnergyCoreRespawnSeconds/initializer"
        messages = [
            _request(1, "initialize", root),
            _request(2, "project/open", root),
            _request(3, "document/open", root, **identity,
                     payload={"path": str(document), "text": source}),
            _request(4, "controlled-edit/plan", root, **identity,
                     payload={"fieldId": score_field, "value": 20}),
            _request(5, "controlled-edit/plan", root, **identity,
                     payload={"fieldId": respawn_field, "value": 0.5}),
            _request(6, "controlled-projection", root, **identity,
                     payload={"sourceText": source,
                              "sourceHash": _sha256_text(source)}),
            _request(7, "shutdown", root),
        ]
        responses, stderr, code = _run(messages)
        assert code == 0, stderr
        score = next(item for item in responses if item["requestId"] == 4)["payload"]
        respawn = next(item for item in responses if item["requestId"] == 5)["payload"]
        projection = next(item for item in responses if item["requestId"] == 6)["payload"]
        assert score["accepted"] is True
        assert score["oldValue"] == "10" and score["replacement"] == "20"
        assert respawn["oldValue"] == "2.0" and respawn["replacement"] == "0.5"
        assert projection["opaque"] is False
        assert len(projection["editableFields"]) == 2
        assert {item["value"] for item in projection["editableFields"]} == {"10", "2.0"}
        start = score["startByte"]
        end = score["endByte"]
        candidate = score["candidateText"].encode("utf-8")
        original = source.encode("utf-8")
        assert candidate[:start] == original[:start]
        assert candidate[start:start + 2] == b"20"
        assert candidate[start + 2:] == original[end:]


def test_controlled_edit_plans_full_arena_rush_source() -> None:
    with tempfile.TemporaryDirectory(prefix="sagascript-language-") as temporary:
        root = Path(temporary).resolve()
        document = root / "Scripts/ArenaRushGameMode.cs"
        document.parent.mkdir(parents=True)
        source = (REPO_ROOT / "Samples/ArenaRush/Scripts/ArenaRushGameMode.cs").read_text()
        project_payload, _ = _package_snapshot(root)
        identity = {"documentId": "arena-rules", "documentVersion": 1}
        messages = [
            _request(1, "initialize", root),
            _request(2, "project/open", root, payload=project_payload),
            _request(3, "document/open", root, **identity,
                     payload={"path": str(document), "text": source}),
            _request(4, "controlled-edit/plan", root, **identity,
                     payload={
                         "fieldId": "field://ArenaRush.ArenaRushGameMode/EnergyCoreScore/initializer",
                         "value": 20,
                     }),
            _request(5, "shutdown", root),
        ]
        responses, stderr, code = _run(messages)
        assert code == 0, stderr
        plan = next(item for item in responses if item["requestId"] == 4)["payload"]
        assert plan["accepted"] is True
        assert plan["oldValue"] == "10"
        assert plan["replacement"] == "20"


def test_metadata_references_require_containment_and_matching_hash() -> None:
    with tempfile.TemporaryDirectory(prefix="sagascript-language-") as temporary:
        root = Path(temporary).resolve()
        reference = root / "references" / "reference.dll"
        reference.parent.mkdir()
        shutil.copy2(CLI_ASSEMBLY, reference)
        digest = "sha256:" + hashlib.sha256(reference.read_bytes()).hexdigest()
        valid_payload = {
            "canonicalRoot": str(root),
            "metadataReferences": [{"path": str(reference), "hash": digest}],
        }
        valid, stderr, code = _run([
            _request(1, "initialize", root),
            _request(2, "project/open", root, payload=valid_payload),
            _request(3, "shutdown", root),
        ])
        assert code == 0, stderr
        assert next(item for item in valid if item["requestId"] == 2)[
            "messageType"] == "response"

        invalid_hash, stderr, code = _run([
            _request(1, "initialize", root),
            _request(2, "project/open", root, payload={
                "canonicalRoot": str(root),
                "metadataReferences": [{"path": str(reference),
                                         "hash": "sha256:" + "0" * 64}],
            }),
            _request(3, "shutdown", root),
        ])
        assert code == 0, stderr
        invalid_response = next(item for item in invalid_hash
                                if item["requestId"] == 2)
        assert invalid_response["messageType"] == "error"
        assert invalid_response["payload"]["code"] == \
            "LanguageService.InvalidReference"

        escaped, stderr, code = _run([
            _request(1, "initialize", root),
            _request(2, "project/open", root, payload={
                "canonicalRoot": str(root),
                "metadataReferences": [{"path": str(CLI_ASSEMBLY),
                                         "hash": digest}],
            }),
            _request(3, "shutdown", root),
        ])
        assert code == 0, stderr
        escaped_response = next(item for item in escaped
                                if item["requestId"] == 2)
        assert escaped_response["messageType"] == "error"
        assert escaped_response["payload"]["code"] == \
            "LanguageService.PathEscape"

        symlink = root / "references" / "escaped-link.dll"
        symlink.symlink_to(CLI_ASSEMBLY)
        escaped_link, stderr, code = _run([
            _request(1, "initialize", root),
            _request(2, "project/open", root, payload={
                "canonicalRoot": str(root),
                "metadataReferences": [{"path": str(symlink),
                                         "hash": digest}],
            }),
            _request(3, "shutdown", root),
        ])
        assert code == 0, stderr
        escaped_link_response = next(item for item in escaped_link
                                     if item["requestId"] == 2)
        assert escaped_link_response["messageType"] == "error"
        assert escaped_link_response["payload"]["code"] == \
            "LanguageService.PathEscape"


def test_package_visibility_filters_features_and_controlled_preflight() -> None:
    with tempfile.TemporaryDirectory(prefix="sagascript-visibility-") as temporary:
        root = Path(temporary).resolve()
        payload, _ = _package_snapshot(root)
        document = root / "ArenaRushGameMode.cs"
        source = (
            "namespace ArenaRush;\n"
            "public sealed class ArenaRushGameMode {\n"
            "  private const int EnergyCoreScore = 10;\n"
            "  private const double EnergyCoreRespawnSeconds = 2.0;\n"
            "  void Inspect() {\n"
            "    SagaEngine.Scripting.Gameplay.DevelopmentDiagnostics."
            "CollectibleInstrumentation.Create(1);\n"
            "    SagaEngine.Scripting.Gameplay.DevelopmentDiagnostics."
            "CollectibleInstrumentation.\n"
            "  }\n}"
        )
        identity = {"documentId": "visibility", "documentVersion": 1}
        score_field = "field://ArenaRush.ArenaRushGameMode/EnergyCoreScore/initializer"
        messages = [
            _request(1, "initialize", root),
            _request(2, "project/open", root, payload=payload),
            _request(3, "document/open", root, **identity,
                     payload={"path": str(document), "text": source}),
            _request(4, "diagnostics", root, **identity, payload={}),
            _request(5, "completion", root, **identity,
                     payload={"position": {"line": 6, "character": 77}}),
            _request(6, "hover", root, **identity,
                     payload={"position": {"line": 5, "character": 86}}),
            _request(7, "controlled-edit/plan", root, **identity,
                     payload={"fieldId": score_field, "value": 20}),
            _request(8, "shutdown", root),
        ]
        responses, stderr, code = _run(messages)
        assert code == 0, stderr
        by_id = {item["requestId"]: item for item in responses
                 if item["messageType"] != "notification"}
        diagnostics = by_id[4]["payload"]
        assert any(item["id"] == "Script.PackageVisibility.DisabledModule"
                   for item in diagnostics)
        labels = {item["label"] for item in by_id[5]["payload"]}
        assert "Create" not in labels
        assert by_id[6]["payload"].get("text", "") == ""
        assert any(item["id"] == "Script.PackageVisibility.DisabledModule"
                   for item in by_id[6]["payload"]["diagnostics"])
        assert by_id[7]["payload"]["accepted"] is False
        assert by_id[7]["payload"]["packageVisibilityPassed"] is False


def test_ownership_input_rejects_hash_and_project_mismatch() -> None:
    with tempfile.TemporaryDirectory(prefix="sagascript-visibility-") as temporary:
        root = Path(temporary).resolve()
        payload, manifest = _package_snapshot(root)
        invalid_hash = dict(payload)
        invalid_hash["ownershipManifest"] = {
            "path": str(manifest), "hash": "sha256:" + "0" * 64}
        responses, stderr, code = _run([
            _request(1, "initialize", root),
            _request(2, "project/open", root, payload=invalid_hash),
            _request(3, "shutdown", root),
        ])
        assert code == 0, stderr
        rejected = next(item for item in responses if item["requestId"] == 2)
        assert rejected["payload"]["code"] == "LanguageService.InvalidOwnership"

        wrong_project = dict(payload)
        wrong_project["projectId"] = "wrong-project"
        responses, stderr, code = _run([
            _request(1, "initialize", root),
            _request(2, "project/open", root, payload=wrong_project),
            _request(3, "shutdown", root),
        ])
        assert code == 0, stderr
        rejected = next(item for item in responses if item["requestId"] == 2)
        assert rejected["payload"]["code"] == \
            "LanguageService.InvalidPackageSnapshot"


def test_scheduler_cancels_and_coalesces_background_analysis() -> None:
    with tempfile.TemporaryDirectory(prefix="sagascript-scheduler-") as temporary:
        root = Path(temporary).resolve()
        document = root / "SchedulerRules.cs"
        references = " ".join("Value++;" for _ in range(20000))
        source_v1 = "class SchedulerRules { int Value; void Run() { " + references + " } }"
        source_v2 = source_v1.replace("class SchedulerRules", "class SchedulerRulesV2")
        source_v3 = source_v1.replace("class SchedulerRules", "class SchedulerRulesV3")
        messages = [
            _request(1, "initialize", root),
            _request(2, "project/open", root),
            _request(3, "document/open", root, documentId="scheduler",
                     documentVersion=1,
                     payload={"path": str(document), "text": source_v1}),
            _request(4, "references", root, documentId="scheduler",
                     documentVersion=1,
                     payload={"position": {"line": 0, "character": 27}}),
            _request(5, "cancel", root, payload={"requestId": 4}),
            _request(6, "document/change", root, documentId="scheduler",
                     documentVersion=2,
                     payload={"path": str(document), "text": source_v2}),
            _request(7, "document/change", root, documentId="scheduler",
                     documentVersion=3,
                     payload={"path": str(document), "text": source_v3}),
            _request(8, "completion", root, documentId="scheduler",
                     documentVersion=3,
                     payload={"position": {"line": 0, "character": 5}}),
            _request(9, "shutdown", root),
        ]
        responses, stderr, code = _run(messages)
        assert code == 0, stderr
        cancelled = next(item for item in responses if item["requestId"] == 4)
        assert cancelled["messageType"] == "error"
        assert cancelled["payload"]["code"] == "LanguageService.Cancelled"
        publications = [item for item in responses
                        if item["messageType"] == "notification" and
                        item["method"] == "analysis/publish"]
        assert len(publications) == 1
        assert publications[0]["documentVersion"] == 3
        completion_index = next(index for index, item in enumerate(responses)
                                if item["requestId"] == 8)
        publication_index = responses.index(publications[0])
        assert completion_index < publication_index


def test_scheduler_wakes_after_completed_background_analysis() -> None:
    with tempfile.TemporaryDirectory(prefix="sagascript-scheduler-wake-") as temporary:
        root = Path(temporary).resolve()
        document = root / "SchedulerWake.cs"
        env = os.environ.copy()
        env.setdefault("DOTNET_CLI_HOME", "/tmp/sagascript-language-service-home")
        env.setdefault("DOTNET_CLI_TELEMETRY_OPTOUT", "1")
        ensure_cli_built(env)
        process = subprocess.Popen(
            ["dotnet", str(CLI_ASSEMBLY), "language-service", "--stdio"],
            cwd=REPO_ROOT, env=env, text=True, stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        assert process.stdin is not None and process.stdout is not None
        received: queue.Queue[dict[str, object]] = queue.Queue()
        reader = threading.Thread(target=lambda: [received.put(json.loads(line))
                                                   for line in process.stdout],
                                  daemon=True)
        reader.start()

        def send(message: dict[str, object]) -> None:
            process.stdin.write(json.dumps(message, ensure_ascii=True) + "\n")
            process.stdin.flush()

        def wait_for(predicate: object) -> dict[str, object]:
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                try:
                    item = received.get(timeout=0.2)
                except queue.Empty:
                    continue
                if predicate(item):  # type: ignore[operator]
                    return item
            raise AssertionError("Timed out waiting for language-service output.")

        send(_request(1, "initialize", root))
        send(_request(2, "project/open", root))
        send(_request(3, "document/open", root, documentId="wake",
                      documentVersion=1,
                      payload={"path": str(document),
                               "text": "class SchedulerWake {}"}))
        wait_for(lambda item: item["messageType"] == "notification" and
                 item.get("documentVersion") == 1)
        send(_request(4, "document/change", root, documentId="wake",
                      documentVersion=2,
                      payload={"path": str(document),
                               "text": "invalid diagnostic token"}))
        invalid = wait_for(lambda item: item["messageType"] == "notification" and
                           item.get("documentVersion") == 2)
        assert invalid["payload"]["diagnostics"]
        send(_request(5, "document/change", root, documentId="wake",
                      documentVersion=3,
                      payload={"path": str(document),
                               "text": "class SchedulerWake {}"}))
        recovered = wait_for(lambda item: item["messageType"] == "notification" and
                             item.get("documentVersion") == 3)
        assert recovered["payload"]["diagnostics"] == []
        send(_request(6, "shutdown", root))
        process.stdin.close()
        assert process.wait(timeout=20) == 0


def test_reconfiguration_commits_b_and_rolls_back_invalid_candidate() -> None:
    with tempfile.TemporaryDirectory(prefix="sagascript-reconfigure-") as temporary:
        root = Path(temporary).resolve()
        document = root / "ReconfiguredRules.cs"
        source = "class ReconfiguredRules { int UnsavedValue = 7; }"
        identity = {"documentId": "reconfigured", "documentVersion": 7}
        candidate_b = {
            "canonicalRoot": str(root),
            "projectId": "",
            "candidateProjectSnapshotIdentity": "snapshot-2",
            "compilerVersion": "4.11.0",
            "languageVersion": "latest-major",
        }
        invalid_candidate_c = {
            **candidate_b,
            "candidateProjectSnapshotIdentity": "snapshot-3",
            "metadataReferences": [{
                "path": str(root.parent / "escaped-reference.dll"),
                "hash": "sha256:" + "0" * 64,
            }],
        }
        messages = [
            _request(1, "initialize", root),
            _request(2, "project/open", root),
            _request(3, "document/open", root, **identity,
                     payload={"path": str(document), "text": source}),
            _request(4, "project/reconfigure", root, payload=candidate_b),
            _request(5, "symbols", root, **identity,
                     projectSnapshotIdentity="snapshot-2", payload={}),
            _request(6, "project/reconfigure", root,
                     projectSnapshotIdentity="snapshot-2",
                     payload=invalid_candidate_c),
            _request(7, "symbols", root, **identity,
                     projectSnapshotIdentity="snapshot-2", payload={}),
            _request(8, "shutdown", root,
                     projectSnapshotIdentity="snapshot-2"),
        ]
        env = os.environ.copy()
        env.setdefault("DOTNET_CLI_HOME",
                       "/tmp/sagascript-language-service-home")
        env.setdefault("DOTNET_CLI_TELEMETRY_OPTOUT", "1")
        ensure_cli_built(env)
        process = subprocess.Popen(
            ["dotnet", str(CLI_ASSEMBLY), "language-service", "--stdio"],
            cwd=REPO_ROOT, env=env, text=True, stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        assert process.stdin is not None and process.stdout is not None
        by_id: dict[int, dict[str, object]] = {}
        for message in messages:
            process.stdin.write(json.dumps(message, ensure_ascii=True) + "\n")
            process.stdin.flush()
            while True:
                line = process.stdout.readline()
                assert line, "Language service closed before responding."
                response = json.loads(line)
                if (response["messageType"] != "notification" and
                        response["requestId"] == message["requestId"]):
                    by_id[int(response["requestId"])] = response
                    break
        process.stdin.close()
        code = process.wait(timeout=20)
        stderr = process.stderr.read() if process.stderr is not None else ""
        assert code == 0, stderr
        assert by_id[4]["messageType"] == "response", by_id[4]
        assert by_id[4]["projectSnapshotIdentity"] == "snapshot-1"
        assert by_id[4]["payload"]["acceptedProjectSnapshotIdentity"] == \
            "snapshot-2"
        assert by_id[5]["messageType"] == "response", by_id[5]
        assert any(item["name"] == "ReconfiguredRules"
                   for item in by_id[5]["payload"])
        assert by_id[6]["messageType"] == "error", by_id[6]
        assert by_id[6]["payload"]["code"] == "LanguageService.PathEscape"
        assert by_id[7]["messageType"] == "response", by_id[7]
        assert any(item["name"] == "ReconfiguredRules"
                   for item in by_id[7]["payload"])


TESTS = (test_reuses_workspace_and_rejects_stale_versions,
         test_utf16_positions_and_path_escape_fail_closed,
         test_all_interactive_features_and_preflight,
         test_malformed_envelope_is_fatal,
         test_controlled_edit_plans_exact_constant_initializer,
         test_controlled_edit_plans_full_arena_rush_source,
         test_metadata_references_require_containment_and_matching_hash,
         test_package_visibility_filters_features_and_controlled_preflight,
         test_ownership_input_rejects_hash_and_project_mismatch,
         test_scheduler_cancels_and_coalesces_background_analysis,
         test_scheduler_wakes_after_completed_background_analysis,
         test_reconfiguration_commits_b_and_rolls_back_invalid_candidate)
