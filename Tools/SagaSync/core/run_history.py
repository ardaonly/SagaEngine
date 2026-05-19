"""In-memory verification run history for SagaSync."""

from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime, timezone

from .export_health import ExportHealth, HEALTH_READY


RUN_NOT_RUN = "not run"
RUN_PASSED = "passed"
RUN_FAILED = "failed"

DISPLAY_READY_UNVERIFIED = "Ready but unverified"
DISPLAY_BLOCKED_BY_VERIFICATION = "Blocked by verification"


@dataclass(frozen=True)
class VerificationRunResult:
    profile: str
    started_at: str
    duration_ms: int
    exit_code: int
    status: str
    command_summary: str


def make_run_result(
    *,
    profile: str,
    started_at: datetime,
    duration_ms: int,
    exit_code: int,
    command_summary: str,
) -> VerificationRunResult:
    started_utc = started_at.astimezone(timezone.utc)
    return VerificationRunResult(
        profile=profile,
        started_at=started_utc.isoformat(timespec="seconds"),
        duration_ms=max(0, duration_ms),
        exit_code=exit_code,
        status=RUN_PASSED if exit_code == 0 else RUN_FAILED,
        command_summary=command_summary,
    )


def profile_display_status(result: VerificationRunResult | None) -> str:
    return result.status if result is not None else RUN_NOT_RUN


def health_display_status(
    health: ExportHealth,
    verification_result: VerificationRunResult | None,
) -> str:
    if verification_result is not None and verification_result.status == RUN_FAILED:
        return DISPLAY_BLOCKED_BY_VERIFICATION
    if health.status == HEALTH_READY and verification_result is None:
        return DISPLAY_READY_UNVERIFIED
    return health.status


def health_display_reason(
    health: ExportHealth,
    verification_result: VerificationRunResult | None,
) -> str:
    reasons = list(health.reasons)
    if verification_result is not None and verification_result.status == RUN_FAILED:
        reasons.append("verification profile failed")
    elif health.status == HEALTH_READY and verification_result is None:
        reasons.append("verification profile has not run")
    return "; ".join(reasons)
