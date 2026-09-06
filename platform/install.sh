#!/data/data/com.termux/files/usr/bin/bash
set -Eeuo pipefail
IFS=$'\n\t'

ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
PREFIX="${PREFIX:-/data/data/com.termux/files/usr}"
STATE="${HOME}/.vibecode"
BIN="${STATE}/bin"
mkdir -p "$STATE" "$BIN" "$STATE/logs" "$STATE/workspaces"

log(){ printf '[vibecode] %s\n' "$*"; }
need(){ command -v "$1" >/dev/null 2>&1 || { log "missing: $1"; return 1; }; }

log 'checking base Termux toolchain'
for x in bash clang make python git; do need "$x" || true; done

if command -v clang >/dev/null 2>&1; then
  log 'building native shell'
  make -C "$ROOT/native/vsh" clean all
  install -m 755 "$ROOT/native/vsh/vsh" "$BIN/vsh"
fi

install -m 755 "$ROOT/platform/tv" "$BIN/tv"
install -m 755 "$ROOT/core/tvd.py" "$STATE/tvd.py"
install -m 755 "$ROOT/platform/tune" "$BIN/tune"
cp "$ROOT/config/default.env" "$STATE/default.env"
cp "$ROOT/services/services.conf" "$STATE/services.conf"

if ! grep -Fq '.vibecode/bin' "$HOME/.bashrc" 2>/dev/null; then
  printf '\n# Termux VibeCode\nexport PATH="$HOME/.vibecode/bin:$PATH"\n' >> "$HOME/.bashrc"
fi

printf '%s\n' "${VIBECODE_VERSION:-0.1.0}" > "$STATE/version"
log "installed VibeCode $(cat "$STATE/version")"
log 'run: tv doctor && tv status'
