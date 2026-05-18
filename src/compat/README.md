# `src/compat`

This directory is for small compatibility functions that provide
standard APIs missing from some supported host platforms.

Typical contents include implementations imported or adapted from
well-established upstream sources, such as OpenBSD libc routines, when
the host platform does not provide them itself.

Examples of the intended scope:

- `strlcpy`
- `strlcat`
- `localtime_r`
- `gmtime_r`
- Windows-only libc shims such as `setenv`
- other small libc-style compatibility routines

Current compatibility routines:

| Routine | Source file | Selection | May be needed on |
| --- | --- | --- | --- |
| `strlcpy` | `strlcpy.c` | feature test | glibc, minimal POSIX, Windows |
| `strlcat` | `strlcat.c` | feature test | glibc, minimal POSIX, Windows |
| `localtime_r` | `localtime_r.c` | Windows-only | Windows CRTs |
| `gmtime_r` | `gmtime_r.c` | Windows-only | Windows CRTs |
| `setenv` | `setenv.c` | Windows-only | Windows CRTs |
| `unsetenv` | `setenv.c` | Windows-only | Windows CRTs |

POSIX hosts should provide `localtime_r`, `gmtime_r`, `setenv`,
and `unsetenv`. Use `src/runtime/sim_tempfile.c` for project code that
needs temporary files instead of adding libc-style tempfile shims here.

Compatibility declarations are split by scope:

- `string_compat.h` declares string routines such as `strlcpy` and
  `strlcat`.
- `sim_win32_compat.h` declares Windows-only shims and CRT spelling
  aliases.

Rules for code placed here:

- keep this directory narrowly focused on compatibility shims
- prefer well-known, established implementations over ad hoc rewrites
- use CMake feature tests for shims that may be needed on multiple hosts,
  such as `strlcpy` and `strlcat`
- guard Windows-only declarations and source files with `_WIN32`
- build Windows-only shims only on Windows
- keep higher-level runtime or simulator logic out of this directory

This directory is not for general utilities, runtime services, or new
cross-platform abstractions. Those belong elsewhere in `src/`.
