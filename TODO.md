# TODO Index

This file is a short index for current work.  Detailed plans belong in
`projects/`, where they can carry context, test strategy, and completion
criteria without turning this file into a stale parallel backlog.

## Current Priorities

- [projects/TESTING-PLAN.md](projects/TESTING-PLAN.md)
  Tracks the long-term path toward comprehensive automated coverage.
- [projects/BUILD_MIGRATION.md](projects/BUILD_MIGRATION.md)
  Tracks the remaining build-system cleanup after the move to maintained
  CMake metadata and a thin compatibility `Makefile`.
- [projects/BUILD_ARTIFACT_PURGE.md](projects/BUILD_ARTIFACT_PURGE.md)
  Tracks old build artifacts and local makefiles that may still be
  removable.
- [projects/MACHINE_WORD_TYPES.md](projects/MACHINE_WORD_TYPES.md)
  Tracks the broad cleanup of signed host types used for guest machine
  words, registers, addresses, and bit fields.
- [projects/VAX_WORD_TYPES.md](projects/VAX_WORD_TYPES.md)
  Tracks the VAX-specific machine-word and address type migration.
- [projects/OBSOLETE_PLATFORMS.md](projects/OBSOLETE_PLATFORMS.md)
  Tracks obsolete host-platform conditionals that should be purged.
- [projects/SIMULATOR_HOST_OS_DEPENDENCE.md](projects/SIMULATOR_HOST_OS_DEPENDENCE.md)
  Tracks places where simulator code still knows too much about host
  operating systems.
- [projects/SIMULATOR_IN_PROCESS_TEST_HARNESS.md](projects/SIMULATOR_IN_PROCESS_TEST_HARNESS.md)
  Tracks the plan for running simulator logic inside unit-test processes.
- [projects/NETWORK_TESTING.md](projects/NETWORK_TESTING.md)
  Tracks realistic automated network-backend testing.
- [projects/DOCUMENTATION.md](projects/DOCUMENTATION.md)
  Tracks larger documentation cleanup and organization work.

## Maintenance Rules

- Do not add large project plans directly here.  Create or update a
  focused file under `projects/` instead.
- Remove entries from this index when the corresponding project file is
  completed, deleted, or no longer an active priority.
- Keep this file free of obsolete absolute paths and historical build
  assumptions.
