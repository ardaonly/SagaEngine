"""Non-GUI SagaSync smoke model."""

from __future__ import annotations

from pathlib import Path

from .commit_plan import build_commit_plan
from .commit_queue import build_commit_queue
from .export_health import evaluate_export_health
from .export_status import load_export_manifest, load_export_states
from .git_status import get_git_status
from .run_history import health_display_status
from .verification import verification_profiles


def build_smoke_payload(repo_root: Path) -> dict[str, object]:
    manifest = load_export_manifest(repo_root)
    states = load_export_states(repo_root, manifest)
    status = get_git_status(repo_root)
    queue = build_commit_queue(status, manifest)
    plan = build_commit_plan(status, manifest)
    health = evaluate_export_health(manifest, states, status)
    return {
        "repo": str(repo_root),
        "branch": status.branch,
        "head": status.head,
        "dirtyCount": status.dirty_count,
        "tools": [tool.name for tool in manifest.tools],
        "states": sorted(states.keys()),
        "queueItems": len(queue),
        "commitPlan": {
            "groups": len(plan.groups),
            "blocked": plan.blocked_count,
            "messages": list(plan.suggested_messages),
        },
        "exportHealth": {item.tool: item.status for item in health},
        "exportHealthDisplay": {item.tool: health_display_status(item, None) for item in health},
        "verificationProfiles": [profile.name for profile in verification_profiles()],
        "verificationResults": {},
    }
