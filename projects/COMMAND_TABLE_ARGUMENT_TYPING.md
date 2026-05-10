# Command Table Argument Typing

## Goal

Make command-table argument semantics explicit, local, and testable.

The SCP command-dispatch tables use shared callback shapes with a single
signed `arg` or `flag` slot. That slot currently carries several different
meanings:

- signed mode selectors
- boolean enable/disable flags
- unsigned bit masks
- sentinel values such as `-1`
- command-file nesting or control-flow state

The table mechanism is useful, but the generic argument type hides what a
specific handler expects. The goal is to keep table-driven dispatch while
moving handlers toward typed boundaries that document the local meaning of
each argument.

## Scope

This project covers the current SCP command-dispatch table families:

- `CTAB`
- `C1TAB`
- `SHTAB`

The affected code is not limited to `src/core/scp.c`. These table types are
also used by runtime command surfaces such as console and timer handling.

## Problem

The overloaded `arg` and `flag` fields make warning cleanup and review harder
than they should be:

- bit-mask handlers need local casts or temporary unsigned variables
- signed-selector handlers and bit-mask handlers look identical at the table
  boundary
- negative sentinels are documented only by local convention
- reviewers must inspect each handler by hand to know whether an argument is
  a selector, mask, boolean, or sentinel-bearing value

The same ambiguity can also become a correctness problem. A table entry can
pass a value that is type-compatible with the generic callback but
semantically wrong for the specific handler.

## Non-Goals

- Do not mechanically change every generic callback signature at once.
- Do not change user-facing command language as part of this cleanup unless
  a specific command-family change requires it and has tests.
- Do not create one table type per semantic kind unless repeated use proves
  that the extra type pays for itself.
- Do not treat every warning fix as permission to redesign the whole command
  dispatcher.

## Direction

Keep the existing table ABI where needed, but introduce typed dispatch
boundaries over time. Useful patterns include:

- small adapter callbacks that decode a generic table argument and call a
  typed internal helper
- private helpers that accept `uint32_t` for bit-mask logic
- private helpers that accept `bool` for enable/disable logic
- named enums for signed mode selectors
- explicit metadata or wrappers for sentinel-bearing command arguments

The right end state is not necessarily one new table type for each semantic
kind. It should, however, be possible to tell from the local type, helper, or
wrapper whether a handler expects a signed selector, boolean, unsigned mask,
or sentinel-bearing value.

## Review Guidance

Treat the current generic `arg` and `flag` fields as compatibility plumbing,
not as a pattern to extend.

Focused warning fixes are acceptable when they make an individual handler's
real semantics clearer. They should not scatter casts through a handler or
make a semantic mismatch harder to see.

Reviewers should ask:

- is this table argument signed data, a selector, a boolean, a mask, or a
  sentinel-bearing value?
- does the code make that local meaning obvious?
- would a typed helper or adapter make the behavior easier to test?
- does the change preserve the existing command-table ABI unless a larger
  refactor is deliberately in scope?
- are signed sentinel users left signed until they are deliberately replaced
  with clearer metadata?

## Suggested First Steps

1. Inventory `CTAB`, `C1TAB`, and `SHTAB` users by semantic use of `arg` or
   `flag`.
2. Identify low-risk handlers where the generic callback can become a tiny
   adapter over a typed helper.
3. Add or extend unit tests around each handler before changing its dispatch
   shape.
4. Migrate one command family at a time rather than changing the table
   architecture globally.
5. Keep signed sentinel users signed until they are deliberately replaced
   with clearer table metadata.

The `SET DEBUG` and `SET NODEBUG` path is a good example of the desired local
cleanup: keep the public command callback compatible with `C1TAB`, but make
the debug-mask logic internally unsigned and directly tested.

## Verification

For each migration slice:

- run the focused unit tests for the touched command family
- run `git diff --check`
- build the touched targets in the debug-warnings tree
- verify that any command-language behavior covered by the changed table is
  unchanged unless the slice intentionally changes it
