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

printf '%s\n' 'checkpoint=bootstrap'
HOME="$TEMP_HOME" PREFIX="$TEMP_PREFIX" bash "$ROOT/platform/install.sh"
printf '%s\n' 'checkpoint=installed'
export HOME="$TEMP_HOME"
export PREFIX="$TEMP_PREFIX"
export PATH="$PREFIX/bin:$HOME/.vibecode/bin:$PATH"
TV="$HOME/.vibecode/bin/tv"

[[ -L "$PREFIX/bin/tv" ]] || { ls -la "$PREFIX/bin" >&2; exit 1; }
printf '%s\n' 'checkpoint=symlinks'
[[ "$(readlink -f "$PREFIX/bin/tv")" == "$HOME/.vibecode/bin/tv" ]] || { readlink -f "$PREFIX/bin/tv" >&2; exit 1; }
printf '%s\n' 'checkpoint=symlink-target'
[[ -f "$HOME/.vibecode/.install-marker" ]]
grep -q '^version=0.1.0$' "$HOME/.vibecode/.install-marker"
grep -q '^prefix=' "$HOME/.vibecode/.install-marker"
printf '%s\n' 'checkpoint=marker'

[[ "$(bash "$TV" version)" == "0.1.0" ]]
printf '%s\n' 'checkpoint=version'
workspace="$(bash "$TV" workspace smoke)"
test -d "$workspace/src"
test -d "$workspace/build"
test -d "$workspace/cache"
test -d "$workspace/tmp"
printf '%s\n' 'checkpoint=workspace'

real_prefix="$PREFIX"
export PREFIX="/data/data/com.termux/files/usr"
set +e
bash "$TV" self-test > "$SELFTEST_FILE" 2>&1
self_rc=$?
set -e
if (( self_rc != 0 )); then
  printf '%s\n' 'self-test output:' >&2
  cat "$SELFTEST_FILE" >&2
  exit "$self_rc"
fi
grep -qx 'SELF_TEST=PASS' <(tail -n 1 "$SELFTEST_FILE")
export PREFIX="$real_prefix"
printf '%s\n' 'checkpoint=self-test'

printf '%s\n' 'installed-ok' | bash "$TV" shell | grep -qx 'installed-ok'
printf '%s\n' 'checkpoint=shell'

set +e
bash "$TV" workspace '../escape' >"$ERROR_FILE" 2>&1
rc=$?
set -e
(( rc == 2 ))
printf '%s\n' 'checkpoint=workspace-validation'

bash "$TV" status >"$STATUS_FILE"
python3 - "$STATUS_FILE" <<'PY'
import json
import sys
with open(sys.argv[1], encoding="utf-8") as stream:
    payload = json.load(stream)
assert 'platform' in payload
assert 'cpu_count' in payload
assert 'android' in payload
PY
printf '%s\n' 'checkpoint=status'

bash "$ROOT/platform/uninstall.sh" >/dev/null
[[ ! -e "$PREFIX/bin/tv" ]]
[[ ! -e "$PREFIX/bin/tune" ]]
[[ ! -e "$PREFIX/bin/tv-ai" ]]
[[ ! -e "$PREFIX/bin/vsh" ]]
[[ ! -f "$HOME/.vibecode/.install-marker" ]]
test -d "$HOME/.vibecode/workspaces/smoke"
printf '%s\n' 'checkpoint=uninstall'

! grep -Fqx '# Termux VibeCode' "$HOME/.bashrc"
! grep -Fqx 'export PATH="$HOME/.vibecode/bin:$PATH"' "$HOME/.bashrc"
printf '%s\n' 'platform smoke: PASS'
