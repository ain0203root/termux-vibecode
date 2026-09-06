#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
make -C "$ROOT/native/vsh" clean all
VSH="$ROOT/native/vsh/vsh"

out="$(printf 'printf hello | tr a-z A-Z\n' | "$VSH")"
test "$out" = "HELLO"

out="$(printf 'echo VIBECODE_OK\n' | "$VSH")"
test "$out" = "VIBECODE_OK"

out="$(printf 'pwd\n' | "$VSH")"
test -n "$out"

tmp="$(mktemp)"
trap 'rm -f "$tmp"' EXIT
printf 'printf abc > %s\n' "$tmp" | "$VSH" >/dev/null
test "$(cat "$tmp")" = "abc"

printf 'export VSH_TEST=ok\necho $VSH_TEST\n' | "$VSH" | grep -qx 'ok'

printf 'true\n' | "$VSH" >/dev/null
printf 'false\n' | "$VSH" >/dev/null

printf '%s\n' 'vsh smoke: PASS'
