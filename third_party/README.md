# Third-Party Source Imports

This directory contains third-party source imported into the zimh tree.

Entries under this directory should be treated as upstream-owned code.  Keep
local zimh integration, simulator glue, build adaptation, tests, and
documentation outside the imported subtree whenever practical.  If a local fix
must be made inside an imported subtree, keep it in a separate commit and
record whether it should be proposed upstream.

Prefer `git subtree --squash` for Git-hosted third-party source imports unless
there is a specific reason to retain full upstream history in this repository.
Squashed subtrees keep normal checkouts simple while preserving a repeatable
update path.  Upstream per-file history remains available in the upstream
repository.

Before updating a subtree:

1. Record the current upstream commit and the target upstream commit.
2. Run the focused tests that cover the current integration.
3. Pull the subtree update with `git subtree pull --squash`.
4. Rebuild and rerun the focused tests plus the broader relevant suites.
5. Update this file with the new upstream hash, upstream commit date, local
   import date, and any important update notes.

## Musashi

Musashi is a portable Motorola 68000-family CPU emulation core.  ZIMH imports
it for use by the AltairZ80 simulator.

- Prefix: `third_party/musashi/`
- Upstream: `https://github.com/kstenerud/Musashi.git`
- Upstream branch: `master`
- Import method: `git subtree --squash`
- Current upstream commit:
  `313ebf1bd9f4d0d93341eb5ce21fd8a119e9dbdd`
- Upstream commit date: `2026-03-08T08:47:58+01:00`
- Upstream commit subject:
  `Merge pull request #117 from eschaton/eschaton/ptrdiff_t`
- Imported into zimh: `2026-05-29`
- ZIMH subtree merge commit:
  `3b89f046a66517e0f62568b0b6d777a1b90c1d72`
- ZIMH subtree squash commit:
  `285a33367aeff12508b6aec7fc329ecb0c593a34`

Update Musashi with:

```sh
git subtree pull \
    --prefix=third_party/musashi \
    --squash \
    https://github.com/kstenerud/Musashi.git \
    <target-ref>
```

Use an explicit upstream tag or commit for `<target-ref>` when practical.  If
updating from upstream `master`, record the resolved commit hash in this file.

Do not wire Musashi directly into simulator code from this README.  Simulator
integration belongs in the owning simulator directory and its build files.
