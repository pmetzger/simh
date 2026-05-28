# SLIRP Follow-Up

## Goal

Track the remaining SLIRP work after replacing the bundled implementation
with external libslirp.

## Remaining Work

1. Windows libslirp discovery

   Decide the supported native Windows path for libslirp.  The likely choices
   are vcpkg, MSYS2/pkg-config, or an explicit manual dependency path.  Windows
   builds require libslirp 4.9.0 or newer because older releases do not expose
   the socket-handle callbacks needed on Win64.

2. CMake dependency abstraction

   Add one internal CMake target for libslirp, such as `zimh_external_slirp`,
   so runtime code does not care whether libslirp came from pkg-config, vcpkg,
   or a CMake package.

3. Configure-path verification

   Add or document checks for:

   - `WITH_SLIRP=OFF`;
   - missing libslirp;
   - too-old libslirp; and
   - missing-dependency report output.

4. Fake host socket interposition

   Consider a future test library that interposes host socket calls made by
   libslirp.  This would let tests cover outbound socket behavior without real
   network access.  The design is platform-sensitive: ELF link wrapping,
   `LD_PRELOAD`, macOS interposition, and Windows import-library techniques
   differ.

5. Packet-level IPv6 tests

   Add lower-level IPv6 unit coverage in `zimh-unit-sim-slirp`, such as
   synthetic IPv6 neighbor discovery or ICMPv6 to the SLIRP gateway.  Guest OS
   coverage belongs in `NETWORK_TESTING.md`; these tests should stay small,
   fast, and easy to debug.
