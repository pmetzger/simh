# `src/runtime` File Guide

This directory holds SIMH's shared runtime support layer.

This file describes what each file is for and what kind of logic belongs
there. It intentionally does not inventory individual functions.

## `sim_BuildROMs.c`

This is the ROM-generation support tool used to convert ROM inputs into
embeddable C data for the build.

## `sim_card.c` and `sim_card.h`

These files own shared punched-card support: card image parsing and
formatting, card deck I/O, and reusable card-device helpers.

## `sim_console.c` and `sim_console.h`

These files own shared console and terminal interaction support:
keyboard input, console output, command-prompt I/O integration, logging,
and debug-output plumbing used across the simulator runtime.

## `sim_disk.c` and `sim_disk.h`

These files own shared disk-image and block-device support: image-file
handling, format-specific disk helpers, and reusable runtime services for
simulated disks.

## `sim_ether.c` and `sim_ether.h`

These files own shared Ethernet and network-interface support used by
simulators: packet transport backends, host-interface discovery, and the
runtime network abstraction above the optional PCAP, TAP, and SLiRP
layers.

## `sim_fio.c` and `sim_fio.h`

These files own shared host file-I/O support: file opening, sizing,
path-handling helpers, and the cross-platform filesystem behaviors that
the runtime layer relies on.

## `sim_host_path.c`, `sim_host_path.h`, and `sim_host_path_internal.h`

These files own small host path-name discovery helpers that are not tied
to a specific file operation. Use them for reusable answers such as the
host temporary-file directory, where the correct spelling or lookup order
varies by operating system. The internal header is only for private
test seams used to verify platform fallback behavior.

## `sim_frontpanel.c` and `sim_frontpanel.h`

These files own shared front-panel support: host UI integration, panel
state updates, and the common runtime services used by simulator front
panels.

## `sim_imd.c` and `sim_imd.h`

These files own the ImageDisk (`.imd`) floppy image format support used
by the runtime and by simulators that consume that media format.

## `sim_scsi.c` and `sim_scsi.h`

These files own shared SCSI transport and command-processing helpers for
simulators that expose SCSI devices.

## `sim_serial.c` and `sim_serial.h`

These files own shared serial-line runtime support that sits above the
lower-level multiplexer and socket layers.

## `sim_sock.c` and `sim_sock.h`

These files own the shared socket and host-networking abstraction:
socket creation, connection management, and the cross-platform runtime
network helpers used by higher-level components.

## `sim_tape.c` and `sim_tape.h`

These files own shared magnetic-tape image support: tape container
formats, record handling, and reusable runtime helpers for simulated
tape devices.

## `sim_time.c`, `sim_time.h`, and `sim_time_internal.h`

These files own the thin runtime time-wrapper layer: testable wrappers
around kernel-facing time and sleep calls, plus internal conversion
helpers needed by platform backends.

## `sim_timer.c` and `sim_timer.h`

These files own the shared timer, calibration, idle, throttle, and
runtime scheduling support used across SIMH.

## `sim_tmxr.c` and `sim_tmxr.h`

These files own the shared terminal multiplexer runtime: line
multiplexing, Telnet support, buffered serial I/O, and the common
multi-line device support used by many simulators.

## `sim_video.c` and `sim_video.h`

These files own the shared video and display runtime abstraction used by
SIMH's graphical simulator components.

## Placement Guidelines

When deciding where new runtime code should go:

- keep media-format logic with the matching `sim_*` runtime module
- keep host I/O and OS-integration helpers in the narrowest existing
  runtime module that owns that concern
- prefer the shared runtime files over simulator-local copies when the
  behavior is reusable across machines
- add a new focused runtime module instead of overloading an unrelated
  one when a helper slice has a clear ownership boundary

If a change feels awkward in every existing runtime file, that is
usually a sign that the code should get a new focused module instead of
being forced into a nearby but mismatched one.
