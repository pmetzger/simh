# printf Format Audit

The FreeBSD 15 Release build exposed many format warnings in legacy
simulator code. Most of the noisy warnings come from LP64 host ABI
differences: on FreeBSD/amd64, fixed-width integer types such as
`uint64_t` may be typedefs of `unsigned long`, while old code often
prints them with `%llx`, `%llo`, or `%lld`.

This is a portability cleanup, not a FreeBSD-specific workaround. The
goal is to make formatted output correct across supported POSIX hosts
and Windows without weakening type checking or hiding real warnings.

## Work

- Audit printf-family warnings from FreeBSD builds.
- Prefer the standard `<inttypes.h>` format macros for fixed-width
  integer types.
- Use project type aliases and existing formatting helpers where they
  already express the intended machine-word type clearly.
- Avoid casts that merely silence warnings unless the cast documents a
  genuine conversion to the printed type.
- Keep changes local and behavior-preserving; many affected strings are
  debug output, but some may be user-visible.

## Testing

- Rebuild on FreeBSD with warnings visible and confirm the audited
  warnings are gone.
- Rebuild on macOS and at least one Linux host to avoid trading one ABI
  warning set for another.
- Run unit and integration tests after each cleanup slice.
- Add focused tests for any changed formatting that affects command
  output, saved files, diagnostics, or other user-visible text.

## Likely First Slice

Start with high-volume, low-risk debug output in individual simulator
directories. Keep each change reviewable by simulator family so that
format fixes can be checked against the machine word sizes and local
type aliases used by that emulator.
