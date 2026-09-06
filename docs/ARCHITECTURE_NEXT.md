# Termux VibeCode Next Architecture

## Goal

Build a high-performance Android userland platform layer above Termux without replacing the Android kernel or breaking the security model.

## Runtime layers

```
Android kernel
    |
Termux userland
    |
VibeCode runtime
    |-- vsh native command engine
    |-- tv control plane
    |-- service supervisor
    |-- plugin runtime
    |-- workspace manager
    |-- observability
```

## Engineering rules

- Hot paths stay native.
- Every performance claim needs a benchmark.
- Every subsystem needs recovery behaviour.
- Updates must be reversible.
- Android capabilities are detected, not assumed.

## Future modules

### vsh evolution

Move from command execution directly toward a structured pipeline:

lexer -> parser -> execution graph -> scheduler -> process lifecycle

### Plugin system

Plugins should declare:

- capabilities
- dependencies
- lifecycle hooks
- resource limits

### Runtime

The runtime should provide:

- service supervision
- health checks
- state snapshots
- crash recovery
- diagnostics

This document is a design target and must follow the implementation reality.
