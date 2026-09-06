#!/usr/bin/env bash
set -Eeuo pipefail

PKG=com.termux
TERMUX_HOME=/data/data/$PKG/files/home
TERMUX_PREFIX=/data/data/$PKG/files/usr
TERMUX_BASH=$TERMUX_PREFIX/bin/bash
TERMUX_PATH=$TERMUX_PREFIX/bin:$TERMUX_PREFIX/bin/applets:/system/bin:/system/xbin
TERMUX_APK="${TERMUX_APK:-${RUNNER_TEMP:-/tmp}/termux-app.apk}"
REMOTE_TMP=/data/local/tmp/vibecode-smoke.sh
REMOTE_REPO=/data/local/tmp/vibecode-repo
TERMUX_REPO=$TERMUX_HOME/vibecode-repo
REMOTE_RESULT=$TERMUX_HOME/vibecode-emulator-result
TERMUX_SCRIPT=$TERMUX_HOME/vibecode-smoke.sh

[[ -s "$TERMUX_APK" ]] || { echo "missing TERMUX_APK: $TERMUX_APK" >&2; exit 1; }

adb wait-for-device
for _ in $(seq 1 120); do
  state=$(adb get-state 2>/dev/null || true)
  [[ "$state" == "device" ]] && break
  sleep 1
done
[[ "$(adb get-state 2>/dev/null)" == "device" ]]

adb install -r "$TERMUX_APK"
adb shell am start -W -n "$PKG/.app.TermuxActivity" >/dev/null

for _ in $(seq 1 60); do
  if adb shell run-as "$PKG" test -x "$TERMUX_BASH" >/dev/null 2>&1; then break; fi
  sleep 2
done
adb shell run-as "$PKG" test -x "$TERMUX_BASH"

adb shell rm -rf "$REMOTE_REPO"
adb shell mkdir -p "$REMOTE_REPO"
adb push platform "$REMOTE_REPO/platform" >/dev/null
adb push core "$REMOTE_REPO/core" >/dev/null
adb push config "$REMOTE_REPO/config" >/dev/null
adb push services "$REMOTE_REPO/services" >/dev/null
adb push native "$REMOTE_REPO/native" >/dev/null

cat > /tmp/vibecode-smoke.sh <<'EOS'
#!/data/data/com.termux/files/usr/bin/bash
set -Eeuo pipefail
export HOME=/data/data/com.termux/files/home
export PREFIX=/data/data/com.termux/files/usr
export TMPDIR=$PREFIX/tmp
export PATH=$PREFIX/bin:$PREFIX/bin/applets:/system/bin:/system/xbin
REPO=$HOME/vibecode-repo
STATE="$HOME/.vibecode"
RESULT="$HOME/vibecode-emulator-result"
exec >"$RESULT" 2>&1
printf 'VIBECODE_ANDROID_SMOKE=1\n'
printf 'uname='; uname -a
printf 'arch='; uname -m
printf 'termux_prefix=%s\n' "$PREFIX"
printf 'android_release='; /system/bin/getprop ro.build.version.release
printf 'android_api='; /system/bin/getprop ro.build.version.sdk
printf 'termux_uid='; id -u
printf 'termux_gid='; id -g
printf 'path=%s\n' "$PATH"

command -v bash
command -v pkg

pkg install -y clang make python git
bash "$REPO/platform/install.sh"

test -L "$PREFIX/bin/tv"
test -L "$PREFIX/bin/vsh"
test "$(tv version)" = '0.1.0'
printf 'install=PASS\n'

tv self-test > "$STATE/android-self-test.txt"
grep -qx 'SELF_TEST=PASS' <(tail -n 1 "$STATE/android-self-test.txt")
printf 'self_test=PASS\n'

workspace=$(tv workspace android-smoke)
for part in src build cache tmp; do test -d "$workspace/$part"; done
printf 'workspace=PASS\n'

printf '%s\n' 'echo shell-ok' | tv shell | grep -qx 'shell-ok'
printf 'vsh_shell=PASS\n'

tv status > "$STATE/android-status.json"
python3 - <<'PY'
import json
from pathlib import Path
payload=json.loads(Path.home().joinpath('.vibecode/android-status.json').read_text())
assert payload['termux_prefix'] == '/data/data/com.termux/files/usr'
assert payload['cpu_count'] >= 1
assert 'android' in payload
PY
printf 'status=PASS\n'

tv doctor > "$STATE/android-doctor.json"
python3 - <<'PY'
import json
from pathlib import Path
payload=json.loads(Path.home().joinpath('.vibecode/android-doctor.json').read_text())
assert payload['ok'] is True
assert payload['required']['termux'] is True
assert not payload['missing_required']
PY
printf 'doctor=PASS\n'

bash "$REPO/platform/uninstall.sh" >/dev/null
test ! -e "$PREFIX/bin/tv"
test ! -e "$PREFIX/bin/vsh"
test -d "$workspace/src"
printf 'uninstall=PASS\n'

printf 'android_capability=PASS\n'
printf 'process_model=PASS\n'
printf 'SMOKE=PASS\n'
EOS

adb push /tmp/vibecode-smoke.sh "$REMOTE_TMP" >/dev/null
adb shell chmod 755 "$REMOTE_TMP"
adb shell run-as "$PKG" cp "$REMOTE_TMP" "$TERMUX_SCRIPT"
adb shell run-as "$PKG" chmod 755 "$TERMUX_SCRIPT"
adb shell run-as "$PKG" rm -rf "$TERMUX_REPO"
adb shell run-as "$PKG" cp -r "$REMOTE_REPO/." "$TERMUX_REPO"

set +e
run_output=$(adb shell run-as "$PKG" env HOME="$TERMUX_HOME" PREFIX="$TERMUX_PREFIX" PATH="$TERMUX_PATH" TMPDIR="$TERMUX_PREFIX/tmp" "$TERMUX_BASH" "$TERMUX_SCRIPT" 2>&1)
run_rc=$?
set -e
printf '%s\n' "$run_output"
printf 'termux_run_rc=%s\n' "$run_rc"

if adb shell run-as "$PKG" test -f "$REMOTE_RESULT"; then
  adb shell run-as "$PKG" cat "$REMOTE_RESULT"
else
  echo 'TERMUX_SMOKE_RESULT=MISSING' >&2
fi

(( run_rc == 0 ))
adb shell run-as "$PKG" grep -q '^SMOKE=PASS$' "$REMOTE_RESULT"
printf '%s\n' 'ANDROID_EMULATOR_SMOKE=PASS'
