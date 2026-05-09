# Numeric quantity parsing cleanup

## Problem

ZIMH has several command-line places where users type a number that denotes a
quantity: disk sizes, memory sizes, transfer sizes, counts, time values, and
similar resource limits.  Those parsers have grown up locally around their
callers, so the exact grammar and suffix semantics can differ by subsystem.

The RAMDISK work exposed the problem through `RAMDISK:SIZE=`, but the issue is
not really disk-specific.  A user who writes `456M` in a storage or memory
context will generally expect a clear rule for what `M` means, and developers
should not have to reimplement overflow checking, suffix handling, and
diagnostics at every call site.

The existing disk capacity setter, `sim_disk_set_capac`, is one example of the
legacy shape.  It parses with the generic `get_uint` helper and then applies
disk-capacity scaling afterward.  That behavior must be preserved unless we
deliberately migrate it, but it is not a good template for new quantity
grammars.

`RAMDISK:SIZE=` therefore uses its own small parser for now.  That is an
acceptable temporary choice, but we should not keep adding local parsers as
new CLI quantities appear.

## Desired Follow-Up

Create a shared numeric quantity parser with explicit, caller-selected
semantics:

- callers choose the base unit: bytes, sectors, words, pages, milliseconds,
  plain counts, or another documented unit;
- callers choose accepted suffix families, such as binary storage suffixes,
  decimal SI suffixes, time suffixes, or no suffixes;
- `K`, `M`, and `G` in binary storage and memory contexts mean 1024,
  1024*1024, and 1024*1024*1024;
- overflow is detected before multiplication or unit conversion;
- zero, maximum values, alignment, and sector-size requirements are enforced
  by the caller so that diagnostics can name the real command constraint;
- legacy callers can preserve their historical grammar while new callers use
  clearer explicit semantics.

After that helper exists, migrate `RAMDISK:SIZE=` to it and use it for future
CLI quantities.  Then audit existing parsers, including disk capacity and
memory-size commands, and decide case by case whether they should retain
their historical grammar or gain clearer replacement forms with compatibility
notes.
