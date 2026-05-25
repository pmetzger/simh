# VAX Machine Word And Address Types

The VAX simulator code still uses signed `int32_t` in many places that
represent VAX machine words, device registers, bus data, or addresses.
Those values are usually bit patterns, not host signed integers.  Using
signed storage for them invites undefined behavior in shifts, overflow,
masking, and address arithmetic.

The goal of this project is to move the VAX simulators to unsigned
types for machine words and addresses, while keeping signed
interpretation explicit and local to instructions or devices whose
architectural semantics require it.

This is not a mechanical `int32_t` to `uint32_t` conversion.  Each value
must be classified by meaning before its type changes.

## Current State

The central CPU state is already mostly unsigned:

- `M` is `uint32_t *`.
- `R`, `STK`, `PSL`, `SISR`, `fault_PC`, and machine-check addresses
  are `uint32_t`.
- Most architectural masks and many local temporaries already operate
  as unsigned values.

The older VAX interfaces are the main problem:

- `Read`, `ReadB`, `ReadW`, `ReadL`, and related memory helpers return
  `int32_t` even though they return VAX data.
- `Write`, `WriteB`, `WriteW`, `WriteL`, `WriteIO`, and `WriteReg`
  often accept VAX data as `int32_t`.
- Device dispatch tables use callbacks such as
  `t_stat (*rd)(int32_t *dat, int32_t ad, int32_t md)` and
  `t_stat (*wr)(int32_t dat, int32_t ad, int32_t md)`.
- Many model-specific register handlers use `int32_t pa`,
  `int32_t val`, or `int32_t data` for physical addresses and device
  register images.
- Floating-point, decimal, quad, octa, and condition-code code uses a
  mixture of signed and unsigned types where some signed operations are
  intentional and others are only legacy representation.

## Type Model

Introduce VAX-specific names before starting broad conversion.  The
names should document domain, not just width.

Suggested initial model:

- `vax_word`: unsigned 32-bit VAX longword or register image.
- `vax_addr`: unsigned 32-bit virtual address.
- `vax_pa`: unsigned 32-bit physical address.
- `vax_sword`: signed 32-bit interpretation of a `vax_word`.
- `vax_byte`: unsigned 8-bit guest byte, where useful.
- `vax_word16`: unsigned 16-bit guest word, where useful.

Prefer `uint32_t` directly only for host-neutral bit helpers or where
the local code is already clearly width-oriented.  Prefer the VAX type
names at subsystem boundaries.

Do not rename host counts, indexes, booleans, status codes, simulator
callback arguments, or delays merely because they are 32-bit integers.

## Signed Interpretation

Signed VAX behavior should happen through helpers or short local casts,
never by storing general machine words in signed variables.

Examples that should remain explicitly signed:

- signed byte, word, longword comparisons;
- signed branch displacement extension;
- signed integer multiply, divide, and overflow detection;
- arithmetic right shifts;
- conversion between integer and floating-point formats;
- signed decimal and packed-decimal operations;
- condition-code calculations that depend on VAX signed overflow.

Examples that should not use signed storage:

- memory contents;
- general registers;
- physical and virtual addresses;
- page-table entries;
- device control/status registers;
- interrupt vector values;
- ROM, NVR, cache, QBus, Unibus, and SBI data paths;
- masks, tags, and register fields.

## Migration Rules

For each slice:

1. Write or extend automated tests first.
2. Classify every touched integer as data, address, signed operand,
   count, index, status, or flag.
3. Change interface types only when all direct callers and callees can
   be updated in the same commit.
4. Keep signed guest behavior explicit with helpers such as
   `vax_word_as_sword()` or operation-specific functions.
5. Avoid compatibility casts that hide remaining signed interfaces.
   Temporary casts are acceptable only at intentionally staged
   boundaries and should have a follow-up note in this file.
6. Run focused unit tests after each small change, then the VAX unit
   suite, then sanitizer and integration gates for larger slices.

## Stage 0: Inventory And Guardrails

Before production changes, create an inventory of VAX integer domains.
This inventory can be text in this file or a small checked-in project
note, not a generated script.

Inventory groups:

- CPU architectural state and instruction operands.
- MMU and physical memory access helpers.
- model-specific system registers.
- QBus, Unibus, SBI, CMI, MBA, UBA, and nexus dispatch tables.
- standard devices and console paths.
- ROM, NVR, cache, and diagnostic memory paths.
- floating-point, decimal, quad, octa, and string instructions.
- graphics adapters and DMA helpers.
- symbolic examine/deposit and boot-loader paths.

Testing required for this stage:

- No production code changes without new tests.
- Add compile-time assertions for any new VAX type aliases.
- Add unit tests for new signed/unsigned conversion helpers.
- Run the existing VAX unit suite as the starting baseline.

## Stage 1: VAX Type Aliases And Helpers

Add the type aliases and small helper functions in the VAX header layer.
These helpers are the vocabulary for later changes.

Helpers to consider:

- convert `vax_word` to signed longword interpretation;
- sign-extend byte and word values to `vax_word`;
- detect byte, word, and longword negative values;
- compute add/subtract carry and overflow without signed overflow;
- arithmetic right shift of a VAX longword;
- mask byte, word, and longword values with unsigned constants.

Testing required:

- Unit-test every helper with `0`, `1`, maximum positive values,
  sign-bit values, all-ones values, and boundary shift counts.
- Include cases for `0x7fffffff`, `0x80000000`, `0xffffffff`,
  `0x7fff`, `0x8000`, `0xffff`, `0x7f`, `0x80`, and `0xff`.
- Test that helper results match the current VAX behavior before using
  them in production paths.

## Stage 2: MMU And Memory Access Boundaries

Convert memory read/write helpers to unsigned VAX data and address
types.  This is the most important boundary because many instruction
and device paths depend on it.

Candidate interfaces:

- `Read`, `Write`;
- `ReadU`, `WriteU`;
- `ReadB`, `ReadW`, `ReadL`, `ReadLP`;
- `WriteB`, `WriteW`, `WriteL`;
- `ReadIO`, `WriteIO`;
- `ReadReg`, `WriteReg`.

Testing required:

- Add direct unit coverage for aligned and unaligned `Read`/`Write`
  behavior before changing signatures.
- Cover byte, word, longword, unaligned word, unaligned longword, and
  page-crossing cases.
- Include values whose high bit is set so tests would fail if signed
  extension leaks into the data path.
- Include address wrap and masking cases near `0xffffffff`, `PAMASK`,
  page boundaries, and longword boundaries.
- Run existing QBus and model system-device unit tests after each
  memory-helper change.

Useful existing tests to extend:

- `tests/unit/simulators/VAX/test_vax_qbus_mem_write.c`
- `tests/unit/simulators/VAX/test_vax_qbus_io_read.c`
- `tests/unit/simulators/VAX/test_vax_qbus_io_write.c`
- `tests/unit/simulators/VAX/test_vax_rom_byte_write_behavior.c`

## Stage 3: Bus And Device Dispatch Interfaces

After the MMU helpers are unsigned, convert bus and device callback
types that carry VAX data or physical addresses.

Candidate areas:

- QBus and Unibus dispatch tables;
- SBI, CMI, nexus, UBA, MBA, and related model buses;
- model-specific `ReadReg`/`WriteReg` implementations;
- device register handlers for DZ, XS, RZ, VA, VC, GPX, NVR, ROM, and
  standard clock/console devices.

Testing required:

- For each converted bus, add tests at the dispatch-table boundary that
  record the physical address, access length, and data value.
- Use values with bit 31 set and values with partial-lane writes.
- Preserve existing lane behavior for byte and word writes.
- Test unmapped, absent, and timeout paths so error handling does not
  depend on signed sentinels.
- Convert one bus family per commit and run the tests for every model
  that uses that bus family.

Useful existing tests to extend:

- `tests/unit/simulators/VAX/test_vax_qbus_io_read.c`
- `tests/unit/simulators/VAX/test_vax_qbus_io_write.c`
- `tests/unit/simulators/VAX/test_vax820_uba_interrupts.c`
- `tests/unit/simulators/VAX/test_vax4xx_dz.c`
- `tests/unit/simulators/VAX/test_vax_xs.c`
- `tests/unit/simulators/VAX/test_vax_uw.c`

## Stage 4: CPU Instruction Helpers

Convert instruction helpers and operand arrays after the memory and bus
boundaries are stable.  Many helpers already take `uint32_t *opnd`, but
return values, accumulators, and local temporaries often remain signed.

Candidate areas:

- operand fetch/decode in `vax_cpu.c`;
- `vax_cpu1.c` integer and string helpers;
- call/return frame helpers;
- branch displacement and effective-address calculation;
- probe and privilege helpers;
- condition-code helpers.

Testing required:

- Add unit tests for each helper before changing its signature.
- Use instruction-level tests where helper-level seams are not yet
  available.
- Cover signed and unsigned comparisons separately.
- Cover add, subtract, negate, increment, decrement, and multiply cases
  that cross `0x7fffffff` and `0x80000000`.
- Cover arithmetic right shift with sign-bit values and all relevant
  edge shift counts.
- Cover address calculations that wrap as VAX longwords.
- Preserve exact condition-code results.

If a helper cannot be tested directly, introduce the smallest seam
needed to test it before changing behavior.

## Stage 5: Floating Point, Decimal, Quad, And Octa

Treat these as separate high-risk migrations.  They encode VAX signed
semantics in compact legacy code and should not be swept up in the main
CPU helper conversion.

Candidate areas:

- `vax_fpa.c`;
- `vax_octa.c`;
- decimal and string instruction helpers;
- `EMUL`, `EDIV`, `ASHQ`, packed decimal, and polynomial operations.

Testing required:

- Add characterization tests before each conversion.
- Cover positive and negative zero-like encodings where relevant.
- Cover maximum finite, minimum negative, reserved operand, overflow,
  underflow, divide-by-zero, and rounding cases.
- Compare against documented VAX results or existing emulator behavior.
- Use differential tests against the pre-change helper when practical.

## Stage 6: Model-Specific Devices And Firmware Paths

Convert remaining model-specific register and firmware paths after the
shared buses are done.

Candidate areas:

- KA4xx system devices and ROM patch paths;
- VAX-750/780/820/860 memory and bus controllers;
- NVR, ROM, diagnostic memory, cache tag/data arrays;
- graphics adapters;
- boot-loader and console setup paths.

Testing required:

- Preserve existing ROM and NVR byte-lane behavior.
- Add register read/write tests for any converted system-device file.
- Use high-bit values in every converted device-register test.
- Boot smoke-test representative models after device-heavy slices.

Useful existing tests to extend:

- `tests/unit/simulators/VAX/test_vax4xx_sysdev_behavior.c`
- `tests/unit/simulators/VAX/test_vax43_sysdev_behavior.c`
- `tests/unit/simulators/VAX/test_vax440_sysdev_behavior.c`
- `tests/unit/simulators/VAX/test_vax_sysdev_behavior.c`
- `tests/unit/simulators/VAX/test_vax4xx_stddev_behavior.c`
- `tests/unit/simulators/VAX/test_vax750_mem.c`

## Stage 7: Examine, Deposit, Symbols, And Save/Restore

After internal representation is stable, audit the interface to SCP and
save/restore code.

Candidate areas:

- `examine` and `deposit` implementations;
- symbolic parsing and formatting;
- register declarations and `REG` storage declarations;
- boot command argument parsing;
- save and restore of device state.

Testing required:

- Extend symbolic tests for signed-looking values such as
  `0xffffffff` and `0x80000000`.
- Add examine/deposit tests for memory, registers, and device
  registers with high-bit values.
- Add save/restore round-trip tests for converted device state.
- Confirm SCP signed display remains only a display choice, not a
  storage choice.

Useful existing tests to extend:

- `tests/unit/simulators/VAX/test_vax_symbols.c`
- `tests/unit/src/core/test_scp_register.c`
- `tests/unit/src/core/test_scp_format.c`

## Build And Test Gates

For every small production change:

- Run the new focused test and confirm it fails before the change when
  the change is behavioral.
- Run the focused test again after the change.
- Run the affected VAX unit tests with `ctest --parallel`.
- Build the affected emulator targets with warnings enabled.

For every stage:

- Run all VAX unit tests.
- Run relevant core SCP unit tests when `REG`, examine/deposit, or
  symbols are touched.
- Run sanitizer or undefined-behavior builds where available.
- Run at least one integration boot smoke test for a representative
  small VAX and one larger/bus-heavy VAX.

Recommended representative integration set:

- VAXstation 4000 VLC or another KA4xx model for ROM, NVR, and QBus.
- VAX-11/780 or VAX-8600 for SBI/nexus style paths.
- VAX-11/750 or VAX-8200 for UBA/MBA-style paths.
- A VAX model that exercises the recently fixed LANCE path.

Integration tests should be scripted only when they become stable
project tests.  During investigation, manual emulator runs are useful,
but a migration slice is not complete until its important behavior has
automated coverage.

## Completion Criteria

This project is complete when:

- VAX machine words and addresses use unsigned VAX-domain types at
  storage and interface boundaries.
- Signed VAX interpretation is local and explicit.
- No VAX memory, bus, or device data path relies on signed overflow,
  signed left shift, or implementation-defined right shift.
- The VAX unit suite covers memory access, bus dispatch, CPU helpers,
  device registers, symbols, and save/restore for high-bit values.
- Representative VAX integration boots pass under normal and sanitizer
  builds.
- Remaining signed `int32_t` values in VAX code are documented by
  meaning: host count/index/status, simulator callback parameter, or
  explicit signed VAX operand.

## Open Questions

- Should the VAX type aliases live directly in `vax_defs.h`, or should
  they move into a smaller `vax_types.h` included by `vax_defs.h`?
- Should bus callback typedefs be centralized so model headers stop
  duplicating signed legacy signatures?
- How much instruction-level testing should be added before touching
  `vax_cpu.c`, given that helper-level seams are currently limited?
- Which VAX integration tests should become permanent acceptance tests,
  and which should remain manual release checks?
