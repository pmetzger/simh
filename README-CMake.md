# Building with CMake

This project uses CMake as the source of truth for build metadata.
Simulator-local `CMakeLists.txt` files, the top-level simulator
inventory, and the packaging metadata are all maintained directly in
CMake.

The top-level `Makefile` is only a thin compatibility wrapper over the
default `build/release` CMake build. The native interface is CMake and
CTest.

For the normal build instructions, start with:

- `BUILDING.md`

For build-system maintenance guidance, see:

- `cmake/CMake-Maintainers.md`
- `cmake/CMake-Simulator-Maintainers.md`

## Preferred Build Layout

Use out-of-tree builds under `build/`:

- `build/release`
- `build/debug`

Built executables stay inside the active build tree under `bin/`, for
example:

- `build/release/bin`
- `build/debug/bin`

Typical release build:

```sh
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -S . -B build/release
cmake --build build/release
ctest --test-dir build/release --parallel --output-on-failure
```

Typical debug build:

```sh
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -S . -B build/debug
cmake --build build/debug
ctest --test-dir build/debug --parallel --output-on-failure
```

## Common Targets

Build the default simulator set:

```sh
cmake --build build/release
```

Build one simulator:

```sh
cmake --build build/release --target pdp11
cmake --build build/release --target vax
```

Build the experimental simulator set:

```sh
cmake --build build/release --target experimental-simulators
```

Build and run the host-side unit tests:

```sh
cmake --build build/release --target unit-tests
```

Build and run the simulator integration tests:

```sh
cmake --build build/release --target integration-tests
```

Build the developer/sample utility targets:

```sh
cmake --build build/release --target extra-tools
cmake --build build/release --target stub
cmake --build build/release --target frontpaneltest
```

## Useful CMake Options

Project-defined options follow two patterns:

- `WITH_*` for runtime or product features
- `ENABLE_*` for tooling or build-time behavior

The most commonly used options are:

- `CMAKE_BUILD_TYPE`
  Default: `Release` for single-config generators when unset.
  Use `Release`, `Debug`, or `RelWithDebInfo` for single-config
  generators such as Ninja. Multi-config generators select the
  configuration at build and test time. `RelWithDebInfo` is optimized
  like a release build while retaining debugger information.
- `ZIMH_C_COMPILER`
  Default: unset.
  Optional project-level C compiler override. Set this before the first
  configure of a build tree.
- `ZIMH_C_STANDARD`
  Default: `17`.
  C language standard used by ZIMH-owned targets.
- `WITH_VIDEO`
  Default: `On`.
  Enable SDL-based graphics and display support.
- `WITH_NETWORK`
  Default: `On`.
  Enable simulator networking support.
- `WITH_ASYNC`
  Default: `On`.
  Enable asynchronous I/O support.
- `WITH_PCAP`
  Default: `On`.
  Enable pcap-backed Ethernet support. With this enabled, pcap
  development files must be installed and discoverable at configure
  time. On Windows, users also need a pcap-compatible runtime installed
  to use pcap networking.
- `WITH_SLIRP`
  Default: `On`.
  Enable SLiRP-backed user-mode networking when the dependency is
  available.
- `WITH_VDE`
  Default: `On` on non-Windows hosts.
  Enable VDE networking when the dependency is available.
- `WITH_TAP`
  Default: `On`.
  Enable TAP networking support where supported by the host.
- `WITH_ROMS`
  Default: `On`.
  Build and embed internal ROM support where applicable.
- `WITH_UNIT_TESTS`
  Default: `On`.
  Build host-side unit tests.
- `BUILD_SHARED_DEPS`
  Default: `Off`.
  Prefer shared dependency runtime linkage when supported by the
  dependency provider.
- `ENABLE_CPPCHECK`
  Default: `Off`.
  Enable `cppcheck` integration.
- `ENABLE_WINAPI_DEPRECATION_WARNINGS`
  Default: `Off`.
  Enable WinAPI deprecation warnings.
- `RELEASE_LTO`
  Default: `Off`.
  Enable release-mode link-time optimization.
- `DEBUG_WALL`
  Default: `Off`.
  Turn on stronger warning settings for debug builds.
- `DEBUG_WARNINGS`
  Default: `Off`.
  Add stricter debug-only warning flags; implies `-Wall` on GCC/Clang.
- `WARNINGS_FATAL`
  Default: `Off`.
  Treat warnings as errors.
- `ZIMH_PACKAGE_SUFFIX`
  Default: empty.
  Optional suffix appended to generated package artifact names.
- `MAC_UNIVERSAL`
  Default: `Off`.
  Build macOS universal binaries.

Examples:

```sh
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DWITH_VIDEO=Off \
  -DDEBUG_WALL=On -S . -B build/debug
```

## Generators

Ninja is the recommended general-purpose generator:

```sh
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -S . -B build/release
```

Visual Studio generators are also supported on Windows. Example:

```powershell
cmake -G "Visual Studio 18 2026" -A Win32 -S . -B build/release
cmake --build build/release --config Release
ctest --test-dir build/release --build-config Release \
  --parallel --output-on-failure
```

The Visual Studio 2026 generator requires CMake 4.2 or newer.

## Notes for Maintainers

If you are changing how simulators are declared or packaged, use these
documents rather than extending this file:

- `cmake/CMake-Maintainers.md`
- `cmake/CMake-Simulator-Maintainers.md`

In particular:

- simulator-local metadata lives in simulator-local `CMakeLists.txt`
- `cmake/simh-simulators.cmake` owns the top-level simulator inventory
- `cmake/simh-packaging.cmake` owns CPack component definitions and the
  Markdown documentation install rule
- `cmake/add_simulator.cmake` owns shared simulator build behavior

The CMake install rule copies the current Markdown documentation under
`docs/` to `CMAKE_INSTALL_DOCDIR`, excluding the legacy Word-document
archive under `docs/legacy-word`.

## Historical Note

Older versions of this repository used generated simulator CMake files
and generator-era helper workflows. Those are gone. Treat the CMake
files in this tree as maintained source files.
