#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="${RUNNER_TEMP:-/tmp}/termux-packages-vibecode"
rm -rf "$WORK"
git clone --depth 1 https://github.com/termux/termux-packages.git "$WORK"
mkdir -p "$WORK/sources/tvsh" "$WORK/packages/tvsh" "$WORK/output"
cp "$ROOT/native/vsh/vsh.c" "$ROOT/native/vsh/Makefile" "$ROOT/native/vsh/bench.c" "$WORK/sources/tvsh/"
cp "$ROOT/termux-package/tvsh/build.sh" "$ROOT/termux-package/tvsh/README" "$WORK/packages/tvsh/"
cd "$WORK"
./scripts/run-docker.sh ./build-package.sh -f -I -a x86_64 -o /home/builder/termux-packages/output tvsh
find output -maxdepth 1 -type f -name 'tvsh_*.deb' -print
