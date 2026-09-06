#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
make -C "$ROOT/native/vsh" clean all
VSH="$ROOT/native/vsh/vsh"

test "$(printf 'printf hello | tr a-z A-Z\n' | "$VSH")" = "HELLO"
test "$(printf 'echo VIBECODE_OK\n' | "$VSH")" = "VIBECODE_OK"
test -n "$(printf 'pwd\n' | "$VSH")"

tmp="$(mktemp)"
outfile="$(mktemp)"
trap 'rm -f "$tmp" "$outfile"' EXIT

printf 'printf abc > %s\n' "$tmp" | "$VSH" >/dev/null
test "$(cat "$tmp")" = "abc"
printf 'echo def >> %s\n' "$tmp" | "$VSH" >/dev/null
test "$(cat "$tmp")" = "abcdef"
printf 'cat < %s\n' "$tmp" | "$VSH" > "$outfile"
test "$(cat "$outfile")" = "abcdef"

printf 'echo redirected > %s\n' "$tmp" | "$VSH" >/dev/null
test "$(cat "$tmp")" = "redirected"

test "$(printf 'export VSH_TEST=ok\necho $VSH_TEST\necho ${VSH_TEST}\necho \"$VSH_TEST\"\necho '\''$VSH_TEST'\''\n' | "$VSH")" = $'ok\nok\nok\n$VSH_TEST'

test "$(printf 'false\necho $?\n' | "$VSH")" = "1"

test "$(printf 'printf bad-command\n' | "$VSH" >/dev/null; echo $?)" -eq 0
set +e
printf 'definitely-not-a-real-command\n' | "$VSH" >/dev/null 2>&1
rc=$?
set -e
test "$rc" -eq 127

test "$(printf 'which sh\n' | "$VSH")" = "$(command -v sh)"
test "$(printf 'echo $$\n' | "$VSH")" -gt 0

printf 'true\n' | "$VSH" >/dev/null
set +e
printf 'false\n' | "$VSH" >/dev/null
rc=$?
set -e
test "$rc" -eq 1

printf '%s\n' 'vsh smoke: PASS'
