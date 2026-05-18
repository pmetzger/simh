# SIM I/O Primitives Audit

## Problem

The `sim_` file I/O APIs mix low-level file opening with command-facing
host path interpretation. In particular, `sim_fopen()` currently applies
quote stripping and `~/` expansion before calling the host C library open
routine. That preserves existing behavior, but it makes the API boundary
unclear: callers cannot tell from the name whether they are asking for raw
`fopen()` behavior or simulator command path behavior.

## Goals

- Document which APIs are intended to behave like host C/POSIX primitives.
- Document which APIs are intended to consume simulator command host paths.
- Separate path interpretation from raw file operations where doing so can
  be audited and tested safely.
- Preserve compatibility for existing command scripts unless a deliberate
  migration plan says otherwise.

## Notes

- The current Windows path fix deliberately keeps `sim_fopen()` behavior
  compatible and only changes quote parsing to preserve backslashes.
- A future cleanup should avoid broad call-site churn until there is a
  tested migration plan for each public `sim_` I/O primitive.
