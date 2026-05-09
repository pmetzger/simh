# `SIM_NOINLINE` and ROM Delay Calibration

## Current Use

`SIM_NOINLINE` is a portability macro in `src/core/sim_defs.h` for asking
the compiler not to inline a function:

- MSVC uses `_declspec (noinline)`.
- GCC-compatible compilers use `__attribute__ ((noinline))`.
- Other compilers get an empty definition.

The macro is currently only used by the ROM delay support in
`src/runtime/sim_timer.c`:

- `_rom_swapb()`
- `sim_rom_read_with_delay()`
- `sim_get_rom_delay_factor()`

That code calibrates a host busy loop intended to simulate regulated ROM
access timing. The no-inline annotations are part of keeping the compiler
from optimizing the loop into something too simple to use as timed work.

## Fragility

This is not a strong contract. `SIM_NOINLINE` is only a compiler hint, and
the surrounding behavior still depends on several assumptions:

- the compiler respects the no-inline request;
- the compiler does not otherwise simplify the arithmetic too aggressively;
- function-call overhead remains stable enough for calibration;
- host CPU frequency, scheduler behavior, and load do not dominate the
  measurement;
- a calibrated host busy loop is an acceptable proxy for simulated ROM
  latency.

The `volatile` state in the loop helps prevent the work from being deleted
entirely, and `SIM_NOINLINE` reduces one class of optimization, but neither
turns the mechanism into a reliable timing abstraction.

## Cleanup Direction

This should be treated as technical debt in the ROM timing machinery rather
than as a general-purpose project facility.

Future work should investigate whether ROM access delay can be represented
more directly in simulator time or device timing state instead of relying on
opaque host CPU work. The right solution may vary by simulator and by the
hardware behavior being modeled, so this note should not be read as requiring
one specific design.

In the meantime, avoid broadening `SIM_NOINLINE` use unless there is a clear
need. New timing-sensitive code should prefer explicit simulator timing
models over calibrated compiler-sensitive busy loops.
