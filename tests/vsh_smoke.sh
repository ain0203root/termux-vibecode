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
printf '%s\n' 'gate.pipeline=PASS'

echo_output="$(run_vsh echo 'echo VIBECODE_OK')"
printf 'echo_output=%q\n' "$echo_output" >&2
[[ "$echo_output" == "VIBECODE_OK" ]]
printf '%s\n' 'gate.echo=PASS'

pwd_output="$(run_vsh pwd 'pwd')"
printf 'pwd_output=%q\n' "$pwd_output" >&2
[[ -n "$pwd_output" ]]
printf '%s\n' 'gate.pwd=PASS'

tmp="$(mktemp)"
outfile="$(mktemp)"
notfound_file="$(mktemp)"
trap 'rm -f "$tmp" "$outfile" "$notfound_file"' EXIT

run_vsh overwrite "printf abc > $tmp" >/dev/null
test "$(cat "$tmp")" = "abc"
printf '%s\n' 'gate.overwrite=PASS'
run_vsh append "echo def >> $tmp" >/dev/null
test "$(cat "$tmp")" = "abcdef"
printf '%s\n' 'gate.append=PASS'
run_vsh input "cat < $tmp" > "$outfile"
test "$(cat "$outfile")" = "abcdef"
printf '%s\n' 'gate.input=PASS'
run_vsh builtin-redirection "echo redirected > $tmp" >/dev/null
test "$(cat "$tmp")" = "redirected"
printf '%s\n' 'gate.builtin-redirection=PASS'

quoted_script=$(cat <<'EOF'
export VSH_TEST=ok
echo $VSH_TEST
echo ${VSH_TEST}
echo "$VSH_TEST"
echo '$VSH_TEST'
EOF
)
expected_quoted=$'ok\nok\nok\n$VSH_TEST'
quoted_output="$(run_vsh quoting "$quoted_script")"
printf 'quoted_output=%q\n' "$quoted_output" >&2
[[ "$quoted_output" == "$expected_quoted" ]]
printf '%s\n' 'gate.quoting=PASS'

status_output="$(run_vsh status $'false\necho $?')"
printf 'status_output=%q\n' "$status_output" >&2
[[ "$status_output" == "1" ]]
printf '%s\n' 'gate.status=PASS'

pid_output="$(run_vsh pid 'echo $$')"
printf 'pid_output=%q\n' "$pid_output" >&2
[[ "$pid_output" =~ ^[0-9]+$ ]]
printf '%s\n' 'gate.pid=PASS'

set +e
run_vsh not-found 'definitely-not-a-real-command' 127 >"$notfound_file" 2>&1
rc=$?
set -e
test "$rc" -eq 0
grep -Fq 'definitely-not-a-real-command:' "$notfound_file"
printf '%s\n' 'gate.not-found=PASS'

test "$(run_vsh which 'which sh')" = "$(command -v sh)"
printf '%s\n' 'gate.which=PASS'
test -z "$(run_vsh true 'true')"
printf '%s\n' 'gate.true=PASS'
test -z "$(run_vsh false 'false' 1)"
printf '%s\n' 'gate.false=PASS'

set +e
run_vsh unmatched-quote 'echo "unterminated' >/dev/null 2>&1
rc=$?
set -e
test "$rc" -eq 0
printf '%s\n' 'gate.unmatched-quote=PASS'

printf '%s\n' 'vsh smoke: PASS'
