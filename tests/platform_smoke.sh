#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
TEMP_HOME="$(mktemp -d)"
STATUS_FILE="$(mktemp)"
ERROR_FILE="$(mktemp)"
trap 'rm -rf "$TEMP_HOME"; rm -f "$STATUS_FILE" "$ERROR_FILE"' EXIT

HOME="$TEMP_HOME" bash "$ROOT/platform/install.sh"
export HOME="$TEMP_HOME"
export PATH="$HOME/.vibecode/bin:$PATH"

[[ "$(tv version)" == "0.1.0" ]]
workspace="$(tv workspace smoke)"
test -d "$workspace/src"
test -d "$workspace/build"
test -d "$workspace/cache"
test -d "$workspace/tmp"

printf 'printf installed-ok\\n' | tv shell | grep -qx 'installed-ok'

set +e
tv workspace '../escape' >"$ERROR_FILE" 2>&1
rc=$?
set -e
(( rc == 2 ))

tv status >"$STATUS_FILE"
python3 - "$STATUS_FILE" <<'PY'
import json
import sys

payload = json.load(open(sys.argv[1], encoding="utf-8"))
assert 'platform' in payload
assert 'cpu_count' in payload
assert 'android' in payload
PY

printf '%s\n' 'platform smoke: PASS'
