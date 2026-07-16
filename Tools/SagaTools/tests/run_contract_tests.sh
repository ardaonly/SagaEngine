#!/usr/bin/env bash
# SPDX-License-Identifier: MPL-2.0

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
manifest="$repo_root/Tools/SagaTools/Cargo.toml"
work_dir="$(mktemp -d "${TMPDIR:-/tmp}/sagatools-contract.XXXXXX")"
trap 'rm -rf "$work_dir"' EXIT

export CARGO_TARGET_DIR="$work_dir/target"
cargo test --locked --manifest-path "$manifest"
cargo build --locked --manifest-path "$manifest"
binary="$CARGO_TARGET_DIR/debug/sagatools"

registry="$work_dir/registry.json"
alternate_registry="$work_dir/alternate.json"
malformed_registry="$work_dir/malformed.json"
probe="$(command -v sh)"

printf '%s\n' \
  '{' \
  '  "tools": {' \
  "    \"runner\": \"$probe\"," \
  '    "missing": "/definitely/not/a/saga/tool"' \
  '  },' \
  '  "installers": {}' \
  '}' > "$registry"
printf '%s\n' '{"tools":{"alternate":"/bin/true"},"installers":{}}' > "$alternate_registry"
printf '%s\n' '{not-json' > "$malformed_registry"

fail() {
  echo "SagaTools contract failure: $*" >&2
  exit 1
}

[[ "$($binary --version)" == "tools 0.2.0" ]] || fail "version output drifted"
help="$($binary --help)"
[[ "$help" == *"thin dispatcher"* ]] || fail "help lost dispatcher boundary"
[[ "$help" == *"--registry <path>"* ]] || fail "help lost registry contract"

list="$($binary --registry "$registry" list)"
[[ "$list" == *"runner   [resolved]"* ]] || fail "resolved tool is not visible"
[[ "$list" == *"missing   [unavailable]"* ]] || fail "unavailable tool is not visible"
[[ "$($binary --registry "$registry" which runner)" == "$probe" ]] || fail "which did not resolve runner"
[[ "$($binary --registry "$registry" where runner)" == "$probe" ]] || fail "where alias drifted"
forwarded="$($binary --registry "$registry" runner -c 'printf "%s|%s" "$1" "$2"' _ alpha "two words" 2>/dev/null)"
[[ "$forwarded" == "alpha|two words" ]] || fail "arguments were not forwarded"

set +e
missing_output="$($binary --registry "$registry" which missing 2>/dev/null)"
missing_exit=$?
unknown_output="$($binary --registry "$registry" which unknown 2>&1)"
unknown_exit=$?
malformed_output="$($binary --registry "$malformed_registry" list 2>&1)"
malformed_exit=$?
set -e

[[ $missing_exit -eq 2 && "$missing_output" == "<unavailable>" ]] || fail "unavailable exit contract drifted"
[[ $unknown_exit -eq 2 && "$unknown_output" == *"unknown tool"* ]] || fail "unknown-tool exit contract drifted"
[[ $malformed_exit -eq 3 && "$malformed_output" == *"failed to load registry"* ]] || fail "malformed registry was not rejected"

explicit="$({ SAGATOOLS_REGISTRY="$alternate_registry" "$binary" --registry "$registry" which runner; } 2>/dev/null)"
[[ "$explicit" == "$probe" ]] || fail "explicit registry did not override environment"

echo "SagaTools dispatcher contract tests passed."
