"""Read-only and dry-run actions exposed by SagaSync."""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class SafeAction:
    label: str
    command: tuple[str, ...]


def export_plan_action() -> SafeAction:
    return SafeAction(label="Export Dry Run", command=("scripts/export", "plan"))


def export_all_dry_run_action() -> SafeAction:
    return SafeAction(label="Export All Dry Run", command=("scripts/export", "--dry-run"))


def export_tool_dry_run_action(tool_name: str) -> SafeAction:
    return SafeAction(
        label=f"Export {tool_name} Dry Run",
        command=("scripts/export", tool_name.lower(), "--dry-run"),
    )


def verification_actions() -> tuple[SafeAction, ...]:
    from .verification import quick_profile

    return quick_profile().actions


def default_toolbar_actions() -> tuple[SafeAction, ...]:
    return (
        export_plan_action(),
        export_all_dry_run_action(),
        export_tool_dry_run_action("Forge"),
    )
