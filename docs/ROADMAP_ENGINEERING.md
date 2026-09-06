# Engineering Roadmap

## Runtime foundation

The platform evolves around explicit layers:

- native execution core
- process lifecycle management
- service supervision
- state persistence
- observability

## vsh evolution

Future shell work:

- lexer/parser separation
- command AST
- better completion model
- command cache
- performance profiling

## Platform services

Services should be declarative, restartable and observable. User data must remain separate from platform state.

## Quality gates

Every major feature should include:

- build verification
- regression tests
- benchmark impact measurement
- Android runtime validation

## Long-term direction

Termux VibeCode remains a userland platform layer: powerful, reversible and compatible with Android security boundaries.
