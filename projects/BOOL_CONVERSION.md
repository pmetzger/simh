# Boolean Conversion Project

## Status

The active C source conversion is complete.

Commit `268ae2f9` converted the legacy SIMH boolean spellings to C standard
boolean spellings:

- `t_bool` became `bool`;
- `TRUE` became `true`;
- `FALSE` became `false`;
- files using `bool`, `true`, or `false` now include `<stdbool.h>` directly;
- the compatibility definitions were removed from `src/core/sim_defs.h`.

The conversion covered core, runtime, component, simulator, and unit-test
sources.  It was intentionally kept separate from project/tooling scratch
files and later squashed into one source commit.

## Rewrite Tools

Two helper scripts from the migration remain useful as retained tooling:

- `tools/dev/bool-convert.py` performs the mechanical source rewrite from
  SIMH boolean spellings to C standard spellings.  It rewrites identifier
  tokens in code, can rewrite comments after a chosen header/history boundary,
  deliberately leaves strings and character literals unchanged, reports
  comment and string hits for review, and inserts `<stdbool.h>` when it can do
  so safely.
- `tools/dev/bool-insert-include.py` is a small dry-run-first helper for
  inserting an include or other line at an explicit line number.  It was used
  when the main rewrite tool correctly refused to guess include placement in
  unusual include blocks.

The other boolean audit scripts were temporary scanners for the old tree:
they depended on `t_bool` declarations or transitional `TRUE`/`FALSE`
definitions and produced reports used during this conversion.  They are useful
as historical scratch tools, but they should not be treated as maintained
repository tooling unless a new audit need appears.

## Verification

After removing the compatibility aliases from `src/core/sim_defs.h`, the tree
was verified with:

```sh
cmake --build build/release
ctest --test-dir build/release --parallel
```

The full release build passed, and the full unit and integration suite passed:
`190/190` tests.

Compilation is now the primary active-code residue check.  Any remaining use
of `t_bool`, `TRUE`, or `FALSE` in compiled C code outside platform headers
should fail unless it is supplied by a platform API such as Windows `BOOL`.

## What Was Audited

Before the mechanical rewrite, the project audited the places most likely to
break when `t_bool` stopped being an integer-width alias:

- storage that was declared boolean but held status codes, byte values,
  register fragments, or masks;
- functions declared as boolean while returning status-like values;
- mask expressions passed to boolean parameters or stored in boolean fields;
- declarations and test doubles whose signatures needed to match after the
  conversion;
- local or vendored `TRUE`/`FALSE` definitions that were not SIMH booleans.

Those audits produced source fixes before the broad spelling conversion.  The
large sweep reports and trackers were useful working notes, but they are no
longer active project state.

## Remaining Caveats

Some textual `TRUE`, `FALSE`, or `t_bool` occurrences may remain outside active
C code:

- historical comments and documentation;
- diagnostic strings that intentionally print those words;
- test text that verifies third-party headers do not leak `TRUE` or `FALSE`;
- Windows API call sites where `TRUE` and `FALSE` are Windows `BOOL` values.

Those should not be treated as regressions unless they become active C
dependencies on the removed SIMH compatibility symbols.

## Follow-Up

- Run a Windows build after the source conversion lands.  Several remaining
  active `TRUE`/`FALSE` tokens are intentionally Windows API values, and the
  Windows compiler is the right validation boundary for those paths.
- Add an automated guard that fails if new active C code introduces `t_bool`,
  `TRUE`, or `FALSE` outside an explicit allowlist.
- Decide separately whether developer documentation should be modernized from
  `t_bool`/`TRUE`/`FALSE` examples to `bool`/`true`/`false`.
