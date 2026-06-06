#!/usr/bin/env sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
runner="$repo_root/Tools/scripts/export_tool_mirrors.py"

usage() {
    cat <<'USAGE'
Usage:
  ./export.sh                       Export changed configured tools.
  ./export.sh all                   Same as no args.
  ./export.sh plan                  Dry-run all configured tools.
  ./export.sh forge                 Export only Forge.
  ./export.sh prism                 Export only Prism.
  ./export.sh sde                   Export only SystemDefinitionEngine.
  ./export.sh changed-since <ref>   Export tools changed since an explicit ref.
  ./export.sh commit-guide          Print suggested monorepo commit groups.

Advanced options are forwarded to Tools/scripts/export_tool_mirrors.py:
  ./export.sh --dry-run --tool Forge --since HEAD~1
USAGE
}

commit_guide() {
    cat <<'GUIDE'
Suggested commit groups:
  Engine:
    git add -A -- Engine Runtime Server Shared Collaboration CMakeLists.txt cmake/modules/SagaTargets.cmake cmake/modules/SagaTests.cmake cmake/modules/SagaInstall.cmake Tests/Unit/Architecture Tests/Unit/Diagnostics Tests/Unit/Shared Tools/scripts/check_public_private_boundary.py Tools/scripts/check_engine_server_boundary.py docs/roadmaps/DIAGNOSTICS_ROADMAP.md
    git commit -m "feat(engine): migrate to Public Private layout"

  Editor/Product:
    git add -A -- Apps/Editor Apps/EditorLab Apps/Saga Editor Tests/Unit/Editor Tests/Unit/Saga
    git commit -m "feat(editor): add first Saga product prototype"

  SDE:
    git add -A -- Tools/SystemDefinitionEngine
    git commit -m "feat(sde): establish compiler foundation"

  Tools:
    git add -A -- Tools/Forge Tools/Host Tools/Prism Tools/SagaTools Tools/common Tools/scripts export.sh core/export .github/workflows/export-tools.yml .dockerignore
    git commit -m "feat(tools): add export and host automation"

  Docs/Project:
    git add -A -- docs README.md LICENSE.md .github/workflows .gitmodules conanfile.py forge.toml shell.nix VERSION Vendor/Diligent Vendor/Diligent.Saga cmake/templates cmake/modules/SagaDistribution.cmake cmake/modules/SagaDiligentVendor.cmake core/manifest/path_rules.json
    git commit -m "docs(project): refresh roadmaps and distribution config"

After commits:
  git push origin main
  ./export.sh plan
  ./export.sh
GUIDE
}

if [ "$#" -gt 0 ]; then
    case "$1" in
        -h|--help|help)
            usage
            exit 0
            ;;
        commit-guide)
            commit_guide
            exit 0
            ;;
        all)
            shift
            exec python3 "$runner" --repo-root "$repo_root" "$@"
            ;;
        plan)
            shift
            exec python3 "$runner" --repo-root "$repo_root" --dry-run "$@"
            ;;
        forge|Forge)
            shift
            exec python3 "$runner" --repo-root "$repo_root" --tool Forge "$@"
            ;;
        prism|Prism)
            shift
            exec python3 "$runner" --repo-root "$repo_root" --tool Prism "$@"
            ;;
        sde|SDE)
            shift
            exec python3 "$runner" --repo-root "$repo_root" --tool SDE "$@"
            ;;
        changed-since)
            shift
            if [ "$#" -eq 0 ]; then
                echo "export.sh: changed-since requires a git ref" >&2
                exit 2
            fi
            ref=$1
            shift
            exec python3 "$runner" --repo-root "$repo_root" --since "$ref" "$@"
            ;;
    esac
fi

exec python3 "$runner" --repo-root "$repo_root" "$@"
