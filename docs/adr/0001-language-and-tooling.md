# ADR 0001: Language and tooling

## Status

Accepted, 2026-05-06

## Context

Foolscap targets two platforms: ESP32-S3 hardware and a desktop simulator. The same application code should compile for both with minimal divergence. The project is built solo, by someone newer to C and embedded, with the goal of producing a high-quality codebase suitable for both personal use and possible future contributors.

## Decision

- **Language:** C11. Native to ESP-IDF. Universally supported on host. Avoids the complexity of C++ for an embedded project at this scale.
- **Build system:** CMake. ESP-IDF requires it; using it for the SDL build keeps tooling uniform.
- **Test framework:** Unity. Standard for embedded C, single-file, host-runnable.
- **Style enforcement:** clang-format. Committed config, no debate.
- **Static analysis:** clang-tidy + cppcheck. Run in CI.
- **Sanitizers:** AddressSanitizer + UndefinedBehaviorSanitizer in the SDL build.
- **License:** MIT. Maximally permissive, friendly to contributors and possible future commercial use.

## Consequences

- New contributors must be comfortable with C11 idioms and CMake.
- Some quality-of-life features available in C++ are unavailable; this is accepted as the cost of language simplicity and ESP-IDF naturalness.
- The SDL simulator build doubles as the test harness, which is a deliberate simplification at v0.1 scope.
