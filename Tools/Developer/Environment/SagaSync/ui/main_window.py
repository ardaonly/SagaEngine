"""Main SagaSync dashboard window."""

from __future__ import annotations

from datetime import datetime, timezone
from pathlib import Path
from time import monotonic

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QPlainTextEdit,
    QTableWidget,
    QTableWidgetItem,
    QVBoxLayout,
    QWidget,
)

from core.actions import SafeAction, default_toolbar_actions
from core.commands import run_command
from core.commit_plan import build_commit_plan, summarize_files
from core.commit_queue import build_commit_queue
from core.export_health import evaluate_export_health
from core.export_status import load_export_manifest, load_export_states
from core.git_status import get_git_status
from core.run_history import (
    VerificationRunResult,
    health_display_reason,
    health_display_status,
    make_run_result,
    profile_display_status,
)
from core.verification import VerificationProfile, verification_profiles


def _format_command(command: tuple[str, ...]) -> str:
    return " ".join(command)


class SagaSyncWindow(QMainWindow):
    def __init__(self, repo_root: Path) -> None:
        super().__init__()
        self.repo_root = repo_root
        self.profile_results: dict[str, VerificationRunResult] = {}
        self.setWindowTitle("SagaSync")
        self.resize(1100, 720)

        central = QWidget()
        layout = QVBoxLayout(central)
        self.setCentralWidget(central)

        self.banner = QLabel()
        self.banner.setWordWrap(True)
        layout.addWidget(self.banner)

        buttons = QHBoxLayout()
        self.refresh_button = QPushButton("Refresh")
        buttons.addWidget(self.refresh_button)
        for action in default_toolbar_actions():
            button = QPushButton(action.label)
            button.clicked.connect(lambda _checked=False, current=action: self.run_and_log(current))
            buttons.addWidget(button)
        self.verify_button = QPushButton("Run Verification")
        buttons.addWidget(self.verify_button)
        buttons.addStretch(1)
        layout.addLayout(buttons)

        self.repo_group = QGroupBox("Repository Overview")
        self.repo_grid = QGridLayout(self.repo_group)
        layout.addWidget(self.repo_group)

        self.queue_table = QTableWidget(0, 4)
        self.queue_table.setHorizontalHeaderLabels(["#", "Step", "Status", "Hint"])
        layout.addWidget(self.queue_table)

        self.commit_plan_table = QTableWidget(0, 6)
        self.commit_plan_table.setHorizontalHeaderLabels(
            ["#", "Group", "Files", "Suggested Message", "Blocked", "Reason"]
        )
        layout.addWidget(self.commit_plan_table)

        self.health_table = QTableWidget(0, 5)
        self.health_table.setHorizontalHeaderLabels(["Tool", "Health", "Source", "Remote", "Reason"])
        layout.addWidget(self.health_table)

        self.profile_table = QTableWidget(0, 6)
        self.profile_table.setHorizontalHeaderLabels(["Profile", "Last Result", "Last Run", "Duration", "Commands", "Run"])
        layout.addWidget(self.profile_table)

        self.log = QPlainTextEdit()
        self.log.setReadOnly(True)
        layout.addWidget(self.log, stretch=1)

        self.refresh_button.clicked.connect(self.refresh)
        self.verify_button.clicked.connect(self.run_verification)

        self.refresh()

    def append_log(self, text: str) -> None:
        self.log.appendPlainText(text.rstrip())

    def run_and_log(self, action: SafeAction) -> None:
        self.append_log(f"$ {_format_command(action.command)}")
        try:
            result = run_command(action.command, self.repo_root)
        except Exception as exc:
            QMessageBox.critical(self, "SagaSync command failed", str(exc))
            return
        output = result.combined_output()
        if output:
            self.append_log(output)
        self.append_log(f"[exit {result.returncode}]")
        self.refresh()

    def run_verification(self) -> None:
        for profile in verification_profiles():
            self.run_profile(profile)

    def run_profile(self, profile: VerificationProfile) -> None:
        started_at = datetime.now(timezone.utc)
        started_monotonic = monotonic()
        passed = True
        last_exit_code = 0
        for action in profile.actions:
            self.append_log(f"$ {_format_command(action.command)}")
            try:
                result = run_command(action.command, self.repo_root)
            except Exception as exc:
                QMessageBox.critical(self, "SagaSync command failed", str(exc))
                duration_ms = int((monotonic() - started_monotonic) * 1000)
                self.profile_results[profile.name] = make_run_result(
                    profile=profile.name,
                    started_at=started_at,
                    duration_ms=duration_ms,
                    exit_code=1,
                    command_summary=profile.command_summary,
                )
                self.refresh()
                return
            output = result.combined_output()
            if output:
                self.append_log(output)
            self.append_log(f"[exit {result.returncode}]")
            last_exit_code = result.returncode
            if not result.passed:
                passed = False
        duration_ms = int((monotonic() - started_monotonic) * 1000)
        self.profile_results[profile.name] = make_run_result(
            profile=profile.name,
            started_at=started_at,
            duration_ms=duration_ms,
            exit_code=0 if passed else last_exit_code or 1,
            command_summary=profile.command_summary,
        )
        self.refresh()

    def _clear_grid(self) -> None:
        while self.repo_grid.count():
            item = self.repo_grid.takeAt(0)
            widget = item.widget()
            if widget is not None:
                widget.deleteLater()

    def refresh(self) -> None:
        try:
            manifest = load_export_manifest(self.repo_root)
            states = load_export_states(self.repo_root, manifest)
            status = get_git_status(self.repo_root)
            queue = build_commit_queue(status, manifest)
            plan = build_commit_plan(status, manifest)
            health = evaluate_export_health(manifest, states, status)
            profiles = verification_profiles()
            export_profile_result = self.profile_results.get("export-safety")
        except Exception as exc:
            QMessageBox.critical(self, "SagaSync refresh failed", str(exc))
            return

        self._clear_grid()
        overview = [
            ("SagaEngine", status.branch, f"dirty: {status.dirty_count}", f"ahead: {status.ahead}"),
        ]
        for tool in manifest.tools:
            state = states.get(tool.name)
            exported = state.source_short if state else "no state"
            synced = "synced" if state and state.synced else "needs check"
            overview.append((f"{tool.name} mirror", manifest.target_branch, exported, synced))

        headers = ("Repo", "Branch", "State", "Remote")
        for column, header in enumerate(headers):
            label = QLabel(f"<b>{header}</b>")
            self.repo_grid.addWidget(label, 0, column)
        for row, values in enumerate(overview, start=1):
            for column, value in enumerate(values):
                self.repo_grid.addWidget(QLabel(str(value)), row, column)

        self.queue_table.setRowCount(len(queue))
        for row, item in enumerate(queue):
            state = "blocked" if item.blocked else "ready"
            values = (str(item.order), item.title, state, item.command_hint)
            for column, value in enumerate(values):
                cell = QTableWidgetItem(value)
                if column == 2 and item.blocked:
                    cell.setForeground(Qt.GlobalColor.red)
                self.queue_table.setItem(row, column, cell)
        self.queue_table.resizeColumnsToContents()

        self.commit_plan_table.setRowCount(len(plan.groups))
        for row, group in enumerate(plan.groups):
            values = (
                str(group.order),
                group.label,
                summarize_files(group.files),
                group.suggested_message,
                "yes" if group.blocked else "no",
                group.reason,
            )
            for column, value in enumerate(values):
                cell = QTableWidgetItem(value)
                if column == 4 and group.blocked:
                    cell.setForeground(Qt.GlobalColor.red)
                self.commit_plan_table.setItem(row, column, cell)
        self.commit_plan_table.resizeColumnsToContents()

        self.health_table.setRowCount(len(health))
        for row, item in enumerate(health):
            display_status = health_display_status(item, export_profile_result)
            values = (
                item.tool,
                display_status,
                item.source,
                item.remote,
                health_display_reason(item, export_profile_result),
            )
            for column, value in enumerate(values):
                cell = QTableWidgetItem(value)
                if column == 1:
                    if display_status == "Ready":
                        cell.setForeground(Qt.GlobalColor.darkGreen)
                    elif display_status.startswith("Blocked"):
                        cell.setForeground(Qt.GlobalColor.red)
                    else:
                        cell.setForeground(Qt.GlobalColor.darkYellow)
                self.health_table.setItem(row, column, cell)
        self.health_table.resizeColumnsToContents()

        self.profile_table.setRowCount(len(profiles))
        for row, profile in enumerate(profiles):
            result = self.profile_results.get(profile.name)
            values = (
                profile.label,
                profile_display_status(result),
                result.started_at if result else "never",
                f"{result.duration_ms} ms" if result else "",
                profile.command_summary,
            )
            for column, value in enumerate(values):
                self.profile_table.setItem(row, column, QTableWidgetItem(value))
            button = QPushButton("Run")
            button.clicked.connect(lambda _checked=False, current=profile: self.run_profile(current))
            self.profile_table.setCellWidget(row, 5, button)
        self.profile_table.resizeColumnsToContents()

        if status.clean:
            self.banner.setText("SagaEngine clean. Run export dry-run before mirror publishing.")
        else:
            self.banner.setText(
                f"SagaEngine has {status.dirty_count} changed file(s). "
                "Finalize source changes before exporting mirrors."
            )
