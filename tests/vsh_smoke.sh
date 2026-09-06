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
    printf 'vsh test failed: %s (rc=%s expected=%s output=%q)\n' "$label" "$rc" "$expected_rc" "$output" >&2
    return 1
  fi
  printf '%s\n' "$output"
}

pipeline_output="$(run_vsh pipeline 'printf hello | tr a-z A-Z')"
printf 'pipeline_output=%q\n' "$pipeline_output" >&2
[[ "$pipeline_output" == "HELLO" ]]

echo_output="$(run_vsh echo 'echo VIBECODE_OK')"
[[ "$echo_output" == "VIBECODE_OK" ]]

pwd_output="$(run_vsh pwd 'pwd')"
[[ -n "$pwd_output" ]]

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

quoted_script=$(cat <<'EOF'
export VSH_TEST=ok
echo $VSH_TEST
echo ${VSH_TEST}
echo "$VSH_TEST"
echo '$VSH_TEST'
EOF
)
expected_quoted=$'ok\nok\nok\n$VSH_TEST'
test "$(run_vsh quoting "$quoted_script")" = "$expected_quoted"

test "$(run_vsh status $'false\necho $?')" = "1"
test "$(run_vsh pid 'echo $$')" -gt 0

test -z "$(run_vsh not-found 'definitely-not-a-real-command' 127 2>/dev/null)"
test "$(run_vsh which 'which sh')" = "$(command -v sh)"
test -z "$(run_vsh true 'true')"
test -z "$(run_vsh false 'false' 1)"

set +e
run_vsh unmatched-quote 'echo "unterminated' >/dev/null 2>&1
rc=$?
set -e
test "$rc" -eq 2

printf '%s\n' 'vsh smoke: PASS'
