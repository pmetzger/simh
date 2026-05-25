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

Use the shared unsigned bit and field helpers in `uint_bits.h` for
common mask, field extraction, field insertion, and aligned byte/word
operations. Prefer extending that API when a generally useful helper is
missing instead of adding local one-off masks, shifts, and byte-lane
calculations in simulator code.

## String APIs

Do not use unbounded string construction APIs in new code:

- use `snprintf` instead of `sprintf`
- use `strlcpy` instead of `strcpy`
- use `strlcat` instead of `strcat`
- usually use `xstrdup` or `xstrndup` for owned string copies
- use `strlappendf` for formatted appends to an existing fixed buffer

The `xstrdup` and `xstrndup` helpers are allocation-aborting variants of
the standard-style `strdup` and `strndup` interfaces. Like the other
fatal allocation wrappers described in the Allocation section, use them
only where that policy is appropriate. Use `strdup` or `strndup`
directly only when local recovery from allocation failure is meaningful.

The `strlappendf` helper is a local ZIMH utility for appending formatted
text to an existing NUL-terminated fixed buffer while preserving
`strlcpy`/`strlcat`-style truncation semantics.

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

Use `dynstr` for internal open-ended string construction where a fixed
buffer would make truncation part of the implementation by accident.
Keep compatibility boundaries explicit: a legacy fixed-buffer API may
still copy from a dynamic result at the boundary until the public API can
be changed.

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

## Temporary Files

Production C code should create named temporary files through
`sim_tempfile_open` or `sim_tempfile_open_stream`. Code that needs an
anonymous scratch stream should use `sim_tmpfile`. Do not add new direct
uses of `mkstemp`, `mkstemps`, `mktemp`, `tmpnam`, C `tmpfile`, or
hand-built process-ID-based temporary paths.

Although C `tmpfile` is a standard interface, it is not a good portable
default on Windows. Use the project temporary-file APIs unless a test is
specifically exercising a compatibility shim.

## Randomness

The randomness API is still being renovated. Until that work is done, do
not add new direct uses of host `rand` or `srand`. Code that needs a
deterministic simulator or test pseudo-random stream should use the
project PRNG interface so behavior is reproducible across hosts.

Future host-quality randomness work should stay separate from the
deterministic simulator PRNG. UUID generation, entropy, security-sensitive
values, and host identity material should eventually use an explicit
host-random or UUID-specific API. If new code needs that before the
shared helper exists, discuss the API instead of choosing a local random
source.

## Host Platform Boundaries

Keep host operating system integration in shared runtime, compatibility,
component, or build-system layers where practical. Simulator device
models should focus on emulated machine behavior, not host GUI, timing,
networking, filesystem, or Windows/POSIX plumbing.

Prefer direct host APIs or shared wrappers for host introspection. Do not
use `popen` merely to ask the host for information such as the hostname,
kernel identity, or CPU model when a direct OS API is practical. `popen`
is acceptable only when the feature intentionally runs an external
command or shell pipeline.

New display work should route through the shared display/video layer,
currently the SDL-backed path, rather than adding or reviving raw host
backends such as direct X11, Win32, or Carbon display code.

## Utility Code

Reusable C support code that is not specific to the simulator framework
belongs under `src/lib` with public headers in `src/include`. Generic
helpers should avoid dependencies on `sim_defs.h` or other
simulator-specific headers.

Use neutral names for generic utility facilities, such as `xalloc`,
`dynstr`, and `string_util`, rather than `sim_*` names. Keep
simulator-specific policy macros and APIs in simulator headers.

## Includes

When adding or touching include blocks, keep system includes
alphabetized where practical. Preserve required ordering and keep
conditional platform blocks readable.

Prefer direct includes for the standard facilities a source file uses.
For example, include `<stdbool.h>` for `bool` and `<stdint.h>` for
fixed-width integer names.

Callers should include the header for the API they use directly rather
than relying on incidental transitive includes.

New headers should use conventional include guards:

```c
#ifndef EXAMPLE_H_
#define EXAMPLE_H_ 1
```

Do not use replacement value `0` for new include guards.

## Build Configuration

Keep standard CMake variables unchanged. For project-defined options,
use `WITH_*` for product or runtime features and `ENABLE_*` for tooling,
analysis, and validation modes. Avoid negative option names such as
`NO_*`, `DONT_*`, and `WITHOUT_*`.

Prefer configuration-time capability checks over hard-coded host-name
checks when the real dependency is a header, function, type, constant,
or behavior. Keep platform-family checks only when the distinction is
inherently about the platform family.

## Vendored Code

Prefer using maintained upstream packages through the build system over
copying third-party source into the tree. Vendor code only when a normal
package dependency is not practical or when there is a deliberate project
reason to carry a copy.

Keep vendored or imported third-party code as close to upstream as
practical. Do not apply broad ZIMH style, formatting, or API-renaming
passes inside vendored code unless the change is needed for integration
and cannot reasonably live in adapter code.

Local integration glue should live outside the vendored subtree where
practical. Preserve upstream license and copyright notices, and record
the upstream version or commit used for substantial vendor imports.

## Licensing

New ZIMH-owned source files should use SPDX license headers from the
start. When touching older SIMH-owned source files, convert the existing
license header to the appropriate SPDX form as part of the local edit
when that can be done without obscuring the functional change.

Do not rewrite imported third-party source merely to convert license
text. License cleanup in vendored code should happen only as part of a
deliberate vendor import or integration pass.

## Formatting

When adding new source files, run `clang-format` on those files before
finishing the change.

Do not run `clang-format` across existing legacy files merely to
normalize formatting. When editing legacy files, keep changes narrowly
scoped and avoid moving aligned trailing comments unless the local edit
requires it.

New functions should be written in the current project style even when
they are added to legacy files. Give every new function a short header
comment written for a maintainer entering this part of the code for the
first time. The comment should explain what the function does for a
reader who is not familiar with the surrounding code, including the
function's purpose, the role it plays in the local design, and any
assumptions the caller must understand to use or modify it safely.
Include important ownership, lifetime, truncation, or failure behavior
when those details are part of the function's contract.

Public functions should have the same level of explanatory comment in
the relevant header file where applicable. Static helper comments belong
next to the helper definition in the `.c` file.

## Testing

Production behavior changes and safety refactors must be covered by
automated tests. Write characterization tests before changing legacy
code when the current behavior must be preserved.

When production code needs a test seam for internal behavior, prefer an
explicit internal header named after the implementation file, such as
`foo_internal.h` for `foo.c`. Internal headers expose implementation
entry points to unit tests and closely related implementation files;
they are not public API. Public headers such as `foo.h` should not grow
test-only declarations, and `_internal.h` headers should not become the
home for interfaces intended for ordinary callers.

Run focused tests for the changed area, then the relevant build target.
For string and format changes, inspect compiler diagnostics for format,
truncation, and bounded-string warnings.
