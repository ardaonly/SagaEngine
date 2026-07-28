# SPDX-License-Identifier: MPL-2.0

from .common import *  # noqa: F403
from .layout import staged_layout_missing, stale_package_outputs

def tar_supports_zstd() -> bool:
    if shutil.which("tar") is None or shutil.which("zstd") is None:
        return False
    result = subprocess.run(
        ["tar", "--help"],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
    )
    return result.returncode == 0 and "--zstd" in result.stdout


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def archive_root_is_valid(archive_path: Path, root_name: str) -> bool:
    result = subprocess.run(
        ["tar", "-tf", str(archive_path)],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if result.returncode != 0:
        return False
    entries = [line.strip() for line in result.stdout.splitlines() if line.strip()]
    return bool(entries) and all(entry == root_name or entry.startswith(f"{root_name}/") for entry in entries)


def generate_archive_and_checksum(
    *,
    output: Path,
    archive_path: Path,
    checksum_path: Path,
    missing: list[str],
    required_inputs: list[dict[str, object]],
    available_inputs: list[dict[str, object]],
    missing_inputs: list[dict[str, object]],
) -> dict[str, object]:
    stale_package_outputs(archive_path, checksum_path)
    status: dict[str, object] = {
        "archivePath": path_text(archive_path),
        "checksumPath": path_text(checksum_path),
        "status": "blocked",
    }

    layout_missing = staged_layout_missing(output)
    layout_ready = not layout_missing
    layout_message = None
    if not layout_ready:
        layout_message = "staged layout incomplete; archive/checksum generation skipped: " + ", ".join(layout_missing)
        missing.append(layout_message)
    record_check(
        label="staged layout completeness for archive",
        kind="directory",
        category="archive",
        paths=[output],
        available=layout_ready,
        required_inputs=required_inputs,
        available_inputs=available_inputs,
        missing_inputs=missing_inputs,
        resolved_path=output if layout_ready else None,
        message=layout_message,
    )
    if not layout_ready:
        status["message"] = layout_message
        return status

    tool_ready = tar_supports_zstd()
    tool_paths = [Path(shutil.which("tar") or "tar"), Path(shutil.which("zstd") or "zstd")]
    tool_message = None
    if not tool_ready:
        tool_message = "archive tool: missing tar with --zstd support or zstd executable"
        missing.append(tool_message)
    record_check(
        label="tar zstd archive tools",
        kind="tool",
        category="archive-tool",
        paths=tool_paths,
        available=tool_ready,
        required_inputs=required_inputs,
        available_inputs=available_inputs,
        missing_inputs=missing_inputs,
        resolved_path=tool_paths[0] if tool_ready else None,
        message=tool_message,
    )
    if not tool_ready:
        status["message"] = tool_message
        return status

    archive_command = [
        "tar",
        "--zstd",
        "--sort=name",
        "--mtime=@0",
        "--owner=0",
        "--group=0",
        "--numeric-owner",
        "-cf",
        str(archive_path),
        "-C",
        str(output.parent),
        output.name,
    ]
    archive_result = subprocess.run(
        archive_command,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    archive_ready = (
        archive_result.returncode == 0
        and archive_path.is_file()
        and archive_path.stat().st_size > 0
        and archive_root_is_valid(archive_path, output.name)
    )
    archive_message = None
    if not archive_ready:
        stale_package_outputs(archive_path, checksum_path)
        archive_error = archive_result.stderr.strip() or "archive generation failed"
        archive_message = f"archive generation failed: {archive_error}"
        missing.append(archive_message)
    record_check(
        label="archive generation",
        kind="file",
        category="archive",
        paths=[archive_path],
        available=archive_ready,
        required_inputs=required_inputs,
        available_inputs=available_inputs,
        missing_inputs=missing_inputs,
        resolved_path=archive_path if archive_ready else None,
        message=archive_message,
    )
    if not archive_ready:
        status["message"] = archive_message
        return status

    checksum = sha256_file(archive_path)
    checksum_path.write_text(f"{checksum}  {archive_path.name}\n", encoding="utf-8")
    checksum_ready = checksum_path.is_file() and checksum_path.stat().st_size > 0
    checksum_message = None
    if not checksum_ready:
        checksum_message = f"checksum generation failed: {checksum_path}"
        missing.append(checksum_message)
    record_check(
        label="checksum generation",
        kind="file",
        category="checksum",
        paths=[checksum_path],
        available=checksum_ready,
        required_inputs=required_inputs,
        available_inputs=available_inputs,
        missing_inputs=missing_inputs,
        resolved_path=checksum_path if checksum_ready else None,
        message=checksum_message,
    )
    if not checksum_ready:
        return {
            "archivePath": path_text(archive_path),
            "checksumPath": path_text(checksum_path),
            "status": "blocked",
            "message": checksum_message,
        }

    return {
        "archivePath": path_text(archive_path),
        "archiveRoot": output.name,
        "checksumPath": path_text(checksum_path),
        "checksumAlgorithm": "sha256",
        "status": "available",
        "nonClaims": [
            "Archive/checksum generation does not claim production readiness.",
            "Archive/checksum generation does not claim enterprise readiness.",
            "Archive/checksum generation does not claim a verified release.",
            "Archive/checksum generation does not claim a verified final release.",
            "Archive/checksum generation does not claim full distribution validation.",
        ],
    }
