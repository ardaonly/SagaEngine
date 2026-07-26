# SPDX-License-Identifier: MPL-2.0

from .archive import *  # noqa: F403
from .common import *  # noqa: F403
from .dotnet_tools import *  # noqa: F403
from .layout import *  # noqa: F403

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", default="Release", help="Build configuration label.")
    parser.add_argument("--build-dir", help="CMake build directory to inspect.")
    parser.add_argument("--output", default=str(DEFAULT_OUTPUT), help="Expected final distribution directory.")
    parser.add_argument(
        "--report-out",
        default=str(DEFAULT_REPORT),
        help="Machine-readable preflight report path.",
    )
    args = parser.parse_args()

    build_dir_candidates = [Path(args.build_dir)] if args.build_dir else default_build_dirs(args.config)
    build_dir = Path(args.build_dir) if args.build_dir else first_existing(build_dir_candidates)
    output = Path(args.output)
    archive_path = archive_path_for(output)
    checksum_path = checksum_path_for(output)
    report_path = Path(args.report_out)
    try:
        report_path.resolve().relative_to(output.resolve())
    except ValueError:
        pass
    else:
        parser.error("--report-out must be outside the staged package directory")
    missing: list[str] = []
    required_inputs: list[dict[str, object]] = []
    available_inputs: list[dict[str, object]] = []
    missing_inputs: list[dict[str, object]] = []

    # A failed invocation must never leave an older or partially staged package
    # looking like the output of this invocation.
    clean_package_outputs(output, archive_path, checksum_path)

    package_name_ready = output.name == PACKAGE_NAME
    package_name_message = None if package_name_ready else (
        f"staging directory basename must be {PACKAGE_NAME!r}, got {output.name!r}"
    )
    if package_name_message:
        missing.append(package_name_message)
    record_check(
        label="package staging identity",
        kind="policy",
        category="environment",
        paths=[output],
        available=package_name_ready,
        required_inputs=required_inputs,
        available_inputs=available_inputs,
        missing_inputs=missing_inputs,
        resolved_path=output if package_name_ready else None,
        message=package_name_message,
    )

    linux_ready = sys.platform.startswith("linux")
    linux_message = None if linux_ready else f"unsupported platform for Linux package: {sys.platform}"
    if linux_message:
        missing.append(linux_message)
    record_check(
        label="Linux host platform",
        kind="platform",
        category="environment",
        paths=[Path(sys.platform)],
        available=linux_ready,
        required_inputs=required_inputs,
        available_inputs=available_inputs,
        missing_inputs=missing_inputs,
        message=linux_message,
    )
    repo_root_ready = Path("scripts/package-linux-saga").is_file() and Path("VERSION").is_file()
    repo_root_message = None if repo_root_ready else "package-linux-saga must run from the repository root"
    if repo_root_message:
        missing.append(repo_root_message)
    record_check(
        label="repository-root working directory",
        kind="directory",
        category="environment",
        paths=[Path.cwd()],
        available=repo_root_ready,
        required_inputs=required_inputs,
        available_inputs=available_inputs,
        missing_inputs=missing_inputs,
        resolved_path=Path.cwd() if repo_root_ready else None,
        message=repo_root_message,
    )

    if build_dir is None:
        message = "build directory: no candidate build directory exists"
        missing.append(message)
        record_check(
            label="build directory",
            kind="directory",
            category="build",
            paths=build_dir_candidates,
            available=False,
            required_inputs=required_inputs,
            available_inputs=available_inputs,
            missing_inputs=missing_inputs,
            message=message,
        )
        build_dir = Path("build")
    else:
        record_check(
            label="build directory",
            kind="directory",
            category="build",
            paths=build_dir_candidates,
            available=True,
            required_inputs=required_inputs,
            available_inputs=available_inputs,
            missing_inputs=missing_inputs,
            resolved_path=build_dir,
        )

    bin_dir = build_dir / "bin"
    check_binary("Saga product shell", [bin_dir / "Saga"], missing, required_inputs, available_inputs, missing_inputs, category="binary")
    check_binary("SagaEditor shell", [bin_dir / "SagaEditor"], missing, required_inputs, available_inputs, missing_inputs, category="binary")
    check_binary("SagaRuntime executable", [bin_dir / "SagaRuntime"], missing, required_inputs, available_inputs, missing_inputs, category="binary")
    check_binary("sagaproject CLI", [Path("Tools/SagaProjectKit/sagaproject")], missing, required_inputs, available_inputs, missing_inputs, category="tool")
    check_binary("sagapack CLI", [Path("Tools/SagaPackager/sagapack")], missing, required_inputs, available_inputs, missing_inputs, category="tool")
    check_binary("sagascript CLI", [Path("Tools/SagaScript/sagascript")], missing, required_inputs, available_inputs, missing_inputs, category="tool")
    check_dir("StarterArena sample", Path("Samples/StarterArena"), missing, required_inputs, available_inputs, missing_inputs, category="sample")
    check_dir("SagaWiki", Path("SagaWiki"), missing, required_inputs, available_inputs, missing_inputs, category="documentation")
    check_file("root VERSION", Path("VERSION"), missing, required_inputs, available_inputs, missing_inputs, category="metadata")
    check_file("root README", Path("README.md"), missing, required_inputs, available_inputs, missing_inputs, category="documentation")
    check_dir("licenses", Path("LICENSES"), missing, required_inputs, available_inputs, missing_inputs, category="documentation")
    check_file("Saga license", Path("LICENSE"), missing, required_inputs, available_inputs, missing_inputs, category="compliance")
    check_file(
        "third-party notices",
        Path("LICENSES/THIRD_PARTY_NOTICES.md"),
        missing,
        required_inputs,
        available_inputs,
        missing_inputs,
        category="compliance",
    )

    staging_status: dict[str, object]
    if missing:
        stale_package_outputs(archive_path, checksum_path)
        staging_status = {
            "status": "blocked",
            "output": path_text(output),
            "message": "layout staging skipped because required real inputs are missing",
        }
    else:
        try:
            staging_status = stage_layout(
                output=output,
                build_dir=build_dir,
                build_type=args.config,
                available_inputs=available_inputs,
                missing=missing,
                required_inputs=required_inputs,
                missing_inputs=missing_inputs,
            )
        except Exception as exc:  # Preserve a report while removing partial output.
            message = f"layout staging failed: {exc}"
            missing.append(message)
            missing_inputs.append(
                {
                    "label": "layout staging",
                    "kind": "directory",
                    "category": "staging",
                    "paths": [path_text(output)],
                    "status": "missing",
                    "message": message,
                }
            )
            staging_status = {"status": "blocked", "output": path_text(output), "message": message}

    if output.is_dir() and not missing:
        policy_errors = validate_layout(output)
        try:
            build_info_data = json.loads((output / "BUILD_INFO.json").read_text(encoding="utf-8"))
            policy_errors.extend(validate_metadata_sets(build_info_data))
        except (OSError, json.JSONDecodeError) as exc:
            policy_errors.append(f"BUILD_INFO policy validation failed: {exc}")
        policy_errors = sorted(set(policy_errors))
        policy_ready = not policy_errors
        if policy_errors:
            missing.extend(policy_errors)
        record_check(
            label="exact distribution whitelist",
            kind="policy",
            category="distribution-layout",
            paths=[output],
            available=policy_ready,
            required_inputs=required_inputs,
            available_inputs=available_inputs,
            missing_inputs=missing_inputs,
            resolved_path=output if policy_ready else None,
            message=None if policy_ready else "; ".join(policy_errors),
        )

    archive_generation_status: dict[str, object] = {
        "archivePath": path_text(archive_path),
        "checksumPath": path_text(checksum_path),
        "status": "skipped",
    }
    if not missing:
        archive_generation_status = generate_archive_and_checksum(
            output=output,
            archive_path=archive_path,
            checksum_path=checksum_path,
            missing=missing,
            required_inputs=required_inputs,
            available_inputs=available_inputs,
            missing_inputs=missing_inputs,
        )

    check_dir("expected final distribution layout", output, missing, required_inputs, available_inputs, missing_inputs, category="distribution-layout")
    check_file("expected distribution VERSION", output / "VERSION", missing, required_inputs, available_inputs, missing_inputs, category="distribution-layout")
    check_file("expected distribution VERIFY.txt", output / "VERIFY.txt", missing, required_inputs, available_inputs, missing_inputs, category="distribution-layout")
    check_file("expected distribution known limitations", output / "KNOWN_LIMITATIONS.md", missing, required_inputs, available_inputs, missing_inputs, category="distribution-layout")
    check_file("expected distribution README", output / "README.md", missing, required_inputs, available_inputs, missing_inputs, category="distribution-layout")
    check_file("expected Saga license", output / "licenses" / "LICENSE-SAGA.md", missing, required_inputs, available_inputs, missing_inputs, category="distribution-layout")
    check_file("expected third-party notices", output / "licenses" / "THIRD_PARTY_NOTICES.md", missing, required_inputs, available_inputs, missing_inputs, category="distribution-layout")
    for tool_name in ["sagaproject", "sagapack", "sagascript"]:
        check_binary(
            f"expected distribution public {tool_name}",
            [output / "bin" / tool_name],
            missing,
            required_inputs,
            available_inputs,
            missing_inputs,
            category="distribution-layout",
        )
    for doc_name in [
        "index.html",
        "pages/editor.html",
        "pages/getting-started.html",
        "pages/not-implemented.html",
        "pages/product.html",
        "pages/runtime.html",
        "pages/scripting.html",
        "pages/server-authority.html",
        "pages/testing.html",
        "pages/toolchain.html",
        "assets/style.css",
    ]:
        check_file(
            f"expected distribution doc {doc_name}",
            output / "SagaWiki" / doc_name,
            missing,
            required_inputs,
            available_inputs,
            missing_inputs,
            category="distribution-layout",
        )
    check_file("expected distribution archive", archive_path, missing, required_inputs, available_inputs, missing_inputs, category="archive")
    check_file("expected distribution checksum", checksum_path, missing, required_inputs, available_inputs, missing_inputs, category="checksum")

    staged_files = staged_file_inventory(output)
    license_files = sorted(
        item["path"] for item in staged_files if str(item.get("path", "")).startswith("licenses/")
    )
    if missing:
        clean_package_outputs(output, archive_path, checksum_path)
        staged_files = []
        license_files = []

    status = "blocked" if missing else "preflight-passed"
    distribution_items = [
        item for item in required_inputs if item.get("category") == "distribution-layout"
    ]
    distribution_missing = [
        item for item in distribution_items if item.get("status") == "missing"
    ]
    distribution_available = [
        item for item in distribution_items if item.get("status") == "available"
    ]
    report: dict[str, object] = {
        "schemaVersion": PACKAGE_SCHEMA_VERSION,
        "packageName": PACKAGE_NAME,
        "version": read_version(),
        "gitCommit": git_commit(),
        "platform": PACKAGE_PLATFORM,
        "tool": "package-linux-saga",
        "status": status,
        "verified": False,
        "config": {
            "config": args.config,
            "buildDirArgument": args.build_dir,
            "outputArgument": args.output,
            "reportOut": path_text(report_path),
        },
        "buildDir": path_text(build_dir),
        "stagingDir": path_text(output),
        "archivePath": path_text(archive_path),
        "checksumPath": path_text(checksum_path),
        "expectedOutput": path_text(output),
        "stagedFiles": staged_files,
        "includedApplications": included_applications(),
        "includedPublicTools": included_public_tools(),
        "includedTargets": application_names(),
        "publicTools": public_tool_names(),
        "excludedDevTools": list(EXCLUDED_DEV_TOOLS),
        "excludedRetiredTools": list(EXCLUDED_RETIRED_TOOLS),
        "excludedSourceSurfaces": list(EXCLUDED_SOURCE_SURFACES),
        "licenseFiles": license_files,
        "notices": "licenses/THIRD_PARTY_NOTICES.md",
        "thirdPartyManifests": [],
        "limitations": list(LIMITATIONS),
        "requiredInputs": required_inputs,
        "availableInputs": available_inputs,
        "missingInputs": missing_inputs,
        "distributionLayoutStatus": {
            "expectedPath": path_text(output),
            "status": "blocked" if missing or distribution_missing else "available",
            "availableEntries": distribution_available,
            "missingEntries": distribution_missing,
        },
        "stagingStatus": staging_status,
        "archiveGenerationStatus": archive_generation_status,
        "archiveStatus": {
            "path": path_text(archive_path),
            "status": status_for_label(required_inputs, "expected distribution archive"),
        },
        "checksumStatus": {
            "path": path_text(checksum_path),
            "status": status_for_label(required_inputs, "expected distribution checksum"),
        },
        "diagnostics": missing
        if missing
        else [
            "preflight passed for currently checked inputs",
            "layout staging completed from real existing inputs",
            "archive/checksum generated from the staged layout",
            "production, enterprise, and verified final release readiness are not claimed",
        ],
        "knownLimitations": list(LIMITATIONS) + [
            "This script stages only the Linux layout directory from real existing inputs.",
            "This script does not build missing engine binaries.",
            "This script publishes sagaproject, sagascript, and sagapack only from real existing .NET projects when a .NET 10 SDK is available.",
            "This script creates only Saga.tar.zst and Saga.sha256 from the staged layout.",
            "This script stages sagaproject, sagascript, and sagapack from real framework-dependent dotnet publish outputs.",
            "Archive/checksum generation does not prove runtime, editor, or tool workflows beyond recorded smoke commands.",
            "Archive/checksum generation does not prove full distribution validation.",
            "A passing preflight report is not maintainer verification.",
        ],
        "nonClaims": [
            "This report does not claim production readiness; the package is not production-ready.",
            "This report does not claim enterprise readiness; the package is not enterprise-ready.",
            "This report does not claim a verified release.",
            "This report does not claim a verified final release.",
            "This report does not claim full distribution validation.",
            "This report does not claim full editor UI readiness.",
            "This report does not claim Visual Blocks editor UI readiness.",
            "This report does not claim cloud collaboration readiness.",
            "This report does not prove runtime, editor, or tool workflows beyond recorded checks.",
            "This report does not include or validate a dedicated-server executable.",
            "This report does not mark any delivery stage verified.",
        ],
    }
    write_report(report_path, report)

    print("package-linux-saga: layout staging plus archive/checksum preflight")
    print(f"package-linux-saga: config={args.config}")
    print(f"package-linux-saga: build_dir={build_dir}")
    print(f"package-linux-saga: expected_output={output}")
    print(f"package-linux-saga: staging_status={staging_status.get('status')}")
    print(f"package-linux-saga: archive_status={archive_generation_status.get('status')}")

    if missing:
        print("package-linux-saga: failed; required real inputs/output layout are missing:")
        for item in missing:
            print(f"- {item}")
        return 1

    print("package-linux-saga: preflight passed; archive/checksum generated without readiness claims")
    return 0



