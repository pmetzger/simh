# Building the Project

This file explains how to build and test the project from a normal developer
checkout using CMake Presets.

The project uses CMake and requires out-of-tree builds managed through unified
configuration, build, and test presets. Do not try to configure directly in
the repository root.

The top-level `Makefile` remains as a thin compatibility wrapper over the
default POSIX build layout. The native interface is completely driven via
CMake Presets and CTest.

## Compiler and OS Support

We support builds for machines running recent vintage POSIX operating systems
(Linux, macOS, FreeBSD) and recent vintage Microsoft Windows (Windows 10 or
newer).

Unlike legacy SIMH forks, _we explicitly do not support_ building on obsolete
operating systems or compilers.

Our goal is to maintain a clean, supportable, and modern codebase that can be
preserved into the far future. We do not provide continuous integration for
unmaintainable architectures, nor do we accept patches that complicate the
codebase for legacy toolchains.

The codebase officially targets the C17 standard, though it is verified to
compile seamlessly under configurations ranging from C99 to C23. Compilers
lacking native C11 support are explicitly unsupported.

## Dependencies

### Core Requirements

At minimum, your host environment must have the following installed:

- A modern C compiler toolchain (C11/C17 capable)
- `cmake` (v3.21 or newer required for full preset compatibility)
- `ninja` or system `make`
- `git` and `bison`
- `libpcre2` (Backend for SCP EXPECT routing commands)
- `pkg-config` / `pkgconf` (Required on POSIX hosts)
- `libuuid` / system UUID generation API

### Optional Feature Packages

To compile with full graphics, console editing, and advanced networking
features, ensure the following are available:

- `SDL2` & `SDL2_ttf` — Windowing, raw input, and high-quality text rendering.
- `freetype` — Embedded font glyph rasterization.
- `libpng` & `zlib` — Image loading and compressed file-handling paths.
- `libedit` — Interactive command-line history and editing (POSIX only).
- `libpcap` — Physical/bridged host network interface packet capture.
- `libslirp` — Local NAT networking capabilities (POSIX >= 4.7.0; Windows >= 4.9.0).
- `libvdeplug` — Virtual Distributed Ethernet backend (POSIX only).

### Test Environment

- `cmocka` (1.1.5 or newer) — Host-side C unit-testing framework.
- Python 3 — Driver engine for system integration tests.

### Installing Dependencies via Script (POSIX and Unixen)

The helper utility scripts below are available to automate dependency
setup across standard environments:

```sh
tools/ci/deps/deps.sh linux        # Debian / Ubuntu apt pipeline
tools/ci/deps/deps.sh osx          # macOS Homebrew pipeline
tools/ci/deps/deps.sh macports     # macOS MacPorts pipeline
```

N.B.: The Github and Gitlab CI/CD pipelines use these utility scripts to
install dependencies, so if they work on CI/CD pipelines, they should work for
you!

### Installing Dependencies on Windows

Windows dependencies split into two categories: external utilities and library
dependencies.

- External utilities, such as `cmake`, `git`, `bison` and `python`/`python3`
  should be installed by an appropriate package manager, such as
  [scoop.sh](https://scoop.sh/).

- [`vcpkg`](https://vcpkg.io/) manages library dependencies, and locally
  installs `SDL2`, `SDL2_ttf`, ..., into the build tree during the `cmake`
  build process.

  __BE SURE TO SET `VCPKG_ROOT` in your environment so that `cmake` understands
  the `vcpkg` toolchain file's location.__

---

## The Preset Workflow

The project defines uniform environments via `CMakePresets.json`. This
eliminates the need to remember explicit compiler choices, binary directories,
or toolchain paths.

### 1. Symmetric Environments (Linux & macOS Ninja/Make)

For standard UNIX targets, the configuration, build, and test preset names
match exactly.

```sh
# Step 1: Configure the tree
cmake --preset ninja-release

# Step 2: Build the targets
cmake --build --preset ninja-release
```

*Generated binaries will land inside `build/release/bin/`. For debug variants,
 substitute `ninja-debug` (which outputs to `build/debug/bin/`).*

### 2. Multi-Config Frameworks (macOS Xcode & Windows MSVC)

Multi-configuration generators use a base configuration layout that handles
building distinct targets cleanly.

#### macOS Xcode:
```sh
# Configure the Xcode project spaces
cmake --preset xcode

# Build a specific configuration layout
cmake --build --preset xcode --config Release
```
*Generated binaries land inside `build/bin/Release/`.*

#### Windows (Visual Studio 2022 / 2026 with vcpkg):

Windows presets explicitly map static compilation configurations via `vcpkg`
toolchains. Because configurations are bound directly to the build presets,
choose the suffix matching your target:

```pwsh
# Configure using the Visual Studio 2022 preset
cmake --preset windows-vs2022

# Compile either the Debug or Release target variants
cmake --build --preset windows-vs2022-release
cmake --build --preset windows-vs2022-debug
```

*Generated binaries will map to `build-vs2022/bin/Release/` or
 `build-vs2022/bin/Debug/` respectively.*

---

## Overriding Preset Variables & Feature Selection

Presets contain default configurations, but they can be fully customized or
trimmed at configuration time by passing standard `-D` compilation arguments.

### Compiling Without Video/Graphics Support

If your target machine lacks `SDL2_ttf` or a full graphical environment, you
can safely strip video capabilities while utilizing your preset:

```sh
cmake --preset ninja-release -DWITH_VIDEO=Off
cmake --build --preset ninja-release
```

### Common Configuration Toggles

- `-DWITH_VIDEO=Off` — Completely disable SDL structural subsystems (Default:
  `On`).
- `-DWITH_NETWORK=Off` — Strip out all external network code stacks (Default:
  `On`).
- `-DWITH_PCAP=Off` — Disable bridge/pcap network capture layers (Default:
  `On`).
- `-DWARNINGS_FATAL=On` — Escalate all compiler warnings into hard build
  errors (Default: `Off`).
- `-DRELEASE_LTO=On` — Force Link-Time Optimization routines on Release
  compilation (Default: `Off`).
- `-DC_DIALIECT={11|17|23|26}` - Set the C compiler's dialect to C11,
  C17, C23 or C26.

---

## Selective Compilation Targets

To avoid compiling the entire matrix of historical simulators, you can isolate
compilation to a specific architecture by targeting it explicitly through the
preset engine:

```sh
cmake --build --preset ninja-release --target pdp11
cmake --build --preset ninja-release --target vax
```

### Key Top-Level Targets

- **Standard Simulator Engine Set:** `cmake --build --preset ninja-release`
- **Experimental Simulators:** `--target experimental-simulators`
- **Host Unit Testing Binary Array:** `--target unit-tests`
- **System Integration Testing Framework:** `--target integration-tests`

*Simulator targets are unprefixed during execution, but compiled output
 filenames are standardized with the `zimh-` prefix (e.g., `zimh-pdp11`).*

---

## Running the Test Suites

The project includes pre-configured **Test Presets** that map directly to your
build layouts. These automated presets encapsulate standard execution
optimization flags, automatically running with the following defaults:

- 8 parallel execution threads (`-j 8`)
- Extended output diagnostics on failure (`--output-on-failure`)
- A 5-minute guard timeout per test item (`--timeout 300`)

Once compilation under a preset concludes successfully, invoke `ctest
--preset` passing the corresponding target environment:

```sh
# For symmetric layout environments (Linux, macOS Ninja)
ctest --preset ninja-release
ctest --preset ninja-debug

# For multi-config or asymmetric environments (Windows MSVC)
ctest --preset windows-vs2022-release
ctest --preset windows-vs2022-debug
```

### Fallback Manual Invocation

If you need to bypass a preset configuration to pass custom `ctest` filtering
flags (like running a specific subset of tests via `-R`), direct `ctest` to
the preset's binary directory manually:

```sh
ctest --test-dir build/release -R pdp11 -j 4
```

Alternatively, you can route testing passes straight through the build
architecture pipelines:

```sh
cmake --build --preset ninja-release --target unit-tests
```

### Known Environmental Testing Constraints

When operating inside strictly confined containers or restricted sandbox
execution spaces, you may encounter a failure on the following target:

- `zimh-uc15`

This specific simulator relies heavily on internal POSIX shared memory
interfaces. If your environment or execution platform blocks `shm_open`
routines via a sandbox policy, this test will fail. This indicates an
environmental restriction rather than a structural code defect; execution will
succeed normally on native hardware.

## Safe Tree Reconfigurations

CMake aggressively caches values inside build spaces. If you modify
fundamental feature flags (such as toggling `WITH_VIDEO`) or make significant
modifications to your environment, the most reliable approach is to clean the
preset's defined folder and restart:

```sh
# Wipe and reconfigure a standard layout folder
rm -rf build/release
cmake --preset ninja-release
```
