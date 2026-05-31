# ZIMH Answers to Frequently Asked Questions

**Copyright Notice**:

ZIMH is a hard fork of SIMH. Much of this FAQ originated in the SIMH
documentation and remains under the original X11-style open source
license; the precise terms are available at:

[LICENSE.txt](../../LICENSE.txt)

# Table of Contents

[1 General Questions](#general-questions)

[1.1 What is ZIMH?](#what-is-zimh)

[1.2 Why was ZIMH created?](#why-was-zimh-created)

[1.3 What is the history of ZIMH?](#what-is-the-history-of-zimh)

[1.4 Who writes and maintains ZIMH?](#who-writes-and-maintains-zimh)

[1.5 How is ZIMH licensed?](#how-is-zimh-licensed)

[1.6 How is ZIMH distributed?](#how-is-zimh-distributed)

[1.7 Which computer systems does ZIMH simulate?](#which-computer-systems-does-zimh-simulate)

[1.8 Which host systems does ZIMH run on?](#which-host-systems-does-zimh-run-on)

[1.9 What software packages are available to run on ZIMH?](#what-software-packages-are-available-to-run-on-zimh)

[1.10 Where can I get more information on ZIMH?](#where-can-i-get-more-information-on-zimh)

[1.11 How do I search for existing answers?](#how-do-i-search-for-existing-answers)

[2 Operational Questions](#operational-questions)

[2.1 How do I build ZIMH on Windows?](#how-do-i-build-zimh-on-windows)

[2.2 How do I use ZIMH with Ethernet support on Windows?](#how-do-i-use-zimh-with-ethernet-support-on-windows)

[2.3 How do I build ZIMH on POSIX systems?](#how-do-i-build-zimh-on-posix-systems)

[2.4 How do I transcribe a real CD for use with ZIMH?](#how-do-i-transcribe-a-real-cd-for-use-with-zimh)

[2.5 How do I transcribe other archival media for use with ZIMH?](#how-do-i-transcribe-other-archival-media-for-use-with-zimh)

[2.6 How can I get text files in and out of ZIMH?](#how-can-i-get-text-files-in-and-out-of-zimh)

[2.6.1 Tape input](#tape-input)

[2.6.2 Serial input and output](#serial-input-and-output)

[2.6.3 Paper Tape input and output](#paper-tape-input-and-output)

[2.6.4 Ethernet input and output](#ethernet-input-and-output)

[2.6.5 Printer output](#printer-output)

[2.7 How can I get binary files in and out of ZIMH?](#how-can-i-get-binary-files-in-and-out-of-zimh)

[2.7.1 Serial input and output](#serial-input-and-output-1)

[2.7.2 Ethernet input and output](#ethernet-input-and-output-1)

[2.8 Can I connect real devices on the host computer to ZIMH?](#can-i-connect-real-devices-on-the-host-computer-to-zimh)

[2.9 My Windows host can't communicate with the PDP-11 or VAX over Ethernet; why?](#my-windows-host-cant-communicate-with-the-pdp-11-or-vax-over-ethernet-why)

[2.10 My POSIX host can't communicate with the PDP-11 or VAX over Ethernet; why?](#my-posix-host-cant-communicate-with-the-pdp-11-or-vax-over-ethernet-why)

[2.11 How can I use my wireless Ethernet card with ZIMH?](#how-can-i-use-my-wireless-ethernet-card-with-zimh)

[2.12 Why doesn’t ZIMH idling work on my POSIX host?](#why-doesnt-zimh-idling-work-on-my-posix-host)

[3 Writing and Debugging New Code](#writing-and-debugging-new-code)

[3.1 What resources are available for writing new simulators?](#what-resources-are-available-for-writing-new-simulators)

[3.2 What debugging facilities are available in ZIMH?](#what-debugging-facilities-are-available-in-zimh)

[3.3 When do I need to use the host debugger for debugging a simulator?](#when-do-i-need-to-use-the-host-debugger-for-debugging-a-simulator)

[3.4 What is the release process for ZIMH?](#what-is-the-release-process-for-zimh)

[4 VAX](#vax)

[4.1 Where can I get software and hobbyist licenses for the VAX?](#where-can-i-get-software-and-hobbyist-licenses-for-the-vax)

[4.2 How do I install VMS?](#how-do-i-install-vms)

[4.3 How do I install NetBSD?](#how-do-i-install-netbsd)

[4.4 How do I install Ultrix?](#how-do-i-install-ultrix)

[4.5 What's the CPU serial number for my hobbyist license PAK?](#whats-the-cpu-serial-number-for-my-hobbyist-license-pak)

[4.6 How do I import and make my hobbyist license PAKs usable?](#how-do-i-import-and-make-my-hobbyist-license-paks-usable)

[4.7 How do I change the simulator from a VAXserver 3900 to a MicroVAX 3900?](#how-do-i-change-the-simulator-from-a-vaxserver-3900-to-a-microvax-3900)

[4.8 Is there an example of the simulator running VMS?](#is-there-an-example-of-the-simulator-running-vms)

[4.9 How can I import files to a simulated VMS environment?](#how-can-i-import-files-to-a-simulated-vms-environment)

[4.9.1 The Easy Way](#the-easy-way)

[4.9.2 Alternatively](#alternatively)

[4.10 How do I make imported files readable on a simulated VMS system?](#how-do-i-make-imported-files-readable-on-a-simulated-vms-system)

[4.11 How can I export files from a simulated VMS environment?](#how-can-i-export-files-from-a-simulated-vms-environment)

[5 PDP-11](#pdp-11)

[5.1 When installing RSTS/E from simulated magnetic tape, the installation process hangs with no error message; why?](#when-installing-rstse-from-simulated-magnetic-tape-the-installation-process-hangs-with-no-error-message-why)

# General Questions

##  What is ZIMH?

ZIMH is a simulator system for historic computers. It is a hard fork
of SIMH, the Computer History Simulation system created by Bob Supnik
and extended by many contributors over many years. ZIMH consists of
many machine simulators built around a common user interface package
and supporting libraries.

ZIMH can be used to simulate computer systems for which sufficient
technical detail is available. The focus is on systems of historic
interest and the software environments that ran on them.

##  Why was ZIMH created?

Significant portions of the computing past are being irretrievably
lost, as old systems are scrapped, documentation and software is
thrown out, media become obsolete or unreadable, and inventors and
pioneers die. ZIMH continues SIMH's preservation mission while taking
a different engineering direction: a cleaner build system, stronger
test coverage, and support for modern host platforms rather than very
old compilers and operating systems.

ZIMH preserves historic computers as portable software that can be run
on modern systems. With ZIMH, anyone with a contemporary computer can
run significant samples from the computing past.

## What is the history of ZIMH?

The SIMH project started in 1993, at the suggestion of Larry Stewart
of DEC. Its immediate purpose was to preserve the fading hardware and
software record of early minicomputers. Since then, the project has
been expanded to include other important systems, spanning the history
of computing from the late 50's to the late 80's.

SIMH's core design is based on an earlier simulation system called
MIMIC. MIMIC was written in the late 1960's at Applied Data Research,
by Mike McCarthy, Len Feshkens, and Bob Supnik. MIMIC was a
mini-computer simulator that ran on the PDP-10. Its purpose was to
facilitate the development and debugging of real-time embedded systems
by using the PDP-10 timesharing environment for program development,
instead of the limited facilities of the native minicomputer
environments. Ironically, given SIMH's mission to preserve the
computing record, all machine-readable copies of MIMIC have been lost.

ZIMH forked from SIMH to continue that preservation work with a more
modern project structure and stricter engineering standards.

## Who writes and maintains ZIMH?

ZIMH is maintained by The ZIMH Project. Much of the code and
documentation was inherited from SIMH, and ZIMH remains grateful for
the original SIMH work by Bob Supnik and the SIMH contributors.

## How is ZIMH licensed?

Much of the inherited SIMH code and documentation is licensed under an
X11-style license. The repository also contains newer ZIMH-owned files
with MIT license headers. See [LICENSE.txt](../../LICENSE.txt) and the
SPDX or file headers in the source tree for the terms that apply to
each file.

Software packages used inside the simulated systems are available
under various terms and conditions; see the documentation included
with each software package.

## How is ZIMH distributed?

ZIMH is distributed in source form from its GitHub repository. Build
instructions are maintained in [BUILDING.md](../../BUILDING.md).

## Which computer systems does ZIMH simulate?

ZIMH simulates many historic computer systems from DEC, IBM, HP, Data
General, Scientific Data Systems, Burroughs, Control Data, and other
manufacturers. See [simulators.md](simulators.md) for the current
catalog of simulator targets and the systems they represent.

## Which host systems does ZIMH run on?

ZIMH supports builds on recent POSIX systems, such as Linux, macOS,
and the BSDs, and on recent Microsoft Windows systems. Windows builds
require Windows 10 or newer. The project does not support building on
historic host platforms or obsolete compiler environments.

See [BUILDING.md](../../BUILDING.md) for current compiler, CMake, and
dependency requirements.

## What software packages are available to run on ZIMH?

The machine-specific documentation describes many operating systems and
software packages that are known to run on the simulated hardware. Some
software for historic systems is separately licensed or distributed and
is not part of the ZIMH source tree.

##  Where can I get more information on ZIMH?

The source repository and documentation are at
<https://github.com/pmetzger/zimh>. Use GitHub Issues for bug reports
and tracked work, and GitHub Discussions for questions and broader
project discussion:

- <https://github.com/pmetzger/zimh/issues>
- <https://github.com/pmetzger/zimh/discussions>

##  How do I search for existing answers?

Start with the repository documentation, GitHub Issues, and GitHub
Discussions. Older SIMH discussions may also be useful for inherited
behavior, but may not reflect ZIMH's current build system, supported
hosts, or project policies.


# Operational Questions

## How do I build ZIMH on Windows?

Use the CMake build. Windows builds require Windows 10 or newer. A
typical Visual Studio build looks like this:

```powershell
cmake -G "Visual Studio 18 2026" -A Win32 -S . -B build/release
cmake --build build/release --config Release
ctest --test-dir build/release --build-config Release `
  --parallel --output-on-failure
```

See [BUILDING.md](../../BUILDING.md) and
[README-CMake.md](../../README-CMake.md) for the current dependency
list and supported build options.

## How do I use ZIMH with Ethernet support on Windows?

ZIMH has two common Windows networking paths:

- `nat:` uses SLiRP-backed user-mode networking. This is usually the
  easiest choice for TCP/IP access to the host or Internet.
- Direct host-interface attachment uses a pcap-compatible
  packet-capture runtime. Npcap is the maintained implementation for
  current Windows systems and is recommended.

To build with SLiRP support, install a suitable `libslirp` development
package. Windows builds require libslirp 4.9.0 or newer. To build
simulator targets with pcap Ethernet support, the pcap development
files must be available when CMake configures the build. At run time,
Npcap or another compatible runtime providing `wpcap.dll` and
`packet.dll` must be installed to use pcap networking.

Download Npcap from <https://npcap.com/#download> and install it as
directed. Depending on how Npcap is installed, direct interface access
may require administrator privileges or a packet driver configured to
start automatically.

## How do I build ZIMH on POSIX systems?

On POSIX systems such as Linux, macOS, and the BSDs, use the CMake
build. A typical full-feature release build looks like this:

```sh
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -S . -B build/release
cmake --build build/release
ctest --test-dir build/release --parallel --output-on-failure
```

Install the dependencies described in [BUILDING.md](../../BUILDING.md)
before configuring the build. Network support depends on which optional
backends are installed and enabled, such as pcap, NAT/SLiRP, VDE, and
host tap support.

## How do I transcribe a real CD for use with ZIMH?

- First, you may be able to access a real CD directly from within a
  simulator using RAW device mode to access the device. On POSIX
  systems such as Linux, macOS, and the BSDs, the device name is
  platform dependent:

```
sim> set rq1 cdrom
sim> att rq1 /dev/cdrom_drive
```

- On Windows, the equivalent would be:

```
sim> set rq1 cdrom
sim> att rq1 //./cdrom0
```

- To make an ISO image of a CD on POSIX systems such as Linux, macOS,
  and the BSDs, copy the raw device to a file with the `dd` command:

```
$ dd if=/dev/raw_cd_device of=/path/cdimage.iso
```

- On Windows, use a current disk-imaging tool that can copy the data
  track to an ISO file. Security software can interfere with smooth
  reads from archival media, and lowering the read speed may produce a
  better image from unreliable CD-R media.

## How do I transcribe other archival media for use with ZIMH?

You must have access to a real system that can read the media to be
transcribed (e.g., a system with a working DECtape drive to read a
DECtape). Most systems have utilities to copy raw data to a disk file;
that file can then be transferred over the console serial line to a
system with an Internet link. Utility programs are available to
convert raw data streams to the image formats supported by ZIMH.

## How can I get text files in and out of ZIMH?

### Tape input

Files coming into a simulated system can easily be achieved by using a
simulated tape drive. This mechanism allows local host system files to
be presented to the simulated system as a set of files on an ANSI
labeled tape. If the simulated system’s operating system knows how to
read ANSI labeled tape this is a good choice.

```
sim> att ts0 -f ANSI-VMS file.txt,file.bin,*.c
sim> att ts0 -f ANSI-RSX11 file.txt,file.bin,*.c
sim> att ts0 -f ANSI-RSTS file.txt,file.bin,*.c
sim> att ts0 -f ANSI-RT11 file.txt,file.bin,*.c

$ MOUNT MSA0: SIMH
%MOUNT-I-WRITELOCK, volume is write locked
%MOUNT-I-MOUNTED, SIMH mounted on _MSA0:
$ TYPE MSA0:file.txt
```

### Serial input and output

Since ZIMH supports the universal serial interface using TELNET, text
can be transferred using one of the serial line transfer protocols
(X/Y/Zmodem, Kermit) or using standard cut and paste techniques, if
the host's TELNET program supports it.

To use the TELNET feature, connect to the ZIMH machine using TELNET,
and set the target environment into a 'receive' mode. This is usually
something like running a text editor. Then tell the TELNET program to
'send', 'transfer', or 'paste' the text that you want sent into the
simulated system.

To get text out of the system, have the TELNET program either log the
output, or if the TELNET program supports a backscroll region you can
use that. Tell the simulated system to 'type' or 'cat' the text file,
sending the output to the TELNET device, where you can edit it into a
text file.

Many TELNET programs also support transferring large files via
X/Y/ZModem or Kermit, which you can use as long as the simulated
system has the appropriate matching program.

C-Kermit from Columbia University (<https://www.columbia.edu/kermit>)
is probably the most universal way to transfer files in and out of
simulated systems.

### Paper Tape input and output

Some systems have paper tape devices which can be attached to a file
on the host system and be read within the simulated system. Systems
with a paper tape reader also have a paper tape punch which can be
used for output.

### Ethernet input and output

If the simulated system supports Ethernet connectivity, you can also
use the various network copy programs (FTP, DECNET) to transfer files.

### Printer output

Finally, you can "print" text files to the simulated line
printer. Printer output is automatically formatted as an ASCII text
file.

## How can I get binary files in and out of ZIMH?

### Serial input and output

Since ZIMH supports the universal serial interface using TELNET,
binary files can be transferred using one of the serial line transfer
protocols (X/Y/ZModem, Kermit) or by converting the binary to a
text-encoded file

(HEXify, UUENCODE, VMShare, etc.) and transferred in text mode (see
section 2.6).

Many TELNET programs also support transferring large files via
X/Y/ZModem or Kermit, which you can use as long as the simulated
system has the appropriate matching program.

C-Kermit from Columbia University (<https://www.columbia.edu/kermit>)
is probably the most universal way to transfer files in and out of
simulated systems.

### Ethernet input and output

If the simulated system supports Ethernet connectivity, you can also
use the various network copy programs (FTP, DECNET) to transfer files.

## Can I connect real devices on the host computer to ZIMH?

Currently Ethernet devices, serial ports and physical disks and/or
CD-ROMs are the devices which can be accessed directly from ZIMH
simulators.

## My Windows host can't communicate with the PDP-11 or VAX over Ethernet; why?

If you are using direct host-interface attachment, Npcap or another
compatible pcap runtime must be installed and usable at run time, and
the simulator must have been built with pcap support. Direct
host-interface attachment on Windows can often communicate with the
host over that same interface, but driver installation, privilege, and
firewall policy can still prevent traffic from flowing.

If the simulated operating system only needs TCP/IP access to the host
or the Internet, NAT/SLiRP networking is often easier and does not
depend on pcap interface capture.

## My POSIX host can't communicate with the PDP-11 or VAX over Ethernet; why?

This applies to POSIX hosts such as Linux, macOS, and the BSDs.
Direct pcap attachment sends and receives raw Ethernet packets through
the selected host interface. Some host network stacks do not naturally
receive packets sent this way, and some host configurations prevent
the simulator from seeing the traffic you expect. This issue can often
be accommodated in one of four ways:

1. Use NAT mode for your network connection. This mode only allows
   TCP/IP traffic to flow between the systems (No DECnet, LAT or other
   proprietary LAN protocols), which will be fine for single simulator
   situations.

2. Add a second Ethernet controller, attach both controllers to the
   same switch or hub, and attach ZIMH to the second controller. Don’t
   configure any host network protocols on the second Ethernet
   controller. The host and simulated system will then communicate
   across the physical network connection.

3. If the host has internal (kernel level) network bridging support,
   then the host’s network configuration can be setup to allow direct
   communication between the host and the simulated system. ZIMH
   networking layer can accommodate `tun`/`tap` and/or `vde`
   networking to achieve this where the host platform supports it.

4. Enable 2 XQ (or XU) devices in the simulator and use one in NAT
   mode to talk to the host system and connect the other to LAN for
   simulator to simulator communications and other LAN protocols.

For current details, see [networking.md](networking.md).

## How can I use my wireless Ethernet card with ZIMH?

The best approach here is to use NAT mode on your network
connection. This will work fine for simulators using TCP/IP to talk to
either their host system and/or to reach the Internet. Meanwhile, as
for directly using the wireless network card the following are some of
the considerations:

Wireless Ethernet is something of a misnomer - it "works like"
Ethernet. Wireless cards behave differently than real Ethernet cards
in promiscuous mode. Some wireless cards can’t operate in promiscuous
mode but can sometimes be successfully used with existing ZIMH
code. Sometimes this will also depend on functionality provided by the
wireless router you may be connected to. Many wireless routers will
not be well behaved when you attempt this.

One of the caveats is that the simulated machine cannot run any
software which changes the simulated MAC address, or the network
connection will stop working. For example, DECNET Phase IV (or Phase V
in compatibility mode) tries to change the MAC of the network card to
AA-00-04-xx-xx-xx. Nor can you preset the wireless MAC address to the
anticipated target DECNET address using something like SMAC to get
DECNET to work; DECNET will see the MAC already preset to the
required DECNET address and generate an invalid media (duplicate
address) fault.

Otherwise, TCP/IP, LAT, VMS Clustering, and DECNET Phase V in
non-compatibility mode work fine.

If direct wireless attachment is attempted, one common workaround is
to set the simulated Ethernet MAC address to match the host wireless
interface's MAC address:

```
sim> set xq mac=00-80-c8-08-ce-db
```

## Why doesn’t ZIMH idling work on my POSIX host?

Some POSIX host systems, such as Linux, macOS, and the BSDs, have
default clock tick sizes which are greater than those required to
produce useful ZIMH idling behavior. Useful idling depends on a
simulator's ability to sleep for intervals that are less than or equal
to the simulated system’s clock tick. Best idling behavior is realized
when sleep intervals can be as small as 1ms.

The `SHOW VERSION` command will display (among other things) the host
OS's clock tick size. If a particular simulator cannot idle usefully on
your host, check the simulator's clock and idle settings and confirm
that the host can provide sufficiently fine-grained sleeps.

# Writing and Debugging New Code

## What resources are available for writing new simulators?

The developer documentation in this repository describes the common
simulator framework, coding standards, test methodology, and build
metadata. Older SIMH documents can still be useful background for
inherited code, but ZIMH's current source tree and developer
documentation are authoritative for new work.

## What debugging facilities are available in ZIMH?

Most simulators provide the following debugging capabilities:

- Symbolic assembly and disassembly of memory contents.
- Numeric examination and modification of the data store of any
  simulated device.
- Numeric search on both memory and device data.
- Visibility to simulator internal structures, such as the event
  queue.
- An unlimited number of instruction breakpoints.
- Proceed counts on breakpoints.
- Automatic execution of simulator commands on a breakpoint.
- Stepped execution (from single step to 'n' steps).
- A PC change queue, usually 64 instructions deep.
- Instruction execution history recording and display.

Specific simulators may provide additional features, such as an
instruction history buffer, CPU and/or device logging, and breakpoints
on memory reads and writes.

## When do I need to use the host debugger for debugging a simulator?

While a simulator is being debugged, its execution of instructions or
debugging support code may be unreliable. During this process, the
programmer may need to use the host debugger to stop in the middle of
an instruction execution, or to trap an error condition. Host debugger
breakpoints should be invisible to the simulator; with the exception
of clock calibration, all simulator events are driven off the event
queue rather than real-world events.

If the programmer needs to force a simulator stop from the host
debugger, most simulators provide an "address stop" global
variable. Setting this variable to `1` will cause the simulator to
stop after completing the current instruction.

## What is the release process for ZIMH?

The latest development code is available in the public source code
repository at <https://github.com/pmetzger/zimh>. See the project
documentation for current versioning and release practices.

# VAX

## Where can I get software and hobbyist licenses for the VAX?

The former HP OpenVMS Hobbyist Program appears to have been
discontinued. VSI provides a Community License Program for current
non-commercial OpenVMS use, but it does not currently provide new VAX
OpenVMS hobbyist licenses. Check VSI's current licensing pages for the
latest available options:

<https://vmssoftware.com/products/licenses/>

## How do I install VMS?

To install VMS, you will need a distribution CD-ROM. Any version after
VMS 5.5-2 should run on the MicroVAX 3900 simulator.

- Transcribe the distribution CD-ROM to an ISO-format CD image
  file. (See question 2.4 for information on how to do this.)
- Set drive `RQ1` to be a CD-ROM.
- Attach the CD-ROM image file to simulated drive `RQ1`.
- Set drive `RQ0` to be the type of disk you want. Be sure that the
  disk is large enough to hold VMS.
- Attach a blank disk image file to simulated drive `RQ0`.
- Boot the CPU.
- When the self-test code completes, boot the CD-ROM.
- Use standalone backup to restore the CD-ROM contents to the
  simulated disk.

```
sim> set rq0 rd54
sim> set rq1 cdrom
sim> att rq0 new_vms.dsk
sim> att rq1 cd_rom_image.iso
sim> boot cpu
:
>>> boot rq1

$ (prompt from standalone backup)
```

A writeup on the procedure can be found on the VMS hobbyist site.

## How do I install NetBSD?

Directions for installing NetBSD are on the NetBSD web site, at
<https://www.netbsd.org/Ports/vax/emulator-howto.html>.

## How do I install Ultrix?

Ultrix is not presently licensed for hobbyist use. If you have a valid
license for Ultrix, and distribution tapes for a version that supports
the MicroVAX 3900 series (V4 or later), then you should be able to
install Ultrix on the simulator.

- Transcribe the distribution tapes to SIMH-format tape image
  files. (See question 2.5 for information on how to do this.)
- Mount the installation tape image on simulated drive `TQ0`.
- Set drive `RQ0` to be the type of disk you want. Be sure that the
  disk is large enough to hold Ultrix.
- Mount a blank disk image file on simulated drive `RQ0`.
- Boot the CPU.
- When the self-test code completes, boot the installation tape.
- The installation tape will guide you through the installation of
  Ultrix.

```
sim> set rq0 rd54
sim> att rq0 new_vms.dsk
sim> att tq0 ultrix_install.tap
sim> boot cpu
   :
>>> boot mua0

(Ultrix installation dialog)
```

## What's the CPU serial number for my hobbyist license PAK?

On a MicroVAX 3900, the CPU serial number is not readable and can be
an arbitrary value. 12345 will work fine.

## How do I import and make my hobbyist license PAKs usable?

See [How can I import files to a simulated VMS environment?][vms-import]
and [How do I make imported files readable on a simulated VMS
system?][vms-attributes]

[vms-import]: #how-can-i-import-files-to-a-simulated-vms-environment
[vms-attributes]: #how-do-i-make-imported-files-readable-on-a-simulated-vms-system

## How do I change the simulator from a VAXserver 3900 to a MicroVAX 3900?

To change the type between a MicroVAX 3900 and a VAXServer 3900 use
the following commands:

```
sim> set cpu model=VAXServer
sim> set cpu model=MicroVAX
```

and boot the simulated VAX.

## Is there an example of the simulator running VMS?

This example assumes you are trying to emulate a MicroVAX 3900 with
64MB of memory, with a single 1GB disk drive, a CDROM, and an Ethernet
controller.

This assumes that you have already copied the VMS distribution CD to
an image file as described above and that the host has working
Ethernet support if you intend to attach the simulated network
controller to a host interface. Path names and Ethernet attachment
names are host-dependent.

```
> zimh-vax                          ; run VAX emulator
sim> set cpu 64m                    ; set memory size to 64MB
sim> load -r vax\ka655x.bin         ; load the MicroVAX 3900 console ROM
sim> attach NVR vax\ka655.nvr       ; create/load a Non-Volatile RAM file
sim> set LPT disable                ; disable devices we don't want/need
sim> set TQ disable                 ;   "
sim> set rq0 ra90                   ; set disk 0 to 1GB (RA90 size)
sim> attach rq0 vax\vaxsys.dsk      ; create/use disk file
sim> set rq1 rrd40                  ; set disk 1 as a cdrom
sim> attach -r rq1 vax\hobbyist.dsk ; attach cdrom dump file as read-only
sim> set rq2 offline                ; turn off disk rq2
sim> set rq3 offline                ; turn off disk rq3
sim> attach xq eth0                 ; attach to host ethernet controller
sim> b cpu                          ; start (boot) VAX console

KA655-B V5.3, VMB 2.7

 1) Dansk                           ; will not appear if the controlling
    ..                              ; keyboard doesn't support multi-
 15) Svenska                        ; national characters!
   (1..15): 5
Performing normal system tests.
40..39..38..37..36..35..34..33..32..31..30..29..28..27..26..25..
24..23..22..21..20..19..18..17..16..15..14..13..12..11..10..9..
8..7..6..5..4..3..
Tests completed.
>>> show device                     ; tell console to show all devices
UQSSP Disk Controller 0 (772150)
-DUA0 (RA90)
-DUA1 (RRD40)

Ethernet Adapter 0 (774440)
-XQA0 (08-00-2B-AA-BB-CC)
>>> b dua1                          ; tell console to boot cdrom
(BOOT/R5:1 DUA1)

2..1..0
```

## How can I import files to a simulated VMS environment?

### The Easy Way

- Present the files to the VMS system using a pseudo tape drive with
  the ANSIFILES tape format.
- This approach is both easy and achieves direct access by the VMS
  system in a single step.

```
> zimh-vax                          ; run VAX emulator
sim> attach ts0 -f ansi-vms Hobbyist-USE-ONLY-VA.TXT,*.exe,
```

within the running simulator:

```
$ mount MSA0: SIMH
%MOUNT-I-MOUNTED, SIMH mounted on _MSA0:
$ @MSA0:Hobbyist-USE-ONLY-VA.TXT
```

### Alternatively

- Create an ISO 9660 CD image containing the files you want to import.
  Older VMS versions may have practical filename limitations; choose
  filenames that the target VMS version can read.
- Attach the simulated CD image to a simulated CD drive.
- Mount the simulated CD as an ISO 9660 file system under VMS.
- Copy the files you need from the simulated CD to the simulated disk.

(Thanks to Tim Stark for this suggestion.)

## How do I make imported files readable on a simulated VMS system?

Files imported using the ANSITAPE paradigm are directly usable without
further manipulation.

Some imported files may need to have their file attributes set
appropriately in order to be easily usable in the simulated VMS
system. This may be the case for text files which come from Unix or
Windows systems that have LF or CRLF line endings and may have been
transported to the VMS system via binary network transport OR via a CD
image.

`DIRECTORY/FULL` will display the file’s attributes.

A file transferred in binary mode will likely have record attributes
that say: "Fixed 512 byte records".

A file’s record attributes can be changed to handle text files with LF
line endings with

```
$ SET FILE/ATTRIBUTE=RFM:STMLF
```

A file’s record attributes can be changed to handle text files with
CRLF line endings with:

```
$ SET FILE/ATTRIBUTE=RFM:STM
```

This will work with the latest version of VMS, but earlier versions
didn’t have the `SET FILE/ATTRIBUTE` command.

## How can I export files from a simulated VMS environment?

- Utility ODS2 (available on the Web) can read an ODS-2 disk image and
  copy files from that image to the host file system.
- Text files can be printed to the simulated line printer, as
  described above.

# PDP-11

## When installing RSTS/E from simulated magnetic tape, the installation process hangs with no error message; why?

RSTS/E installation from magnetic tape requires that the tape be write
locked.
