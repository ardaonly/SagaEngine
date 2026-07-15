#!/usr/bin/env python3
"""Canonical whitelist policy for the default Linux Saga candidate package."""

from __future__ import annotations

import hashlib
import os
from pathlib import Path, PurePosixPath
from typing import Any, Iterable


PACKAGE_NAME = "Saga"
PACKAGE_PLATFORM = "linux"
PACKAGE_SCHEMA_VERSION = 2

PRODUCT_APPLICATIONS = (
    {"name": "Saga", "path": "bin/Saga", "role": "product-shell"},
    {"name": "SagaEditor", "path": "bin/SagaEditor", "role": "editor"},
    {"name": "SagaRuntime", "path": "bin/SagaRuntime", "role": "runtime"},
)

PUBLIC_TOOLS = (
    {
        "name": "sagapack",
        "wrapperPaths": ["bin/sagapack", "tools/sagapack"],
        "payloadRoot": "tools/.saga-tools/sagapack",
    },
    {
        "name": "sagaproject",
        "wrapperPaths": ["bin/sagaproject", "tools/sagaproject"],
        "payloadRoot": "tools/.saga-tools/sagaproject",
    },
    {
        "name": "sagascript",
        "wrapperPaths": ["bin/sagascript", "tools/sagascript"],
        "payloadRoot": "tools/.saga-tools/sagascript",
    },
)

ALLOWED_ROOT_FILES = frozenset(
    {"BUILD_INFO.json", "KNOWN_LIMITATIONS.md", "README.md", "VERIFY.txt", "VERSION"}
)
ALLOWED_TOP_LEVEL_DIRECTORIES = frozenset(
    {"bin", "SagaWiki", "licenses", "samples", "tools"}
)
APPROVED_DOCUMENTS = frozenset(
    {
        "SagaWiki/assets/sandbox-render-01.png",
        "SagaWiki/assets/sandbox-render-02.png",
        "SagaWiki/assets/style.css",
        "SagaWiki/index.html",
        "SagaWiki/pages/architecture.html",
        "SagaWiki/pages/editor.html",
        "SagaWiki/pages/getting-started.html",
        "SagaWiki/pages/licensing.html",
        "SagaWiki/pages/module-boundaries.html",
        "SagaWiki/pages/not-implemented.html",
        "SagaWiki/pages/product.html",
        "SagaWiki/pages/repository-layout.html",
        "SagaWiki/pages/runtime.html",
        "SagaWiki/pages/scripting.html",
        "SagaWiki/pages/server-authority.html",
        "SagaWiki/pages/source-of-truth.html",
        "SagaWiki/pages/testing.html",
        "SagaWiki/pages/toolchain.html",
    }
)
REQUIRED_LICENSE_PATHS = frozenset(
    {
        "licenses/LICENSE-SAGA.md",
        "licenses/THIRD_PARTY_NOTICES.md",
    }
)
ALLOWED_EXECUTABLE_PATHS = frozenset(
    {item["path"] for item in PRODUCT_APPLICATIONS}
    | {path for item in PUBLIC_TOOLS for path in item["wrapperPaths"]}
    | {f"{item['payloadRoot']}/{item['name']}" for item in PUBLIC_TOOLS}
)
ALLOWED_TREE_PREFIXES = tuple(
    sorted(
        ["licenses/", "samples/StarterArena/"]
        + [f"{item['payloadRoot']}/" for item in PUBLIC_TOOLS]
    )
)

FORBIDDEN_EXECUTABLE_NAMES = frozenset(
    {
        "EditorLab",
        "MultiplayerSandboxHeadless",
        "Prism",
        "RenderClientSmokeTest",
        "SDE",
        "SagaChaosLab",
        "SagaClient",
        "SagaSandbox",
        "SagaServer",
        "SagaStateCheck",
        "SagaStressArena",
    }
)
EXCLUDED_DEV_TOOLS = (
    {"name": "EditorLab", "reason": "developer editor lab"},
    {"name": "SagaSandbox", "reason": "developer/manual sandbox"},
    {"name": "SagaStressArena", "reason": "internal stress and soak evidence"},
    {"name": "SagaChaosLab", "reason": "internal chaos evidence"},
    {"name": "SagaStateCheck", "reason": "internal deterministic validation"},
    {"name": "MultiplayerSandboxHeadless", "reason": "repository-only sample evidence"},
    {"name": "RenderClientSmokeTest", "reason": "test/probe identity"},
    {"name": "Forge", "reason": "developer build orchestrator"},
    {"name": "SagaTools", "reason": "developer repository tooling"},
    {"name": "test-executables", "reason": "test-only binaries"},
)
EXCLUDED_RETIRED_TOOLS = (
    {"name": "Prism", "reason": "retired tool surface"},
    {"name": "SDE", "reason": "retired tool surface"},
    {"name": "SagaClient", "reason": "retired legacy client executable"},
    {"name": "SagaServer", "reason": "retired server executable; SagaServerLib remains legal"},
)
EXCLUDED_SOURCE_SURFACES = ()

LIMITATIONS = (
    "The packager is Linux-only and must be run from the Saga repository root.",
    "Archive generation requires tar with Zstandard support and zstd; checksum generation uses SHA-256.",
    "The Python archive does not stage native shared-library dependency closure or claim clean-machine portability.",
    "Packaged public tools are framework-dependent and require a compatible .NET runtime; publishing requires a .NET 10 SDK.",
    "Nix-store and nix-shell tool discovery are environment-specific fallbacks.",
    "SagaEditor is presence-checked but no GUI is launched by headless archive smoke.",
    "SagaRuntime and editor workflow evidence remains bounded by the current smoke report.",
    "The package is not signed and does not include an installer or updater.",
    "No SBOM or machine-readable third-party manifest is included.",
    "Passing whitelist, archive, or smoke checks does not establish release or package readiness.",
    "No dedicated-server executable is included.",
)


def application_names() -> list[str]:
    return sorted(str(item["name"]) for item in PRODUCT_APPLICATIONS)


def public_tool_names() -> list[str]:
    return sorted(str(item["name"]) for item in PUBLIC_TOOLS)


def included_applications() -> list[dict[str, str]]:
    return sorted((dict(item) for item in PRODUCT_APPLICATIONS), key=lambda item: item["name"])


def included_public_tools() -> list[dict[str, Any]]:
    return sorted((dict(item) for item in PUBLIC_TOOLS), key=lambda item: item["name"])


def _normal_rel(path: str) -> str:
    pure = PurePosixPath(path)
    text = pure.as_posix()
    while text.startswith("./"):
        text = text[2:]
    return text


def is_allowed_file_path(path: str) -> bool:
    rel = _normal_rel(path)
    if rel in ALLOWED_ROOT_FILES or rel in APPROVED_DOCUMENTS or rel in ALLOWED_EXECUTABLE_PATHS:
        return True
    return any(rel.startswith(prefix) and rel != prefix.rstrip("/") for prefix in ALLOWED_TREE_PREFIXES)


def is_allowed_directory_path(path: str) -> bool:
    rel = _normal_rel(path).rstrip("/")
    if rel in ALLOWED_TOP_LEVEL_DIRECTORIES:
        return True
    exact_files = ALLOWED_ROOT_FILES | APPROVED_DOCUMENTS | REQUIRED_LICENSE_PATHS | ALLOWED_EXECUTABLE_PATHS
    if any(file_path.startswith(f"{rel}/") for file_path in exact_files):
        return True
    for prefix in ALLOWED_TREE_PREFIXES:
        tree = prefix.rstrip("/")
        if tree == rel or tree.startswith(f"{rel}/") or rel.startswith(f"{tree}/"):
            return True
    return False


def _is_forbidden_name(name: str) -> bool:
    folded = name.casefold()
    return any(folded == forbidden.casefold() for forbidden in FORBIDDEN_EXECUTABLE_NAMES)


def _looks_like_test_executable(rel: str) -> bool:
    path = PurePosixPath(rel)
    folded_parts = [part.casefold() for part in path.parts]
    name = path.name.casefold()
    return any(part in {"test", "tests"} for part in folded_parts) or "test" in name


def validate_layout(root: Path) -> list[str]:
    """Return deterministic blocking policy violations for a staged filesystem."""
    errors: list[str] = []
    if not root.is_dir():
        return [f"missing distribution layout: {root}"]

    for required in sorted(ALLOWED_ROOT_FILES | APPROVED_DOCUMENTS | REQUIRED_LICENSE_PATHS | ALLOWED_EXECUTABLE_PATHS):
        path = root / required
        if required in ALLOWED_EXECUTABLE_PATHS:
            if not path.is_file() or path.stat().st_size == 0 or not bool(path.stat().st_mode & 0o111):
                errors.append(f"missing required executable: {required}")
        elif not path.is_file():
            errors.append(f"missing required file: {required}")

    sample_root = root / "samples" / "StarterArena"
    if not sample_root.is_dir():
        errors.append("missing required directory: samples/StarterArena")

    for path in sorted(root.rglob("*"), key=lambda item: item.relative_to(root).as_posix()):
        rel = path.relative_to(root).as_posix()
        top = PurePosixPath(rel).parts[0]
        if top not in ALLOWED_TOP_LEVEL_DIRECTORIES and rel not in ALLOWED_ROOT_FILES:
            errors.append(f"unknown top-level package path: {rel}")
        if path.is_dir():
            if not is_allowed_directory_path(rel):
                errors.append(f"unapproved package directory: {rel}")
            if _looks_like_test_executable(rel):
                errors.append(f"test payload directory is forbidden in package: {rel}")
            for surface in EXCLUDED_SOURCE_SURFACES:
                if rel == surface["path"] or rel.startswith(f"{surface['path']}/"):
                    errors.append(f"excluded source surface in package: {rel}")
            continue
        if not is_allowed_file_path(rel):
            errors.append(f"unapproved package file: {rel}")
        basename = path.name
        executable = path.is_file() and bool(path.stat().st_mode & 0o111)
        if _is_forbidden_name(basename):
            errors.append(f"forbidden executable basename in package: {rel}")
        if executable and _looks_like_test_executable(rel):
            errors.append(f"test executable is forbidden in package: {rel}")
        if executable and rel not in ALLOWED_EXECUTABLE_PATHS:
            errors.append(f"unrecognized executable-bit file: {rel}")
        for surface in EXCLUDED_SOURCE_SURFACES:
            surface_parts = tuple(PurePosixPath(str(surface["path"])).parts)
            rel_parts = tuple(PurePosixPath(rel).parts)
            if any(rel_parts[index : index + len(surface_parts)] == surface_parts for index in range(len(rel_parts))):
                errors.append(f"excluded source surface in package: {rel}")
                break

    return sorted(set(errors))


def validate_archive_entries(entries: Iterable[str], root_name: str = PACKAGE_NAME) -> list[str]:
    """Validate names before extraction; executable modes are checked after extraction."""
    errors: list[str] = []
    seen_file = False
    prefix = f"{root_name}/"
    for raw in entries:
        entry = raw.strip()
        if not entry:
            continue
        if entry in {root_name, prefix}:
            continue
        if not entry.startswith(prefix):
            errors.append(f"archive entry is outside {root_name}/: {entry}")
            continue
        rel = entry[len(prefix) :].rstrip("/")
        pure = PurePosixPath(rel)
        if not rel or pure.is_absolute() or ".." in pure.parts:
            errors.append(f"unsafe archive entry: {entry}")
            continue
        if entry.endswith("/"):
            if not is_allowed_directory_path(rel):
                errors.append(f"unapproved archive directory: {rel}")
            if _looks_like_test_executable(rel):
                errors.append(f"test payload directory is forbidden in archive: {rel}")
            continue
        seen_file = True
        if not is_allowed_file_path(rel):
            errors.append(f"unapproved archive file: {rel}")
        if _is_forbidden_name(pure.name):
            errors.append(f"forbidden executable basename in archive: {rel}")
        for surface in EXCLUDED_SOURCE_SURFACES:
            surface_parts = tuple(PurePosixPath(str(surface["path"])).parts)
            rel_parts = tuple(pure.parts)
            if any(rel_parts[index : index + len(surface_parts)] == surface_parts for index in range(len(rel_parts))):
                errors.append(f"excluded source surface in archive: {rel}")
                break
    if not seen_file:
        errors.append("archive contains no package files")
    return sorted(set(errors))


def validate_metadata_sets(data: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    expected_apps = application_names()
    expected_tools = public_tool_names()
    if data.get("includedTargets") != expected_apps:
        errors.append(f"includedTargets must equal {expected_apps}")
    if data.get("publicTools") != expected_tools:
        errors.append(f"publicTools must equal {expected_tools}")

    app_objects = data.get("includedApplications")
    if app_objects != included_applications():
        errors.append("includedApplications must equal the canonical application policy")
    tool_objects = data.get("includedPublicTools")
    if tool_objects != included_public_tools():
        errors.append("includedPublicTools must equal the canonical public-tool policy")
    if data.get("excludedDevTools") != list(EXCLUDED_DEV_TOOLS):
        errors.append("excludedDevTools must equal the canonical exclusion policy")
    if data.get("excludedRetiredTools") != list(EXCLUDED_RETIRED_TOOLS):
        errors.append("excludedRetiredTools must equal the canonical exclusion policy")
    if data.get("excludedSourceSurfaces") != list(EXCLUDED_SOURCE_SURFACES):
        errors.append("excludedSourceSurfaces must equal the canonical exclusion policy")
    return errors


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def staged_file_inventory(root: Path) -> list[dict[str, Any]]:
    inventory: list[dict[str, Any]] = []
    if not root.is_dir():
        return inventory
    for path in sorted(root.rglob("*"), key=lambda item: item.relative_to(root).as_posix()):
        if path.is_dir():
            continue
        rel = path.relative_to(root).as_posix()
        if path.is_symlink():
            inventory.append(
                {
                    "path": rel,
                    "kind": "symlink",
                    "executable": False,
                    "linkTarget": os.readlink(path),
                }
            )
            continue
        inventory.append(
            {
                "path": rel,
                "kind": "file",
                "sizeBytes": path.stat().st_size,
                "sha256": sha256_file(path),
                "executable": bool(path.stat().st_mode & 0o111),
            }
        )
    return inventory


def inventory_matches(root: Path, inventory: Iterable[dict[str, Any]]) -> bool:
    return list(inventory) == staged_file_inventory(root)
