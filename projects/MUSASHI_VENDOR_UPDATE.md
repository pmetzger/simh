# Replace Embedded Musashi With Upstream Subtree Import

## Goal

Replace the embedded Musashi Motorola 68K emulator sources in this tree with a
clearly tracked upstream import of the maintained upstream project:

https://github.com/kstenerud/Musashi

The preferred import mechanism is `git subtree`, not a hand-copied vendor
drop.  This still keeps the source available in normal checkouts, but it gives
the project a repeatable upstream update path.

The end state should make it obvious which files are upstream Musashi, which
files are local zimh integration glue, and how to update Musashi again later
without hand-auditing a tangled mix of upstream and local changes.

## Recommended Direction

Use `git subtree --squash` to import upstream Musashi under a dedicated prefix:

```text
third_party/musashi/
```

This is the best fit if zimh keeps local integration outside the subtree and
does not intend to maintain a long-lived fork of Musashi internals.

The subtree import is still "vendored" in the broad sense that the code is
present in this repository, but it is not a conventional copy/paste vendor
drop.  The upstream commit is tracked by Git metadata and future updates can be
performed with another subtree pull.

Prefer `--squash` for the first import unless there is a concrete need to keep
all upstream Musashi history in the zimh repository.  A squashed subtree keeps
repository history smaller and makes update commits easy to review at the zimh
level.  It does mean upstream per-file history lives in the upstream Musashi
repository, not directly in zimh.

Do not use a git submodule for this.  A submodule would keep upstream history
cleanly separate, but it would also make normal checkouts, CI, source archives,
and user builds depend on an extra repository checkout step.

## Current Situation

The AltairZ80 M68K support currently carries Musashi-derived files under
`simulators/AltairZ80/m68k/`.  The tree also contains an `example/` copy under
that directory, so there may be more than one embedded copy of upstream Musashi
material.

The local files have accumulated zimh/SIMH integration changes, warning fixes,
line-ending normalization, whitespace cleanup, and build-system integration.
Some of those changes may be legitimate local adapter code, while others may be
obsolete once the upstream version is imported cleanly.

## Constraints

- Do not make a large source replacement without automated tests around the
  current behavior first.
- Keep upstream Musashi files as close to upstream as practical.
- Put local integration code in separate zimh-owned files where possible.
- Avoid making broad style cleanup inside vendored files.
- Preserve license and copyright notices from upstream.
- Record the exact upstream commit or tag used for the subtree import.
- Do not hide local zimh changes inside the subtree import commit.

The subtree import commit itself may be a squashed upstream snapshot.  That is
acceptable as long as it contains only upstream Musashi content and records the
upstream commit or tag.  Local adapter code, build changes, behavior fixes, and
test changes should remain separate commits.

## Proposed Layout

Use a dedicated third-party subtree location, for example:

```text
third_party/musashi/
```

Put local notes about third-party subtree policy and update procedures in a
top-level third-party note rather than inside the Musashi subtree, for example:

```text
third_party/README.md
```

Keep zimh-specific configuration and adapters outside the upstream subtree, for
example:

```text
simulators/AltairZ80/m68kconf_zimh.h
simulators/AltairZ80/m68k_adapter.c
simulators/AltairZ80/m68k_adapter.h
```

The exact names can change during implementation, but the ownership split
should remain:

- `third_party/musashi/`: upstream code imported from `kstenerud/Musashi`.
- `simulators/AltairZ80/`: zimh integration, memory callbacks, simulator glue,
  build wiring, and tests.

If importing the whole upstream repository adds examples or tools that zimh
does not build, leave them in the subtree unless they create a real maintenance
problem.  A subtree works most cleanly when the prefix corresponds to the
upstream repository root.  CMake can choose only the runtime sources zimh
actually needs.

Avoid adding zimh-only README files inside `third_party/musashi/`.  Keeping
local subtree policy in `third_party/README.md` makes the guidance more visible
and avoids creating local files under the upstream-owned prefix.

## Work Plan

1. Inventory all Musashi-derived files currently in the repository.
   - Search for `MUSASHI`, `Karl Stenerud`, `m68kmake`, `m68kcpu`, and
     `m68kconf`.
   - Confirm whether the `example/` directory is used by any build or tests.
   - Identify whether any non-AltairZ80 simulator uses the same embedded copy.

2. Record current local behavior before changing sources.
   - Run the existing AltairZ80 integration test.
   - Add targeted tests if current coverage is not enough to catch a broken
     M68K path.
   - Prefer tests that exercise CP/M-68K boot or a small 68K instruction smoke
     program through the simulator boundary.

3. Import upstream Musashi with `git subtree`.
   - Choose a specific upstream tag or commit.
   - Record the upstream URL and commit hash in a local subtree note.
   - Import upstream files without local edits in the same commit.
   - Prefer a command shaped like:

```sh
git subtree add \
    --prefix=third_party/musashi \
    --squash \
    https://github.com/kstenerud/Musashi.git \
    <tag-or-commit>
```

4. Separate local configuration from upstream code.
   - Move zimh-specific memory access callbacks, interrupt callbacks, and
     simulator integration into zimh-owned adapter/config files.
   - Point upstream Musashi at the zimh config through its supported
     `MUSASHI_CNF` mechanism or equivalent build configuration.
   - Avoid editing upstream headers just to include zimh headers.

5. Replace the old embedded copy.
   - Update CMake to build the subtree-imported Musashi sources.
   - Remove duplicate or obsolete embedded Musashi files from
     `simulators/AltairZ80/m68k/`.
   - Keep only local adapter/config files in the simulator directory.

6. Reapply only necessary local fixes.
   - Review old local changes one by one against upstream.
   - Keep only changes needed for zimh integration or correctness.
   - If a local correctness fix is not present upstream, document it and
     consider preparing an upstreamable patch.
   - If a fix must live inside `third_party/musashi/`, keep it in its own
     commit and document whether it should be split and proposed upstream.

7. Verify.
   - Clean build from scratch.
   - Debug-warnings build.
   - Unit tests.
   - Full integration tests.
   - A focused AltairZ80/M68K test pass.

## Commit Structure

Prefer a small series rather than one large commit:

1. Add characterization tests for current AltairZ80 M68K behavior.
2. Import upstream Musashi into `third_party/musashi/` unchanged with subtree.
3. Add zimh adapter/config files and build wiring.
4. Switch AltairZ80 to the subtree-imported Musashi.
5. Remove obsolete embedded Musashi copies.
6. Apply any required local fixes as separate, explained commits.

## Updating Later

After the first import, update the subtree with a command shaped like:

```sh
git subtree pull \
    --prefix=third_party/musashi \
    --squash \
    https://github.com/kstenerud/Musashi.git \
    <new-tag-or-commit>
```

Before pulling, run the focused AltairZ80/M68K tests against the current tree
to confirm the baseline.  After pulling, rebuild and rerun the same tests plus
the broader relevant unit and integration suites.

If zimh ever needs to contribute a subtree-local fix back upstream, use
`git subtree split --prefix=third_party/musashi` to create an upstream-shaped
history.  This should be rare if local integration stays outside the subtree.

## Risks

- The old embedded copy may contain local fixes not present upstream.
- The current build may depend on non-obvious include ordering or direct
  inclusion of `.c` files.
- Generated Musashi files may need a clear policy: vendor generated outputs,
  regenerate during build, or regenerate only during vendor updates.
- The `example/` tree may be unused but still valuable as upstream reference;
  decide whether vendoring upstream documentation/examples is useful or just
  noise.
- Squashed subtree imports make upstream history review an explicit upstream
  repository task rather than something visible directly in zimh history.

## Open Questions

- Which upstream commit or tag should be the first vendored baseline?
- Should the subtree import include upstream examples and generator tools, or
  only the runtime files needed by zimh?
- Are there existing AltairZ80 M68K tests strong enough to catch regressions, or
  do we need a new CP/M-68K smoke test before replacement?
