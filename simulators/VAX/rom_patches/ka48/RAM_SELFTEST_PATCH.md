# KA48 RAM Self-Test Bypass Patch

This patch bypasses the early KA48 RAM self-test routine used by the
VAXstation 4000 VLC boot ROM.  It is useful for emulator test runs where the
host already provides reliable memory and waiting for the ROM progress bar is
not part of the behavior under test.

The patch replaces the first instruction after the routine entry mask with:

```asm
movl    $0x1, %r0
ret
```

Patch location:

- ROM virtual address: `0x2004de0a`
- ROM file offset: `0x0de0a`
- Original bytes: `c2 2c 5e d0`
- Replacement bytes: `d0 01 50 04`

`0x2004de08` is the entry mask for the RAM self-test routine.  Patching at
`0x2004de0a` preserves normal VAX `CALLS` frame setup and return cleanup while
returning `r0 == 1`, the success result expected by the caller.

The manual simulator equivalent is:

```text
deposit ROM de08 01d00ffc
deposit ROM de0c 10ac0450
```

Those deposits preserve the entry mask and write the same four patch bytes at
the unaligned byte address `0x0de0a`.

To regenerate the expected bytes, build `skip_ram_selftest.s` with a VAX
assembler and extract the `.text` section.  The companion `Makefile` assumes a
local `vax-netbsdelf` binutils tree under `tools/local/vax-binutils`.

Residual risk:

- This bypasses real RAM diagnostics.  It should only be used when skipping
  the ROM memory test is an explicit test or debugging choice.
- The offset is validated against the bundled KA48 ROM image.  If a different
  ROM revision is substituted, confirm the original bytes and routine
  semantics before moving this descriptor.
