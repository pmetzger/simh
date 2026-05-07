# Standard Integer Type Migration

The simulator tree still carries historical integer typedefs such as
`int8`, `uint8`, `int16`, `uint16`, `int32`, `uint32`, `t_int64`, and
`t_uint64`.  These were useful when C compilers and Windows environments
did not consistently provide C99 integer types, but this project now
assumes C17 on recent POSIX and Windows hosts.

We should migrate internal implementation code toward the standard
`<stdint.h>` names: `int8_t`, `uint8_t`, `int16_t`, `uint16_t`,
`int32_t`, `uint32_t`, `int64_t`, and `uint64_t`.  The migration should
be staged and test-covered rather than mechanical across the whole tree
in one pass.  Public simulator APIs, save-file formats, and code that
models simulated machine words need special care so that type spelling
changes do not hide representation changes.

`t_value`, `t_addr`, and other SIMH domain types should be audited
separately.  Some are not merely historical aliases; they describe
simulator API contracts or configurable host/simulator widths.

We should also consider adding a project-wide `uchar_t` typedef for
character-processing use.  This would represent the unsigned character
domain required by `<ctype.h>` and byte-oriented text parsing, distinct
from `uint8_t`, which should remain the spelling for simulated machine
bytes and raw numeric byte data.  If added, `uchar_t` should be
documented as a character type, not a machine byte type, and introduced
with a small audit of parsers that currently cast plain `char` before
calling `ctype.h`.
