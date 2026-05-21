# Host-Side Unit Tests

SIMH now has a host-side unit-test harness under `tests/unit/`.
These tests are separate from the simulator regression suites driven by
`.ini` and `.do` files. They are intended for common host-side code such
as `src/runtime/`, extracted helpers from `src/core/`, and other code
that can be exercised directly without booting a simulated machine.

## Framework

The unit-test framework is `cmocka` 1.1.5 or newer.

The top-level CMake build enables host-side unit tests with the
`WITH_UNIT_TESTS` option. When `cmocka` is available, CMake creates the
`unittest` interface target, adds `tests/unit/`, and registers unit test
executables with `ctest`.

Mutation testing with `Mull` is intended future work. It is not part of
the first host-side unit-test rollout, but the long-term plan is to use
it once the unit-test harness and basic coverage workflow are
established.

## Layout

- `tests/unit/CMakeLists.txt`
  Registers harness-level tests and adds the mirrored unit-test
  subtrees.
- `tests/unit/src/`
  Host-side unit tests that mirror the `src/` tree.
- `tests/unit/simulators/`
  Host-side unit tests that will mirror the `simulators/` tree.
- `tests/unit/support/`
  Shared support code linked into every unit-test executable.
- `tests/unit/fixtures/`
  Golden files and other checked-in fixture data used by unit tests.
- `tests/unit/test_unit_smoke.c`
  Minimal smoke test proving the harness is working.
- `tests/unit/test_unit_support.c`
  Example test for the support helpers.

## Adding A Unit Test

1. Mirror the source tree under `tests/unit/`.
   Examples:
   - `src/core/scp_parse.c` ->
     `tests/unit/src/core/test_scp_parse.c`
   - `src/runtime/sim_card.c` ->
     `tests/unit/src/runtime/test_sim_card.c`
   - `simulators/PDP11/pdp11_cpu.c` ->
     `tests/unit/simulators/PDP11/test_pdp11_cpu.c`
2. Register the new test in the nearest `CMakeLists.txt` under that
   mirrored subtree.
3. Put reusable helpers in `tests/unit/support/` instead of duplicating
   them across tests.
4. Put checked-in fixture files in `tests/unit/fixtures/`.

Current examples:

```cmake
add_unit_test(unit-scp-parse
    LABEL unit
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_scp_parse.c
    INCLUDES
        ${PROJECT_SOURCE_DIR}/tests/unit/support
)
```

## Support Library

The shared support library is `simh_unit_support`. Its public interface
is declared in:

- `tests/unit/support/test_support.h`
- `tests/unit/support/test_simh_personality.h`
- `tests/unit/support/test_scp_fixture.h`

Current helpers:

- `simh_test_source_root`
  Return the repository source root used for the current test build.
- `simh_test_binary_root`
  Return the CMake build directory for the current test build.
- `simh_test_init_device_unit`
  Initialize a minimal one-unit `DEVICE`/`UNIT` pair for common-source
  tests.
- `simh_test_join_path`
  Build one filesystem path from a base directory and relative leaf.
- `simh_test_fixture_path`
  Build a full path to a file under `tests/unit/fixtures/`.
- `simh_test_make_temp_dir`
  Create a temporary directory under the current build tree.
- `simh_test_read_file`
  Read a file into memory.
- `simh_test_read_fixture`
  Read a fixture file into memory.
- `simh_test_write_file`
  Write a file from a memory buffer.
- `simh_test_files_equal`
  Compare two files byte-for-byte.
- `simh_test_remove_path`
  Remove a file or recursively remove a directory tree.
- `simh_test_reset_simulator_state`
  Reset the minimal shared simulator personality used by common-source
  tests.
- `simh_test_set_sim_name`
  Set the shared simulator name used by the test personality.
- `simh_test_set_devices`
  Install a null-terminated device list for tests that need shared SCP
  code to see a simulator device table.
- `simh_test_init_multiunit_device`
  Initialize a multi-unit device for SCP lookup and naming tests.
- `simh_test_install_devices`
  Reset the shared simulator state and install a device table in one
  step for SCP-facing tests.
- `simh_test_free_unit_names`
  Release cached unit names allocated during SCP naming tests.

These helpers are intentionally simple. Add to them as new unit tests
need stable infrastructure.

## Ethernet Test Backend

Ethernet unit tests can attach devices to the deterministic `test:`
backend implemented by `sim_ether_test.c`. A test opens it through the
normal attach path with a name such as `test:case-name`, then uses
`sim_ether_test.h` to inject received frames, pop transmitted frames,
count queued packets, or force transmit failures.

The backend is process-local and intended for synchronous unit-test
control code. If a test drives simulator execution from another thread,
it must serialize calls to the `sim_ether_test.h` helpers with that
execution.

## Building And Running

Configure a build with unit tests enabled:

```sh
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -S . -B build/release
```

Build specific unit-test executables:

```sh
ninja -C build/release zimh-unit-smoke zimh-unit-support
```

Run just the host-side unit tests:

```sh
ctest --test-dir build/release \
  -R 'zimh-unit-(smoke|support)' \
  --output-on-failure
```

List all tests currently known to `ctest`:

```sh
ctest --test-dir build/release -N
```

## Conventions

- Keep tests readable and explicit.
- Prefer checked-in fixtures for file-format behavior.
- Prefer support-library helpers over ad hoc temp-file code.
- Characterization tests are fine when cleaning up legacy code.
- It is acceptable for test code to be larger than the production code
  it protects.

The goal is not minimal test code. The goal is clear, dependable tests
for the common host-side codebase.
