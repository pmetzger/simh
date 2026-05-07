# VAX Header And Interface Cleanup

Many VAX source files declare cross-file functions and globals with
local `extern` blocks instead of including headers that own those
interfaces.  This makes it easy for prototypes to drift, hides subsystem
boundaries, and leaves warning cleanup to be handled piecemeal in each
file.

The KA4xx standard-device cleanup exposed the problem, but it is not
limited to that model family.  Examples include system-device files that
locally declare device entry points for ROM, NVR, DZ, RD, XS, video, NAR,
clock, and related register handlers.  Similar patterns exist elsewhere
in the VAX tree.

## Goals

- Audit VAX `.c` files for local `extern` declarations of functions and
  globals.
- Decide which declarations belong in existing model `*_defs.h` files,
  which deserve focused subsystem headers, and which should become
  `static` because they are not true cross-file interfaces.
- Keep `_internal.h` headers for implementation-private seams and unit
  tests, not for normal cross-file simulator APIs.
- Replace repeated local prototype blocks with includes of the owning
  header.
- Build affected VAX targets with `-Wmissing-prototypes` and related
  warnings enabled after each cleanup slice.

## Notes

Do this incrementally.  The VAX tree has old conventions, and a broad
mechanical header rewrite would be risky.  Each slice should first map
the real consumers and definitions, then move only coherent interface
groups to the right header.
