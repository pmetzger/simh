## IBM 1130 Utility CMake Migration

The IBM 1130 utility programs now have a maintained CMake build-check
target. They are still not installed with the normal simulator binaries.

## Current State

- `simulators/Ibm1130/utils/*.c` contains standalone utility programs.
- `simulators/Ibm1130/utils/util_io.c` and `util_io.h` are shared by
  those utilities.
- `simulators/Ibm1130/utils/CMakeLists.txt` defines non-default,
  non-installed utility executable targets.
- `ibm1130-utils` builds all eight utilities as an explicit check target:
  `asm1130`, `bindump`, `checkdisk`, `disklist`, `diskview`, `mkboot`,
  `punches`, and `viewdeck`.
- The old local POSIX makefile and Microsoft VC2/NMAKE `.mak` files have
  been removed.

## Remaining Work

The utilities now build with the CMake release configuration used during
the migration, but they are not covered by behavioral tests. They also
still produce substantial warning noise under the debug-warnings build,
mostly from legacy style issues such as missing prototypes, unused
parameters, sign comparisons, and nonliteral format strings.

Future work should:

1. Add useful behavioral tests around utilities when stable fixtures are
   available.
2. Triage and reduce debug-warnings noise without broad formatting churn.
3. Decide whether any utilities should eventually be installed or packaged
   with the IBM 1130 simulator.
