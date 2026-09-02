# 🧵 The mode-dispatch consolidation pattern (added 2026-09-01)

This is a real pattern from the working house, added after a real
refactor: several khtpm-family renderer binaries (a taskbar strip, a
per-entity tile/pal window) had evolved *separately* for months and had
started silently duplicating the same real logic — opacity handling,
`override_redirect` toggling, the same "reapply on change" polling
convention — each with its own slightly-different copy. One improvement
(a real bugfix) had to be applied two or three times in two or three
places to actually fix the thing everywhere.

The fix wasn't "extract the shared bits into a `.h`/`.c` pair and link
it into both binaries." **This house doesn't do that** — see doc 1's
own "no custom `.h` files for shared structs" reasoning; the same logic
extends to app-level `.c` files. The real house rule, stated plainly:

> Either it's genuinely the same file (same binary, same process), or
> it's a genuinely separate process talking over fork/exec + file-IPC.
> No cross-`.c` linking or `#include`-splicing to share behavior
> *within* one binary. No linkers as a workaround for duplication.

## The actual fix: fold the whole file in as a "mode"

Instead of extracting a shared library, the entire content of each
sibling binary's source file was relocated **verbatim** into one
existing renderer file (`khtpm_core_render.c`), each becoming its own
mode with its own `_main()`-style entry point (e.g. `strip_main()`,
`tp_main()`), and the real `main()` picks which mode to run based on
**argv shape** — not a `--mode=` flag, just the natural shape each
binary's own real callers already invoke it with:

```c
int main(int argc, char **argv) {
    if (argc == 2) {
        /* two sibling modes share this exact argc - disambiguate by a
         * real, on-disk fact instead of inventing a new flag */
        char marker[PATH_BUF];
        snprintf(marker, sizeof(marker), "%s/#.desktop", argv[1]);
        if (access(marker, F_OK) == 0) return strip_main(argc, argv);
        return tp_main(argc, argv);
    }
    /* every other mode always takes a real .chtpm path as argv[2] */
    ...
}
```

Every real launcher script/caller needed **zero changes** to its own
argv — they were already calling the binary with exactly the shape
that still works, they just now resolve to a different (bigger) binary
on disk.

## The actual mechanical steps (in order)

1. **Find real name collisions before touching anything.** Extract
   every top-level `static` function name and global variable name from
   both files (a simple `grep`+`sort`), then `comm -12` the two sorted
   lists. This catches *most* collisions.
2. **A real build attempt is the real ground truth, not the grep.**
   The static scan is a floor, not a guarantee — an actual `gcc`
   "conflicting types" error caught at least one real collision the
   naive name-list scan missed entirely (two genuinely different
   `hit_test()` functions, same name, nothing else in common).
3. **For each collision, ask "are these the same thing?"** — compare
   the two function *bodies* directly, byte-for-byte if you have to.
   - Identical → keep ONE copy, delete the donor's duplicate, keep
     every one of the donor's own call sites unchanged (they call the
     same function either way).
   - Genuinely different → rename the donor's copy to something
     mode-specific (`hit_test` → `sp_hit_test`), fix its own call sites.
4. **Rename the donor's own `main()`** to a mode-specific name
   (`strip_main`, `tp_main`) — this is the mode's real entry point now.
5. **Splice the donor's whole body in** (structs/globals/functions,
   verbatim — comments, `#ifdef _WIN32` platform shims, everything) as
   one big block, usually right before the real, shared `main()`.
6. **Add the one dispatch line/branch** in the real `main()`.
7. **Build. Fix whatever collisions step 1-3 missed.** Then actually
   run it — launch a real instance, take a real screenshot, click a
   real button — before calling it done.

## Why this isn't the same as doc 1's "one job per binary"

Doc 1's small-ops guidance (`change_gold.c`, `show_message.c`) is still
completely correct and unaffected by any of this — that's for small,
genuinely single-purpose utility tools, and merging those together
would be a real regression (harder to test alone, harder to reason
about). This pattern applies specifically when several **renderer/GUI
variants of basically the same kind of program** have organically split
apart over time and are now duplicating real behavior between them —
merging them removes the duplication while runtime process *isolation*
is kept exactly where it still matters: the house's own real manager
process (a pure-logic worker with zero Xlib calls) stayed a genuinely
separate fork/exec binary throughout this whole refactor, completely
untouched — that was never a duplication problem, so it was never a
merge candidate.
