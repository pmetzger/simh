# CMake Simulator Maintainers

This document explains how to maintain simulator build metadata in the CMake
build.

The short version is:

- simulator build metadata is maintained directly in CMake
- do not treat simulator-local `CMakeLists.txt` files as generated artifacts

## Core Rule

When changing how a simulator is built, update the simulator's local
`CMakeLists.txt` and the top-level simulator inventory in
`cmake/simh-simulators.cmake` as needed.

## Where Build Metadata Lives

### Top-Level Inventory

`cmake/simh-simulators.cmake` owns:

- the top-level `add_subdirectory(...)` list for simulator directories
- shared simulator/source-root variables such as `PDP11D`, `VAXD`, `DISPLAYNG`
- shared display source variables used by simulator-local metadata

If you add a brand-new simulator directory, you will usually need to:

1. add any required shared path variables here
2. add the directory with `add_subdirectory(...)`

### Simulator-Local Metadata

Each simulator directory should have a small local `CMakeLists.txt`.

That file owns:

- the local simulator targets in that directory
- the local source file lists
- simulator-specific include directories
- simulator-specific compile definitions
- local helper `set(...)` bundles when they reduce repetition
- local target-specific special cases, if they truly belong only to that
  simulator family

Examples:

- `simulators/SSEM/CMakeLists.txt`
- `simulators/PDP11/CMakeLists.txt`
- `simulators/VAX/CMakeLists.txt`

### Shared Build Logic

`cmake/add_simulator.cmake` owns the common target-construction logic.

Keep shared logic there. Do not duplicate it in simulator-local files.

## Typical Local File Shape

Most simulator `CMakeLists.txt` files should be small and declarative.

Typical shape:

```cmake
## Simulator family name
##
## Update this file as the simulator's sources and local build metadata
## change. Keep shared target-construction logic in the common CMake helpers.
##
## Not yet: if this simulator later gets a local unit-test subtree, declare
## it explicitly here with add_subdirectory(unit-tests).

add_simulator(example
    SOURCES
        example_cpu.c
        example_sys.c
    INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}
    DEFINES
        VM_EXAMPLE
    LABEL EXAMPLE
    PKG_FAMILY default_family
    TEST example)
```

Some directories need more than this. That is fine when the extra logic is
truly local to the simulator family.

Example:
- `simulators/VAX/CMakeLists.txt`
  contains local shared source bundles and a `zimh-microvax3900`
  executable alias rule that are specific to the VAX family.

## Adding a Source File to an Existing Simulator

If a simulator gets a new source file:

1. open the simulator directory's `CMakeLists.txt`
2. find the relevant `add_simulator(...)` target
3. add the new source file under `SOURCES`
4. if multiple targets in the same directory share the file, consider putting
   it into a local `set(...)` bundle instead of repeating it
5. rebuild that simulator target
6. run the simulator's regression test path

Example:

- for `pdp11`, edit `simulators/PDP11/CMakeLists.txt`
- for `vax`, edit `simulators/VAX/CMakeLists.txt`

## Adding a New Simulator Target to an Existing Directory

If a directory already exists and you want to add another simulator target in
that family:

1. open the local `CMakeLists.txt`
2. decide whether the new target can reuse existing local `set(...)` bundles
3. add a new `add_simulator(...)` block
4. set:
   - `SOURCES`
   - `INCLUDES`
   - `DEFINES`
   - `FEATURE_*` flags as needed
   - `LABEL`
   - `PKG_FAMILY`
   - `TEST`
5. rebuild that target
6. run its regression test

## Adding a Brand-New Simulator Directory

For a new simulator directory:

1. create the simulator directory under `simulators/`
2. add a local `CMakeLists.txt`
3. use one or more `add_simulator(...)` blocks in that file
4. if the file needs shared path variables, add them in
   `cmake/simh-simulators.cmake`
5. add the new directory with `add_subdirectory(...)` in
   `cmake/simh-simulators.cmake`
6. if packaging needs a new family, update the packaging metadata as well
7. configure, build, and test through CMake

## Meaning of Common add_simulator Fields

These are the fields most maintainers will touch.

### `SOURCES`

The source files compiled into the simulator target.

### `INCLUDES`

Additional include directories for that target.

Usually this includes:

- `${CMAKE_CURRENT_SOURCE_DIR}`

and sometimes other simulator-family directories when code is shared.

### `DEFINES`

Target-specific compile definitions passed as preprocessor symbols.

Examples:

- `VM_PDP11`
- `UC15`
- `VM_VAX`
- `VAX_630`

### `LABEL`

CTest grouping metadata for the simulator test.

This is used to label regression tests in a family, such as:

- `PDP11`
- `VAX`
- `NOVA`

### `PKG_FAMILY`

Packaging/install component-family metadata.

Examples:

- `pdp11_family`
- `vax_family`
- `dgnova_family`

### `TEST`

The regression test name associated with the simulator.

Examples:

- `pdp11`
- `uc15`
- `vax-diag`

### `FEATURE_VIDEO`

This simulator uses SIMH video support.

### `FEATURE_DISPLAY`

This simulator uses the display subsystem.

### `FEATURE_INT64`

Use the SIMH `int64`-enabled core library variant.

### `FEATURE_FULL64`

Use the full 64-bit core/address variant.

### `BUILDROMS`

This simulator depends on the built-in ROM-generation support.

### `USES_AIO`

This simulator uses the async-I/O-capable core build.

## When to Use Local `set(...)` Bundles

Local `set(...)` source bundles are appropriate when:

- multiple simulator targets in the same directory share a large common source
  set
- the bundle is local and simulator-family-specific

They are not appropriate when:

- they only save one or two repeated lines
- they are generic helper abstractions that belong in shared CMake logic

Good example:

- `simulators/VAX/CMakeLists.txt`

## When Extra Local CMake Logic Is Acceptable

Extra local logic is acceptable if it is genuinely local to that simulator
family.

Examples:

- target-specific alias creation
- a simulator-family-specific post-build step
- local source bundles reused by many targets in that directory

Do not move generic logic into local simulator files. If multiple unrelated
directories need the same rule, it probably belongs in shared CMake helper
code instead.

## Local Test Subdirectories

If a simulator family later gets a local `unit-tests/` subtree, declare it
explicitly in the local `CMakeLists.txt`.

Do not resurrect old generator-era conditions like `HAVE_UNITY_FRAMEWORK`.

If local simulator tests are added later, use an explicit supported pattern
such as:

```cmake
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/unit-tests/CMakeLists.txt")
  add_subdirectory(unit-tests)
endif ()
```

Only add that when the subtree actually exists.

## How to Verify Changes

After editing simulator-local CMake metadata:

1. check for whitespace issues:

```sh
git diff --check -- simulators/<dir>/CMakeLists.txt
```

2. rebuild the target:

```sh
cmake --build build/release --target <simulator>
```

3. run the associated regression path:

```sh
ctest --test-dir build/release --parallel -R zimh-<name> \
  --output-on-failure
```

If you are using another existing build directory for the work, use that
directory consistently for the verification pass.

## Do Not Do These Things

- do not edit the top-level `Makefile` to change simulator source lists
- do not treat simulator `CMakeLists.txt` files as generated output
- do not add dead framework hooks such as `HAVE_UNITY_FRAMEWORK`
- do not move generic build logic into individual simulator directories

## Historical Context

This repository moved from Makefile-owned simulator metadata to
maintained CMake-owned simulator metadata.

That means:

- simulator-local `CMakeLists.txt` files are maintained source files
- the generator-era workflow should not be revived
- the top-level `Makefile` should remain a compatibility wrapper rather
  than becoming a second metadata authority
