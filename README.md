# Termux VibeCode

A high-performance, userland-first platform for turning Termux into a reproducible developer workstation on Android.

## What this is

Termux VibeCode keeps the Android kernel and Android security model intact, but replaces the operator experience with a native command-line layer, a service supervisor, fast project workspaces, observability, safe performance profiles, and optional AI routing.

The current implementation provides:

- `tv` — one control CLI for status, diagnostics, services, workspaces, tuning, AI and the native shell.
- `vsh` — a small native C shell with interactive editing, history, built-ins, pipelines, redirections, quoting and shell-status/environment expansion.
- `tvd` — a compact control/state engine; the hot command path stays native and shell-first.
- `supervisor.py` — opt-in service lifecycle management with process-group cleanup, PID identity checks and bounded restart backoff.
- `tune` — safe userland performance-profile detection without pretending Android kernel knobs are universally writable.
- reproducible installation, ownership tracking and reversible uninstall.
- native, sanitizer, static-analysis, Python, installed-platform, package and Android-emulator CI.
- `tvsh` package builds using the official Termux package infrastructure for `x86_64` and `aarch64`.

Termux VibeCode is deliberately a platform layer above the Termux userland rather than a fork of the Android kernel or the complete Termux distribution.

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
tv shell
```

The installer exposes VibeCode commands directly through `$PREFIX/bin`, so a new shell session is not required for the command symlinks to become visible. It records its ownership marker under `~/.vibecode/.install-marker`.

## Uninstall

To remove the VibeCode command layer without deleting projects, state or logs:

```sh
bash platform/uninstall.sh
```

Uninstall refuses to modify the current prefix unless the ownership marker matches that prefix.

## Native shell

`vsh` is designed around a small, measurable hot path. It supports interactive line editing, history navigation, `cd`, `pwd`, `echo`, `export`, `unset`, `history`, `which`, `true`, `false`, `:`, `exit` and `help`. It also supports pipelines, `<`, `>`, `>>`, quoting, backslash escaping, `$NAME`, `${NAME}`, `$$` and `$?`.

The shell is intentionally not presented as a Bash replacement. Unsupported shell-language features remain outside the implemented contract until they have corresponding regression coverage.

## Performance doctrine

Hot paths should be native, short, cacheable and measurable. Python is used for orchestration, diagnostics and control-plane tasks, not as the interactive shell parser or command dispatcher.

The project never assumes that a rooted device exists. Elevated capabilities are detected explicitly and are not required for the base platform.

## Verification

Host CI runs shell syntax/static analysis, Clang static analysis, strict native compilation, native smoke tests, a repeatable microbenchmark, installed-platform end-to-end tests, sanitizer instrumentation and Python tests.

The Android workflow boots GitHub-hosted Android Emulator instances on API 34 and 35, installs the pinned official Termux debug APK, initializes the real Termux userland, installs the required build tools, installs VibeCode into that userland, builds and executes `vsh`, exercises `tv status`/`tv doctor` and workspace creation, and verifies the reversible uninstall path.

The package workflow builds `tvsh` through the upstream Termux package builder and validates the resulting Debian package contents before publishing CI artifacts.

## Architecture

```text
Android kernel / framework
          |
        Termux
          |
   +------+------+
   |             |
  VSH            TV
(native)      (control)
   |             |
   +------+------+
          |
        TVD
          |
 state / services / logs
          |
 workspaces / toolchains / AI
```

## Repository layout

```text
platform/       installer, launcher, tuning and lifecycle
core/           supervisor, state and diagnostics
native/vsh/     native interactive shell and benchmark
services/       declarative long-running services
config/         safe defaults and tuning profiles
bench/          repeatable microbenchmarks
termux-package/ local Termux package recipe
tests/          shell/native/control-plane tests
.github/        CI, package and Android emulator validation
```

## Scope

This is a platform project, not an Android kernel replacement. The goal is to make Termux feel like a coherent operating environment for serious development while remaining reversible, transparent and testable.
