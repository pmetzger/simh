# Random Number Generation

The codebase currently hides uses of `rand()` and `srand()` behind project
macros in `scp.h`:

```c
#define rand sim_rand
#define srand(seed) sim_srand(seed)
```

The implementation in `scp.c` is a small Lehmer generator. This avoids
depending on host libc `rand()` behavior and keeps results reproducible, but it
is still a weak generator by modern standards.

## Current Uses

- `src/runtime/sim_tmxr.c` seeds from `sim_os_msec()` while polling
  connections, then uses one random bit to vary whether outgoing or incoming
  connection completion is checked first. This avoids symmetric virtual null
  modem connection races.
- `src/runtime/sim_disk.c` uses random bytes in the fallback UUID generator.
  This is a weak use of randomness and should be replaced with a better
  UUID-generation path.
- `src/runtime/sim_disk.c` uses `srand(0)` and `rand()` for deterministic disk
  test coverage.
- `src/runtime/sim_tape_testlib.c` uses `srand(0)` and `rand()` for
  deterministic tape test data.
- `src/runtime/sim_console.c` uses `rand()` for remote console sampling and
  repeat dithering.
- `src/runtime/sim_ether.c` uses random bytes to fill loopback packets. The
  current helper appears to reseed on every call because its initialization flag
  is never set.
- Several simulators use `rand()` for incidental simulated hardware behavior,
  including random disk sector fields, GUI update jitter, timer/probability
  behavior, and DDCMP corruption-test behavior.

## Goals

- Replace the current Lehmer generator with a small, deterministic PRNG with
  better statistical behavior.
- Keep deterministic test fixtures deterministic.
- Avoid using the simulator PRNG where host-provided randomness or UUID APIs are
  the correct interface.
- Preserve the existing public `sim_srand()` and `sim_rand()` interface unless
  there is a clear reason to add a new one.

## Suggested Direction

Use a compact, well-known non-cryptographic PRNG such as PCG32 or xoshiro for
`sim_rand()`. Either would be a substantial improvement over the current
generator while remaining small and portable. PCG32 is a good default candidate
because it has a simple 64-bit state, straightforward seeding, and good quality
for simulation jitter and deterministic test data.

This should not be treated as cryptographic randomness. Uses that need
host-quality randomness, such as UUID generation, should use platform APIs or a
separate host-random helper.

## Work Items

1. Add tests that lock down deterministic `sim_srand()` / `sim_rand()` behavior
   for the selected replacement generator.
2. Replace the current Lehmer implementation in `scp.c` with the selected
   generator.
3. Audit current callers and separate deterministic simulation/test randomness
   from host-random needs.
4. Fix the `sim_ether.c` helper so it does not reseed on every byte.
5. Replace the fallback UUID byte generator in `sim_disk.c` with an appropriate
   host-random or UUID-specific implementation.
