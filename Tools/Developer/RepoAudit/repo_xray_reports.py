# SPDX-License-Identifier: MPL-2.0
"""Deterministic human- and machine-readable Repo Xray report writers."""

import csv
import json
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

from repo_xray_support import wiki_area, write_text


def write_json(data: dict[str, Any], out: Path) -> None:
    write_text(out, json.dumps(data, indent=2, ensure_ascii=False))


def write_csv_file_index(data: dict[str, Any], out: Path) -> None:
    out.parent.mkdir(parents=True, exist_ok=True)
    fields = [
        "path", "category", "extension", "sizeBytes", "lines",
        "gitStatus", "sha256_16", "textKind",
    ]
    with out.open("w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for rec in data["files"]:
            w.writerow({k: rec.get(k, "") for k in fields})


def md_escape(s: Any) -> str:
    return str(s).replace("|", "\\|").replace("\n", " ")


def write_findings_md(data: dict[str, Any], out: Path) -> None:
    findings = data["findings"]
    lines = ["# Findings", ""]

    if not findings:
        lines.append("No findings.")
    else:
        order = {"high": 0, "medium": 1, "low": 2, "info": 3}
        findings = sorted(findings, key=lambda f: (order.get(f["severity"], 9), f["category"], f["path"]))

        for f in findings:
            lines.append(f"## {f['severity'].upper()} — {f['category']}")
            lines.append("")
            lines.append(f"- Path: `{f['path']}`")
            lines.append(f"- Message: {f['message']}")
            if f.get("evidence"):
                lines.append(f"- Evidence: `{f['evidence']}`")
            lines.append("")

    write_text(out, "\n".join(lines))


def write_docs_audit_md(data: dict[str, Any], out: Path) -> None:
    docs = data["docs"]["documents"]
    work_item_mentions = data["docs"]["workItemMentions"]

    lines = ["# Docs Audit", ""]

    by_area = defaultdict(list)
    for d in docs:
        area = wiki_area(d["path"])
        by_area[area].append(d)

    lines.append("## Docs by Area")
    lines.append("")
    lines.append("| Area | Count |")
    lines.append("|---|---:|")
    for area, items in sorted(by_area.items()):
        lines.append(f"| `{md_escape(area)}` | {len(items)} |")

    lines.append("")
    lines.append("## Long / Suspicious Docs")
    lines.append("")
    for d in sorted(docs, key=lambda x: x["lines"], reverse=True)[:80]:
        markers = ", ".join(d.get("markers", []))
        lines.append(f"- `{d['path']}` — lines={d['lines']}, work items={d.get('workItemMentions', [])}, markers=[{markers}]")

    lines.append("")
    lines.append("## Work item Mentions")
    lines.append("")
    for work_item, paths in sorted(work_item_mentions.items(), key=lambda kv: int(kv[0])):
        lines.append(f"### Work item {work_item}")
        for p in paths[:40]:
            lines.append(f"- `{p}`")
        if len(paths) > 40:
            lines.append(f"- ... {len(paths) - 40} more")
        lines.append("")

    write_text(out, "\n".join(lines))


def write_work_item_audit_md(data: dict[str, Any], out: Path) -> None:
    work_item_mentions = {int(k): v for k, v in data["docs"]["workItemMentions"].items()}
    lines = ["# Work item Audit", ""]

    if not work_item_mentions:
        lines.append("No work item mentions detected.")
        write_text(out, "\n".join(lines))
        return

    nums = sorted(work_item_mentions)
    lines.append(f"- Work item range: `{min(nums)}–{max(nums)}`")
    lines.append(f"- Work item count with mentions: `{len(nums)}`")
    lines.append("")

    missing = [n for n in range(min(nums), max(nums) + 1) if n not in work_item_mentions]
    lines.append("## Missing Work item Numbers in Detected Range")
    lines.append("")
    if missing:
        lines.append(", ".join(map(str, missing[:300])))
        if len(missing) > 300:
            lines.append(f"\n... {len(missing) - 300} more")
    else:
        lines.append("None.")

    lines.append("")
    lines.append("## Work item Sprawl")
    lines.append("")
    for work_item in nums:
        paths = work_item_mentions[work_item]
        if len(paths) >= 5:
            lines.append(f"- Work item `{work_item}` appears in `{len(paths)}` files.")
            for p in paths[:10]:
                lines.append(f"  - `{p}`")
    write_text(out, "\n".join(lines))


def write_boundary_audit_md(data: dict[str, Any], out: Path) -> None:
    findings = [
        f for f in data["findings"]
        if f["category"] in {
            "public-boundary-leak",
            "layer-boundary-risk",
            "retired-ownership-root",
            "flat-source-directory",
            "important-untracked-file",
            "many-untracked-files",
        }
    ]

    lines = ["# Boundary Audit", ""]
    if not findings:
        lines.append("No boundary findings.")
    else:
        for f in findings:
            lines.append(f"- **{f['severity']}** `{f['category']}` — `{f['path']}`: {f['message']}")
            if f.get("evidence"):
                lines.append(f"  - Evidence: `{f['evidence']}`")
    write_text(out, "\n".join(lines))


def write_tools_audit_md(data: dict[str, Any], out: Path) -> None:
    lines = ["# Tools Audit", ""]
    commands = data["tools"]["cliCommandsByFile"]

    if not commands:
        lines.append("No CLI commands detected.")
    else:
        for path, cmds in sorted(commands.items()):
            lines.append(f"## `{path}`")
            for c in cmds:
                lines.append(f"- `{c}`")
            lines.append("")

    lines.append("## Tool-related findings")
    lines.append("")
    for f in data["findings"]:
        if "tool" in f["category"] or f["path"].startswith("Tools/"):
            lines.append(f"- **{f['severity']}** `{f['category']}` — `{f['path']}`: {f['message']}")

    write_text(out, "\n".join(lines))


def write_samples_audit_md(data: dict[str, Any], out: Path) -> None:
    lines = ["# Samples Audit", ""]
    sample_files = [f for f in data["files"] if f["path"].startswith("Samples/")]

    by_sample = defaultdict(list)
    for f in sample_files:
        parts = f["path"].split("/")
        sample = parts[1] if len(parts) > 1 else "<root>"
        by_sample[sample].append(f)

    lines.append("## Samples")
    lines.append("")
    for sample, files in sorted(by_sample.items()):
        ext = Counter(x["extension"] for x in files)
        lines.append(f"### `{sample}`")
        lines.append(f"- Files: `{len(files)}`")
        lines.append(f"- Extensions: `{dict(ext.most_common())}`")
        generated = [x["path"] for x in files if "/Generated/" in x["path"]]
        scenes = [x["path"] for x in files if "/Scenes/" in x["path"] or x["path"].endswith(".scene.json")]
        lines.append(f"- Scene-like files: `{len(scenes)}`")
        lines.append(f"- Generated-like files: `{len(generated)}`")
        if scenes:
            lines.append("- Scene files:")
            for p in scenes[:20]:
                lines.append(f"  - `{p}`")
        lines.append("")

    lines.append("## Sample Findings")
    lines.append("")
    for f in data["findings"]:
        if f["path"].startswith("Samples/") or "sample" in f["category"]:
            lines.append(f"- **{f['severity']}** `{f['category']}` — `{f['path']}`: {f['message']}")

    write_text(out, "\n".join(lines))


def write_build_reports_md(data: dict[str, Any], out: Path) -> None:
    reports = data["buildReports"]
    lines = ["# Build Reports Audit", ""]

    if not reports:
        lines.append("No JSON build reports detected.")
    else:
        lines.append("| Path | Status Summary |")
        lines.append("|---|---|")
        for r in sorted(reports, key=lambda x: x["path"]):
            status = r.get("status") or r.get("sampleStatus") or {}
            lines.append(f"| `{md_escape(r['path'])}` | `{md_escape(json.dumps(status, ensure_ascii=False))}` |")

    write_text(out, "\n".join(lines))


def write_ai_context_pack_md(data: dict[str, Any], out: Path) -> None:
    lines = ["# SagaEngine AI Context Pack", ""]

    lines.append("## Repo Summary")
    lines.append("")
    meta = data["metadata"]
    lines.append(f"- Files: `{meta['fileCount']}`")
    lines.append(f"- Tracked files: `{meta['trackedCount']}`")
    lines.append(f"- Untracked files: `{meta['untrackedCount']}`")
    lines.append(f"- Dirty paths: `{meta['dirtyCount']}`")
    lines.append("")

    lines.append("## Critical Findings")
    lines.append("")
    critical = [f for f in data["findings"] if f["severity"] in {"high", "medium"}]
    if not critical:
        lines.append("No high/medium findings.")
    else:
        for f in critical[:120]:
            lines.append(f"- **{f['severity']}** `{f['category']}` — `{f['path']}`: {f['message']}")
            if f.get("evidence"):
                lines.append(f"  - Evidence: `{f['evidence']}`")
    lines.append("")

    lines.append("## Modules")
    lines.append("")
    lines.append("| Module | Files | Source | Docs | Tests | Untracked | Dirty |")
    lines.append("|---|---:|---:|---:|---:|---:|---:|")
    for m in data["modules"]:
        lines.append(
            f"| `{m['name']}` | {m['fileCount']} | {m['sourceCount']} | "
            f"{m['docCount']} | {m['testCount']} | {m['untrackedCount']} | {m['dirtyCount']} |"
        )

    lines.append("")
    lines.append("## Tool Commands")
    lines.append("")
    for path, cmds in sorted(data["tools"]["cliCommandsByFile"].items()):
        lines.append(f"### `{path}`")
        for c in cmds[:100]:
            lines.append(f"- `{c}`")
        lines.append("")

    lines.append("## Build Report Statuses")
    lines.append("")
    for r in sorted(data["buildReports"], key=lambda x: x["path"])[:200]:
        status = r.get("status") or r.get("sampleStatus") or {}
        lines.append(f"- `{r['path']}`: `{json.dumps(status, ensure_ascii=False)}`")

    lines.append("")
    lines.append("## Untracked Important Paths")
    lines.append("")
    untracked = data["git"]["untracked"]
    if not untracked:
        lines.append("No untracked files detected.")
    else:
        for p in untracked[:300]:
            if p.startswith(("Engine/Source/", "Samples/", "SagaWiki/", "Tools/", "Tests/", "cmake/")):
                lines.append(f"- `{p}`")

    lines.append("")
    lines.append("## Tree Evaluation")
    lines.append("")
    lines.append("```text")
    lines.extend(data["tree"][:1500])
    lines.append("```")
    lines.append("")

    write_text(out, "\n".join(lines))


def write_tree(data: dict[str, Any], out: Path) -> None:
    write_text(out, "\n".join(data["tree"]) + "\n")

