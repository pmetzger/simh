# Purge Legacy Display Backends

The active display path should be the SDL-based path:

1. Simulator display engines emulate device behavior.
2. `src/components/display/display.c` provides the shared XY display layer.
3. `src/components/display/sim_ws.c` implements the `ws_*` backend using
   `src/runtime/sim_video.c` and SDL.

The simulator display engines are not themselves backend implementations.
They sit above the backend abstraction and should continue to work through the
SDL-backed `ws_*` implementation.

## Goal

Remove raw host display backends that bypass SDL:

- `src/components/display/x11.c`
- `src/components/display/win32.c`
- `src/components/display/carbon.c`

After this cleanup, display support should consistently route through
`sim_ws.c` and SDL.

## Keep

These files are simulator display engines or shared display logic, not legacy
host backends:

- `src/components/display/display.c`
- `src/components/display/display.h`
- `src/components/display/iii.c`
- `src/components/display/iii.h`
- `src/components/display/ng.c`
- `src/components/display/ng.h`
- `src/components/display/type340.c`
- `src/components/display/type340.h`
- `src/components/display/type340cmd.h`
- `src/components/display/vt11.c`
- `src/components/display/vt11.h`
- `src/components/display/vtmacs.h`
- `src/components/display/ws.h`

Some standalone display test utilities may still mention or demonstrate the
old raw backends. They should be audited separately before deletion:

- `src/components/display/test.c`
- `src/components/display/tst340.c`
- `src/components/display/vttest.c`
- `src/components/display/README`

## Process

1. Confirm no CMake targets build `x11.c`, `win32.c`, or `carbon.c`.
2. Search non-CMake build notes and documentation for references to those
   files.
3. Remove the legacy backend source files.
4. Update documentation and standalone build notes so SDL is the only supported
   backend.
5. Build with video enabled and run the video/display-related unit and
   integration tests.

