# SPDX-License-Identifier: MPL-2.0
"""Real subprocess coverage for the long-lived language-service protocol."""

from __future__ import annotations

import json
import hashlib
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

from .support.environment import CLI_ASSEMBLY, REPO_ROOT, ensure_cli_built


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
            _request(6, "controlled-projection", root, **identity, payload={}),
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


TESTS = (test_reuses_workspace_and_rejects_stale_versions,
         test_utf16_positions_and_path_escape_fail_closed,
         test_all_interactive_features_and_preflight,
         test_malformed_envelope_is_fatal,
         test_controlled_edit_plans_exact_constant_initializer,
         test_metadata_references_require_containment_and_matching_hash)
