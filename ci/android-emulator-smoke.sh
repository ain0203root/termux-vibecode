#!/usr/bin/env bash
set -Eeuo pipefail

PKG=com.termux
REMOTE_HOME=/data/data/$PKG/files/home
REMOTE_TMP=/data/local/tmp/vibecode-smoke.sh
RESULT=$REMOTE_HOME/vibecode-emulator-result

adb wait-for-device
adb shell 'getprop sys.boot_completed' | grep -q '^1$'

adb install -r "$TERMUX_APK"
adb shell am start -W -n "$PKG/.app.TermuxActivity" >/dev/null

# First launch performs bootstrap extraction. Give the app a bounded window.
for _ in $(seq 1 30); do
  if adb shell run-as "$PKG" test -x /data/data/$PKG/files/usr/bin/bash >/dev/null 2>&1; then break; fi
  sleep 2
done
adb shell run-as "$PKG" test -x /data/data/$PKG/files/usr/bin/bash

cat > /tmp/vibecode-smoke.sh <<'EOS'
#!/data/data/com.termux/files/usr/bin/bash
set -Eeuo pipefail
HOME=/data/data/com.termux/files/home
ROOT="$HOME/vibecode-ci"
RESULT="$HOME/vibecode-emulator-result"
exec >"$RESULT" 2>&1
printf 'VIBECODE_ANDROID_SMOKE=1\n'
printf 'uname='; uname -a
printf 'arch='; uname -m
printf 'termux_prefix=%s\n' "$PREFIX"
printf 'android_release='; getprop ro.build.version.release
printf 'android_api='; getprop ro.build.version.sdk

pkg update -y
pkg install -y clang make
rm -rf "$ROOT"
mkdir -p "$ROOT"
cp -R /data/local/tmp/vibecode-src/native "$ROOT/"
make -C "$ROOT/native/vsh" clean all
printf 'native_build=PASS\n'

out=$(printf 'printf hello | tr a-z A-Z\n' | "$ROOT/native/vsh")
printf 'pipeline=%s\n' "$out"
test "$out" = "HELLO"

out=$(printf 'pwd\n' | "$ROOT/native/vsh")
test -n "$out"
printf 'builtin_pwd=PASS\n'

out=$(printf 'echo VIBECODE_OK\n' | "$ROOT/native/vsh")
test "$out" = "VIBECODE_OK"
printf 'builtin_echo=PASS\n'

printf 'SMOKE=PASS\n'
EOS

adb push /tmp/vibecode-smoke.sh "$REMOTE_TMP" >/dev/null
adb push native/vsh/vsh.c /data/local/tmp/vibecode-src/native/vsh/vsh.c >/dev/null
adb push native/vsh/Makefile /data/local/tmp/vibecode-src/native/vsh/Makefile >/dev/null
adb shell chmod 755 "$REMOTE_TMP"
adb shell run-as "$PKG" mkdir -p files/home/.shortcuts
adb shell run-as "$PKG" cp "$REMOTE_TMP" "files/home/.shortcuts/vibecode-smoke.sh"
adb shell run-as "$PKG" chmod 755 "files/home/.shortcuts/vibecode-smoke.sh"

adb shell am startservice -n "$PKG/.app.TermuxService" -a com.termux.service_execute -d "$REMOTE_HOME/.shortcuts/vibecode-smoke.sh" >/dev/null

for _ in $(seq 1 90); do
  if adb shell run-as "$PKG" test -f "files/home/vibecode-emulator-result" >/dev/null 2>&1; then break; fi
  sleep 2
done

adb shell run-as "$PKG" cat "files/home/vibecode-emulator-result"
adb shell run-as "$PKG" grep -q '^SMOKE=PASS$' "files/home/vibecode-emulator-result"
printf '%s\n' 'ANDROID_EMULATOR_SMOKE=PASS'
