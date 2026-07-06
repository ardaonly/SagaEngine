#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Arda Koyuncu
# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

from pathlib import Path
import argparse
import json
from typing import Any


def latest_codemodel(reply_dir: Path) -> Path:
    indexes = sorted(
        reply_dir.glob("index-*.json"),
        key=lambda path: path.stat().st_mtime,
    )
    if indexes:
        index = json.loads(indexes[-1].read_text(encoding="utf-8"))
        candidates: list[str] = []

        def walk(value: Any) -> None:
            if isinstance(value, dict):
                if value.get("kind") == "codemodel" and value.get("jsonFile"):
                    version = value.get("version", {})
                    if version.get("major") == 2:
                        candidates.append(value["jsonFile"])
                for child in value.values():
                    walk(child)
            elif isinstance(value, list):
                for child in value:
                    walk(child)

        walk(index)
        if candidates:
            return reply_dir / candidates[-1]

    candidates = sorted(
        reply_dir.glob("codemodel-v2-*.json"),
        key=lambda path: path.stat().st_mtime,
    )
    if not candidates:
        raise FileNotFoundError(f"No CMake File API codemodel in {reply_dir}")
    return candidates[-1]


def collect(build_dir: Path, selected_config: str | None) -> dict:
    reply_dir = build_dir / ".cmake" / "api" / "v1" / "reply"
    codemodel_path = latest_codemodel(reply_dir)
    codemodel = json.loads(codemodel_path.read_text(encoding="utf-8"))

    payload = {
        "schema": "sagaengine.cmake-target-graph.v2",
        "build_dir": str(build_dir.resolve()),
        "codemodel": codemodel_path.name,
        "configurations": [],
    }

    for configuration in codemodel.get("configurations", []):
        config_name = configuration.get("name", "") or "<default>"
        if selected_config and selected_config != config_name:
            continue

        id_to_name = {
            item.get("id"): item.get("name")
            for item in configuration.get("targets", [])
        }
        config_payload = {"name": config_name, "targets": []}

        for summary in configuration.get("targets", []):
            target_path = reply_dir / summary["jsonFile"]
            target = json.loads(target_path.read_text(encoding="utf-8"))
            install = target.get("install")
            destinations = []
            if isinstance(install, dict):
                destinations = [
                    item.get("path")
                    for item in install.get("destinations", [])
                    if item.get("path")
                ]

            config_payload["targets"].append(
                {
                    "name": target.get("name", summary.get("name", "")),
                    "type": target.get("type", ""),
                    "source_directory": target.get("paths", {}).get("source", ""),
                    "build_directory": target.get("paths", {}).get("build", ""),
                    "sources": sorted(
                        [
                            {
                                "path": item.get("path"),
                                "generated": bool(item.get("isGenerated", False)),
                            }
                            for item in target.get("sources", [])
                            if item.get("path")
                        ],
                        key=lambda item: item["path"],
                    ),
                    "dependencies": sorted(
                        {
                            id_to_name.get(dep.get("id"), dep.get("id", ""))
                            for dep in target.get("dependencies", [])
                            if dep.get("id")
                        }
                    ),
                    "install_destinations": sorted(set(destinations)),
                    "artifacts": sorted(
                        {
                            item.get("path")
                            for item in target.get("artifacts", [])
                            if item.get("path")
                        }
                    ),
                }
            )

        config_payload["targets"].sort(key=lambda item: item["name"])
        payload["configurations"].append(config_payload)

    if selected_config and not payload["configurations"]:
        raise ValueError(f"CMake configuration not found: {selected_config}")
    return payload


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("build_dir", type=Path)
    parser.add_argument("--config")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    payload = collect(args.build_dir.resolve(), args.config)
    text = json.dumps(payload, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text, encoding="utf-8", newline="\n")
        print(args.output)
    else:
        print(text, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
