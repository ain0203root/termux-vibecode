#!/data/data/com.termux/files/usr/bin/bash
set -Eeuo pipefail

N="${N:-10000}"
BIN="${VIBECODE_VSH:-native/vsh/vsh}"
make -C native/vsh -s all
printf 'benchmark commands=%s\n' "$N"
start="$(date +%s%N)"
for ((i=0; i<N; i++)); do printf 'true\n'; done | "$BIN" >/dev/null
end="$(date +%s%N)"
ms=$(( (end - start) / 1000000 ))
awk -v n="$N" -v ms="$ms" 'BEGIN { if (ms < 1) ms=1; printf "elapsed_ms=%d commands_per_sec=%.0f\n", ms, n*1000/ms }'
