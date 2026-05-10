# Async Identifier Rename

## Goal

Normalize identifier-level uses of `asynch` and `ASYNCH` to `async` and
`ASYNC` so the codebase uses one spelling for asynchronous I/O machinery.

The desired spelling for current source is `async` in C identifiers and
`ASYNC` in macros, CMake definitions, command metadata, and documentation
that refers to current identifiers.

## Scope

Rename current identifier-like uses, including:

- C variables and struct fields, such as `asynch_io` and
  `asynch_io_latency`
- C functions, such as `sim_set_asynch` and `sim_show_asynch`
- preprocessor macros and compile definitions, such as `SIM_ASYNCH_IO`,
  `SIM_ASYNCH_CLOCKS`, `SIM_ASYNCH_MUX`, and `NO_ASYNCH_MUX`
- CMake variables or compile-definition lists that inject those names
- help and documentation macro names, such as `HLP_SET_ASYNCH`
- current comments and project documentation that name the changed
  identifiers

Out of scope for the mechanical rename:

- ordinary prose that says "asynchronous"
- guest scripts, diagnostic listings, generated outputs, or imported
  artifacts whose spelling is owned outside the current source interface
- compatibility command tokens, unless the command-language decision below
  is made in the same slice

## Command Language

The simulator currently exposes commands such as:

- `SET ASYNCH`
- `SET NOASYNCH`
- `SHOW ASYNCH`
- `SET CLOCK ASYNCH`
- `SET CLOCK NOASYNCH`

The internal handlers, help macros, and command metadata should use `ASYNC`
names after this cleanup. The command tokens seen by users need a deliberate
compatibility decision. Pick one option before changing command strings:

- keep the existing command spelling while renaming only internal handlers;
- add `ASYNC` and `NOASYNC` aliases while keeping `ASYNCH` compatibility; or
- intentionally rename the user-facing command language and document the
  compatibility break.

The safest initial implementation is to rename internal identifiers and add
`ASYNC`/`NOASYNC` aliases while preserving `ASYNCH`/`NOASYNCH`.

## Proposed Commit Slices

1. Runtime field cleanup
   - Rename disk, tape, and ethernet fields from `asynch_io` to `async_io`
     and `asynch_io_latency` to `async_io_latency`.
   - Update adjacent comments that refer to those fields.

2. Core/build macro cleanup
   - Rename `SIM_ASYNCH_IO`, `SIM_ASYNCH_CLOCKS`, and `SIM_ASYNCH_MUX` to
     `SIM_ASYNC_IO`, `SIM_ASYNC_CLOCKS`, and `SIM_ASYNC_MUX`.
   - Rename `NO_ASYNCH_MUX` and `TMUF_NOASYNCH` if those flag names are
     included in this cleanup.
   - Update CMake compile-definition injection and all guarded source code.

3. Core function and help macro cleanup
   - Rename internal handlers and helper macros such as `sim_set_asynch`,
     `sim_show_asynch`, `sim_timer_change_asynch`, `HLP_SET_ASYNCH`, and
     `HLP_SHOW_ASYNCH`.
   - Keep or alias command table strings according to the command-language
     decision above.

4. Documentation and current comments
   - Update current developer documentation and current code comments that
     refer to renamed identifiers.
   - Leave unrelated prose alone when it does not name a changed identifier.

## Verification

After each slice:

- run `rg` for identifier-like `asynch` and `ASYNCH` uses in the touched
  area;
- run `git diff --check`;
- build the touched targets in the debug-warnings tree.

Before promotion:

- run a clean normal build;
- run a debug-warnings build;
- run unit tests;
- run integration tests, with attention to async-capable PDP-11 disk/tape
  paths, TMXR/console behavior, timer behavior, and VAX register tables that
  expose async status.
