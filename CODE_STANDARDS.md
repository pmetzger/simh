# Code Standards

This repository targets C17 or better on supported host platforms.
Assume recent POSIX systems and recent Windows APIs; do not preserve
workarounds for older C dialects or obsolete host environments unless
there is an explicit project decision to do so.

## Standard Types

Use standard C type names where they express the intended interface:

- `bool`, `true`, and `false` from `<stdbool.h>`
- fixed-width integer types from `<stdint.h>`, such as `uint32_t`,
  `int64_t`, and `uint8_t`
- `uint_t` and `uchar_t` from `sim_types.h` for compact spelling of
  `unsigned int` and `unsigned char`

Any source file that uses `bool`, `true`, or `false` must include
`<stdbool.h>` directly. Any source file that uses `<stdint.h>` type
names must include `<stdint.h>` directly or include a project header
whose purpose is to provide those type aliases.

Machine words, registers, addresses, and bit fields should normally use
unsigned types. C does not define signed integer overflow, and shifts of
signed values are easy to get wrong. New simulator code should use
unsigned types consistently for machine state.

## String APIs

Do not use unbounded string construction APIs in new code:

- use `snprintf` instead of `sprintf`
- use `strlcpy` instead of `strcpy`
- use `strlcat` instead of `strcat`

When replacing legacy code, preserve behavior and add tests first. Use
the return values of bounded APIs when truncation matters to the caller.

Treat `strncpy` and `strncat` as special cases, not as general safety
APIs. `strncpy` may omit the terminating NUL and may pad the
destination. `strncat` takes a count of bytes to append, not the total
destination size. Uses that intend ordinary C string copy or append
should normally become `strlcpy`, `strlcat`, or tracked-offset
`snprintf`.

If a fixed buffer is the wrong abstraction, record the issue and discuss
a dynamic-string refactor. Do not fold large buffer-design changes into
a mechanical safety pass without agreement.

## Allocation

Use `xmalloc`, `xcalloc`, `xrealloc`, `xstrdup`, and `xstrndup` for
ordinary internal allocations where meaningful local recovery is not
possible. This includes small bookkeeping objects, dynamic strings,
owned copies, and parse/build buffers. If these allocations fail, the
process is already in a state where pretending to continue is usually
less safe than stopping.

Do not use fatal allocation wrappers for large user-directed resource
requests where recovery is part of the interface. Simulator memory,
RAM disks, large media buffers, optional caches, and other allocations
whose sizes come directly from user configuration should validate the
request, report an error, and leave simulator state coherent so the
user can correct the command.

## Time APIs

Code must distinguish host wall-clock time, host monotonic/watchdog
time, guest-visible device time, and simulator event/instruction time.
Do not use one time domain as a substitute for another.

Kernel-facing calls that obtain the current time or wait for time to
pass must go through the runtime wrappers in `sim_time.h`, such as
`sim_clock_gettime`, `sim_time`, `sim_nanosleep`, and `sim_sleep`.
These wrappers are thin seams around the modern POSIX-shaped time APIs,
chiefly `clock_gettime` and `nanosleep`. They make time-dependent code
testable and keep platform differences out of core, runtime, and
simulator logic.

The project uses these POSIX-shaped APIs on every supported host,
including Windows. When a supported platform does not provide a function
directly, the compatibility layer supplies the same interface. New code
should therefore call the shared wrappers or the standardized
POSIX-shaped helper directly, rather than adding local `_WIN32` branches
or older API alternatives.

Timestamp-processing helpers that transform an existing timestamp but do
not obtain time from the host or wait should be used directly. Prefer
`localtime_r`, `gmtime_r`, `strftime`, `mktime`, and `difftime`. These
are deterministic transformations of caller-supplied time values and do
not need the test seam used for kernel-facing clock and sleep calls.

Do not add new direct uses of `time`, `gettimeofday`, `sleep`, `usleep`,
or raw `clock_gettime` / `nanosleep` in common runtime, core, or
simulator code unless there is a documented reason. Do not use
`localtime`, `gmtime`, `ctime`, or `asctime` in new code; use the
reentrant POSIX-shaped interfaces and explicit formatting instead.

Simulator event scheduling and guest-visible timers are not host
watchdogs. Test timeouts and host safety guards must use a host time
domain that remains correct even when guest time is accelerated,
stalled, or virtualized.

## Includes

When adding or touching include blocks, keep system includes
alphabetized where practical. Preserve required ordering and keep
conditional platform blocks readable.

Prefer direct includes for the standard facilities a source file uses.
For example, include `<stdbool.h>` for `bool` and `<stdint.h>` for
fixed-width integer names.

## Formatting

When adding new source files, run `clang-format` on those files before
finishing the change.

Do not run `clang-format` across existing legacy files merely to
normalize formatting. When editing legacy files, keep changes narrowly
scoped and avoid moving aligned trailing comments unless the local edit
requires it.

New functions should be written in the current project style even when
they are added to legacy files. Give every new function a short header
comment that explains what it does to a reader who is not familiar with
the surrounding code. Include important ownership, lifetime, truncation,
or failure behavior in that comment when those details are part of the
function's contract.

Public functions should have the same level of explanatory comment in
the relevant header file where applicable. Static helper comments belong
next to the helper definition in the `.c` file.

## Testing

Production behavior changes and safety refactors must be covered by
automated tests. Write characterization tests before changing legacy
code when the current behavior must be preserved.

Run focused tests for the changed area, then the relevant build target.
For string and format changes, inspect compiler diagnostics for format,
truncation, and bounded-string warnings.
