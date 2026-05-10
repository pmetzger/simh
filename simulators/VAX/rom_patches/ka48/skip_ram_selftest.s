/*
 * KA48 ROM RAM self-test bypass patch.
 *
 * Apply these bytes at ROM virtual address 0x2004de0a, immediately after the
 * entry mask for the early RAM self-test routine at 0x2004de08.  This lets
 * VAX CALLS build and tear down the call frame normally, then returns r0 == 1
 * exactly like a passing test routine.
 */
        .text
        .globl  skip_ram_selftest_patch
skip_ram_selftest_patch:
        movl    $0x1, %r0
        ret
