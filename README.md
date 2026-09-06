# Termux VibeCode

A high-performance, userland-first platform for turning Termux into a reproducible developer workstation on Android.

## What this is

Termux VibeCode keeps the Android kernel and Android security model intact, but replaces the operator experience with a native command-line layer, a supervisor, fast project workspaces, observability, and optional AI routing.

The first milestone is concrete:

- `tv` — one control CLI for setup, status, doctor, services, workspaces and tuning.
- `vsh` — a tiny native C shell with built-ins, pipelines, redirections, history and cached command lookup.
- `tvd` — a small control/state engine; the hot command path stays native and shell-first.
- `tune` — safe userland performance profile detection without pretending Android kernel knobs are universally writable.
- reproducible bootstrap and verification.
- CI on every change.

Termux itself already provides a package manager and Android-compatible software built with the Android NDK; this project builds a platform layer above that base rather than forking the whole Termux distribution.

## Install

```sh
git clone https://github.com/ain0203root/termux-vibecode.git
cd termux-vibecode
bash platform/install.sh
```

Then:

```sh
tv doctor
tv status
tv tune
```

To try the native shell without replacing your login shell:

```sh
tv shell
```

## Performance doctrine

Hot paths should be native, short, cacheable and measurable. Python is used for orchestration, diagnostics and control-plane tasks, not as the interactive shell parser or command dispatcher.

The project never assumes that a rooted device exists. When elevated capabilities are present, a future capability layer may use them explicitly; without them the platform remains functional.

## Repository layout

```text
platform/     bootstrap, launcher and userland profiles
core/         supervisor, state and diagnostics
native/vsh/   native interactive shell
services/     declarative long-running services
config/       safe defaults and tuning profiles
bench/        repeatable microbenchmarks
tests/        shell/native/control-plane tests
.github/      CI
```

## Scope

This is a platform project, not an Android kernel replacement. The goal is to make Termux feel like a coherent operating environment for serious development while remaining reversible and transparent.
