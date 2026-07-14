"""Verification profile definitions for SagaSync."""

from __future__ import annotations

from dataclasses import dataclass

from .actions import SafeAction, export_plan_action


@dataclass(frozen=True)
class VerificationProfile:
    name: str
    label: str
    actions: tuple[SafeAction, ...]

    @property
    def command_summary(self) -> str:
        return " && ".join(" ".join(action.command) for action in self.actions)


def quick_profile() -> VerificationProfile:
    return VerificationProfile(
        name="quick",
        label="Quick",
        actions=(
            SafeAction(label="Whitespace Check", command=("git", "diff", "--check")),
            export_plan_action(),
        ),
    )


def forge_profile() -> VerificationProfile:
    return VerificationProfile(
        name="forge",
        label="Forge",
        actions=(
            SafeAction(label="Forge Build", command=("python3", "Tools/Forge/build.py", "-j", "1")),
            SafeAction(
                label="Forge CTest",
                command=("ctest", "--test-dir", "Tools/Forge/build", "--output-on-failure"),
            ),
        ),
    )


def export_safety_profile() -> VerificationProfile:
    return VerificationProfile(
        name="export-safety",
        label="Export Safety",
        actions=(export_plan_action(),),
    )


def verification_profiles() -> tuple[VerificationProfile, ...]:
    return (
        quick_profile(),
        forge_profile(),
        export_safety_profile(),
    )
