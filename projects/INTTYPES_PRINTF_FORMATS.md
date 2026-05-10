# Inttypes printf format migration

`src/core/sim_printf_fmts.h` provides local cross-platform format
modifier macros for pointer-sized, `size_t`, and 64-bit integer
printing. The fixed-width integer portions should be retired in favor
of the standard `<inttypes.h>` macros.

## Goal

Use standard C formatting macros for fixed-width integer values:

- `PRId64`, `PRIu64`, `PRIx64`, and related macros for `int64_t` and
  `uint64_t`;
- the matching `SCN*` macros if formatted input sites need them later.

This should remove the need for project-specific `T_INT64_FMT` and
`T_UINT64_FMT` handling for fixed-width integer values.

## Scope

- Audit all users of `T_INT64_FMT` and `T_UINT64_FMT`.
- Replace fixed-width integer format construction with `<inttypes.h>`
  macros.
- Keep unrelated pointer and `size_t` formatting helpers only if they
  still provide value beyond the standard library facilities.
- Update developer documentation after the code migration.

## Notes

This is intentionally separate from the stdint spelling migration. The
current type-name conversion should not also change format string
construction, because format strings have their own warning and runtime
failure modes.
