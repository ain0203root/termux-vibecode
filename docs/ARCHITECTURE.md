# Architecture

## Principle

VibeCode is a platform layer above the Termux userland. It does not pretend to replace the Android kernel, Android init, or the Termux package ecosystem.

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

## Native hot path

The interactive shell is implemented in C. It uses direct terminal I/O, a compact tokenizer, `fork`/`execvp` for pipelines and no Python process on each command. This is the first performance boundary.

## Control plane

`tv` is intentionally a small shell entry point. Python is reserved for discovery, diagnostics and stateful orchestration where its startup cost does not dominate interactive command latency.

## Performance rules

1. No telemetry process in the interactive hot path.
2. No network call for a local command.
3. Cache deterministic discovery information where safe.
4. Keep logs append-only and bounded by policy in later milestones.
5. Benchmark before claiming a regression or improvement.
6. Never use privileged Android interfaces unless a capability is explicitly available.

## Roadmap

### Foundation

Native shell, control plane, deterministic installer, diagnostics and CI.

### Runtime

A real supervisor with restart policy, process groups, health checks, dependency ordering and bounded logs.

### Toolchain

Workspace manifests, reproducible caches, compiler selection, parallel build orchestration and artifact provenance.

### AI fabric

Provider-neutral routing with local sockets, backpressure, cancellation, retry budgets, credential isolation and usage accounting.

### Android integration

Optional Termux:Boot integration, foreground-service companion, notification and power-awareness hooks, all opt-in.

### Distribution

Versioned bootstrap bundles, signed release artifacts and migration/rollback tooling.
