# Simulator Catalog

This page lists the user-facing simulator targets in the CMake build.
Each entry starts with the target name used by CMake and then gives a
human-readable description of the system or family it represents.

Unless otherwise noted, these targets are part of the normal simulator
inventory and can be built directly with:

```sh
cmake --build build/release --target <target>
```

## DEC

### PDP family

- `pdp1`: DEC PDP-1.
- `pdp4`: DEC PDP-4.
- `pdp6`: DEC PDP-6.
- `pdp7`: DEC PDP-7.
- `pdp8`: DEC PDP-8.
- `pdp9`: DEC PDP-9.
- `pdp10`: Shared DEC PDP-10 simulator build.
- `pdp10-ka`: DEC PDP-10 KA10.
- `pdp10-ki`: DEC PDP-10 KI10.
- `pdp10-kl`: DEC PDP-10 KL10.
- `pdp10-ks`: DEC PDP-10 KS10.
- `pdp11`: DEC PDP-11.
- `pdp15`: DEC PDP-15.
- `uc15`: DEC UC15, a PDP-11-derived machine.

### VAX family

- `infoserver100`: DEC InfoServer 100.
- `infoserver1000`: DEC InfoServer 1000.
- `infoserver150vxt`: DEC InfoServer 150 VXT.
- `microvax1`: DEC MicroVAX I.
- `microvax2`: DEC MicroVAX II.
- `microvax2000`: DEC MicroVAX 2000.
- `microvax3100`: DEC MicroVAX 3100.
- `microvax3100e`: DEC MicroVAX 3100e.
- `microvax3100m80`: DEC MicroVAX 3100 Model 80.
- `rtvax1000`: DEC RTVAX 1000.
- `vax`: Generic VAX executable, commonly used for the MicroVAX
  3900/VAXserver 3900 configuration.
- `vax730`: DEC VAX-11/730.
- `vax750`: DEC VAX-11/750.
- `vax780`: DEC VAX-11/780.
- `vax8200`: DEC VAX 8200.
- `vax8600`: DEC VAX 8600.
- `vaxstation3100m30`: DEC VAXstation 3100 Model 30.
- `vaxstation3100m38`: DEC VAXstation 3100 Model 38.
- `vaxstation3100m76`: DEC VAXstation 3100 Model 76.
- `vaxstation4000m60`: DEC VAXstation 4000 Model 60.
- `vaxstation4000vlc`: DEC VAXstation 4000 VLC.
- `microvax3900`: Alias name for the `vax` executable.

## IBM

- `i1401`: IBM 1401 data processing system.
- `i1620`: IBM 1620 scientific computer.
- `i650`: IBM 650 magnetic drum computer.
- `i701`: IBM 701 Defense Calculator.
- `i7010`: IBM 7010 and 1410 family systems.
- `i704`: IBM 704 scientific computer.
- `i7070`: IBM 7070 and 7074 family systems.
- `i7080`: IBM 7080, 702, 705, and 705 III family systems.
- `i7090`: IBM 7090, 7094, 709, and 704 family systems.
- `i7094`: IBM 7094.
- `ibm1130`: IBM 1130.
- `s3`: IBM System/3.

## Intel

- `intel-mds`: Intel Intellec MDS development system.
- `scelbi`: SCELBI 8H, an early Intel 8008-based microcomputer.

### Configured but not yet supported

- `ibmpc`: IBM PC target present in CMake as an explicit opt-in target,
  but currently known broken and not part of the default build.
- `ibmpcxt`: IBM PC XT target present in CMake as an explicit opt-in
  target, but currently known broken and not part of the default build.

## Hewlett-Packard

- `hp2100`: HP 2116, HP 2100, HP 21MX, and HP 1000 family systems.
- `hp3000`: HP 3000 family.

## Data General

- `eclipse`: Data General Eclipse.
- `nova`: Data General Nova.

## Scientific Data Systems and related systems

- `sds`: Scientific Data Systems SDS 940.
- `sigma`: Scientific Data Systems/Xerox Data Systems Sigma 32-bit
  family.
- `sel32`: Gould/SEL 32-bit systems.

## Burroughs

- `b5500`: Burroughs B5500.

## Control Data

- `cdc1700`: Control Data 1700.

## Honeywell

- `h316`: Honeywell H-316 and H-516 family.

## AT&T

- `3b2-400`: AT&T 3B2/400.
- `3b2-700`: AT&T 3B2/700.

## Norsk Data

- `nd100`: Norsk Data ND-100.

## Interdata

- `id16`: Interdata 16-bit systems.
- `id32`: Interdata 7/32 and 8/32 32-bit systems.

## GRI Systems

- `gri`: GRI-909.

## Librascope

- `lgp`: Librascope/Royal McBee LGP-30 and LGP-21 family.

## Imlac

- `imlac`: Imlac PDS family.

## Experimental

- `alpha`: DEC Alpha work-in-progress simulator.
- `pdq3`: PDQ-3 experimental simulator.
- `sage`: SAGE air-defense system simulator.

## Other systems

- `altair`: MITS Altair 8800 with Intel 8080 CPU.
- `altairz80`: Altair Z80 system with M68000 support.
- `besm6`: Soviet BESM-6 mainframe.
- `linc`: DEC LINC.
- `ssem`: Manchester Small-Scale Experimental Machine.
- `swtp6800mp-a`: SWTP 6800 MP-A.
- `swtp6800mp-a2`: SWTP 6800 MP-A2.
- `tt2500`: TT2500 terminal system.
- `tx-0`: MIT Lincoln Laboratory TX-0.
