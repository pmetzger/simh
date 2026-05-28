# CMake Maintainer Guide

This document explains the CMake build layout for the project.
It is aimed at maintainers who need to change build logic, dependency
handling, simulator packaging, or top-level workflow behavior.

For the narrower day-to-day task of adding a simulator source file or
editing a simulator-local `CMakeLists.txt`, see:

- `cmake/CMake-Simulator-Maintainers.md`

## Scope

The project treats CMake as the source of truth for build metadata.

In practical terms:

- simulator-local build metadata lives in simulator-local `CMakeLists.txt`
  files
- the top-level simulator directory inventory lives in
  `cmake/simh-simulators.cmake`
- packaging component declarations live in `cmake/simh-packaging.cmake`
- shared target construction logic lives in `cmake/add_simulator.cmake`
- the top-level `Makefile` is only a thin compatibility wrapper over
  `build/release`

## Core Layout

The files that matter most are:

- `CMakeLists.txt`
  The top-level configure/generate driver.

- `cmake/add_simulator.cmake`
  Shared simulator target construction logic.

- `cmake/simh-simulators.cmake`
  The maintained list of simulator subdirectories and shared simulator
  path/source variables.

- `cmake/simh-packaging.cmake`
  The maintained CPack component definitions and Markdown documentation
  install rule.

- `cmake/platform-quirks.cmake`
  Compiler and platform-specific warning, optimization, and linker
  tweaks.

- `cmake/os-features.cmake`
  Configuration-time capability probes.

- `cmake/dep-locate.cmake`
  Dependency discovery.

- `cmake/dep-link.cmake`
  Interface library construction for discovered dependencies.

- `cmake/cpack-setup.cmake`
  CPack configuration.

- `tests/unit/CMakeLists.txt`
  Host-side unit test integration.

## Top-Level Flow

The top-level `CMakeLists.txt` does the following:

1. sets the project and default configuration behavior
2. defines project options
3. configures platform quirks and feature probes
4. locates optional dependencies and assembles interface libraries
5. includes `cmake/simh-simulators.cmake`
6. enables host-side unit tests when requested and available
7. configures packaging through `cpack-setup.cmake` and
   `simh-packaging.cmake`

That means most build-maintainer work falls into one of three buckets:

- top-level policy and options
- shared helper behavior
- simulator inventory and packaging metadata

## Important Conventions

### Build Directories

The project assumes out-of-tree builds. The preferred layout is:

- `build/release`
- `build/debug`

Runtime artifacts belong under the active build tree, normally in:

- `build/release/bin`
- `build/debug/bin`

The compatibility `Makefile` auto-configures and uses `build/release`.

### Standard CMake Variables

Keep standard CMake variables as CMake defines them.

Examples:

- `CMAKE_BUILD_TYPE`
- `CMAKE_SOURCE_DIR`
- `CMAKE_BINARY_DIR`
- `CMAKE_INSTALL_PREFIX`

Do not invent project-local aliases for these.

### Project Option Naming

Project-defined option names should be kept coherent.

Use this naming policy:

- use `WITH_*` for product/runtime features
- use `ENABLE_*` for tooling or analysis features
- avoid negative names like `NO_*` and `DONT_*` in new work

Bring project-defined options toward that scheme when touching them.

## Shared Simulator Build Logic

`cmake/add_simulator.cmake` is the center of the simulator build
infrastructure.

It is responsible for:

- building the shared SIMH core libraries
- selecting the correct core variant for each simulator target
- wiring common include paths, compile definitions, and interface
  libraries
- creating simulator regression tests through `add_test`
- integrating ROM generation where needed
- handling simulator install metadata

If a behavior is common across unrelated simulator families, it probably
belongs here rather than in a simulator-local `CMakeLists.txt`.

## Simulator Inventory

`cmake/simh-simulators.cmake` owns two kinds of data:

- shared path/source variables used by simulator-local metadata
- the top-level `add_subdirectory(...)` list for simulator directories

It may also define a small number of top-level simulator group targets
when those are user-facing workflow concepts.

Example:

- `experimental-simulators`

That target builds the experimental simulator set:

- `alpha`
- `pdq3`
- `sage`

## Simulator-Local Files

Each simulator directory should have a small maintained local
`CMakeLists.txt`.

That file may contain:

- one or more `add_simulator(...)` calls
- local `set(...)` bundles for shared source lists within that simulator
  family
- local target-specific special cases that only apply to that family

That file should not become a private framework of its own.

For simulator-local maintenance guidance, use:

- `cmake/CMake-Simulator-Maintainers.md`

## Dependency Handling

Dependency logic is split into three parts:

- `cmake/os-features.cmake`
  Capability checks and configuration-time probes.

- `cmake/dep-locate.cmake`
  Package discovery and dependency presence checks.

- `cmake/dep-link.cmake`
  Construction of the interface libraries consumed by simulator targets.

The important design rule is:

- dependencies come from the host or the configured package manager; the
  CMake build does not fetch and build third-party libraries as a
  configure/build fallback
- prefer feature detection when the real question is capability
- only use platform-family conditionals when the behavior is truly tied
  to a platform family

If you are deciding whether something belongs in
`platform-quirks.cmake` or `os-features.cmake`, the rough rule is:

- compiler flags and platform-specific build behavior:
  `platform-quirks.cmake`
- configuration-time capability or semantics checks:
  `os-features.cmake`

## Packaging

Packaging metadata is maintained directly in:

- `cmake/simh-packaging.cmake`

This file defines:

- the runtime support component
- simulator family components
- the CPack components named by simulator target `PKG_FAMILY` fields
- the Markdown documentation install rule

Examples:

- `default_family`
- `pdp11_family`
- `vax_family`
- `experimental_family`

When a simulator target's `PKG_FAMILY` changes, the corresponding family
must exist in `simh-packaging.cmake`.

The general CPack configuration lives in:

- `cmake/cpack-setup.cmake`

That file handles:

- package naming
- generator-specific settings
- generated package dependency metadata
- installer customization wiring

## Tests

There are two distinct test layers:

- host-side unit tests under `tests/unit/`
- simulator regression tests registered through `add_simulator(...)`

Common maintainer workflows:

- build the default simulator set:
  - `cmake --build build/release`

- build the experimental set:
  - `cmake --build build/release --target experimental-simulators`

- run all configured tests:
  - `ctest --test-dir build/release --parallel --output-on-failure`

- run a simulator regression slice:
  - `ctest --test-dir build/release --parallel -R zimh-pdp11 --output-on-failure`

- run host-side unit tests:
  - `ctest --test-dir build/release --parallel -R 'zimh-unit-' --output-on-failure`

## When To Edit Which File

Edit `CMakeLists.txt` when you are changing:

- top-level options
- top-level policy
- configure/generate flow
- unit-test enablement

Edit `cmake/add_simulator.cmake` when you are changing:

- shared simulator target behavior
- common test registration behavior
- common ROM wiring
- common install logic for simulator targets

Edit `cmake/simh-simulators.cmake` when you are changing:

- shared simulator path variables
- the set of simulator directories in the build
- top-level simulator group targets

Edit `cmake/simh-packaging.cmake` when you are changing:

- package-family definitions
- simulator family names
- runtime support packaging component declarations

Edit a simulator-local `CMakeLists.txt` when you are changing:

- that simulator family's source files
- local compile definitions
- local include directories
- local family-specific special cases

## What Not To Do

- do not treat simulator-local `CMakeLists.txt` files as generated output
- do not add new metadata authority back into the top-level `Makefile`
- do not duplicate generic target-construction logic in simulator-local
  files
- do not add new build behavior to unused generator code

## Ongoing Cleanup Themes

Some cleanup themes still matter for maintainers:

- continue normalizing CMake option naming
- prefer explicit CMake and CTest workflow targets
- prefer feature checks over OS-name checks when that better expresses
  the real dependency

When in doubt, prefer the solution that makes the tree look more like a
normal hand-maintained CMake project:

- local metadata in local `CMakeLists.txt`
- shared behavior in shared helper files
- packaging metadata in packaging files
- no regeneration step
