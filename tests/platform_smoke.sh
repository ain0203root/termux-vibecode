#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
TEMP_HOME="$(mktemp -d)"
trap 'rm -rf "$TEMP_HOME"' EXIT

HOME="$TEMP_HOME" bash "$ROOT/platform/install.sh"
export HOME="$TEMP_HOME"
export PATH="$HOME/.vibecode/bin:$PATH"

[[ "$(tv version)" == "0.1.0" ]]
workspace="$(tv workspace smoke)"
test -d "$workspace/src"
test -d "$workspace/build"
test -d "$workspace/cache"
test -d "$workspace/tmp"

set +e
tv workspace '../escape' >/tmp/vibecode-platform-smoke.err 2>&1
rc=$?
set -e
(( rc == 2 ))

tv status >/tmp/vibecode-platform-status.json
python3 - <<'PY'
import json
from pathlib import Path

payload = json.loads(Path('/tmp/vibecode-platform-status.json').read_text())
assert 'platform' in payload
assert 'cpu_count' in payload
PY

printf '%s\n' 'platform smoke: PASS'
