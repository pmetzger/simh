# VAX Machine Word Types

The VAX simulator code still commonly uses `int32` for 32-bit machine
words, including values that are best understood as uninterpreted bit
patterns.  The `sim_rom_read_with_delay()` cleanup exposed this at the
interface between VAX ROM read callbacks and shared runtime code: the
delay helper is passed a ROM data word, not an address or signed host
integer.

The long-term goal should be to audit VAX value representation and move
bit-pattern data paths toward unsigned types such as `uint32` or a VAX
word typedef.  Signed types should be reserved for places where the VAX
semantics actually require signed interpretation.

The audit should distinguish at least these categories:

- Machine addresses, which should use address-oriented types.
- Machine words and register contents, which should generally be
  unsigned bit patterns.
- Host loop counters, array indexes, and sizes, which should use host
  count/index types where practical.
- Explicitly signed VAX operands, where signed interpretation should be
  local and intentional.

The memory-read callback interfaces appear to be a broader architectural
boundary.  They currently return `int32` even when the caller is passing
around a 32-bit machine word.  Changing that convention should be done
under focused VAX unit and integration coverage rather than folded into
small undefined-behavior fixes.

The VAX architectural masks in `vax_defs.h` should also be audited.
Constants such as `BMASK`, `WMASK`, `LMASK`, and related sign or field
masks describe guest bit patterns, but some are currently plain signed
integer literals.  Making those masks unsigned may simplify many local
UB fixes and better document their domain, but it will affect usual
arithmetic conversions throughout the VAX CPU, compatibility-mode, MMU,
and device code.  That change should be a separate, broadly tested VAX
cleanup rather than a side effect of a narrow sanitizer repair.
