#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
TEMP_HOME="$(mktemp -d)"
TEMP_PREFIX="$TEMP_HOME/data/data/com.termux/files/usr"
STATUS_FILE="$(mktemp)"
ERROR_FILE="$(mktemp)"
SELFTEST_FILE="$(mktemp)"
trap 'rm -rf "$TEMP_HOME"; rm -f "$STATUS_FILE" "$ERROR_FILE" "$SELFTEST_FILE"' EXIT
mkdir -p "$TEMP_PREFIX/bin"

HOME="$TEMP_HOME" PREFIX="$TEMP_PREFIX" bash "$ROOT/platform/install.sh"
export HOME="$TEMP_HOME"
export PREFIX="$TEMP_PREFIX"
export PATH="$PREFIX/bin:$HOME/.vibecode/bin:$PATH"

[[ -L "$PREFIX/bin/tv" ]]
[[ -L "$PREFIX/bin/tune" ]]
[[ -L "$PREFIX/bin/tv-ai" ]]
[[ -L "$PREFIX/bin/vsh" ]]
[[ "$(readlink -f "$PREFIX/bin/tv")" == "$HOME/.vibecode/bin/tv" ]]
[[ -f "$HOME/.vibecode/.install-marker" ]]
grep -q '^version=0.1.0$' "$HOME/.vibecode/.install-marker"
grep -q '^prefix=' "$HOME/.vibecode/.install-marker"

[[ "$(tv version)" == "0.1.0" ]]
workspace="$(tv workspace smoke)"
test -d "$workspace/src"
test -d "$workspace/build"
test -d "$workspace/cache"
test -d "$workspace/tmp"

# Exercise the Android-oriented self-test contract while keeping all files
# inside the isolated temporary prefix used by this host-side test.
real_prefix="$PREFIX"
export PREFIX="/data/data/com.termux/files/usr"
tv self-test > "$SELFTEST_FILE"
grep -qx 'SELF_TEST=PASS' <(tail -n 1 "$SELFTEST_FILE")
export PREFIX="$real_prefix"

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

with open(sys.argv[1], encoding="utf-8") as stream:
    payload = json.load(stream)
assert 'platform' in payload
assert 'cpu_count' in payload
assert 'android' in payload
PY

bash "$ROOT/platform/uninstall.sh" >/dev/null
[[ ! -e "$PREFIX/bin/tv" ]]
[[ ! -e "$PREFIX/bin/tune" ]]
[[ ! -e "$PREFIX/bin/tv-ai" ]]
[[ ! -e "$PREFIX/bin/vsh" ]]
[[ ! -f "$HOME/.vibecode/.install-marker" ]]
test -d "$HOME/.vibecode/workspaces/smoke"

! grep -Fqx '# Termux VibeCode' "$HOME/.bashrc"
! grep -Fqx 'export PATH="$HOME/.vibecode/bin:$PATH"' "$HOME/.bashrc"

printf '%s\n' 'platform smoke: PASS'
