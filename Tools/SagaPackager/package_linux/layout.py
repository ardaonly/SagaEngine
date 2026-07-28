# SPDX-License-Identifier: MPL-2.0

from .common import *  # noqa: F403
from .dotnet_tools import stage_packaged_dotnet_tools

def verification_text() -> str:
    return f"""# Saga Linux Layout Verification

This file describes presence checks for the staged layout and archive/checksum
outputs. Passing these commands does not prove production readiness, enterprise
readiness, verified release status, final release verification, full
distribution validation, workflow correctness, or maintainer verification.

Run from the staged layout root:

```bash
test -f VERSION
test -f KNOWN_LIMITATIONS.md
python3 -m json.tool BUILD_INFO.json
test -x bin/Saga
test -x bin/SagaRuntime
test -x tools/sagaproject
test -x tools/sagapack
test -x tools/sagascript
test -x bin/sagaproject
test -x bin/sagapack
test -x bin/sagascript
test -d samples/StarterArena
test -d Engine/Distribution
test -f Engine/Managed/SagaScript.RuntimeBridge/SagaScript.RuntimeBridge.dll
test -f README.md
test -f SagaWiki/index.html
test -f SagaWiki/assets/style.css
test -f SagaWiki/assets/wiki.js
test -f SagaWiki/pages/licensing.html
test -d licenses
test -f licenses/LICENSE-SAGA.md
test -f licenses/THIRD_PARTY_NOTICES.md
```

Run from the parent `build/dist/linux` directory after package generation:

```bash
sha256sum -c Saga.sha256
tar -tf Saga.tar.zst
```

The archive/checksum checks only prove that `Saga.tar.zst` exists, that
`Saga.sha256` covers that archive, and that the archive can be listed.
"""


def known_limitations_text() -> str:
    return """# Saga Linux Layout Known Limitations

This staged layout is not a final distribution.

- Not production-ready.
- Not enterprise-ready.
- Not a verified release.
- Not a verified final release.
- Not full distribution validation.
- Archive/checksum evidence is limited to `Saga.tar.zst` and `Saga.sha256`.
- Packaged .NET tool wrappers dispatch to real framework-dependent published artifacts and require a compatible .NET runtime.
- No full editor UI is claimed.
- No Visual Blocks editor UI is claimed.
- No enterprise collaboration is claimed.
- No cloud workspace is claimed.
- Passing `scripts/package` still does not establish package or release readiness.
- The Python archive does not stage native shared-library dependency closure and is not a clean-machine portability claim.
- No dedicated-server executable is included.
- Runtime, editor, and tool workflows are proven only where the current smoke report records a passing command.
- No delivery stage is marked verified by this staged layout.
"""


def stage_layout(
    *,
    output: Path,
    build_dir: Path,
    build_type: str,
    available_inputs: list[dict[str, object]],
    missing: list[str],
    required_inputs: list[dict[str, object]],
    missing_inputs: list[dict[str, object]],
) -> dict[str, object]:
    staged_inputs: list[dict[str, object]] = []

    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True, exist_ok=True)

    binaries = [
        ("Saga product shell", "Saga"),
        ("SagaRuntime executable", "SagaRuntime"),
    ]
    for label, name in binaries:
        copy_file(resolved_input(available_inputs, label), output / "bin" / name, staged_inputs, label)

    tool_packaging_status = stage_packaged_dotnet_tools(
        output=output,
        staged_inputs=staged_inputs,
        missing=missing,
        required_inputs=required_inputs,
        available_inputs=available_inputs,
        missing_inputs=missing_inputs,
    )

    copy_tree(resolved_input(available_inputs, "StarterArena sample"), output / "samples" / "StarterArena", staged_inputs, "StarterArena sample")
    copy_tree(
        resolved_input(available_inputs, "engine package distribution"),
        output / "Engine" / "Distribution",
        staged_inputs,
        "engine package distribution",
    )
    copy_file(
        resolved_input(available_inputs, "SagaScript runtime bridge"),
        output / "Engine" / "Managed" / "SagaScript.RuntimeBridge" /
            "SagaScript.RuntimeBridge.dll",
        staged_inputs,
        "SagaScript runtime bridge",
    )
    copy_file(resolved_input(available_inputs, "root README"), output / "README.md", staged_inputs, "root README top-level")
    wiki = resolved_input(available_inputs, "SagaWiki")
    for relative in [
        Path("index.html"),
        Path("assets/style.css"),
        Path("assets/wiki.js"),
        Path("pages/licensing.html"),
    ]:
        copy_file(
            wiki / relative,
            output / "SagaWiki" / relative,
            staged_inputs,
            f"SagaWiki {relative.as_posix()}",
        )
    copy_tree(resolved_input(available_inputs, "licenses"), output / "licenses", staged_inputs, "licenses")
    copy_file(
        resolved_input(available_inputs, "Saga license"),
        output / "licenses" / "LICENSE-SAGA.md",
        staged_inputs,
        "Saga license",
    )
    copy_file(
        resolved_input(available_inputs, "third-party notices"),
        output / "licenses" / "THIRD_PARTY_NOTICES.md",
        staged_inputs,
        "third-party notices",
    )

    version_source = resolved_input(available_inputs, "root VERSION")
    copy_file(version_source, output / "VERSION", staged_inputs, "root VERSION")

    (output / "VERIFY.txt").write_text(verification_text(), encoding="utf-8")
    staged_inputs.append(
        {
            "label": "staged VERIFY.txt",
            "kind": "generated-metadata",
            "source": "scripts/package",
            "destination": path_text(output / "VERIFY.txt"),
        }
    )
    (output / "KNOWN_LIMITATIONS.md").write_text(known_limitations_text(), encoding="utf-8")
    staged_inputs.append(
        {
            "label": "staged known limitations",
            "kind": "generated-metadata",
            "source": "scripts/package",
            "destination": path_text(output / "KNOWN_LIMITATIONS.md"),
        }
    )

    staged_inputs.append(
        {
            "label": "staged BUILD_INFO.json",
            "kind": "generated-metadata",
            "source": "scripts/package",
            "destination": path_text(output / "BUILD_INFO.json"),
        }
    )

    source_commit = git_commit()
    built_at_utc = datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")
    included_targets = application_names()
    public_tools = public_tool_names()
    license_files = sorted(
        path.relative_to(output).as_posix()
        for path in (output / "licenses").rglob("*")
        if path.is_file()
    )
    build_info: dict[str, object] = {
        "schemaVersion": PACKAGE_SCHEMA_VERSION,
        "packageName": PACKAGE_NAME,
        "product": "SagaEngine",
        "channel": "project-readiness-candidate",
        "platform": "linux",
        "version": read_version(),
        "commit": source_commit,
        "buildType": build_type,
        "builtAtUtc": built_at_utc,
        "includedTargets": included_targets,
        "publicTools": public_tools,
        "includedApplications": included_applications(),
        "includedPublicTools": included_public_tools(),
        "excludedDevTools": list(EXCLUDED_DEV_TOOLS),
        "excludedRetiredTools": list(EXCLUDED_RETIRED_TOOLS),
        "excludedSourceSurfaces": list(EXCLUDED_SOURCE_SURFACES),
        "licenseFiles": license_files,
        "notices": "licenses/THIRD_PARTY_NOTICES.md",
        "thirdPartyManifests": [],
        "limitations": list(LIMITATIONS),
        "samples": ["StarterArena"],
        "knownLimitationsFile": "KNOWN_LIMITATIONS.md",
        "verification": {
            "verified": False,
            "status": "layout-staged-for-archive",
            "packageReport": "build/reports/linux_package_preflight_report.json",
            "smokeReport": "build/reports/linux_distribution_smoke_report.json",
            "candidateReport": "build/reports/linux_distributable_candidate_report.json",
            "contractReport": "build/reports/linux_distribution_contract_report.json",
        },
        "tool": "scripts/package",
        "status": "layout-staged-for-archive",
        "verified": False,
        "archiveTarget": path_text(archive_path_for(output)),
        "buildDir": path_text(build_dir),
        "checksumTarget": path_text(checksum_path_for(output)),
        "output": path_text(output),
        "stagedInputs": staged_inputs,
        "toolPackagingStatus": tool_packaging_status,
        "missingInputs": missing,
        "staging": {
            "stagedInputs": staged_inputs,
            "toolPackagingStatus": tool_packaging_status,
            "missingInputs": missing,
        },
        "finalAcceptanceGaps": [
            "lib/ is not staged unless real shared library distribution artifacts are produced.",
            "include/ is not staged unless real public SDK headers are selected for distribution.",
            "share/saga/ is not staged unless real templates, profiles, schemas, plugins, or CMake package files are selected for distribution.",
            "Packaged Runtime and Editor workflows remain bounded by the current smoke reports.",
        ],
        "nonClaims": [
            "This layout is not a final distribution.",
            "This layout does not claim production readiness.",
            "This layout does not claim enterprise readiness.",
            "This layout does not claim a verified release.",
            "This layout does not claim a verified final release.",
            "This layout does not claim full distribution validation.",
            "This layout does not claim full gameplay readiness.",
            "This layout does not claim full editor workflow.",
            "This layout does not claim full Visual Blocks UI.",
            "This layout does not claim full packaged tool workflow validation.",
            "This layout does not mark any delivery stage verified.",
        ],
    }
    if source_commit is not None:
        build_info["sourceCommit"] = source_commit

    (output / "BUILD_INFO.json").write_text(
        json.dumps(build_info, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )

    return {
        "status": "staged",
        "output": path_text(output),
        "stagedInputs": staged_inputs,
        "toolPackagingStatus": tool_packaging_status,
        "nonClaims": build_info["nonClaims"],
    }


def stale_package_outputs(archive_path: Path, checksum_path: Path) -> None:
    for path in [checksum_path, archive_path]:
        if path.exists():
            path.unlink()


def clean_package_outputs(output: Path, archive_path: Path, checksum_path: Path) -> None:
    stale_package_outputs(archive_path, checksum_path)
    if output.exists():
        if output.is_dir() and not output.is_symlink():
            shutil.rmtree(output)
        else:
            output.unlink()


def staged_layout_missing(output: Path) -> list[str]:
    required_files = [
        "VERSION",
        "VERIFY.txt",
        "KNOWN_LIMITATIONS.md",
        "BUILD_INFO.json",
        "README.md",
        "bin/Saga",
        "bin/SagaRuntime",
        "bin/sagaproject",
        "bin/sagapack",
        "bin/sagascript",
        "tools/sagaproject",
        "tools/sagapack",
        "tools/sagascript",
        "licenses/LICENSE-SAGA.md",
        "licenses/THIRD_PARTY_NOTICES.md",
        "SagaWiki/index.html",
        "SagaWiki/assets/style.css",
        "SagaWiki/assets/wiki.js",
        "SagaWiki/pages/licensing.html",
        "Engine/Managed/SagaScript.RuntimeBridge/SagaScript.RuntimeBridge.dll",
    ]
    required_dirs = [
        "samples/StarterArena",
        "Engine/Distribution",
        "SagaWiki/assets",
        "SagaWiki/pages",
        "licenses",
    ]

    missing_entries: list[str] = []
    if not output.is_dir():
        missing_entries.append(path_text(output))
        return missing_entries

    executable_rels = {
        "bin/Saga",
        "bin/SagaRuntime",
        "bin/sagaproject",
        "bin/sagapack",
        "bin/sagascript",
        "tools/sagaproject",
        "tools/sagapack",
        "tools/sagascript",
    }
    for rel in required_files:
        path = output / rel
        if rel in executable_rels:
            if not is_executable(path):
                missing_entries.append(path_text(path))
        elif not path.is_file():
            missing_entries.append(path_text(path))
    for rel in required_dirs:
        path = output / rel
        if not path.is_dir():
            missing_entries.append(path_text(path))
    return missing_entries
