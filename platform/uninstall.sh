#!/data/data/com.termux/files/usr/bin/bash
set -Eeuo pipefail

PREFIX_VALUE="${PREFIX:-}"
[[ "$PREFIX_VALUE" == /data/data/com.termux/files/usr ]] || {
  printf '%s\n' 'VibeCode uninstall must run inside the official Termux userland.' >&2
  exit 2
}

STATE="${HOME}/.vibecode"
PREFIX_BIN="$PREFIX_VALUE/bin"
FILES=(
  "$PREFIX_BIN/tv"
  "$PREFIX_BIN/tune"
  "$PREFIX_BIN/tv-ai"
  "$PREFIX_BIN/vsh"
)

removed=0
for file in "${FILES[@]}"; do
  if [[ -e "$file" || -L "$file" ]]; then
    rm -f -- "$file"
    removed=$((removed + 1))
  fi
done

if [[ -f "$STATE/.install-marker" ]]; then
  rm -f -- "$STATE/.install-marker"
fi

if [[ -f "$HOME/.bashrc" ]]; then
  tmp=$(mktemp)
  awk '
    $0 == "# Termux VibeCode" { skip=1; next }
    skip && $0 == "export PATH=\"$HOME/.vibecode/bin:$PATH\"" { skip=0; next }
    skip && $0 == "" { skip=0; next }
    { print }
  ' "$HOME/.bashrc" > "$tmp"
  mv "$tmp" "$HOME/.bashrc"
fi

printf 'removed=%s\n' "$removed"
printf '%s\n' 'VibeCode binaries removed; ~/.vibecode state, logs and workspaces were preserved.'
