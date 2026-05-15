#!/usr/bin/env sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
#
# Usage notes:
#   ./export.sh --dry-run
#     Show which tool mirrors would be exported without pushing anything.
#
#   ./export.sh --tool Prism
#     Export only one configured tool mirror. Repeat --tool for multiple tools.
#
#   ./export.sh --since HEAD~1
#     Compare tool paths against an explicit base ref instead of saved state.
#
# Default behavior is tool-scoped: each mirror is pushed only when that tool's
# path changed since its last recorded export state. Keep commits tool-specific
# by committing Prism, Forge, and SDE changes separately before running export.
exec python3 "$repo_root/Tools/scripts/export_tool_mirrors.py" --repo-root "$repo_root" "$@"
