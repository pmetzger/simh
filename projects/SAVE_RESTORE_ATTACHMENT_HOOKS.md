# Save/Restore Attachment Hooks

## Problem

SCP save/restore currently assumes that attached media contents live outside
the simulator state file.  The save file records unit state and attachment
filenames, and restore reopens those filenames.  That is sufficient for normal
disk and tape image files because the media bytes are already durable host
objects.

`RAMDISK:` exposes a gap in that model.  A ramdisk's media bytes are volatile
process memory, but SCP's generic save loop has no subsystem-neutral hook that
lets an attachment backend prepare or persist its media before SCP writes the
attachment name.  The current RAMDISK implementation therefore has a narrow
bridge in `scp.c`:

```c
r = sim_disk_save_if_ramdisk(uptr);
```

That is deliberately small, but it is still a smell: generic SCP save logic now
knows about a disk-specific attachment behavior.

## Broader Risk

This is probably not unique to ramdisks.  Any future attachment backend whose
contents or connection state are not fully represented by a durable host file
will have the same problem.  Examples could include:

- memory-backed or generated media;
- temporary or deleted backing files;
- compressed, remote, or transactional attachment backends;
- future device models where attachment state has sidecar metadata that must be
  synchronized with simulator `SAVE`;
- attachments whose restore behavior must validate or transform external state.

Without a generic hook, each case is likely to add another special-case call to
SCP, which would make save/restore harder to reason about and harder to test.

## Desired Direction

SCP should expose a generic attachment save/restore preparation interface
instead of knowing about particular storage backends.  The shape might be a
unit-level or device-level callback invoked while SCP is walking attached units,
before it serializes the attachment filename.

Possible shape:

```c
if (uptr->attachment_save != NULL) {
    r = uptr->attachment_save(uptr);
    if (r != SCPE_OK)
        return r;
}
```

The exact API needs design work.  Questions include:

- Should the hook live on `UNIT`, `DEVICE`, or a subsystem-specific attachment
  descriptor?
- Is one pre-save hook enough, or do we also need a post-restore hook?
- Should the hook be allowed to alter the serialized attachment name?
- How should errors be reported so users can tell which attachment prevented
  `SAVE` or `RESTORE`?
- How should this interact with old simulator save files?

## Recommendation

Do not broaden the RAMDISK implementation merely to hide the current smell.
Keep the narrow `sim_disk_save_if_ramdisk(uptr)` bridge until we are ready to
design a general attachment lifecycle hook under tests.

When we do that work, start with characterization tests around existing
save/restore of attached files, then introduce a generic hook with one
production user: RAMDISK.
