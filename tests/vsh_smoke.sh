#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
make -C "$ROOT/native/vsh" clean all
VSH="$ROOT/native/vsh/vsh"

run_vsh() {
  local label="$1"
  local script="$2"
  local expected_rc="${3:-0}"
  local output
  set +e
  output=$(timeout 5s "$VSH" <<< "$script")
  local rc=$?
  set -e
  if [[ "$rc" -ne "$expected_rc" ]]; then
    printf 'vsh test failed: %s (rc=%s expected=%s)\n' "$label" "$rc" "$expected_rc" >&2
    printf '%s\n' "$output" >&2
    return 1
  fi
  printf '%s\n' "$output"
}

test "$(run_vsh pipeline $'printf hello | tr a-z A-Z')" = "HELLO"
test "$(run_vsh echo $'echo VIBECODE_OK')" = "VIBECODE_OK"
test -n "$(run_vsh pwd $'pwd')"

tmp="$(mktemp)"
outfile="$(mktemp)"
trap 'rm -f "$tmp" "$outfile"' EXIT

run_vsh overwrite "printf abc > $tmp" >/dev/null
test "$(cat "$tmp")" = "abc"
run_vsh append "echo def >> $tmp" >/dev/null
test "$(cat "$tmp")" = "abcdef"
run_vsh input "cat < $tmp" > "$outfile"
test "$(cat "$outfile")" = "abcdef"
run_vsh builtin-redirection "echo redirected > $tmp" >/dev/null
test "$(cat "$tmp")" = "redirected"

test "$(run_vsh quoting $'export VSH_TEST=ok\necho $VSH_TEST\necho ${VSH_TEST}\necho \"$VSH_TEST\"\necho '\''$VSH_TEST'\'')" = $'ok\nok\nok\n$VSH_TEST'
test "$(run_vsh status $'false\necho $?')" = "1"
test "$(run_vsh pid $'echo $$')" -gt 0

test "$(run_vsh not-found $'definitely-not-a-real-command' 127 2>/dev/null)" = ""
test "$(run_vsh which $'which sh')" = "$(command -v sh)"
test "$(run_vsh true $'true')" = ""
test "$(run_vsh false $'false' 1)" = ""

set +e
run_vsh unmatched-quote 'echo "unterminated' >/dev/null 2>&1
rc=$?
set -e
test "$rc" -eq 2

printf '%s\n' 'vsh smoke: PASS'
