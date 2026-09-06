#!/data/data/com.termux/files/usr/bin/bash
set -Eeuo pipefail

STATE="${HOME}/.vibecode"
MARKER="$STATE/.install-marker"
[[ -f "$MARKER" ]] || {
  printf '%s\n' 'VibeCode install marker not found; refusing to modify the Termux prefix.' >&2
  exit 2
}

PREFIX_VALUE="${PREFIX:-}"
[[ -n "$PREFIX_VALUE" && "$PREFIX_VALUE" == */files/usr ]] || {
  printf '%s\n' 'VibeCode uninstall must run inside a registered Termux-style prefix.' >&2
  exit 2
}

recorded_prefix="$(awk -F= '$1 == "prefix" { print substr($0, index($0, "=") + 1); exit }' "$MARKER")"
[[ "$recorded_prefix" == "$PREFIX_VALUE" ]] || {
  printf '%s\n' 'Current PREFIX does not match the VibeCode installation marker; refusing to modify files.' >&2
  exit 2
}

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

rm -f -- "$MARKER"

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
