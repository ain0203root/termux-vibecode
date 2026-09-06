# Termux VibeCode

A high-performance, userland-first platform for turning Termux into a reproducible developer workstation on Android.

## What this is

Termux VibeCode keeps the Android kernel and Android security model intact, but replaces the operator experience with a native command-line layer, a supervisor, fast project workspaces, observability, safe performance profiles, and optional AI routing.

The current milestone is concrete:

- `tv` — one control CLI for setup, status, doctor, services, workspaces and tuning.
- `vsh` — a small native C shell with built-ins, pipelines, redirections, history and environment expansion.
- `tvd` — a compact control/state engine; the hot command path stays native and shell-first.
- `tune` — safe userland performance-profile detection without pretending Android kernel knobs are universally writable.
- reproducible bootstrap and verification.
- native, sanitizer, Python, installed-platform and Android-emulator CI.
- `tvsh` packaged with the official Termux package builder for `x86_64` and `aarch64`.

Termux itself already provides a package manager and Android-compatible software built with the Android NDK; this project builds a focused platform layer above that base rather than forking the whole Termux distribution.

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

The project also publishes a native `tvsh` Termux package from CI. The package workflow builds both `x86_64` and `aarch64` variants using the upstream Termux package infrastructure.

## Performance doctrine

Hot paths should be native, short, cacheable and measurable. Python is used for orchestration, diagnostics and control-plane tasks, not as the interactive shell parser or command dispatcher.

The project never assumes that a rooted device exists. When elevated capabilities are present, a future capability layer may use them explicitly; without them the platform remains functional.

## Verification

Every change is checked on the host with strict native warnings, native smoke tests, a microbenchmark, an installed-platform end-to-end test, sanitizer instrumentation, and Python tests. The Android workflow additionally boots GitHub-hosted Android Emulator instances on API 34 and 35, installs the pinned official Termux debug APK, initializes the real Termux userland, builds the native shell inside that userland, and executes the Android smoke suite.

The Android test deliberately validates the Termux process context rather than bypassing it. The public Termux command-execution API is permission-protected, so the debug-APK test harness uses the package UID directly through `run-as` instead of relying on an exported service that is not publicly callable.

## Repository layout

```text
platform/     bootstrap, launcher and userland profiles
core/         supervisor, state and diagnostics
native/vsh/   native interactive shell
services/     declarative long-running services
config/       safe defaults and tuning profiles
bench/        repeatable microbenchmarks
termux-package/ local Termux package recipe
tests/        shell/native/control-plane tests
.github/      CI and Android emulator validation
```

## Scope

This is a platform project, not an Android kernel replacement. The goal is to make Termux feel like a coherent operating environment for serious development while remaining reversible, transparent and testable.

The native shell is intentionally smaller than Bash today. Unsupported shell-language features are not presented as implemented; the project grows that surface only when the behavior can be tested on both Linux and Android.
