# Allocation Failure Policy

The codebase currently handles allocation failure inconsistently. Some
callers check `malloc`, `calloc`, `realloc`, or `strdup` and return an
error code. Others check only the first allocation in a sequence, or do
not check at all before using the returned pointer. This creates a false
appearance of recovery: after a failed allocation, many simulator paths
cannot realistically continue without leaving partially initialized
state behind.

For this project, the preferred long-term policy is to stop pretending
that most allocation failures are locally recoverable. In software like
ZIMH, an allocation failure usually means the process is already unable
to proceed coherently. Continuing may be more dangerous than exiting
because it can leave devices, register tables, attach state, dynamic
buffers, or event queues only partly updated.

## Proposed Direction

Introduce central allocation wrappers, similar in spirit to X11's
`xmalloc`, `xcalloc`, and `xrealloc`, that either return a valid pointer
or terminate execution with a clear diagnostic. Likely wrappers:

```c
void *xmalloc(size_t size);
void *xcalloc(size_t count, size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *s);
char *xstrndup(const char *s, size_t n);
```

Once such wrappers exist, ordinary code can assume allocation succeeds.
That reduces defensive boilerplate, avoids half-hearted local cleanup,
and keeps failure behavior consistent across the tree.

## Scope

This should be a separate project from mechanical string-safety work.
Do not casually change allocation failure semantics while replacing
unsafe string APIs. When an allocation failure issue is noticed during
another project, record it here or in that project's follow-up list
unless fixing it is essential to the current change.

## Open Questions

- Where should the wrappers live: `sim_string`, a new allocation module,
  or another small runtime utility header/source pair?
- Should wrappers call `abort`, `exit`, or an existing SIMH fatal-error
  path?
- What diagnostic format should be used so failures are useful in
  automated test and integration logs?
- Are there a few boundary APIs where allocation failure genuinely is
  recoverable and should remain explicit?
