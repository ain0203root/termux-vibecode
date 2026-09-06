#!/usr/bin/env bash
set -Eeuo pipefail

PKG=com.termux
TERMUX_HOME=/data/data/$PKG/files/home
TERMUX_PREFIX=/data/data/$PKG/files/usr
TERMUX_BASH=$TERMUX_PREFIX/bin/bash
TERMUX_PATH=$TERMUX_PREFIX/bin:$TERMUX_PREFIX/bin/applets:/system/bin:/system/xbin
TERMUX_APK="${TERMUX_APK:-${RUNNER_TEMP:-/tmp}/termux-app.apk}"
REMOTE_TMP=/data/local/tmp/vibecode-smoke.sh
REMOTE_SRC=/data/local/tmp/vibecode-src/native/vsh
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

adb shell mkdir -p "$REMOTE_SRC"
adb push native/vsh/vsh.c "$REMOTE_SRC/vsh.c" >/dev/null
adb push native/vsh/Makefile "$REMOTE_SRC/Makefile" >/dev/null

cat > /tmp/vibecode-smoke.sh <<'EOS'
#!/data/data/com.termux/files/usr/bin/bash
set -Eeuo pipefail
export HOME=/data/data/com.termux/files/home
export PREFIX=/data/data/com.termux/files/usr
export TMPDIR=$PREFIX/tmp
export PATH=$PREFIX/bin:$PREFIX/bin/applets:/system/bin:/system/xbin
ROOT="$HOME/vibecode-ci"
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
command -v clang || true

pkg install -y clang make
rm -rf "$ROOT"
mkdir -p "$ROOT"
cp -R /data/local/tmp/vibecode-src/native "$ROOT/"
make -C "$ROOT/native/vsh" clean all
printf 'native_build=PASS\n'

out=$(printf 'printf hello | tr a-z A-Z\n' | "$ROOT/native/vsh/vsh")
printf 'pipeline=%s\n' "$out"
test "$out" = "HELLO"

out=$(printf 'export VIBECODE_TEST=ok\necho $VIBECODE_TEST\n' | "$ROOT/native/vsh/vsh")
test "$out" = "ok"
printf 'env_expansion=PASS\n'

out=$(printf 'echo $$\n' | "$ROOT/native/vsh/vsh")
printf 'pid=%s\n' "$out"
echo "$out" | grep -Eq '^[0-9]+$'

printf 'android_capability=PASS\n'
printf 'process_model=PASS\n'
printf 'SMOKE=PASS\n'
EOS

adb push /tmp/vibecode-smoke.sh "$REMOTE_TMP" >/dev/null
adb shell chmod 755 "$REMOTE_TMP"
adb shell run-as "$PKG" cp "$REMOTE_TMP" "$TERMUX_SCRIPT"
adb shell run-as "$PKG" chmod 755 "$TERMUX_SCRIPT"

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
