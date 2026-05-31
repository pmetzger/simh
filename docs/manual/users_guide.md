# ZIMH User’s Guide

**Copyright Notice**

ZIMH is a hard fork of SIMH. Much of this guide originated in
the SIMH documentation and remains under an X11-style open
source license; the precise terms are available at:

[LICENSE.txt](../../LICENSE.txt)

# Table of Contents

[Introduction](#introduction)

[1 Running A Simulator](#running-a-simulator)

[2 Building ZIMH](#building-zimh)

[3 Simulator Conventions](#simulator-conventions)

[4 Commands](#commands)

[4.1 Loading and Saving Programs](#loading-and-saving-programs)

[4.2 Saving and Restoring State](#saving-and-restoring-state)

[4.3 Resetting Devices](#resetting-devices)

[4.4 Connecting and Disconnecting Devices](#connecting-and-disconnecting-devices)

[4.5 Examining and Changing State](#examining-and-changing-state)

[4.6 Evaluating Instructions](#evaluating-instructions)

[4.7 Running a Simulated Program](#running-a-simulated-program)

[4.7.1 Controlling the Simulation Rate](#controlling-the-simulation-rate)

[4.8 Stopping the Simulator](#stopping-the-simulator)

[4.8.1 Simulator Detected Stop Conditions](#simulator-detected-stop-conditions)

[4.8.2 User Specified Stop Conditions](#user-specified-stop-conditions)

[4.8.3 Breakpoints](#breakpoints)

[4.8.4 Execution Time Limits](#execution-time-limits)

[4.9 Halt on Output Data](#halt-on-output-data)

[4.9.1 Switches](#switches)

[4.9.2 Determining which output matched](#determining-which-output-matched)

[4.10 Injecting Input Data](#injecting-input-data)

[4.10.1 Delay](#delay)

[4.10.2 After](#after)

[4.10.3 Escaping String Data](#escaping-string-data-1)

[4.11 Setting Device Parameters](#setting-device-parameters)

[4.12 Displaying Parameters and Status](#displaying-parameters-and-status)

[4.13 Altering the Simulated Configuration](#altering-the-simulated-configuration)

[4.14 Console Options](#console-options)

[4.15 Remote Simulator Command Interface](#remote-simulator-command-interface)

[4.15.1 Remote Master Mode](#remote-master-mode)

[4.16 Executing Command Files](#executing-command-files)

[4.16.1 Default Command File Executed on Simulator Startup](#default-command-file-executed-on-simulator-startup)

[4.16.2 Change Command Execution Flow](#change-command-execution-flow)

[4.16.3 Subroutine Calls](#subroutine-calls)

[4.16.4 Pausing Command Execution](#pausing-command-execution)

[4.16.5 Displaying Arbitrary Text](#displaying-arbitrary-text)

[4.16.6 Testing Simulator State](#testing-simulator-state)

[4.16.7 Trapping on Command Completion Conditions](#trapping-on-command-completion-conditions)

[4.16.8 Command Arguments](#command-arguments)

[4.16.9 Command Aliases](#command-aliases)

[4.17 Executing System Commands](#executing-system-commands)

[4.18 Getting Help](#getting-help)

[4.19 Recording Simulator Activities](#recording-simulator-activities)

[4.19.1 Switches](#switches-1)

[4.20 Controlling Debugging](#controlling-debugging)

[4.20.1 Switches](#switches-2)

[4.20.2 Device Debug Options](#device-debug-options)

[4.20.3 Displaying Debug Settings](#displaying-debug-settings)

[4.21 Exiting the Simulator](#exiting-the-simulator)

[4.22 Manipulating Files within the Simulator](#manipulating-files-within-the-simulator)

[4.22.1 Manipulating File Archives](#manipulating-file-archives)

[4.22.2 Transferring Data from the Web](#transferring-data-from-the-web)

[Appendix 1: File Representations](#appendix-1-file-representations)

[A.1 Hard Disks](#a.1-hard-disks)

[A.2 Floppy Disks](#a.2-floppy-disks)

[A.3 Magnetic Tapes](#a.3-magnetic-tapes)

[A.4 Line Printers](#a.4-line-printers)

[A.5 DECtapes](#a.5-dectapes)

[Acknowledgements](#acknowledgements)

# Introduction

This guide documents the ZIMH simulators. These simulators are
open source software; refer to the license terms above for
conditions of use. You may file bug reports at
<https://github.com/pmetzger/zimh/issues> or start discussions at
<https://github.com/pmetzger/zimh/discussions>.

This guide explains the parts of ZIMH shared by the simulators:
starting a simulator, using the simulator command interface,
configuring devices, running command files, and understanding
common file formats. Machine-specific manuals document details
unique to each simulated computer.

# Running A Simulator

To start a simulator, run its executable. Installed simulator
executables are normally named `zimh-<target>`.

The simulator recognizes three command line switches: `-q`, `-v`, and
`-e`. If `-q` is specified, certain informational messages are
suppressed. The `-v` and `-e` switches pertain only to command files
and are described in
[Executing Command Files](#executing-command-files).

The simulator interprets the arguments on the command line, if any, as
the file name and arguments for a `DO` command:

```
$ zimh-pdp10 {switches} {<startup file> {arg,arg,...}}
```

If no file is specified on the command line, the simulator looks
for a startup file consisting of the simulator name, including
any path components, plus the extension `.ini`. If a startup file
is specified, either on the command line or implicitly via the
`.ini` capability, it should contain simulator commands, one per
line. These commands can set up standard parameters, for example,
disk sizes.

After initializing its internal structures and processing any
startup file, the simulator prints its name and version. The
simulator command interface then prompts for input with:

```
sim>
```

# Building ZIMH

ZIMH builds with CMake. Build outside the source tree, normally
under `build/`. The top-level `Makefile` is only a compatibility
wrapper over the default CMake build.

Current build instructions, dependency lists, platform-specific
package names, and build examples are maintained in
[BUILDING.md](../../BUILDING.md). CMake options are described in
[README-CMake.md](../../README-CMake.md).

ZIMH provides accurate simulations of older computers for users
of modern host systems. It supports modern POSIX systems and
modern Windows, using compilers with C17 support. POSIX systems
include operating systems such as Linux, the BSDs, and macOS.
Unlike historical SIMH, ZIMH does not try to run on obsolete
host operating systems or old compiler environments.

Optional features, such as network and video support, require the
corresponding host libraries to be available when ZIMH is built.
Some simulators require host 64-bit integer support. This is
expected on supported modern hosts, but may matter for unusual
cross-builds or older environments.

The CMake build can compile the full normal simulator set or
selected individual simulators. Some operating system package
managers may provide pre-built ZIMH packages, and project-built
binaries for selected platforms may be published on GitHub in
future releases.

# Simulator Conventions

A simulator consists of a series of devices, the first of which is
always the CPU. A device consists of named registers and one or more
numbered units. Registers correspond to device state, units to device
address spaces. Thus, the `CPU` device might have registers like `PC`,
`ION`, etc., and a unit corresponding to main memory; a disk device
might have registers like `BUSY`, `DONE`, etc., and units
corresponding to individual disk drives. Except for main memory,
device address spaces are simulated as unstructured binary disk files
in the host file system. The `SHOW CONFIG` command displays the
simulator configuration.

A simulator keeps time in terms of arbitrary units, usually one time
unit per instruction executed. Simulated events (such as completion of
I/O) are scheduled at some number of time units in the future. The
simulator executes synchronously, invoking event processors when
simulated events are scheduled to occur. Even asynchronous events,
like keyboard input, are handled by polling at synchronous
intervals. The `SHOW QUEUE` command displays the simulator event
queue.

# Commands

Simulator commands consist of a command verb, optional switches, and
optional arguments. Switches take the form:

```
-<letter>{<letter>...}
```

Multiple switches may be specified separately or together: `-abcd` and
`-a -b -c -d` are treated identically. Verbs, switches, and other input
(except for file names) are *case insensitive*.

Any command beginning with semicolon (`;`) is considered a comment and
ignored.

## Loading and Saving Programs

The `LOAD` command (abbreviation `LO`) loads a file in binary loader
format:

```
load <filename> {implementation options}
```

The types of formats supported are simulator specific. Options (such
as load within range) are also simulator specific. The load command
may not be meaningful or supported at all in some simulators.

The `DUMP` command (abbreviation `DU`) dumps memory in binary loader
format:

```
dump <filename> {implementation options}
```

The types of formats supported are simulator specific. Options (such
as dump within range) are also simulator specific. The dump command
may not be meaningful or supported at all in some simulators.

## Saving and Restoring State

The `SAVE` command (abbreviation `SA`) saves the complete state of the
simulator to a file. This includes the contents of main memory and all
registers, and the I/O connections of devices:

```
save <filename>
```

The `RESTORE` command (abbreviation `REST`, alternately `GET`)
restores a previously saved simulator state:

```
restore {-d} {-f} {-q} <filename>
```

Notes:

1. `SAVE` file format compresses zeroes to minimize file size.
2. The simulator can’t restore active incoming telnet sessions to
   multiplexer devices, but the listening ports will be restored
   across a save/restore.
3. `-d` switch avoids detaching and reattaching devices during a
   restore.
4.  `-f` switch overrides the date timestamp check for attached files
    during a restore.
5. `-q` suppresses warning messages about save file version information.

## Resetting Devices

The `RESET` command (abbreviation `RE`) resets a device or the entire
simulator to a predefined condition. If switch `-p` is specified, the
device is reset to its power-up state:

|                  |                        |
|------------------|------------------------|
| `RESET`          | reset all devices      |
| `RESET -p`       | powerup all devices    |
| `RESET ALL`      | reset all devices      |
| `RESET <device>` | reset specified device |

Typically, `RESET` stops any in-progress I/O operation, clears any
interrupt request, and returns the device to a quiescent state. It
does not clear main memory or affect I/O connections.

## Connecting and Disconnecting Devices

Except for main memory and network devices, units normally use
unstructured binary files in the host file system. Before using
one of these units, attach it to a host file with the `ATTACH`
command (abbreviation `AT`):

```
ATTACH <unit> <filename>
```

With the `-n` switch, `ATTACH` creates a new file or truncates
an existing file to zero length, and prints a message.

If the file does not exist, `ATTACH` normally creates it and
prints a message. With the `-e` switch, `ATTACH` instead reports
an error when the file does not exist.

With the `-r` switch, or when the file is write protected,
`ATTACH` tries to open the file read only. If the file does not
exist, or the unit does not support read only operation, an error
occurs. Input-only devices, such as paper-tape readers, and
devices with write lock switches, such as disks and tapes,
support read only operation; other devices do not. If a file is
attached read only, its contents can be examined but not
modified.

With the `-a` switch, `ATTACH` opens a sequential output-only
device, such as a line printer or paper tape punch, in append
mode. New output is added after any existing file data.

For simulated disks that use the common disk library, `ATTACH` can also
create a volatile memory-backed disk instead of using a host file:

```
ATTACH <disk_unit> RAMDISK:
ATTACH <disk_unit> RAMDISK:<size>
ATTACH <disk_unit> RAMDISK:SIZE=<size>
ATTACH <disk_unit> RAMDISK:TYPE=<drive-type>
ATTACH <disk_unit> RAMDISK:TYPE=<drive-type>,SIZE=<size>
ATTACH <disk_unit> RAMDISK:TYPE=<drive-type>,FROM=<diskfile>
ATTACH <disk_unit> RAMDISK:TYPE=<drive-type>,SAVE=<diskfile>
```

The `RAMDISK:` spelling is required. (Bare `RAMDISK` is treated
as a normal host file name.) If no size is specified, the
ramdisk uses the same size that would be used when creating a
new disk container file for the current disk type. An unnamed
parameter is a size, so `RAMDISK:456M` is equivalent to
`RAMDISK:SIZE=456M`. Keyed options are order-independent. For
example, `RAMDISK:SIZE=456M,TYPE=RA81` and
`RAMDISK:TYPE=RA81,SIZE=456M` are equivalent.

Explicit sizes are byte counts by default and may use `K`, `M`, or `G`
suffixes.  The suffixes are binary: `RAMDISK:4K` allocates 4096 bytes.

`FROM=<diskfile>` copies the contents of an existing simulator
disk image into the ramdisk at attach time and then closes the
source file. If the source image is smaller than the ramdisk, the
remainder is zero-filled. If the source image is larger than the
ramdisk, the attach is rejected. `FROM=` may be used with
read-only attach; the ramdisk is seeded first, then guest writes
are rejected.

`SAVE=<diskfile>` names the simulator disk image to write when
simulator `SAVE` is run. The `SAVE=` path is not opened,
created, truncated, or validated at attach time. If `SAVE=` is
omitted, the host null file is used and the attach command warns
that simulator `SAVE` and `RESTORE` will not preserve this
ramdisk. When a simulator `RESTORE` reattaches the ramdisk, it
reads the contents from the `SAVE=` image. The restore image must
match the ramdisk size exactly.

Ramdisk contents are volatile.  They are discarded when the unit is
detached or the simulator exits.  Ramdisk support requires host
`fmemopen` support; hosts without `fmemopen`, including Windows, reject
`RAMDISK:` attach requests.  Read-only `RAMDISK:` attaches are accepted;
they create volatile storage that the guest can read but not write.

Disk-container management and file initialization switches such
as `-c`, `-d`, `-e`, `-i`, `-k`, `-m`, `-o`, `-v`, and `-x` do
not apply to `RAMDISK:` and are rejected.

For simulated magnetic tapes, the `ATTACH` command can specify the
format of the attached tape image file:

```
ATTACH -f <tape_unit> <format> <filename>
```

The currently supported tape image file formats are:

|        |                                 |
|--------|---------------------------------|
| `SIMH` | SIMH simulator format           |
| `E11`  | E11 simulator format            |
| `TPC`  | TPC format                      |
| `P7B`  | Pierce simulator 7-track format |

The tape format can also be set with the SET command before `ATTACH`:

```
SET <tape_unit> FORMAT=<format>
ATT <tape_unit> <filename>
```

The format of an attached file can be displayed with the `SHOW` command:

```
SHOW <tape_unit> FORMAT
```

For Telnet-based terminal emulators, the `ATTACH` command associates
the master unit with a TCP/IP port:

```
ATTACH <unit> <port>
```

The port is a decimal number between 1 and 65535 that is not used by
standard TCP/IP protocols.

For Ethernet emulators, the `ATTACH` command selects the host
networking backend or interface used by the simulated Ethernet
device:

```
ATTACH <unit> <network attachment>
```

The `DETACH` (abbreviation `DET`) command breaks the association
between a unit and a file, port, or network device:

|                 |                       |
|-----------------|-----------------------|
| `DETACH ALL`    | detach all units      |
| `DETACH <unit>` | detach specified unit |

The `EXIT` command performs an automatic `DETACH ALL`.

## Examining and Changing State

Four commands examine and change state:

- `EXAMINE` (abbreviated `E`) examines state
- `DEPOSIT` (abbreviated `D`) changes state
- `IEXAMINE` (interactive examine, abbreviated `IE`) examines state
  and allows the user to interactively change it.
- `IDEPOSIT` (interactive deposit, abbreviated `ID`) allows the user
  to interactively change state.

All four commands take the form

```
command {modifiers} <object list>
```

`DEPOSIT` must also include a deposit value at the end of the
command.

There are four kinds of modifiers: switches, device/unit name, search
specifier, and for `EXAMINE`, output file. Switches have been
described previously. A device/unit name identifies the device and
unit whose address space is to be examined or modified. If no device
is specified, the CPU (main memory) is selected; if a device but no
unit is specified, unit 0 of the device is selected.

The search specifier provides criteria for testing addresses or
registers to see if they should be processed. A specifier consists of
a logical operator, a relational operator, or both, optionally
separated by spaces.

```
{<logical op> <value>} <relational op> <value>
```

where the logical operator is `&` (and), `|` (or), or `^` (exclusive
or), and the relational operator is `=` or `==` (equal), `!` or `!=`
(not equal), `>=` (greater than or equal), `>` (greater than), `<=`
(less than or equal), or `<` (less than).

If a logical operator is specified without a relational operator, it
is ignored. If a relational operator is specified without a logical
operator, no logical operation is performed. All comparisons are
unsigned.

The output file modifier redirects command output to a file instead of
the console. An output file modifier consists of `@` followed by a
valid file name.

Modifiers may be specified in any order. If multiple modifiers of the
same type are specified, later modifiers override earlier
modifiers. Note that if the device/unit name comes after the search
specifier, the search values will be interpreted in the radix of the CPU,
rather than of the device/unit.

The "object list" consists of one or more of the following, separated
by commas:

|          |                        |
|----------|------------------------|
| register | the specified register |
| register[sub1-sub2] | the specified register array locations, starting at location sub1 up to and including location sub2 |
| register[sub1/length] | the specified register array locations, starting at location sub1 up to but not including sub1+length |
| register[ALL] | all locations in the specified register array |
| register1-register2 | all the registers starting at register1 up to and including register2 |
| address | the specified location |
| address1-address2 | all locations starting at address1 up to and including address2 |
| address/length | all locations starting at address up to but not including address+length |
| `STATE` | all registers in the device |
| `ALL` | all locations in the unit |
| `$` | use the most recently referenced value as an address to reference (indirect) |
| `.` | use the most recently referenced address again. |

Switches can be used to control the format of display information:

|               |                        |
|---------------|------------------------|
| `-o` or `-8`  | display as octal       |
| `-d` or `-10` | display as decimal     |
| `-h` or `-16` | display as hexadecimal |
| `-2`          | display as binary      |

Simulators typically provide these additional switches for address locations:

|      |                                 |
|------|---------------------------------|
| `-a` | display as ASCII                |
| `-c` | display as character string     |
| `-m` | display as instruction mnemonic |

Simulators may also accept symbolic input; see the
machine-specific documentation.

Examples:

|                |                                   |
|----------------|-----------------------------------|
| `ex 1000-1100` | examine 1000 to 1100              |
| `ex .`         | examine most recent address again |
| `de PC 1040`   | set PC to 1040                    |
| `ie 40-50`     | interactively examine 40:50       |
|`ie >1000 40-50` | interactively examine the subset of locations 40:50 that are \>1000 |
| `ex rx0 50060`    | examine 50060, RX unit 0                                 |
| `ex rx sbuf[3-6]` | examine `SBUF[3]` to `SBUF[6]` in `RX`                     |
| `ex r4,$`         | examine R4 and where it points                           |
| `de all 0`        | set main memory to 0                                     |
| `de &77>0 0`      | set all addresses whose low order bits are non-zero to 0 |
| `ex -m @memdump.txt 0-7777` | dump memory to file |

Note: to terminate an interactive command, type a bad value
(e.g., `XYZ`) when input is requested.

## Evaluating Instructions

The `EVAL` command evaluates a symbolic instruction and returns the
equivalent numeric value. This is useful for obtaining numeric
arguments for a search command:

```
EVAL <expression>
```

Examples:

On the VAX simulator:

```
sim> eval addl2 r2,r3
0: 005352C0
sim> eval addl2 #ff,6(r0)
0: 00FF8FC0
4: 06A00000
sim> eval 'AB
0: 00004241
```

On the PDP-8:

```
sim> eval tad 60
0: 1060
sim> eval tad 300
tad 300
Can't be parsed as an instruction or data
```

`tad 300` fails, because with an implicit `PC` of 0, location 300
can't be reached with direct addressing.

## Running a Simulated Program

The `RUN` command (abbreviated `RU`) resets all devices, deposits its
argument (if given) in the `PC`, and starts execution. If no argument
is given, execution starts at the current `PC`.

The `GO` command does *not* reset devices, deposits its argument (if
given) in the `PC`, and starts execution. If no argument is given,
execution starts at the current `PC`.

The `CONTINUE` command (abbreviated `CO`) does *not* reset devices and
resumes execution at the current `PC`.

The `STEP` command (abbreviated `S`) resumes execution at the current
`PC` for the number of instructions given by its argument. If no
argument is supplied, one instruction is executed.

The `BOOT` command (abbreviated `B`) resets all devices and bootstraps
the device and unit given by its argument. If no unit is supplied,
unit 0 is bootstrapped. The specified unit must be attached.

Because it initializes all I/O devices, `RUN` is usually not the
right command after a program has started. The `GO` or `CONTINUE`
commands are generally equivalent in hardware to running the CPU
and are the usual way to resume execution after a programmed
halt. If an I/O reset is required before resuming execution, use
`RESET` followed by `GO` instead of `RUN`. If `RUN` is entered a
second time without an explicit preceding `RESET`, a warning is
printed on the simulation console:

```
Resetting all devices... This may not have been your intention.
The GO and CONTINUE commands do not reset devices.
```

Execution then resumes. The warning may be suppressed by adding
the `-Q` switch to the `RUN` command.

### Controlling the Simulation Rate

By default, the simulator runs as fast as possible. On hosts
where ZIMH can adjust scheduling priority, it may lower its
priority while simulated code is running. The simulator may still
consume substantial processing resources on the host system. This
can raise power consumption and operating temperature, and can
drain a laptop battery more quickly.

The `SET THROTTLE` command reduces the effective execution rate
to a specified number of instructions per second or a percentage
of total host computing time:

|                            |                                    |
|----------------------------|------------------------------------|
| `SET THROTTLE xM`          | Set execution rate to x MIPS       |
| `SET THROTTLE xK`          | Set execution rate to x KIPS       |
| `SET THROTTLE x%`          | Limit simulator to x% of host time |
| `SET THROTTLE insts/delay` | Execute ‘insts’ instructions and then sleep for ‘delay’ milliseconds |

Throttling is only available on host systems that implement a
precision real-time delay function.

`xM`, `xK` and `x%` modes require the simulator to execute sufficient
instructions to actually calibrate the desired execution rate relative
to wall clock time. Very short running programs may complete before
calibration completes and therefore before the simulated execution
rate can match the desired rate.

The `SET NOTHROTTLE` command turns off throttling. The `SHOW THROTTLE`
command shows the current settings for throttling.

Some simulators implement a different form of resource management
called idling. Idling suspends simulated execution whenever the
program running on the simulator is doing nothing, and runs the
simulator at full speed when there is work to do. Throttling and
idling are mutually exclusive.

## Stopping the Simulator

Programs run until the simulator detects an error or stop
condition, or until the user stops execution.

### Simulator Detected Stop Conditions

These simulator-detected conditions stop simulation:

- `HALT` instruction. If a `HALT` instruction is decoded, simulation stops.

- Breakpoint. The simulator may support breakpoints (see below).

- I/O error. If an I/O error occurs during simulation of an I/O
  operation, and the device stop-on-I/O-error flag is set, simulation
  usually stops.

- Processor condition. Certain processor conditions can stop
  simulation; these are described with the individual simulators.

### User Specified Stop Conditions

Typing the interrupt character stops simulation. The interrupt
character is defined by the `WRU` (where are you) console option and
is initially set to ASCII code 5 (i.e. CTRL-E, `^E`).

### Breakpoints

A simulator may offer breakpoint capability. A simulator may define
breakpoints of different types, identified by letter (for example, `E`
for execution, `R` for read, `W` for write, etc.). At the moment, most
simulators support only `E` (execution) breakpoints.

Associated with a breakpoint are a count and, optionally, one or more
actions. Each time the breakpoint is taken, the associated count is
decremented. If the count is less than or equal to zero, the
breakpoint occurs; otherwise, it is deferred. When the breakpoint
occurs, the optional actions are automatically executed.

A breakpoint is set by the `BREAK` command:

```
BREAK {-types} {<addr range>{[count]},{addr range...}}{;action;action...}
```

If no type is specified, the simulator-specific default breakpoint
type (usually `E` for execution) is used. If no address range is
specified, the current `PC` is used. As with `EXAMINE` and `DEPOSIT`,
an address range may be a single address, a range of addresses
low-high, or a relative range of address/length.

Examples:

|                          |                                                  |
|--------------------------|--------------------------------------------------|
| `BREAK`                  | set `E` break at current PC                      |
| `BREAK -e 200`           | set `E` break at 200                             |
| `BREAK 2000/2[2]`        | set `E` breaks at 2000,2001 with `count = 2`     |
| `BREAK 100;EX AC;D MQ 0` | set `E` break at 100 with actions `EX AC` and `D MQ 0` |
| `BREAK 100;`             | delete action on break at 100                    |

Currently set breakpoints can be displayed with the `SHOW BREAK` command:

```
SHOW {-types} BREAK {ALL|<addr range>{,<addr range>...}}
```

Locations with breakpoints of the specified type are displayed.

Finally, breakpoints can be cleared by the `NOBREAK` command.

If an action command must contain a semicolon, that action command
should be enclosed in quotes so that the entire action command can be
distinguished from the action separator.

### Execution Time Limits

Execution time limits stop a simulator after a configured amount
of simulated work or elapsed time. They are useful for scripted
tests, automated diagnostics, and other runs that should fail
rather than run forever if the simulated program never reaches an
expected success or failure condition. The `RUNLIMIT` command
sets an execution limit.

```
RUNLIMIT n {CYCLES|MICROSECONDS|SECONDS|MINUTES|HOURS}
NORUNLIMIT
```

Equivalently:

```
SET RUNLIMIT n {CYCLES|MICROSECONDS|SECONDS|MINUTES|HOURS}
SET NORUNLIMIT
```

The run limit state can be examined with:

```
SHOW RUNLIMIT
```

If no units are specified, the default unit is cycles. Once a run
limit has been reached, any subsequent `GO`, `RUN`, `CONTINUE`,
`STEP`, or `BOOT` command causes the simulator to exit. Use
`NORUNLIMIT` to clear a run limit, or set a new `RUNLIMIT` to
replace the old one.

## Halt on Output Data

Breakpoints stop execution at specific simulated addresses. By
contrast, `EXPECT` stops execution, or runs an action command,
when a simulated device prints specified output. This is
especially useful for command files and tests that need to wait
for prompts, diagnostic messages, or other output from the
simulated system.

```
EXPECT {dev:line} {[count]} {HALTAFTER=n,}"<string>" {actioncommand {; actioncommand} …}
NOEXPECT {dev:line}
SHOW EXPECT {dev:line}
```

`<dev:line>` selects a particular device, or a line on a
simulated multiplexer device. If it is omitted, `EXPECT` watches
the simulated system’s console.

`[count]` tells `EXPECT` to wait for the match string to appear
that many times before the rule matches.

`HALTAFTER=n` delays the halt or action for `n` simulated
instructions after the output matches. With the `-t` switch, `n`
is interpreted as microseconds instead. If `HALTAFTER` is
omitted, the default is to stop immediately when the rule matches.

The string argument must be delimited by quote characters. Quotes may
be either single or double but the opening and closing quote
characters must match. Data in the string may contain escaped
character strings.

When `EXPECT` rules are defined, ZIMH evaluates them against each
character as it is written to the device. Matching is
character-oriented, not line-oriented. To match a complete line,
include the simulated system’s line ending sequence in the rule,
for example `"\r\n"`.

Once output has matched an `EXPECT` rule, that output cannot
match another rule. Output produced before an `EXPECT` rule is
defined is not matched against that rule.

The `NOEXPECT` command removes one or more `EXPECT` rules for
the console or a specific multiplexer line.

The `SHOW EXPECT` command displays pending `EXPECT` rules and
settings for the console or a specific multiplexer line.

If an action command must contain a semicolon, that action command
should be enclosed in quotes so that the entire action command can be
distinguished from the action separator.

### Switches

Switches can change how `EXPECT` rules behave.

#### `-p`

`EXPECT` rules are one-time rules by default. A rule is removed
automatically when it matches unless it is defined with the `-p`
switch, which makes it persistent.

#### `-c`

With the `-c` switch, an `EXPECT` rule clears all pending
`EXPECT` rules on the current device when it matches output from
that device.

#### `-r`

With the `-r` switch, the match string is interpreted as a
regular expression applied to the output stream. The regular
expression may contain parenthesized subgroups.

The regular expression syntax is that of the PCRE2 library, which is
nearly the same as Perl’s regular expressions. See
[the PCRE2 syntax manual](https://pcre2project.github.io/pcre2/doc/pcre2pattern/)
for details.

#### `-i`

With the `-i` switch, matching for a regular expression `EXPECT`
rule is case-insensitive. The `-i` switch is valid only with
regular expression rules (`-r`).

#### Escaping String Data

The following character escapes are explicitly supported when expect
rules are defined *without* using regular expression match patterns:

| escape | action                                                        |
|--------|---------------------------------------------------------------|
| `\r`   | Expect the ASCII Carriage Return character (Decimal value 13) |
| `\n`   | Expect the ASCII Linefeed character (Decimal value 10)        |
| `\f`   | Expect the ASCII Formfeed character (Decimal value 12)        |
| `\t`   | Expect the ASCII Horizontal Tab character (Decimal value 9)   |
| `\v`   | Expect the ASCII Vertical Tab character (Decimal value 11)    |
| `\b`   | Expect the ASCII Backspace character (Decimal value 8)        |
| `\\`   | Expect the ASCII Backslash character (Decimal value 92)       |
| `\'`   | Expect the ASCII Single Quote character (Decimal value 39)    |
| `\"`   | Expect the ASCII Double Quote character (Decimal value 34)    |
| `\?`   | Expect the ASCII Question Mark character (Decimal value 63)   |
| `\e`   | Expect the ASCII Escape character (Decimal value 27)          |

as well as octal character values of the form:

|            |                                      |
|------------|--------------------------------------|
| `\n{n{n}}` | where each n is an octal digit (0-7) |

and hex character values of the form:

|          |                                         |
|----------|-----------------------------------------|
| `\xh{h}` | where each h is a hex digit (0-9A-Fa-f) |

### Determining which output matched

When an `EXPECT` rule matches data in the output stream, the rule
that matched is recorded in the variable `_EXPECT_MATCH_PATTERN`.

If the `EXPECT` rule was a regular expression rule, then
`_EXPECT_MATCH_GROUP_0` is set to the whole string that matched.
If the match pattern had parenthesized subgroups, the variables
`_EXPECT_MATCH_GROUP_1` through `_EXPECT_MATCH_GROUP_n` are set
to the corresponding subgroup matches.

## Injecting Input Data

The `SEND` command queues input for later delivery to the
simulated console. `AFTER` controls when delivery begins, and
`DELAY` controls the delay between characters.

```
SEND {AFTER=n,}{DELAY=n,}"<string>"
NOSEND
SHOW SEND
```

The string argument must be delimited by quote characters. Quotes may
be either single or double but the opening and closing quote
characters must match. Data in the string may contain escaped
character strings.

`SEND` can also queue input for a line on a simulated serial
device.

```
SEND <dev>:line {AFTER=n,}{DELAY=n,}"<string>"
NOSEND <dev>:line
SHOW SEND <dev>:line
```

The `NOSEND` command removes undelivered input pending for the
console or a specific multiplexer line.

The `SHOW SEND` command displays pending `SEND` activity for the
console or a specific multiplexer line.

### Delay

`DELAY` specifies the minimum instruction delay between delivered
characters. The delay value persists across `SEND` commands to
the same target, either the console or a serial device. The delay
parameter can be set by itself with:

```
SEND {<dev>:line} {DELAY=n}
```

The default value of the delay parameter is 1000.

### After

`AFTER` specifies the minimum number of instructions that must
execute before the first queued character is delivered. The after
value persists across `SEND` commands to the same target, either
the console or a serial device. The after parameter value can be
set by itself with:

```
SEND {<dev>:line} {AFTER=n}
```

If the after parameter isn’t explicitly set, it defaults to the value
of the delay parameter.

### Escaping String Data

The following character escapes are explicitly supported:

| escape | action                                                       |
|--------|--------------------------------------------------------------|
| `\r`   | Sends the ASCII Carriage Return character (Decimal value 13) |
| `\n`   | Sends the ASCII Linefeed character (Decimal value 10)        |
| `\f`   | Sends the ASCII Formfeed character (Decimal value 12)        |
| `\t`   | Sends the ASCII Horizontal Tab character (Decimal value 9)   |
| `\v`   | Sends the ASCII Vertical Tab character (Decimal value 11)    |
| `\b`   | Sends the ASCII Backspace character (Decimal value 8)        |
| `\\`   | Sends the ASCII Backslash character (Decimal value 92)       |
| `\'`   | Sends the ASCII Single Quote character (Decimal value 39)    |
| `\"`   | Sends the ASCII Double Quote character (Decimal value 34)    |
| `\?`   | Sends the ASCII Question Mark character (Decimal value 63)   |
| `\e`   | Sends the ASCII Escape character (Decimal value 27)          |

as well as octal character values of the form:

|            |                                      |
|------------|--------------------------------------|
| `\n{n{n}}` | where each n is an octal digit (0-7) |

and hex character values of the form:

|          |                                         |
|----------|-----------------------------------------|
| `\xh{h}` | where each h is a hex digit (0-9A-Fa-f) |

## Setting Device Parameters

The `SET` command (abbreviated `SE`) changes one or more device
parameters:

```
SET <device> <parameter>{=<value},{<parameter>{=<value>},...}
```

It can also change one or more unit parameters:

```
SET <unit> <parameter>{=<value>},{<parameter>{=<value>},...}
```

Most parameters are simulator-specific and device-specific. Disk
drives, for example, can usually be set `WRITEENABLED` or write
`LOCKED`; if a device supports multiple drive types, `SET` can
select the drive type.

All devices recognize the following parameters:

|       |                          |
|-------|--------------------------|
| `OCT` | sets the data radix = 8  |
| `DEC` | sets the data radix = 10 |
| `HEX` | sets the data radix = 16 |

## Displaying Parameters and Status

The `SHOW` command (abbreviated `SH`) displays one or more
device parameters:

```
SHOW {<modifiers>} <device> <parameter>{=<value>},
       {<parameter>{=<value>},...}
```

It can also display one or more unit parameters:

```
SHOW {<modifiers>} <unit> <parameter>{=<value>},
       {<parameter>{=<value>},...}
```

There are two kinds of modifiers: switches and output file. Switches
have been described previously. The output file modifier redirects
command output to a file instead of the console. An output file
modifier consists of `@` followed by a valid file name.

All devices implement parameters `RADIX` (the display radix),
`MODIFIERS` (list of valid modifiers), and `NAMES` (logical
name). Other device and unit parameters are implementation-specific.

`SHOW` is also used to display global simulation state:

|            |                                      |
|------------|--------------------------------------|
| `SHOW CONFIGURATION` | shows the simulator configuration and the status of all devices and units |
| `SHOW DEVICES`   | shows the simulator configuration                      |
| `SHOW FEATURES`  | shows the simulator configuration with descriptions    |
| `SHOW MODIFIERS` | shows all available modifiers                          |
| `SHOW NAMES`     | shows all logical names                                |
| `SHOW QUEUE`     | shows the simulator event queue                        |
| `SHOW TIME`      | shows the elapsed time since the last `RUN`            |
| `SHOW VERSION`   | shows the simulator version and options                |
| `SHOW <device>`  | shows the status of the named device                   |
| `SHOW <unit>`    | shows the status of the named unit                     |
| `SHOW THROTTLE`  | shows the current throttling mode                      |
| `SHOW SHOW`      | shows the show options for all devices                 |
| `SHOW ETHERNET`  | shows the status/availability of host Ethernet devices |
| `SHOW SERIAL`    | shows the status of host serial ports                  |
| `SHOW MULTIPLEXER` | shows the status of attached multiplexer devices     |
| `SHOW DEFAULT` | shows the current working directory      |
| `SHOW DEBUG`   | shows the debug state of the simulator   |
| `SHOW LOG`     | shows the logging state of the simulator |

The `SHOW DEVICES`, `SHOW CONFIGURATION`, and `SHOW FEATURES`
commands normally display all devices in the simulator. With the
`-E` switch, they display only enabled devices, for example
`SHOW -E DEVICES`.

`SHOW QUEUE` and `SHOW TIME` display time in simulator-specific units;
typically, one time unit represents one instruction execution.

## Altering the Simulated Configuration

In most simulators, the `SET <device> DISABLED` command removes the
specified device from the configuration. A `DISABLED` device is
invisible to running programs. The device can still be `RESET`, but it
cannot be `ATTACH`ed, `DETACH`ed, or `BOOT`ed. `SET <device> ENABLED`
restores a disabled device to the configuration.

The normal `SET <device> DISABLED` command fails if any unit is
attached or has pending activity. `SET -F <device> DISABLED`
forces the disable by detaching attached units and canceling
pending unit activity. If a forced disable fails while detaching
units, some units may already have been detached or canceled.

Most multi-unit devices allow units to be enabled or disabled:

```
SET <unit> ENABLED
SET <unit> DISABLED
```

When a unit is disabled, `SHOW DEVICE` does not display it.

The standard device names can be supplemented with logical
names. Logical names must be unique within a simulator (that is, they
cannot be the same as an existing device name). To assign a logical
name to a device:

|                              |                           |
|------------------------------|---------------------------|
| `ASSIGN <device> <log-name>` | assign log-name to device |


To remove a logical name:

|                     |                     |
|---------------------|---------------------|
| `DEASSIGN <device>` | remove logical name |

To show the current logical name assignment:

|                       |                           |
|-----------------------|---------------------------|
| `SHOW <device> NAMES` | show logical name, if any |

To show all logical names:

```
SHOW NAMES
```

## Console Options

The `SET CONSOLE` command controls how ZIMH connects the
simulated machine’s console terminal. This is the console seen by
software running inside the simulator. It is distinct from the
`sim>` command prompt, although both normally use the same local
terminal when no Telnet or serial console is configured.

By default, the simulated console terminal uses the process’s
controlling terminal. It can instead listen on a Telnet port. A
Telnet client then provides the console terminal session used by
the simulated machine. The client’s terminal emulation, such as
VT100 emulation, determines how screen-oriented software appears.

|                             |                                               |
|-----------------------------|-----------------------------------------------|
| `SET CONSOLE TELNET=<port>` | listen for console Telnet sessions on port |
| `SET CONSOLE NOTELNET`      | disable console Telnet                        |

By default, connections to the Telnet port are unrestricted. The
port specifier may include access rules with `;ACCEPT=rule-detail`
or `;REJECT=rule-detail`. A rule can name an IP address, a host
name, or a network block in CIDR form. Rules are processed in
order. If no rule accepts the connection, the connection is
rejected.

An unbuffered Telnet console requires an active Telnet
connection while simulated code is running. When a command that
starts execution, such as `RUN`, `GO`, `CONTINUE`, `STEP`, or
`BOOT`, is entered with no Telnet client connected, ZIMH waits up
to 30 seconds for a connection. If no client connects in that
time, the command times out and simulated execution does not
start. If the connection is lost during console input or output,
the simulator stops with a connection-lost condition.

Console buffering lets the simulator continue running when no
Telnet client is connected. Console output is saved in a circular
buffer, with older output discarded when the buffer fills. When a
Telnet client connects, ZIMH sends the buffered output to the
session and then uses the connection normally. Enable console
buffering with:

|                             |                                               |
|-----------------------------|-----------------------------------------------|
| `SET CONSOLE TELNET=BUFFERED{=bufsiz} ` | enable console buffering and optionally set the buffer size to ‘bufsiz’. The default buffer size is 32768. |
| `SET CONSOLE TELNET=NOBUFFER` | disable console buffering |

Output sent to the console Telnet session can also be logged to a
file:

|                                      |                                 |
|--------------------------------------|---------------------------------|
| `SET CONSOLE TELNET=LOG=<filename> ` | log console port output to file |
| `SET CONSOLE TELNET=NOLOG `          | disable logging                 |

The console provides a limited key remapping capability:

|                           |                                                  |
|---------------------------|--------------------------------------------------|
| `SET CONSOLE WRU=<value>` | interpret ASCII code value as `WRU`                |
| `SET CONSOLE BRK=<value>` | interpret ASCII code value as `BREAK` (0 disables) |
| `SET CONSOLE DEL=<value>` | interpret ASCII code value as `DELETE`            |
| `SET CONSOLE PCHAR=<value>` | bit mask of printable characters in range `[31,0]` |

A simulated console terminal can be connected to a serial port on
the host system.


|                                 |                                           |
|---------------------------------|-------------------------------------------|
| `SET CONSOLE SERIAL=ser0`       | connect console to serial port `ser0`     |
| `SET CONSOLE SERIAL=COM1`       | connect console to serial port `COM1`     |
| `SET CONSOLE SERIAL=/dev/ttyS0` | connect console to serial port `/dev/ttyS0` |

The available serial ports on the host system can be displayed
with:

|               |                                        |
|---------------|----------------------------------------|
| `SHOW SERIAL` | display available serial ports on host |

Serial port speed, character size, parity, and stop bits can be
specified by appending them to the serial port name:

```
SET CONSOLE SERIAL=ser0;2400-8N1
```

This connects at 2400 baud with 8-bit characters, no parity, and
1 stop bit. The default serial speed, character size, parity, and
stop bits are `9600-8N1`.


The `SHOW CONSOLE` command displays the current state of console
options. Values are hexadecimal on hexadecimal CPUs and octal on
all others.

|                       |                                 |
|-----------------------|---------------------------------|
| `SHOW CONSOLE`        | show all console options        |
| `SHOW CONSOLE TELNET` | show console Telnet state       |
| `SHOW CONSOLE WRU`    | show value assigned to `WRU`    |
| `SHOW CONSOLE BRK`    | show value assigned to `BREAK`  |
| `SHOW CONSOLE DEL`    | show value assigned to `DELETE` |
| `SHOW CONSOLE PCHAR`  | show value assigned to `PCHAR`  |

Both `SET CONSOLE` and `SHOW CONSOLE` accept multiple parameters,
separated by commas, e.g.,

|               |                                        |
|---------------|----------------------------------------|
|`SET CONSOLE WRU=5,DEL=177`| set code values for `WRU` and `DEL`|

## Remote Simulator Command Interface

The simulator command interface prompts with `sim>` and accepts
commands such as `RUN`, `ATTACH`, `SET`, and `SHOW`. It is
distinct from the simulated machine’s console terminal, although
both normally share the same local terminal when no Telnet or
serial console is configured.

During simulator execution, it can be useful to enter simulator
commands to inspect or adjust the simulator’s configuration or
operation. This can be done from the session that started the
simulator, but that session may be unavailable, inconvenient to
access, or running in the background. The `SET REMOTE` commands
enable a Telnet-accessible remote simulator command interface:

|               |                                        |
|---------------|----------------------------------------|
|`SET REMOTE TELNET=<port>` | listen for remote command sessions on port |
|`SET REMOTE BUFFERSIZE=bufsize`| specify remote command output buffer size |
| `SET REMOTE NOTELNET` | disable remote command Telnet |
|`SET REMOTE CONNECTIONS=n`| specify the number of concurrent remote command sessions |
|`SET REMOTE TIMEOUT=secs` | specify the remote command idle timeout |

By default, connections to the remote command port are
unrestricted. The port specifier may include access rules with
`;ACCEPT=rule-detail` or `;REJECT=rule-detail`. A rule can name
an IP address, a host name, or a network block in CIDR form.
Rules are processed in order. If no rule accepts the connection,
the connection is rejected.

The remote command interface configuration can be viewed with:

|               |                                      |
|---------------|--------------------------------------|
| `SHOW REMOTE` | display remote command interface configuration |

The remote simulator command interface has two ordinary command
modes and one special master mode:

- **Single Command Mode**: In single command mode, you enter one
  simulator command at a time while simulated execution continues.
  Only commands that can safely run without stopping simulated
  execution are available.

- **Multiple Command Mode**: In multiple command mode, you first
  enter the `WRU` character (usually `^E`). This suspends
  simulated execution and makes a larger command set available.
  When you are done, enter `CONTINUE` to resume simulated
  execution. If you do not enter a complete command before the
  timeout specified by `SET REMOTE TIMEOUT=seconds`, ZIMH
  automatically processes a `CONTINUE` command and simulated
  execution resumes.

- **Remote Master Mode**: Remote Master Mode is enabled with
  `SET REMOTE MASTER`. It gives the primary remote command
  session a broader execution-control command set, including
  commands such as `RUN`, `GO`, `BOOT`, `BREAK`, `NOBREAK`, and
  `EXIT`, which ordinary remote command sessions do not permit.

A subset of normal simulator commands is available in remote
command sessions.

The Single Command Mode commands are: `ATTACH`, `DETACH`, `PWD`,
`EXAMINE`, `EVALUATE`, `REPEAT`, `COLLECT`, `SAMPLEOUT`, `EXECUTE`,
`DIR`, `LS`, `ECHO`, `ECHOF`, `SHOW`, `DEBUG`, `NODEBUG`, and
`HELP`.

The Multiple Command Mode commands are: `EXAMINE`, `DEPOSIT`,
`EVALUATE`, `ATTACH`, `DETACH`, `ASSIGN`, `DEASSIGN`, `CONTINUE`,
`REPEAT`, `COLLECT`, `SAMPLEOUT`, `PWD`, `SAVE`, `DIR`, `LS`,
`ECHO`, `ECHOF`, `SET`, `SHOW`, and `HELP`.

A remote command session closes when an `EOF` character is entered
(i.e. `^D` or `^Z`).

### Remote Master Mode

Remote Master Mode allows a remote TCP session to drive simulator
execution more directly than an ordinary remote command session can.
This mode is potentially useful for applications such as a simulated
CPU front panel interface or remote debugging with a GDB “Serial
Stub”.

Remote Master Mode is enabled with:

|                     |                   |
|---------------------|-------------------|
| `SET REMOTE MASTER` | enter Remote Master Mode |

Operation in Remote Master Mode requires a Telnet connection to
the remote simulator command interface, configured with
`SET REMOTE TELNET=port`. The simulated machine’s console
terminal must also be configured through Telnet or a host serial
port. If the simulated console terminal uses Telnet, ZIMH waits
for both Telnet connections before entering Remote Master Mode.
The primary remote command session becomes the master session. In
ordinary use, this is the first remote command session that
connected and remains connected. If no primary remote command
session is connected when Remote Master Mode is enabled, ZIMH
waits for one. If the master session disconnects, Remote Master
Mode ends.

A Remote Master Mode session can leave master mode with:

|                       |                   |
|-----------------------|-------------------|
| `SET REMOTE NOMASTER` | leave Remote Master Mode |

The Remote Master Mode commands are: `EXAMINE`, `DEPOSIT`,
`EVALUATE`, `ATTACH`, `DETACH`, `ASSIGN`, `DEASSIGN`, `CONTINUE`,
`STEP`, `REPEAT`, `COLLECT`, `SAMPLEOUT`, `EXECUTE`, `PWD`, `SAVE`,
`CD`, `DIR`, `LS`, `ECHO`, `ECHOF`, `SET`, `SHOW`, `HELP`, `EXIT`,
`QUIT`, `RUN`, `GO`, `BOOT`, `BREAK`, `NOBREAK`, `EXPECT`,
`NOEXPECT`, `DEBUG`, `NODEBUG`, and `SEND`.

## Executing Command Files

The simulator can execute command files with the `DO` command:

|                                |                          |
|--------------------------------|--------------------------|
| `DO <filename> {arguments...}` | execute commands in file |

The `DO` command allows command files to contain substitutable
arguments. The string `%n`, where `n` is between 1 and 9, is replaced
with argument n from the `DO` command line. The string `%0` is
replaced with `<filename>`. The sequences `\%` and `\\` are replaced
with the literal characters `%` and `\` respectively. Arguments with
spaces can be enclosed in matching single or double quotation marks.

If the switch `-v` is specified, the commands in the file are echoed
before they are executed.

If the switch `-e` is specified, command processing (including nested
command invocations) will be aborted if a command error is
encountered. (A simulation stop never aborts processing; use `ASSERT`
to catch unexpected stops.) Without the switch, all errors except
`ASSERT` failures will be ignored, and command processing will
continue.

`DO` commands may be nested up to ten invocations deep.

Several commands are particularly useful within command files. While
they may be executed interactively, they have only limited
functionality when so used.

### Default Command File Executed on Simulator Startup

When a simulator starts, ZIMH looks for startup command files in
the following order:

1. If a file named `simh.ini` is located in your `HOME` directory,
   it is executed.
2. If no `simh.ini` file is found in your `HOME` directory, ZIMH
   looks for `simh.ini` in the current working directory.
3. If the simulator was invoked with command-line arguments, the
   arguments are treated as a command file name and arguments for
   that command file.
4. If the simulator was invoked without command-line arguments,
   ZIMH looks in the current working directory for a command file
   whose name is the simulator binary name with `.ini` appended.

Up to two separate command files may be executed during startup.
The `simh.ini` file provides shared local preferences for all
simulators used on the system. A simulator-specific `.ini` file
can then provide setup commands for that simulator.

### Change Command Execution Flow

Commands in a command file execute in sequence until either an error
trap occurs (when a command completes with an error status), or when
an explicit request is made to start command execution elsewhere with
the `GOTO` command:

```
GOTO <label>
```

Labels are lines in a command file whose first non-whitespace
character is "`:`". The target of a `GOTO` is the first matching
label encountered in the current `DO` command file. Since labels
do nothing except mark `GOTO` targets, they can also be used as
comments in `DO` command files.

### Subroutine Calls

Control can be transferred to a labeled subroutine with:

```
CALL <label>
EXIT

:label
ECHO subroutine called
RETURN
```

### Pausing Command Execution

A simulator command file may wait for a specific period of time with:

```
SLEEP <number>[SUFFIX]...
```

Pause for *number* seconds. *SUFFIX* may be `s` for seconds (the
default), `m` for minutes, `h` for hours, or `d` for days.
*NUMBER* may be an arbitrary floating-point number.

Given two or more arguments, pause for the amount of time specified by
the sum of their values.

Note: A `SLEEP` command is interruptible with `SIGINT` (`^C`).

### Displaying Arbitrary Text

The `ECHO` and `ECHOF` commands are useful ways of annotating command
files.

#### `ECHO` command

`ECHO` prints its argument to command output and the log,
followed by a newline.


|                 |                          |
|-----------------|--------------------------|
| `ECHO <string>` | output string |

If there is no argument, `ECHO` prints a blank line. This may be
used to provide spacing in command output or the log.

#### `ECHOF` command

`ECHOF` prints its argument to command output and the log,
followed by a newline.

|                                         |                          |
|-----------------------------------------|--------------------------|
| `ECHOF {-n} "string"|<string>`          | output string            |
| `ECHOF {-n} dev:line "string"|<string>` | output to specified line |

If there is no argument, `ECHOF` prints a blank line. The `-n`
switch suppresses the output of a newline character. If the
string to be output is surrounded by quotes, the quoted string is
interpreted as described in
[Escaping String Data](#escaping-string-data-1) before it is
printed. The surrounding quotes are not printed.

A command alias can be used to replace the `ECHO` command with the
`ECHOF` command as described in
[Command Aliases](#command-aliases).

### Testing Simulator State

The `ASSERT` command tests a simulator state condition expression
and halts command file execution if the condition is false:

```
ASSERT <condition-expression>
```

If the condition is false, an "Assertion failed" message is
printed, and any running command file is aborted. Otherwise, the
command has no effect.

The `IF` command tests a simulator state condition expression and
executes one or more commands if the condition is true:

```
IF <condition-expression> <action>{; <action>…}
{ELSE <action>{; <action>…}}
```

If the condition is true, the action command or commands are
executed. Otherwise, the command has no effect. An optional
`ELSE` command immediately following an `IF` command runs its
command arguments if the `IF` condition was not satisfied.

If an action command must contain a semicolon, that action command
should be enclosed in quotes so that the entire action command can be
distinguished from the action separator.

#### Condition Expressions

##### Internal Simulator Variable Expressions

```
{NOT} {<dev>} <reg>{<logical-op><value>}<conditional-op><value>
```

If `<dev>` is not specified, `CPU` is assumed. `<reg>` is a register
(scalar or subscripted) belonging to the indicated device. The
`<conditional-op>` and optional `<logical-op>` are the same as those
used for "search specifiers" by the `EXAMINE` and `DEPOSIT` commands
(see above). The `<value>`s are expressed in the radix specified for
`<reg>`, not in the radix for the device.

If the `<logical-op>` and `<value>` are specified, the target register
value is first altered as indicated. The result is then compared to
the `<value>` via the `<conditional-op>`. If the `NOT` unary operator
precedes the expression, the resulting value is inverted.

##### C-Style Expressions

Comparisons can optionally be done with complete C-style
computational expressions. These expressions use the C operators
shown in the table below and may reference constants, environment
variables, and simulator registers. C-style expression evaluation
is initiated by enclosing the expression in parentheses.

|      |                       |
|------|-----------------------|
| `(`  | Open Parenthesis      |
| `)`  | Close Parenthesis     |
| `-`  | Subtraction           |
| `+`  | Addition              |
| `*`  | Multiplication        |
| `/`  | Division              |
| `%`  | Modulus               |
| `&&` | Logical AND           |
| `\|\|` | Logical OR          |
| `&`  | Bitwise AND           |
| `\|` | Bitwise Inclusive OR  |
| `^`  | Bitwise Exclusive OR  |
| `>>` | Bitwise Right Shift   |
| `<<` | Bitwise Left Shift    |
| `==` | Equality              |
| `!=` | Inequality            |
| `<=` | Less than or Equal    |
| `<`  | Less than             |
| `>=` | Greater than or Equal |
| `>`  | Greater than          |
| `!`  | Logical Negation      |
| `~`  | Bitwise Complement    |

##### String Comparison Expressions

String values can be compared with:

```
{-i} {NOT} "<string1>"|EnvVarName1 <compare-op> "<string2>"|EnvVarName2
```

The `-i` switch, if present, causes comparisons to be case
insensitive. The `-w` switch, if present, causes comparisons to allow
arbitrary runs of whitespace to be equivalent to a single space. The
`-i` and `-w` switches may be combined. `<string1>` and `<string2>`
are quoted string values that may have environment variables
substituted as desired. Either string may be an environment
variable name whose expanded value is used in place of the
explicitly quoted string. `<compare-op>` may be one of:

| op    | meaning               |
|-------|-----------------------|
| `==`  | equal                 |
| `EQU` | equal                 |
| `!=`  | not equal             |
| `NEQ` | not equal             |
| `<`   | less than             |
| `LSS` | less than             |
| `<=`  | less than or equal    |
| `LEQ` | less than or equal    |
| `>`   | greater than          |
| `GTR` | greater than          |
| `>=`  | greater than or equal |
| `GEQ` | greater than or equal |

Comparisons are generic. If both `string1` and `string2` are
composed entirely of numeric digits, the strings are converted to
numbers and a numeric comparison is performed. For example,
`"+1" EQU "1"` is true.

##### File Existence Test

File existence can be determined with:

```
{NOT} EXIST "<filespec>"
{NOT} EXIST <filespec>
```

Specifies a true condition if the file exists, or false if
preceded by `NOT`.

##### File Comparison Test

Files can have their contents compared with:

```
-F{W} {NOT} "<filespec1>" == "<filespec2>"
```

Specifies a true condition if the indicated files have the same
contents, or false if preceded by `NOT`. If the `-W` switch is
present, arbitrary runs of whitespace are treated as a single
space during file content comparison.

#### Example Test of Simulator State

A command file might be used to bootstrap an operating system that
halts after the initial load from disk. The `ASSERT` command is then
used to confirm that the load completed successfully by examining the
CPU's `A` register for the expected value:

```
; OS bootstrap command file
;
ATTACH DS0 os.disk
BOOT DS
; A register contains error code; 0 = good boot
ASSERT A=0
ATTACH MT0 sys.tape
ATTACH MT1 user.tape
RUN
```

In the example, if the `A` register is not 0, the `ASSERT A=0`
command is echoed and the command file is aborted with an
"Assertion failed" message. Otherwise, the command file continues
bringing up the operating system.

Alternatively, the `IF` command could be used to solve the same problem:

```
; OS bootstrap command file
;
ATTACH DS0 os.disk
BOOT DS
; A register contains error code; 0 = good boot
IF NOT A=0 ECHO Assertion Failed; EXIT AFAIL
ATTACH MT0 sys.tape
ATTACH MT1 user.tape
RUN
```

The `IF` command can also use a more complex C-style expression
to solve the same problem:

```
; OS bootstrap command file
;
ATTACH DS0 os.disk
BOOT DS
; A register contains error code; 0 = good boot
IF NOT (((A*256)>>7)==0) ECHO Assertion Failed; EXIT AFAIL
ATTACH MT0 sys.tape
ATTACH MT1 user.tape
RUN
```

### Trapping on Command Completion Conditions

Error traps can be taken when a command returns an error status.
The `ON` command specifies actions to perform for particular
status returns.

#### Enabling Error Traps

Error trapping is enabled with:

|          |                    |
|----------|--------------------|
| `set on` | enable error traps |

Error trapping is initially disabled.

#### Disabling Error Traps

Error trapping is disabled with:

|           |                     |
|-----------|---------------------|
| `set noon` | disable error traps |

#### ON Command

To set the action(s) to take when a specific error status is returned
by a command in the currently running do command file:

```
on <statusvalue> commandtoprocess{; additionalcommandtoprocess}
```

To clear the action or actions for a specific error status:

```
on <statusvalue>
```

To set the default action or actions to take when any otherwise
unspecified error status is returned by a command in the
currently running `DO` command file:

```
on error commandtoprocess{; additionalcommandtoprocess}
```

To clear the default action or actions for otherwise unspecified
error status returns:

```
on error
```

##### Parameters

Error traps can be taken for any command that returns a status
other than `SCPE_STEP`, `SCPE_OK`, and `SCPE_EXIT`.

`ON` traps can specify any of these status values:

```
NXM, UNATT, IOERR, CSUM, FMT, NOATT, OPENERR, MEM, ARG,
STEP, UNK, RO, INCOMP, STOP, TTIERR, TTOERR, EOF, REL,
NOPARAM, ALATT, TIMER, SIGERR, TTYERR, SUB, NOFNC, UDIS,
NORO, INVSW, MISVAL, 2FARG, 2MARG, NXDEV, NXUN, NXREG,
NXPAR, NEST, IERR, MTRLNT, LOST, TTMO, STALL, AFAIL,
NOTATT, AMBREG
```

These values can be indicated by name or by their internal numeric
value (not recommended).

###### CONTROL-C Trapping

A special `ON` trap describes actions to take when `CONTROL_C`
(also known as `SIGINT`) occurs during execution of simulator
commands or command files.

|                         |                                |
|-------------------------|--------------------------------|
| `on CONTROL_C <action>` | perform action(s) after CTRL+C |
| `on CONTROL_C`          | restore default CTRL+C action  |

The default `ON CONTROL_C` handler exits nested `DO` command
files and returns to the `sim>` prompt.

Note 1: When a simulator is executing instructions, entering
CTRL+C delivers the CTRL+C character to the simulated machine as
input. Stop simulated instruction execution by entering the `WRU`
character, usually CTRL+E (`^E`). Once instruction execution has
stopped, CTRL+C can be entered and potentially acted on by the
`ON CONTROL_C` trap handler.

Note 2: The `ON CONTROL_C` trapping is not affected by the `SET ON` and
`SET NOON` commands.

### Command Arguments

Token `%0` expands to the command file name.

Token `%n`, where `n` is a single digit, expands to the nth
argument.

Token `%*` expands to the complete argument set (`%1` to `%9`).

The input sequence `%%` represents a literal `%`. All other character
combinations are rendered literally.

Omitted parameters result in null-string substitutions.

Tokens preceded and followed by `%` characters expand using the
first matching value from built-in variables, then variables set
by simulator commands, then host environment variables. Built-in
variables are listed below.

#### File Path Argument Parsing

Command arguments and environment variables can be parsed as file paths
with `%~...%` substitutions.  In the examples below, `I` can be a
numeric `DO` argument such as `1`, or the name of an environment
variable.

| token     | interpretation                                           |
|-----------|----------------------------------------------------------|
| `%~I%`    | value of `%I%` with surrounding quotes removed           |
| `%~fI%`   | value of `%I%` expanded to a full path                   |
| `%~pI%`   | path portion only                                        |
| `%~nI%`   | file name without extension                              |
| `%~xI%`   | file extension only                                      |
| `%~tI%`   | file timestamp                                           |
| `%~zI%`   | file size                                                |
| `%~pnI%`  | path and file name without extension                     |
| `%~nxI%`  | file name and extension                                  |

The modifier letters can be combined and are appended in the order
given.  For example, `%~nx1%` expands the first `DO` argument to a file
name and extension, while `%~ztnxFILE%` expands environment variable
`FILE` to file size, timestamp, file name, and extension.

#### `DO` command argument manipulation

The `SHIFT` command shifts the `%1` through `%9` arguments one
position to the left.

#### Built-In Variables

| name           | interpretation                              |
|----------------|---------------------------------------------|
| `%DATE%`       | `yyyy-mm-dd`                                |
| `%TIME%`       | `hh:mm:ss`                                  |
| `%DATETIME%`   | `yyyy-mm-ddThh:mm:ss`                       |
| `%LDATE%`      | `mm/dd/yy` (Locale Formatted)               |
| `%LTIME%`      | `hh:mm:ss am/pm` (Locale Formatted)           |
| `%CTIME%`      | `Www Mmm dd hh:mm:ss yyyy` (Locale Formatted) |
| `%UTIME%`      | Unix time in seconds since January 1, 1970 UTC |
| `%DATE_YYYY%`  | `yyyy` (0000-9999)                            |
| `%DATE_YY%`    | `yy` (00-99)                                  |
| `%DATE_MM%`    | `mm` (01-12)                                  |
| `%DATE_MMM%`   | `mmm` (JAN-DEC)                               |
| `%DATE_MONTH%` | `month` (January-December)                    |
| `%DATE_DD%`    | `dd` (01-31)                                  |
| `%DATE_WW%`    | `ww` (01-53) ISO 8601 week number             |
| `%DATE_WYYYY%` | `yyyy` (0000-9999) ISO 8601 week year number  |
| `%DATE_D%`     | `d` (1-7) ISO 8601 day of week                |
| `%DATE_JJJ%`   | `jjj` (001-366) day of year                   |
|`%DATE_19XX_YY%`| `yy` (A year prior to 2000 with the same calendar days as the current year)|
|`%DATE_19XX_YYYY%`| `yyyy` (A year prior to 2000 with the same calendar days as the current year)|
| `%TIME_HH%`     | `hh` (00-23)                                               |
| `%TIME_MM%`     | `mm` (00-59)                                               |
| `%TIME_SS%`     | `ss` (00-59)                                               |
| `%TIME_MSEC%`   | `msec` (000-999)                                           |
| `%STATUS%`      | Status value from the last command executed              |
| `%TSTATUS%`     | The text form of the last status value                   |
| `%SIM_VERIFY%`  | The Verify/Verbose mode of the current `DO` command file |
| `%SIM_VERBOSE%` | The Verify/Verbose mode of the current `DO` command file |
| `%SIM_QUIET%`   | The Quiet mode of the current `DO` command file          |
|`%SIM_MESSAGE%`| The message display status of the current `DO` command file|
| `%SIM_NAME%`     | The name of the computer that is being simulated. |
| `%SIM_BIN_NAME%` | The name of the simulator program binary          |
| `%SIM_BIN_PATH%` | The full path of the simulator program binary     |
| `%SIM_OSTYPE%`   | The host operating system running the simulator   |
| `%SIM_NULL_DEVICE%` | The host null-device path                     |
| `%SIM_TMPDIR%`   | The host temporary-file directory path            |
| `%SIM_RUNLIMIT%` | The active `RUNLIMIT` numeric value, if any       |
| `%SIM_RUNLIMIT_UNITS%` | The units of the active `RUNLIMIT`, if any |

#### Variables Set by Commands

Some commands set additional variables for later command substitution.

- `_FILE_COMPARE_DIFF_OFFSET`
  When a `FILE COMPARE` operation finds a difference, this variable is
  set to the zero-based file offset of the first differing byte.

#### Environment Variables

Environment variables can be explicitly defined and used later
for command substitution:

```
SET ENV variablename=value
```

##### Gathering User Input Into an Environment Variable

Prompt for user input with:

```
SET ENV -p "Prompt String" name=<default>
```

The `-p` switch displays the indicated prompt string and saves
the supplied input in the environment variable `name`. If no
input is provided, the value specified as `default` is used.

##### Arithmetic Expressions

Store the result of an arithmetic expression in a variable with:

```
SET ENVIRONMENT -A name=<expression>
```

The expression can contain any of these C language operators:

|      |                       |
|------|-----------------------|
| `(`  | Open Parenthesis      |
| `)`  | Close Parenthesis     |
| `-`  | Subtraction           |
| `+`  | Addition              |
| `*`  | Multiplication        |
| `/`  | Division              |
| `%`  | Modulus               |
| `&&` | Logical AND           |
| `\|\|` | Logical OR            |
| `&`  | Bitwise AND           |
| `\|`  | Bitwise Inclusive OR  |
| `^`  | Bitwise Exclusive OR  |
| `>>` | Bitwise Right Shift   |
| `<<` | Bitwise Left Shift    |
| `==` | Equality              |
| `!=` | Inequality            |
| `<=` | Less than or Equal    |
| `<`  | Less than             |
| `>=` | Greater than or Equal |
| `>`  | Greater than          |
| `!`  | Logical Negation      |
| `~`  | Bitwise Complement    |

Operator precedence is consistent with C language precedence.

An expression can contain arbitrary combinations of constant
values, simulator registers, and environment variables. For
example:

```
sim> SET ENV -A A=7+2
sim> ECHO A=%A%
A=9
sim> SET ENV -A A=A-1
sim> ECHO A=%A%
A=8
```

### Command Aliases

Commands can be aliased with environment variables. For example:

```
sim> set env echo=echof
sim> echo "Hello there"
Hello there
```

## Executing System Commands

The simulator can run host operating system commands with the
`!` (spawn) command:

```
! <host operating system command>
```

If no operating system command is provided, the simulator attempts to
launch the host operating system’s command shell.

## Getting Help

The `HELP` command prints information about one command or about
all commands:

|                            |                                                 |
|----------------------------|-------------------------------------------------|
| `HELP`                     | print all `HELP` messages                       |
| `HELP <command>`           | print `HELP` for command                        |
| `HELP <device>`            | print `HELP` for device                         |
| `HELP <device> REGISTERS`  | print `HELP` for device register variables      |
| `HELP <device> ATTACH`     | print `HELP` for device specific attach         |
| `HELP <device> SET`        | print `HELP` for device specific `SET` commands |
| `HELP <device> SHOW`       | print `HELP` for device specific `SHOW` commands |

## Recording Simulator Activities

Interactions with the simulator command interface (the interface
that prompts with `sim>`) and the output that those interactions
produce can be recorded to a log file.

|                      |                                 |
|----------------------|---------------------------------|
| `SET LOG <filename>` | direct log output to file       |
| `SET LOG STDERR`     | direct log output to stderr     |
| `SET LOG DEBUG`      | direct log output to debug file |
| `SET NOLOG`          | disable output logging          |

Output produced by the simulated machine’s console terminal is
also written to the configured log file if the console terminal
is not provided through a Telnet session (i.e. `SET CONSOLE
TELNET=port`). Output produced by a console Telnet session can be
logged separately with:

|                                     |                               |
|-------------------------------------|-------------------------------|
| `SET CONSOLE TELNET=LOG=<filename>` | direct log output to log file |

### Switches

By default, log output is written at the end of the specified log
file. A new log file can be created if the `-n` switch is used on the
command line.

## Controlling Debugging

Some simulated devices can produce debug output to help diagnose
complicated problems. Debug output may be sent to a variety of
places, or disabled entirely:

|                        |                                 |
|------------------------|---------------------------------|
| `SET DEBUG STDOUT`     | direct debug output to stdout   |
| `SET DEBUG STDERR`     | direct debug output to stderr   |
| `SET DEBUG LOG`        | direct debug output to log file |
| `SET DEBUG <filename>` | direct debug output to file     |
| `SET NODEBUG`          | disable debug output            |

### Switches

Debug messages contain a timestamp indicating the number of
simulated instructions executed before the debug event.

Debug messages can also include additional information.

#### `-f`

The `-f` switch suppresses the internal logic that coalesces
successive identical debug output lines into one line followed by
an indication of how many times the same line was output. This
mode is most appropriate when output is displayed in real time to
`STDOUT` or `STDERR`.

#### `-t`

The `-t` switch causes debug output to contain a time of day displayed
as `hh:mm:ss.msec`.

#### `-a`

The `-a` switch causes debug output to contain a time of day displayed
as `seconds.msec`.

#### `-r`

The `-r` switch makes the time displayed by the `-t` or `-a`
switch relative to the start time of debugging. If neither `-t`
nor `-a` is explicitly specified, `-t` is implied.

#### `-p`

The `-p` switch adds the output of the `PC` (Program Counter) to each
debug message.

#### `-n`

The `-n` switch causes a new/empty file to be written to. The default
is to append to an existing debug log file.

#### `-d`

The `-d` switch causes data blob output to also display the data as
RADIX-50 characters.

#### `-e`

The `-e` switch causes data blob output to also display the data as
EBCDIC characters.

### Device Debug Options

If debug output is enabled, individual devices can be controlled with
the `SET` command. If a device has only a single debug flag:

|                        |                             |
|------------------------|-----------------------------|
| `SET <device> DEBUG`   | enable device debug output  |
| `SET <device> NODEBUG` | disable device debug output |

If the device has individual, named debug flags:

|                                  |                                     |
|----------------------------------|-------------------------------------|
| `SET <device> DEBUG`             | enable all debug flags              |
| `SET <device> DEBUG=n1;n2;... `  | enable debug flags `n1`, `n2`, ...  |
| `SET <device> NODEBUG=n1;n2;...` | disable debug flags `n1`, `n2`, ... |
| `SET <device> NODEBUG`           | disable all debug flags             |

If debug output is directed to stdout, it will be intermixed with
normal simulator output.

### Displaying Debug Settings

The current debug output destination, options, and
device-specific debug settings can be displayed with:

|              |                                    |
|--------------|------------------------------------|
| `SHOW DEBUG` | display the current debug settings |

## Exiting the Simulator

`EXIT` (synonyms `QUIT` and `BYE`) returns control to the operating
system. An optional numeric exit status may be provided for use
by a calling operating system script.

|                 |                                |
|-----------------|--------------------------------|
| `EXIT {status}` | return to the operating system |

## Manipulating Files within the Simulator

ZIMH provides simulator commands that run selected host-side
tools for manipulating file containers and transferring files or
data into and out of simulated environments.

These tools are normally provided by the host operating system.
Exposing them through the simulator command interface makes it
easier to write portable command files that test or build
simulated environments.

### Manipulating File Archives

The `tar` command can create and extract archives:

|                             |                               |
|-----------------------------|-------------------------------|
| `tar -czf xyz.tar.gz *.dsk` | archive disk image files      |
| `tar -xf xyz.tar`           | extract files from an archive |

### Transferring Data from the Web

The `curl` command can retrieve data from the web:

```
curl -L https://github.com/pmetzger/zimh/archive/refs/heads/master.zip --output zimh-master.zip
```

# Appendix 1: File Representations

All file representations are little-endian. On big-endian hosts, the
simulator automatically performs any required byte swapping.

## A.1 Hard Disks

Hard disks are represented as unstructured binary files of 16b data
items for the 12b and 16b simulators, of 32b data items for the 18b,
24b, and 32b simulators, and 64b for the 36b simulators.

Device simulations that use the `sim_disk` library can also use
Virtual Hard Disks, as described by the Microsoft Open
Specification. On Windows and Linux hosts, they can also use raw
host disks or CD-ROM devices. Disk containers may have 512 bytes
of metadata beyond the emulated capacity of the drive. If
present, the metadata describes details about the emulated drive
and additional parameters that may be useful in simulation.

## A.2 Floppy Disks

Floppy disks are represented as unstructured binary files of 8b data
items. They are nearly identical to the floppy disk images for Doug
Jones' PDP-8 simulator but lack the initial 256 byte header. A utility
for converting between the two formats is easily written.

## A.3 Magnetic Tapes

SIMH format magnetic tapes are represented as unstructured binary
files of 8b data items. Each record starts with a 32b record header,
in little endian format. If the record header is not a special header,
it is followed by n 8b bytes of data, followed by a repeat of the 32b
record header. A 1 in the high order bit of the record header
indicates an error in the record. If the byte count is odd, the record
is padded to even length; the pad byte is undefined.

Special record headers occur only once and have no data. The currently
defined special headers are:

|              |               |
|--------------|---------------|
| `0x00000000` | file mark     |
| `0xFFFFFFFF` | end of medium |
| `0xFFFFFFFE` | erase gap     |

Magnetic tapes are endian independent and consistent across simulator
families. A magnetic tape produced by the Nova simulator will appear
to have its 16b words byte swapped if read by the PDP-11 simulator.

ZIMH can read and write E11-format magnetic tape images. E11 format
differs from SIMH format only for odd-length records; the data portion
of E11 records is not padded with an extra byte.

ZIMH can read TPC-format magnetic tape images. TPC format uses a 16b
record header, with `0x0000` denoting file mark. The record header is
not repeated at the end of the record. Odd-length records are padded
with an extra byte. Some TPC formatted tapes have an end of medium
indicated as a record length of `0xffff` with no data following the
record length.

ZIMH can read Pierce-format seven-track magnetic tape images. Pierce
format uses only 6 data bits, and one parity bit, in each byte. The
high order bit indicates start of record. End of file is indicated by
a record of one (occasionally two) bytes consisting of code `017`
(octal).

ZIMH can read and write AWS format tape images. AWS format uses
a three-word 16b little-endian record header containing the next
record size, previous record size, and a flag word. The flag word
uses `0x40` for tape marks and `0xA0` for data records.

ZIMH can directly read from tar files. Tar files are presented
through the tape interface as fixed-size tape records. The
default tar record size is 10240 bytes. A specific record size
can be specified with the `-B` switch on the `ATTACH` command.
Since the input file is presented as fixed-size records to the
simulated system reading from a tape, other data can be presented
the same way. For example, if a system can read 80-byte card
images from a tape drive, a binary input file can be attached
with `-B 80` and read 80 bytes at a time.

ZIMH can present local text and binary files on a pseudo tape
device. This uses one of the ANSI-VMS, ANSI-RT11, ANSI-RSTS, or
ANSI-RSX11 tape formats. The `ATTACH` command for any ANSI
format tape takes a list of file names, which may contain
wildcards, and makes them available to the simulated system as an
ANSI-labeled tape. Each file’s attributes are presented to the
simulated operating system. Text files may contain LF or CRLF
line endings; an operating system that can read an ANSI tape sees
them as normal data in the expected form. Binary files are
presented as fixed-size 512-byte records.

## A.4 Line Printers

Line printer output is represented by an ASCII file of lines
separated by newline characters. Overprinting is represented by a
line ending in carriage return rather than newline.

## A.5 DECtapes

DECtapes are structured as fixed-length blocks. PDP-1/4/7/9/15
DECtapes use 578 blocks of 256 32b words. Each 32b word contains 18b
(6 lines) of data. PDP-11 DECtapes use 578 blocks of 256 16b
words. Each 16b word contains 6 lines of data, with 2b omitted. This
is compatible with native PDP-11 DECtape dump facilities, and with
John Wilson's PUTR Program. PDP-8 DECtapes use 1474 blocks of 129 16b
words. Each 16b word contains 12b (4 lines) of data. PDP-8 OS/8 does
not use the 129th word of each block, and OS/8 DECtape dumps contain
only 128 words per block. A utility, `dtos8cvt.c`, is provided to
convert OS/8 DECtape dumps to simulator format.

A known issue in DECtape format is that when a block is recorded in
one direction and read in the other, the bits in a word are scrambled
(to the complement obverse). In this context, complement obverse
means that the word is split into 3-bit groups corresponding to
DECtape data lines; the bits are complemented, and the 3-bit
groups are reversed end-for-end. For example, a 12-bit word
whose groups are `ABC DEF GHI JKL` becomes
`~JKL ~GHI ~DEF ~ABC`. The PDP-11 deals with this problem by
performing an automatic complement obverse on reverse writes and
reads. The other systems leave this problem to software.

The simulator represents this difference as follows. On the PDP-11,
all data is represented in normal form. Data reads and writes are not
direction sensitive; read all and write all are direction
sensitive. Real DECtapes that are read forward will generate images
with the correct representation of the data.

On the other systems, forward write creates data in normal form, while
reverse write creates data in complement obverse form. Forward read
(and read all) performs no transformations, while reverse read (and
read all) changes data to the complement obverse. Real DECtapes that
are read forward will generate data in normal form for blocks written
forward, and complement obverse data for blocks written in reverse,
corresponding to the simulator format.

# Acknowledgements

SIMH would not have been possible without help from around the
world. I would like to acknowledge the help of the following people,
all of whom donated their time and talent to this "computer
archaeology" project:

| Name | Contribution |
| --- | --- |
| Bill Ackerman | PDP-1 consulting |
| Alan Bawden | ITS consulting |
| Winfried Bergmann | Linux port testing |
| Phil Budne | Solaris port testing |
| Max Burnet | PDP information, documentation, and software |
| J. David Bryan | HP simulators |
| Robert Alan Byer | VMS socket support and testing |
| James Carpenter | Linux port testing |
| Chip Charlot | PDP-11 RT-11, RSTS/E, RSX-11M legal permissions |
| Louis Chrétien | Macintosh porting |
| Dave Conroy | HP 21xx documentation, PDP-10, PDP-18b debugging |
| L Peter Deutsch | PDP-1 LISP software |
| Ethan Dicks | PDP-11 2.9 BSD debugging |
| John Dundas | PDP-11 CPU debugging, programmable clock simulator |
| Jonathan Engdahl | PDP-11 device debugging |
| Carl Friend | Nova and Interdata documentation, and RDOS software |
| Megan Gentry | PDP-11 integer debugging |
| Dave Gesswein | PDP-8 and PDP-9/15 documentation, PDP-8 DECtape, disk, and paper-tape images, PDP-9/15 DECtape images |
| Dick Greeley | PDP-8 OS/8 and PDP-10 TOPS-10/20 legal permissions |
| Gordon Greene | PDP-1 LISP machine-readable source |
| Lynne Grettum | PDP-11 RT-11, RSTS/E, RSX-11M legal permissions |
| Franc Grootjen | PDP-11 2.11 BSD debugging |
| Doug Gwyn | Portability debugging |
| Kevin Handy | TS11/TSV05 documentation, Makefile |
| Ken Harrenstein | KLH PDP-10 simulator |
| Bill Haygood | PDP-8 information, simulator, and software |
| Wolfgang Helbig | DZ11 implementation |
| Mark Hittinger | PDP-10 debugging |
| Dave Hittner | SCP debugging, DEQNA emulator and Ethernet library |
| Sellam Ismail | GRI-909 documentation |
| Jay Jaeger | IBM 1401 consulting |
| Doug Jones | PDP-8 information, simulator, and software |
| Brian Knittel | IBM 1130 simulator, SCP extensions for GUI support |
| Al Kossow | HP 21xx, Varian 620, TI 990, Interdata, DEC documentation and software |
| Arthur Krewat | DZ11 changes for the PDP-10 |
| Mirian Crzig Lennox | ITS and DZ11 debugging |
| Don Lewine | Nova documentation and legal permissions |
| Tim Litt | PDP-10 hardware documentation and schematics, tape images, and software sources |
| Tim Markson | DZ11 debugging |
| Bill McDermith | HP 2100 debugging, 12565A simulator |
| Scott McGregor | PDP-11 Unix legal permissions |
| Jeff Moffatt | HP 2100 information, documentation, and software |
| Alec Muffett | Solaris port testing |
| Terry Newton | HP 21MX debugging |
| Thord Nilson | DZ11 implementation |
| Charles Owen | Nova moving head disk debugging, Altair simulator, Eclipse simulator, IBM System 3 simulator, IBM 1401 diagnostics, debugging, and magnetic tape boot |
| Sergio Pedraja | MinGW environment debugging |
| Derek Peschel | PDP-10 debugging |
| Paul Pierce | IBM 1401 diagnostics, media recovery |
| Mark Pizzolato | SCP, Ethernet, and VAX simulator improvements |
| Hans Pufal | PDP-10 debugging, PDP-15 bootstrap, DOS-15 recovery, DOS-15 documentation, PDP-9 restoration |
| Bruce Ray | Software, documentation, bug fixes, and new devices for the Nova, OS/2 porting |
| Craig St Clair | DEC documentation |
| Richard Schedler | Public repository maintenance |
| Peter Schorn | Macintosh porting |
| Stephen Schultz | PDP-11 2.11 BSD debugging |
| Olaf Seibert | NetBSD port testing |
| Brian & Barry Silverman | PDP-1 simulator and software |
| Tim Shoppa | Nova documentation, RDOS software, PDP-10 and PDP-11 software archive, hosting for SIMH site |
| Van Snyder | IBM 1401 zero footprint bootstraps |
| Michael Somos | PDP-1 debugging |
| Hans-Michael Stahl | OS/2 port testing, TERMIOS implementation |
| Tim Stark | TS10 PDP-10 simulator |
| Larry Stewart | Initial suggestion for the project |
| Bill Strecker | Permission to revert copyrights |
| Chris Suddick | PDP-11 floating point debugging |
| Ben Supnik | Macintosh timing routine |
| Bob Supnik | SIMH simulators |
| Ben Thomas | VMS character-by-character I/O routines |
| Warren Toomey | PDP-11 Unix software |
| Deb Toivonen | DEC documentation |
| Mike Umbricht | DEC documentation, H316 documentation and schematics |
| Leendert Van Doorn | PDP-11 UNIX V6 debugging, TERMIOS implementation |
| Fred Van Kempen | Ethernet code, RK611 emulator, PDP-11 debugging, VAX/Ultrix debugging |
| Holger Veit | OS/2 socket support |
| David Waks | PDP-8 ESI-X and PDP-7 SIM8 software |
| Tom West | Nova documentation |
| Adrian Wise | H316 simulator, documentation, and software |
| John Wilson | PDP-11 simulator and software |
| Joe Young | RP debugging on Ultrix-11 and BSD |
| Jordi Guillaumes i Pons | Testing and CR11/CD11 fixes |

In addition, the following companies have graciously licensed their
software at no cost for hobbyist use:

| Company |
| --- |
| Data General Corporation |
| Digital Equipment Corporation |
| Compaq Computer Corporation |
| Mentec Corporation |
| The Santa Cruz Operation |
| Caldera Corporation |
| Hewlett-Packard Corporation |
