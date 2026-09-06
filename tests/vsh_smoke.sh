#!/data/data/com.termux/files/usr/bin/bash
set -Eeuo pipefail
ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
make -C "$ROOT/native/vsh" clean all
out="$(printf 'printf hello | tr a-z A-Z\n' | "$ROOT/native/vsh")"
grep -q 'HELLO' <<<"$out"
out="$(printf 'printf abc > /tmp/vsh-smoke-out\n' | "$ROOT/native/vsh")"
test -s /tmp/vsh-smoke-out
grep -q abc /tmp/vsh-smoke-out
printf '%s\n' 'vsh smoke: PASS'
