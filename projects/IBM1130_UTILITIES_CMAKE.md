## IBM 1130 Utility CMake Migration

The IBM 1130 utility programs still have legacy local build files, but
they are not built by the maintained CMake build.

## Current State

- `simulators/Ibm1130/utils/*.c` contains standalone utility programs.
- `simulators/Ibm1130/utils/util_io.c` and `util_io.h` are shared by
  those utilities.
- `simulators/Ibm1130/utils/makefile` is a non-Windows local makefile.
- `simulators/Ibm1130/utils/*.mak` are old Microsoft VC2+/NMAKE build
  files.
- `simulators/Ibm1130/CMakeLists.txt` currently has `add_subdirectory`
  for `utils` commented out.
- `docs/simulators/Ibm1130/ibm1130.md` still documents the legacy
  `utils/*.mak` files.

## Problem

The utilities appear to be useful, but their only local build paths are
outside the maintained CMake build. Removing the old makefiles before
adding CMake targets would strand the tools; keeping them indefinitely
preserves obsolete build paths and Windows-only idioms such as
`$(OUTDIR)/nul`.

An attempted POSIX build with Clang and the legacy makefile showed that
the utilities are not currently C17-clean.  These are separate from the
boolean spelling cleanup and should be fixed as part of renovating the
utility build rather than folded into unrelated source rewrites.

Observed blockers include:

- `bindump.c` uses `memset` without including `<string.h>`.
- `disklist.c` uses `min` without a declaration visible to Clang.
- `disklist.c` assigns through a casted pointer expression, which is not
  valid C.
- `punches.c` uses `unlink` and `strnicmp` without portable declarations.

The same standalone build also reports existing format-string warnings in
`asm1130.c` and `bindump.c`.  Those warnings should be triaged when the
utilities are brought under the maintained warning/test workflow.

## Desired Outcome

- Build the IBM 1130 utilities through CMake.
- Remove `simulators/Ibm1130/utils/makefile`.
- Remove `simulators/Ibm1130/utils/*.mak`.
- Update IBM 1130 documentation so it no longer points users at the
  legacy makefiles.
- Keep the utility build behavior explicit enough that it can be tested
  on POSIX and Windows.

## Likely Work

1. Add `simulators/Ibm1130/utils/CMakeLists.txt`.
2. Add executable targets for:
   - `asm1130`
   - `bindump`
   - `checkdisk`
   - `disklist`
   - `diskview`
   - `mkboot`
   - `punches`
   - `viewdeck`
3. Link or compile `util_io.c` consistently for each target.
4. Decide whether these utilities should be installed with the IBM 1130
   simulator or just built as helper tools.
5. Enable the utilities from `simulators/Ibm1130/CMakeLists.txt`.
6. Build the utility targets on the local platform.
7. Fix the C17 compile blockers listed above.
8. Where practical, add focused smoke tests for utility invocation.
9. Remove the legacy makefiles once the CMake targets work.
10. Update `docs/simulators/Ibm1130/ibm1130.md`.

## Success Criteria

- A normal CMake build can build the IBM 1130 utilities.
- The legacy local makefiles are gone.
- The IBM 1130 documentation describes the supported build path.
- No documentation points users at removed makefiles.
