# SPDX-License-Identifier: MPL-2.0
from __future__ import annotations

from .support.environment import *
from .support.evidence import *
from .support.templates import *

def test_template_plan_is_non_mutating_and_create_is_atomic() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        destination = root / "CreatedProject"
        plan_path = root / "plan.json"
        report_path = root / "create.json"
        plan_result = run_cli(
            "template-plan",
            "--destination",
            str(destination),
            "--display-name",
            "Created Project",
            "--template-id",
            "starter-arena",
            "--template",
            str(REPO_ROOT / "Samples" / "StarterArena"),
            "--out",
            str(plan_path),
        )
        assert plan_result.returncode == 0, plan_result.stderr
        assert not destination.exists()
        plan = json.loads(plan_path.read_text(encoding="utf-8"))
        assert plan["planHash"].startswith("sha256:")
        assert len(plan["projectId"]) == 36
        planned_paths = [item["path"] for item in plan["files"]]
        assert "SagaPackage.lock" not in planned_paths
        assert "SagaPackage.lock.license" in planned_paths

        create = run_cli("template-create", "--plan", str(plan_path), "--sagapack", str(SAGAPACK),
                         "--engine-root", str(REPO_ROOT), "--runtime-bridge", str(RUNTIME_BRIDGE),
                         "--out", str(report_path))
        assert create.returncode == 0, create.stderr
        report = json.loads(report_path.read_text(encoding="utf-8"))
        assert report["status"] == "Passed"
        assert destination.is_dir()
        manifest = json.loads((destination / "StarterArena.sagaproj").read_text(encoding="utf-8"))
        assert manifest["projectId"] == plan["projectId"]
        assert manifest["displayName"] == "Created Project"
        lock_path = destination / "SagaPackage.lock"
        assert lock_path.exists()
        assert (destination / "SagaPackage.lock.license").exists()
        lock = json.loads(lock_path.read_text(encoding="utf-8"))
        assert lock["projectId"] == plan["projectId"]
        assert lock["projectHash"] == report["projectHash"]
        assert lock["projectId"] != json.loads((REPO_ROOT / "Samples/StarterArena/SagaPackage.lock").read_text(encoding="utf-8"))["projectId"]
        for item in plan["files"]:
            if item["path"] == "StarterArena.sagaproj":
                continue
            assert (destination / item["path"]).read_bytes() == (REPO_ROOT / "Samples/StarterArena" / item["path"]).read_bytes()
        verify = subprocess.run([str(SAGAPACK), "lock-verify", "--project", str(destination / "StarterArena.sagaproj"),
                                 "--engine-root", str(REPO_ROOT), "--lock", str(lock_path),
                                 "--generated-out", str(destination / "Build" / "Packages"),
                                 "--runtime-bridge", str(RUNTIME_BRIDGE), "--out", str(root / "verify.json")],
                                cwd=REPO_ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
        assert verify.returncode == 0, verify.stderr + verify.stdout
        assert not list(root.glob(".CreatedProject.saga-staging-*"))

def test_template_create_rejects_tampered_plan_without_destination() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        destination = root / "MustRemainAbsent"
        plan_path = root / "plan.json"
        report_path = root / "create.json"
        result = run_cli(
            "template-plan",
            "--destination",
            str(destination),
            "--template-id",
            "starter-arena",
            "--template",
            str(REPO_ROOT / "Samples" / "StarterArena"),
            "--out",
            str(plan_path),
        )
        assert result.returncode == 0
        plan = json.loads(plan_path.read_text(encoding="utf-8"))
        plan["displayName"] = "Tampered"
        write_json(plan_path, plan)

        create = run_cli("template-create", "--plan", str(plan_path), "--sagapack", str(SAGAPACK),
                         "--engine-root", str(REPO_ROOT), "--runtime-bridge", str(RUNTIME_BRIDGE),
                         "--out", str(report_path))
        assert create.returncode == 1
        assert not destination.exists()
        report = json.loads(report_path.read_text(encoding="utf-8"))
        assert report["diagnostics"][0]["code"] == "Project.Template.CreateFailed"
        assert not list(root.glob(".MustRemainAbsent.saga-staging-*"))

def test_arena_rush_template_identity_and_substitution() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        destination = root / "EnergyChallenge"
        plan_path = root / "arena-plan.json"
        report_path = root / "arena-create.json"
        planned = run_cli(
            "template-plan",
            "--template-id", "arena-rush",
            "--template", str(REPO_ROOT / "Samples/ArenaRush"),
            "--destination", str(destination),
            "--display-name", "My Energy Challenge",
            "--out", str(plan_path),
        )
        assert planned.returncode == 0, planned.stderr + planned.stdout
        plan = json.loads(plan_path.read_text(encoding="utf-8"))
        assert plan["templateId"] == "arena-rush"
        assert plan["declaredSubstitutions"] == ["displayName", "projectId"]
        created = run_cli(
            "template-create", "--plan", str(plan_path),
            "--sagapack", str(SAGAPACK),
            "--engine-root", str(REPO_ROOT),
            "--runtime-bridge", str(RUNTIME_BRIDGE),
            "--out", str(report_path),
        )
        assert created.returncode == 0, created.stderr + created.stdout
        manifest = json.loads(
            (destination / "ArenaRush.sagaproj").read_text(encoding="utf-8")
        )
        assert manifest["projectId"] == plan["projectId"]
        assert manifest["displayName"] == "My Energy Challenge"
        assert (destination / "Scripts/ArenaRushGameMode.cs").is_file()
        assert (destination / "Content/UI/ThirdParty/DejaVu/DejaVuSans.ttf").is_file()
        assert json.loads(report_path.read_text(encoding="utf-8"))["status"] == "Passed"

def test_arena_rush_source_truth_and_structural_failures() -> None:
    arena = REPO_ROOT / "Samples/ArenaRush"
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        report = root / "checked-in.json"
        validated = run_cli(
            "validate", "--project", str(arena / "ArenaRush.sagaproj"),
            "--out", str(report),
        )
        assert validated.returncode == 0, validated.stderr + validated.stdout
        scene_path = arena / "Scenes/ArenaRush.sagascene"
        scene_text = scene_path.read_text(encoding="utf-8")
        for transient in (
            "score", "remainingTime", "lifecycleState", "respawnTimers",
            "occupancy", "velocity", "results",
        ):
            assert transient not in scene_text
        scene = json.loads(scene_text)
        entities = {item["entityId"]: item for item in scene["entities"]}
        game_mode = next(
            component
            for component in entities["game-mode"]["components"]
            if component["type"] == "ScriptBehavior"
        )
        assert game_mode["instanceConfiguration"] == {
            "activeCollectibleCount": 5,
            "challengeDurationSeconds": 60,
            "coreValue": 10,
            "respawnDelaySeconds": 2,
            "targetProgress": 100,
        }
        assert [item["type"] for item in
                entities["challenge-start-marker"]["components"]] == [
            "Transform", "TriggerVolume", "ScriptEventBinding",
        ]
        for marker in (
            "spawn-01", "spawn-02", "spawn-03", "spawn-04", "spawn-05",
        ):
            assert [item["type"] for item in entities[marker]["components"]] == [
                "Transform",
            ]
        for entity in scene["entities"]:
            component_types = [item["type"] for item in entity["components"]]
            assert len(component_types) == len(set(component_types))

        project = root / "ArenaRush"
        shutil.copytree(arena, project, ignore=shutil.ignore_patterns("Build"))
        manifest_path = project / "ArenaRush.sagaproj"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        manifest["scenes"][0]["path"] = "../outside.sagascene"
        write_json(manifest_path, manifest)
        unsafe = run_cli(
            "validate", "--project", str(manifest_path),
            "--out", str(root / "unsafe.json"),
        )
        assert unsafe.returncode == 1

        manifest["scenes"][0]["path"] = "Scenes/missing.sagascene"
        write_json(manifest_path, manifest)
        missing = run_cli(
            "validate", "--project", str(manifest_path),
            "--out", str(root / "missing.json"),
        )
        assert missing.returncode == 1

        manifest["schemaVersion"] = 99
        write_json(manifest_path, manifest)
        unknown = run_cli(
            "validate", "--project", str(manifest_path),
            "--out", str(root / "schema.json"),
        )
        assert unknown.returncode == 1

def test_duplicate_project_scene_identity_is_rejected() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project = root / "StarterArena"
        shutil.copytree(REPO_ROOT / "Samples/StarterArena", project)
        manifest_path = project / "StarterArena.sagaproj"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        manifest["scenes"].append(dict(manifest["scenes"][0]))
        write_json(manifest_path, manifest)
        report = root / "report.json"
        result = run_cli("validate", "--project", str(manifest_path), "--out", str(report))
        assert result.returncode == 1
        codes = [item["code"] for item in json.loads(report.read_text(encoding="utf-8"))["diagnostics"]]
        assert "Project.Scene.DuplicateId" in codes

def test_template_packager_failure_leaves_no_destination_or_staging() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        destination = root / "PackageFailure"
        plan = root / "plan.json"
        report = root / "report.json"
        assert run_cli("template-plan", "--destination", str(destination),
                       "--template-id", "starter-arena", "--template",
                       str(REPO_ROOT / "Samples/StarterArena"), "--out", str(plan)).returncode == 0
        result = run_cli("template-create", "--plan", str(plan), "--sagapack", str(root / "missing-sagapack"),
                         "--engine-root", str(REPO_ROOT), "--runtime-bridge", str(RUNTIME_BRIDGE),
                         "--out", str(report))
        assert result.returncode == 1
        assert not destination.exists()
        assert not list(root.glob(".PackageFailure.saga-staging-*"))

def test_template_packager_nonzero_is_structured_and_cleans_staging() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        destination, plan, report = template_plan_for(root, "NonZero")
        fake = root / "fake-packager"
        write_fake_packager(fake, "printf 'captured-stdout\\n'\nprintf 'captured-stderr\\n' >&2\nexit 17\n")
        result = run_cli("template-create", "--plan", str(plan), "--sagapack", str(fake),
                         "--engine-root", str(REPO_ROOT), "--runtime-bridge", str(RUNTIME_BRIDGE),
                         "--out", str(report))
        assert result.returncode == 1
        diagnostic = json.loads(report.read_text(encoding="utf-8"))["diagnostics"][0]
        assert diagnostic["code"] == "Project.Template.SagaPackagerFailed"
        assert "exit code 17" in diagnostic["message"]
        assert "captured-stdout" in diagnostic["message"]
        assert "captured-stderr" in diagnostic["message"]
        assert not destination.exists()
        assert not list(root.glob(".NonZero.saga-staging-*"))

def test_template_packager_timeout_kills_tree_and_cleans_staging() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        destination, plan, report = template_plan_for(root, "Timeout")
        fake = root / "fake-packager"
        write_fake_packager(fake, "sleep 30\n")
        result = run_cli_env({"SAGAPROJECT_TEMPLATE_PACKAGER_TIMEOUT_MS": "200"},
                             "template-create", "--plan", str(plan), "--sagapack", str(fake),
                             "--engine-root", str(REPO_ROOT), "--runtime-bridge", str(RUNTIME_BRIDGE),
                             "--out", str(report))
        assert result.returncode == 1
        diagnostic = json.loads(report.read_text(encoding="utf-8"))["diagnostics"][0]
        assert diagnostic["code"] == "Project.Template.SagaPackagerTimeout"
        assert not destination.exists()
        assert not list(root.glob(".Timeout.saga-staging-*"))

def test_template_packager_heavy_stderr_drains_without_deadlock() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        destination, plan, report = template_plan_for(root, "HeavyStderr")
        fake = root / "fake-packager"
        write_fake_packager(fake, """
command="$1"
shift
lock=""
while [ "$#" -gt 0 ]; do
  if [ "$1" = "--lock" ]; then shift; lock="$1"; fi
  shift || true
done
i=0
while [ "$i" -lt 30000 ]; do printf 'heavy-stderr-%s-xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\\n' "$i" >&2; i=$((i + 1)); done
if [ "$command" = "resolve" ]; then printf '{}\\n' > "$lock"; fi
exit 0
""")
        result = run_cli_env({"SAGAPROJECT_TEMPLATE_PACKAGER_TIMEOUT_MS": "20000"},
                             "template-create", "--plan", str(plan), "--sagapack", str(fake),
                             "--engine-root", str(REPO_ROOT), "--runtime-bridge", str(RUNTIME_BRIDGE),
                             "--out", str(report))
        assert result.returncode == 0, result.stderr + result.stdout
        assert destination.exists()
        assert not list(root.glob(".HeavyStderr.saga-staging-*"))

TESTS = (
    test_template_plan_is_non_mutating_and_create_is_atomic,
    test_template_create_rejects_tampered_plan_without_destination,
    test_arena_rush_template_identity_and_substitution,
    test_arena_rush_source_truth_and_structural_failures,
    test_duplicate_project_scene_identity_is_rejected,
    test_template_packager_failure_leaves_no_destination_or_staging,
    test_template_packager_nonzero_is_structured_and_cleans_staging,
    test_template_packager_timeout_kills_tree_and_cleans_staging,
    test_template_packager_heavy_stderr_drains_without_deadlock,
)
