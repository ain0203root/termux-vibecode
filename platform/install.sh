#!/data/data/com.termux/files/usr/bin/bash
set -Eeuo pipefail
IFS=$'\n\t'
ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
STATE="${HOME}/.vibecode"
BIN="$STATE/bin"
mkdir -p "$STATE" "$BIN" "$STATE/logs" "$STATE/workspaces" "$STATE/run"

log(){ printf '[vibecode] %s\n' "$*"; }
log 'checking Termux toolchain'
for x in bash clang make python3 git; do command -v "$x" >/dev/null 2>&1 || log "optional tool missing: $x"; done

if command -v clang >/dev/null 2>&1 && command -v make >/dev/null 2>&1; then
  make -C "$ROOT/native/vsh" clean all
  install -m 755 "$ROOT/native/vsh/vsh" "$BIN/vsh"
else
  log 'clang/make unavailable; native shell will not be installed'
fi

install -m 755 "$ROOT/platform/tv" "$BIN/tv"
install -m 755 "$ROOT/platform/tune" "$BIN/tune"
install -m 755 "$ROOT/platform/tv-ai" "$BIN/tv-ai"
install -m 755 "$ROOT/core/tvd.py" "$STATE/tvd.py"
install -m 755 "$ROOT/core/capabilities.py" "$STATE/capabilities.py"
install -m 755 "$ROOT/core/supervisor.py" "$STATE/supervisor.py"
cp "$ROOT/config/default.env" "$STATE/default.env"
cp "$ROOT/config/performance.env" "$STATE/performance.env"
cp "$ROOT/services/services.conf" "$STATE/services.conf"
printf '%s\n' "${VIBECODE_VERSION:-0.1.0}" > "$STATE/version"

# Termux already places $PREFIX/bin on PATH. Use symlinks so the installed
# commands become visible to the current Termux environment without requiring
# the parent shell to be re-sourced; state remains centralized under ~/.vibecode.
if [[ -n "${PREFIX:-}" && -d "$PREFIX/bin" && -w "$PREFIX/bin" ]]; then
  ln -sfn "$BIN/tv" "$PREFIX/bin/tv"
  ln -sfn "$BIN/tune" "$PREFIX/bin/tune"
  ln -sfn "$BIN/tv-ai" "$PREFIX/bin/tv-ai"
  if [[ -x "$BIN/vsh" ]]; then
    ln -sfn "$BIN/vsh" "$PREFIX/bin/vsh"
  fi
fi

if ! grep -Fq '.vibecode/bin' "$HOME/.bashrc" 2>/dev/null; then
  printf '\n# Termux VibeCode\nexport PATH="$HOME/.vibecode/bin:$PATH"\n' >> "$HOME/.bashrc"
fi
log "installed VibeCode $(cat "$STATE/version")"
log 'run: tv doctor && tv status'
