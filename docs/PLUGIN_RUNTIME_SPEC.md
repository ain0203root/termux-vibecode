# VibeCode Plugin Runtime Specification

## Purpose

Define a safe extension model for VibeCode modules without coupling plugins to the core runtime.

## Plugin manifest

Each plugin declares:

- name
- version
- required runtime version
- capabilities
- dependencies
- lifecycle hooks
- resource limits

## Lifecycle

```
install -> validate -> load -> start -> health -> stop -> unload
```

## Runtime rules

- Plugins cannot assume Android capabilities.
- Resource limits are explicit.
- Failures must be isolated from the core runtime.
- State changes must be recoverable.

## Future implementation

The runtime will expose a native API for hot paths and a command interface for administration.
