#!/data/data/com.termux/files/usr/bin/bash
set -Eeuo pipefail
ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
VSH="$ROOT/native/vsh/vsh"
make -C "$ROOT/native/vsh" clean all
out="$(printf 'printf hello | tr a-z A-Z\n' | "$VSH")"
grep -q 'HELLO' <<<"$out"
out="$(printf 'printf abc > /tmp/vsh-smoke-out\n' | "$VSH")"
test -s /tmp/vsh-smoke-out
grep -q abc /tmp/vsh-smoke-out
printf '%s\n' 'vsh smoke: PASS'
