# SPDX-License-Identifier: MPL-2.0
from __future__ import annotations

from .support.environment import *
from .support.blocks import *
from .support.worker import *

def test_project_forms_analyze_compile_and_project_blocks() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project = root / "StarterArena"
        shutil.copytree(STARTER_ROOT, project)
        analyze = root / "analyze"
        compile_manifests = root / "compile-manifests"
        compile_artifacts = root / "compile-artifacts"
        blocks = root / "blocks"

        assert run_cli("analyze", "--project", str(project / "StarterArena.sagaproj"), "--out", str(analyze)).returncode == 0
        compile_result = run_cli(
            "compile",
            "--project",
            str(project / "StarterArena.sagaproj"),
            "--out",
            str(compile_manifests),
            "--artifacts-out",
            str(compile_artifacts),
        )
        assert compile_result.returncode == 0, compile_result.stderr + compile_result.stdout
        assert list(compile_artifacts.glob("*.scripts.dll"))
        assert run_cli("project-blocks", "--project", str(project / "StarterArena.sagaproj"), "--out", str(blocks)).returncode == 0
        assert (analyze / "analysis_report.json").exists()
        assert (blocks / "visual_blocks_projection_v1.json").exists()
        analysis = json.loads((analyze / "analysis_report.json").read_text(encoding="utf-8"))
        assert analysis["behaviors"][0]["behaviorId"] == "StarterArena.DoorController"
        projection = json.loads((blocks / "visual_blocks_projection_v1.json").read_text(encoding="utf-8"))
        assert any(item["blockKind"] == "ScriptClassBlock" for item in projection["blocks"])

def test_project_worker_is_deterministic_across_clean_roots() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        first_project = root / "first" / "StarterArena"
        second_project = root / "second" / "StarterArena"
        shutil.copytree(STARTER_ROOT, first_project)
        shutil.copytree(STARTER_ROOT, second_project)
        first_output = root / "first-output"
        second_output = root / "second-output"
        first = prepare_worker_request(root / "first-work", first_project, first_output, "deterministic-worker")
        second = prepare_worker_request(root / "second-work", second_project, second_output, "deterministic-worker")

        first_result = run_cli("worker", "--request", str(first[0]), "--result", str(first[1]), "--progress", str(first[2]))
        second_result = run_cli("worker", "--request", str(second[0]), "--result", str(second[1]), "--progress", str(second[2]))

        assert first_result.returncode == 0, first_result.stderr + first_result.stdout
        assert second_result.returncode == 0, second_result.stderr + second_result.stdout
        first_manifest, first_artifacts = read_current_artifacts(first_output)
        second_manifest, second_artifacts = read_current_artifacts(second_output)
        assert (first_output / "manifests" / "current.json").read_bytes() == (
            second_output / "manifests" / "current.json"
        ).read_bytes()
        assert first_artifacts == second_artifacts
        assert set(first_artifacts) == {
            "scripts.assembly",
            "scripts.assembly-manifest",
            "scripts.behaviors",
            "scripts.block-projection",
            "scripts.capabilities",
            "scripts.named-actions",
            "scripts.runtime-config",
            "scripts.source-map",
        }
        assert first_manifest["sourceRevision"] == "revision-1"
        behavior = json.loads(first_artifacts["scripts.behaviors"])
        assert behavior["schemaVersion"] == 2
        assert behavior["behaviors"][0]["behaviorId"] == "StarterArena.DoorController"
        assert [item["name"] for item in behavior["behaviors"][0]["lifecycleMethods"]] == ["OnStart", "OnUpdate"]
        assert behavior["behaviors"][0]["stateAccesses"] == [{
            "operation": "SetBool", "valueType": "Bool", "key": "door.target-open"}]
        actions = json.loads(first_artifacts["scripts.named-actions"])
        assert actions["actions"][0]["actionId"] == "Interact"
        projection = json.loads(first_artifacts["scripts.block-projection"])
        assert projection["filteredApiPolicy"] == "ExcludeRawAndInternal"
        assert "NativeBridge" not in json.dumps(projection)
        assert projection["opaqueRegions"][0]["classification"] == "OpaqueCSharp"
        assert projection["roots"][0]["displayName"] == "WHEN Interact"
        assert projection["roots"][0]["children"][0]["displayName"] == "set boolean state"
        assert projection["roots"][0]["children"][0]["stateKey"] == "door.target-open"
        assert projection["packageGenerationId"] == first_manifest["packageGenerationId"]
        assert projection["diagnostics"] == []

def test_arena_rush_worker_aggregates_contract_only_surfaces() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project = root / "ArenaRush"
        shutil.copytree(ARENA_RUSH_ROOT, project, ignore=shutil.ignore_patterns("Build"))
        output = root / "output"
        request, result, progress, _ = prepare_worker_request(
            root / "work", project, output, "arena-rush-worker"
        )
        worker = run_cli(
            "worker", "--request", str(request), "--result", str(result),
            "--progress", str(progress),
        )
        assert worker.returncode == 0, worker.stderr + worker.stdout
        _, artifacts = read_current_artifacts(output)
        projection = json.loads(artifacts["scripts.block-projection"])
        allowed = projection["allowedApis"]
        identities = [(item["surfaceId"], item["apiId"]) for item in allowed]
        assert identities == sorted(identities)
        assert any(
            item["apiId"] == "ChallengeHighLevel.StartOnce"
            and item["runtimeReadiness"] == "ContractOnly"
            and item["packageId"] == "com.saga.gameplay.challenge"
            and item["sourcePath"].endswith("GameplayContracts.cs")
            for item in allowed
        )
        assert any(
            item["apiId"] == "CollectiblesHighLevel.Collect"
            and item["module"] == "collectibles.highlevel"
            for item in allowed
        )
        serialized = json.dumps(projection, sort_keys=True)
        assert "collectibles.lowlevel" not in serialized
        assert "instrumentation" not in serialized
        assert "NativeBridge" not in serialized
        assert all(region["editable"] is False for region in projection["opaqueRegions"])

def test_worker_state_contract_follows_only_reachable_behavior_helpers() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project = root / "StarterArena"
        shutil.copytree(STARTER_ROOT, project)
        source = project / "Scripts" / "DoorController.cs"
        source.write_text(
            """// SPDX-License-Identifier: MIT
using SagaEngine.Scripting;
namespace StarterArena;
public sealed class DoorController : SagaBehavior
{
    public override void OnStart() { Publish(); CycleA(); }
    [SagaAction("Interact")]
    public void OnInteract() { OpenDoor(); }
    public override void OnUpdate(float deltaTime) { _ = Context.Self.Transform; }
    private void OpenDoor() { Publish(); }
    private void Publish() { Context.State.TrySetBool("door.target-open", true); }
    private void CycleA() { CycleB(); }
    private void CycleB()
    {
        Context.State.TrySetBool("cycle.flag", true);
        CycleA();
    }
    private void NeverReached()
    {
        Context.State.TrySetBool("unreachable.permission", true);
    }
}
""",
            encoding="utf-8",
        )
        output = root / "output"
        request = prepare_worker_request(root / "work", project, output, "state-call-graph")
        result = run_cli("worker", "--request", str(request[0]), "--result", str(request[1]),
                         "--progress", str(request[2]))
        assert result.returncode == 0, result.stderr + result.stdout
        _, artifacts = read_current_artifacts(output)
        behavior = json.loads(artifacts["scripts.behaviors"])
        assert behavior["schemaVersion"] == 2
        assert behavior["behaviors"][0]["stateAccesses"] == [
            {"operation": "SetBool", "valueType": "Bool", "key": "cycle.flag"},
            {"operation": "SetBool", "valueType": "Bool", "key": "door.target-open"},
        ]
        assert "unreachable.permission" not in artifacts["scripts.behaviors"].decode()

def test_worker_rejects_dynamic_state_key_inside_reachable_helper() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project = root / "StarterArena"
        shutil.copytree(STARTER_ROOT, project)
        source = project / "Scripts" / "DoorController.cs"
        source.write_text(
            """// SPDX-License-Identifier: MIT
using SagaEngine.Scripting;
namespace StarterArena;
public sealed class DoorController : SagaBehavior
{
    public override void OnStart() { Write("door.target-open"); }
    [SagaAction("Interact")]
    public void OnInteract() { Write("door.target-open"); }
    public override void OnUpdate(float deltaTime) { _ = Context.Self.Transform; }
    private void Write(string key) { Context.State.TrySetBool(key, true); }
}
""",
            encoding="utf-8",
        )
        output = root / "output"
        request = prepare_worker_request(root / "work", project, output, "dynamic-state-key")
        result = run_cli("worker", "--request", str(request[0]), "--result", str(request[1]),
                         "--progress", str(request[2]))
        assert result.returncode != 0
        report = json.loads(request[1].read_text(encoding="utf-8"))
        assert any(item["code"] == "Script.Worker.StateKeyNotConstant"
                   for item in report["diagnostics"])
        assert not (output / "manifests" / "current.json").exists()

def test_worker_mid_operation_cancellation_preserves_current_manifest() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project = root / "StarterArena"
        shutil.copytree(STARTER_ROOT, project)
        output = root / "output"
        initial = prepare_worker_request(root / "initial", project, output, "initial-worker")
        assert run_cli("worker", "--request", str(initial[0]), "--result", str(initial[1]), "--progress", str(initial[2])).returncode == 0
        current = output / "manifests" / "current.json"
        before = current.read_bytes()
        request, result, progress, cancellation = prepare_worker_request(root / "cancel", project, output, "cancel-worker", "revision-2")
        process = worker_process(request, result, progress, stage_delay_ms=150)
        wait_for_stage(progress, "Analyzing")
        cancellation.write_text(json.dumps({"schemaVersion": 1, "operationId": "cancel-worker"}) + "\n", encoding="utf-8")
        stdout, stderr = process.communicate(timeout=15)

        assert process.returncode == 1, stderr + stdout
        report = json.loads(result.read_text(encoding="utf-8"))
        assert report["status"] == "Cancelled"
        assert report["failureClass"] == "Cancellation"
        assert report["cancelled"] is True
        assert report["artifacts"] == []
        assert current.read_bytes() == before

def test_worker_stale_result_preserves_current_manifest() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project = root / "StarterArena"
        shutil.copytree(STARTER_ROOT, project)
        output = root / "output"
        initial = prepare_worker_request(root / "initial", project, output, "initial-worker")
        assert run_cli("worker", "--request", str(initial[0]), "--result", str(initial[1]), "--progress", str(initial[2])).returncode == 0
        current = output / "manifests" / "current.json"
        before = current.read_bytes()
        request, result, progress, _ = prepare_worker_request(root / "stale", project, output, "stale-worker", "revision-2")
        process = worker_process(request, result, progress, stage_delay_ms=150)
        wait_for_stage(progress, "Compiling")
        source = project / "Scripts" / "DoorController.cs"
        source.write_text(source.read_text(encoding="utf-8") + "\n", encoding="utf-8")
        stdout, stderr = process.communicate(timeout=15)

        assert process.returncode == 1, stderr + stdout
        report = json.loads(result.read_text(encoding="utf-8"))
        assert report["status"] == "StaleResult"
        assert report["failureClass"] == "StaleResult"
        assert report["stale"] is True
        assert current.read_bytes() == before

def test_worker_final_publication_barrier_preserves_current_manifest() -> None:
    for mutation in ["cancel", "source", "lock", "reference", "api", "bridge"]:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            project = root / "StarterArena"
            shutil.copytree(STARTER_ROOT, project)
            output = root / "output"
            initial = prepare_worker_request(root / "initial", project, output, "initial-worker")
            assert run_cli("worker", "--request", str(initial[0]), "--result", str(initial[1]),
                           "--progress", str(initial[2])).returncode == 0
            current = output / "manifests/current.json"
            before = current.read_bytes()
            request, result, progress, cancellation = prepare_worker_request(
                root / "barrier-work", project, output, "barrier-worker", "revision-2")
            barrier = root / "publication-barrier"
            process = worker_process(request, result, progress, publication_barrier=barrier)
            deadline = time.monotonic() + 20
            while not (barrier.with_suffix(".reached")).exists():
                if process.poll() is not None or time.monotonic() >= deadline:
                    raise AssertionError(f"worker did not reach publication barrier for {mutation}")
                time.sleep(0.01)
            payload = json.loads(request.read_text(encoding="utf-8"))
            if mutation == "cancel":
                cancellation.write_text(json.dumps({"schemaVersion": 1, "operationId": "barrier-worker"}) + "\n", encoding="utf-8")
            elif mutation == "source":
                source = project / "Scripts/DoorController.cs"
                source.write_text(source.read_text(encoding="utf-8") + "\n", encoding="utf-8")
            else:
                key = {"lock": "packageLock", "reference": "referenceManifest", "api": "apiSurfaceManifest"}.get(mutation)
                target = Path(payload[key]["path"]) if key else Path(payload["runtimeBridge"]["assemblyPath"])
                target.write_bytes(target.read_bytes() + b" ")
            barrier.with_suffix(".release").write_text("release\n", encoding="utf-8")
            stdout, stderr = process.communicate(timeout=20)
            assert process.returncode == 1, stderr + stdout
            report = json.loads(result.read_text(encoding="utf-8"))
            assert report["status"] == ("Cancelled" if mutation == "cancel" else "StaleResult")
            assert current.read_bytes() == before

def test_worker_rejects_non_public_and_duplicate_named_actions() -> None:
    for name, transform, code in [
        ("private", lambda text: text.replace("public void OnInteract()", "private void OnInteract()"),
         "Script.Worker.ActionNotInvocable"),
        ("duplicate", lambda text: text.replace("public override void OnUpdate", "[SagaAction(\"Interact\")]\n    public void OnInteractAgain() {}\n\n    public override void OnUpdate"),
         "Script.Worker.ActionDuplicate"),
        ("duplicate-behavior", lambda text: text + "\npublic sealed class DoorController : SagaBehavior {}\n",
         "Script.Worker.BehaviorDuplicate"),
    ]:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            project = root / "StarterArena"
            shutil.copytree(STARTER_ROOT, project)
            source = project / "Scripts/DoorController.cs"
            source.write_text(transform(source.read_text(encoding="utf-8")), encoding="utf-8")
            request, result, progress, _ = prepare_worker_request(root / "work", project, root / "output", name)
            worker = run_cli("worker", "--request", str(request), "--result", str(result), "--progress", str(progress))
            assert worker.returncode == 1
            codes = [item["code"] for item in json.loads(result.read_text(encoding="utf-8"))["diagnostics"]]
            assert code in codes

def test_worker_rejects_missing_scene_action_and_corrupt_reference_metadata() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project = root / "StarterArena"
        shutil.copytree(STARTER_ROOT, project)
        source = project / "Scripts" / "DoorController.cs"
        source.write_text(
            source.read_text(encoding="utf-8").replace('[SagaAction("Interact")]', '[SagaAction("Use")]'),
            encoding="utf-8",
        )
        request, result, progress, _ = prepare_worker_request(root / "action", project, root / "action-output", "missing-action")
        action_result = run_cli("worker", "--request", str(request), "--result", str(result), "--progress", str(progress))
        assert action_result.returncode == 1
        action_report = json.loads(result.read_text(encoding="utf-8"))
        assert action_report["failureClass"] == "Analysis"
        assert "Script.Worker.SceneActionMissing" in [item["code"] for item in action_report["diagnostics"]]

        clean_project = root / "CleanStarterArena"
        shutil.copytree(STARTER_ROOT, clean_project)
        request, result, progress, _ = prepare_worker_request(root / "reference", clean_project, root / "reference-output", "corrupt-reference")
        payload = json.loads(request.read_text(encoding="utf-8"))
        references = Path(payload["referenceManifest"]["path"])
        references.write_text(references.read_text(encoding="utf-8") + " ", encoding="utf-8")
        reference_result = run_cli("worker", "--request", str(request), "--result", str(result), "--progress", str(progress))
        assert reference_result.returncode == 1
        reference_report = json.loads(result.read_text(encoding="utf-8"))
        assert reference_report["failureClass"] == "DependencyValidation"
        assert "Script.Worker.VerifiedInputHashMismatch" in [item["code"] for item in reference_report["diagnostics"]]

def test_worker_publish_failure_is_structured_and_does_not_create_current() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project = root / "StarterArena"
        shutil.copytree(STARTER_ROOT, project)
        output = root / "not-a-directory"
        request, result, progress, _ = prepare_worker_request(root / "publish", project, output, "publish-failure")
        request_payload = json.loads(request.read_text(encoding="utf-8"))
        request_payload["unknownField"] = True
        request.write_text(json.dumps(request_payload, indent=2) + "\n", encoding="utf-8")
        invalid = run_cli("worker", "--request", str(request), "--result", str(result), "--progress", str(progress))
        assert invalid.returncode == 1
        invalid_report = json.loads(result.read_text(encoding="utf-8"))
        assert invalid_report["failureClass"] == "InvalidRequest"
        assert "Script.Worker.UnknownField" in [item["code"] for item in invalid_report["diagnostics"]]

        request_payload.pop("unknownField")
        request.write_text(json.dumps(request_payload, indent=2) + "\n", encoding="utf-8")
        output.write_text("occupied\n", encoding="utf-8")

        worker = run_cli("worker", "--request", str(request), "--result", str(result), "--progress", str(progress))

        assert worker.returncode == 1
        report = json.loads(result.read_text(encoding="utf-8"))
        assert report["failureClass"] == "Publication"
        assert "Script.Worker.PublishFailed" in [item["code"] for item in report["diagnostics"]]
        assert output.is_file()

def test_shared_boundary_scan_remains_clean() -> None:
    matches: list[str] = []
    for path in sorted(SHARED_SCRIPTING_ROOT.rglob("*")):
        if not path.is_file():
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            text = path.read_text(encoding="utf-8", errors="replace")
        for line_number, line in enumerate(text.splitlines(), start=1):
            if any(token in line for token in SHARED_BOUNDARY_FORBIDDEN):
                relative = path.relative_to(REPO_ROOT)
                matches.append(f"{relative}:{line_number}: {line.strip()}")

    assert not matches, "Forbidden shared-boundary references found:\n" + "\n".join(matches)

def test_runtime_bridge_binary_identity_excludes_source_revision() -> None:
    project = RUNTIME_BRIDGE_PROJECT.read_text(encoding="utf-8")
    assert "<Deterministic>true</Deterministic>" in project
    assert "<ContinuousIntegrationBuild>true</ContinuousIntegrationBuild>" in project
    assert "<DebugType>none</DebugType>" in project
    assert "<DebugSymbols>false</DebugSymbols>" in project
    assert (
        "<IncludeSourceRevisionInInformationalVersion>false"
        "</IncludeSourceRevisionInInformationalVersion>"
    ) in project

TESTS = (
    test_project_forms_analyze_compile_and_project_blocks,
    test_project_worker_is_deterministic_across_clean_roots,
    test_arena_rush_worker_aggregates_contract_only_surfaces,
    test_worker_mid_operation_cancellation_preserves_current_manifest,
    test_worker_stale_result_preserves_current_manifest,
    test_worker_final_publication_barrier_preserves_current_manifest,
    test_worker_rejects_non_public_and_duplicate_named_actions,
    test_worker_rejects_missing_scene_action_and_corrupt_reference_metadata,
    test_worker_publish_failure_is_structured_and_does_not_create_current,
    test_shared_boundary_scan_remains_clean,
    test_runtime_bridge_binary_identity_excludes_source_revision,
)
