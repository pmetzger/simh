# I650 Test Artifact Isolation

The I650 integration test currently leaves generated files in
`simulators/I650/sw` after the test run. CTest provides
`SIMH_TEST_OUTPUT_DIR` under the build tree, but `i650_test.ini` changes
directory into the source fixture tree:

```text
cd %~p0
cd ../sw
```

The nested scripts then create relative output files such as
`console.txt`, `debug.txt`, `print.txt`, `deck_in.dck`, and
`deck_out.dck` in the source tree.

## Alternative 1: stage the I650 fixture tree

Copy the I650 `tests` and `sw` directories into a build-tree test root
and point CTest at the staged `i650_test.ini`.

Pros:

- Keeps the existing I650 scripts mostly unchanged.
- Preserves their current relative-path assumptions.
- Moves all generated artifacts under `build/`.
- Provides a reusable pattern for other legacy integration tests.

Cons:

- Copies fixture data into the build tree at configure time.
- CMake needs a small new `add_simulator()` option for staged tests.

This is the recommended first fix.

## Alternative 2: teach scripts to use `SIMH_TEST_OUTPUT_DIR`

Keep running the source-tree script, but update the I650 `.ini` files so
read-only fixtures come from the source tree and generated files go to
`SIMH_TEST_OUTPUT_DIR`.

Pros:

- Avoids copying the fixture tree.
- Makes output intent explicit in the scripts.

Cons:

- More invasive: many nested scripts assume relative paths.
- Easy to miss one generated filename.
- Ties old simulator command scripts to CTest/CMake environment details.

## Alternative 3: add cleanup only

Extend the I650 cleanup block to delete all generated files.

Pros:

- Smallest code change.

Cons:

- Still writes into the source tree during the test.
- Fragile on Windows because open files may not delete.
- Does not prevent stale files after interrupted or failed tests.

This treats the symptom, not the bug.

## Alternative 4: make CTest set the working directory

Set the CTest `WORKING_DIRECTORY` to a build-tree output directory.

Pros:

- Simple in CMake.

Cons:

- Does not help this test as written, because `i650_test.ini` immediately
  changes directory relative to its own script location.
- Would still require script changes or staging.

## Recommendation

Add a small `add_simulator()` staging option and use it for I650:

```cmake
add_simulator(i650
    ...
    TEST i650
    TEST_STAGE_DIRS
        tests
        sw)
```

The helper should copy those directories to a build-tree test root and
use the staged `tests/i650_test.ini` for the CTest command. The source
tree remains read-only during the integration test, and the existing I650
script ecosystem can continue using its current relative paths.
