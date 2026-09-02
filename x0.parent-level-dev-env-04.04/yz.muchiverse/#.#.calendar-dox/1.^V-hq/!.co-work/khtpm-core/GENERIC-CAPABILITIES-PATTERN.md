# 🧩 The generic-capabilities pattern (added 2026-09-01)

A real, live lesson from this same week's house work — arguably more
important than `CONSOLIDATION-PATTERN.md`'s own file-merging technique,
because it's the difference between a shared renderer that stays clean
forever and one that slowly rots.

## The temptation

You have one shared, generic renderer serving several different apps
(a database editor, a chat window, a network browser...). App #4 needs
something the generic renderer doesn't do yet — say, a real text-input
field. The fast way to ship it: add a flag.

```c
/* DON'T DO THIS */
static int g_is_my_new_app = 0;
...
if (g_is_my_new_app) { /* my app's own special input handling */ }
```

This looks harmless the first time. It is not. Every app that does this
adds its own flag and its own branch, at every real dispatch site the
new behavior touches (redraw, key handling, click routing, focus
tracking...) — this house found one shared file with **7 apps** doing
this, each with a flag checked at **~15 separate call sites**. The
renderer stops being generic at all; it's 7 apps glued into one file,
each one afraid to touch the others.

## The real, adopted answer: generic capabilities, not per-app flags

Don't ask "how does *this app* need the renderer to behave?" Ask "what
is the smallest, real, reusable **capability** that solves this for
every app that will ever need it, including ones that don't exist yet?"
Two real examples, both built for exactly this reason:

### 1. Live re-parse on file change

Problem: a renderer that parses its layout file once, at startup, can
never show new content without the whole window relaunching.

Generic fix: one function, gated on the file's own real mtime, called
once per event-loop tick:

```c
static time_t g_layout_mtime = 0;

static int reparse_if_changed(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    if (st.st_mtime == g_layout_mtime) return 0; /* cheap - one stat() per tick */
    g_layout_mtime = st.st_mtime;
    /* ... re-run your real parse + rebuild the Elem tree here ... */
    return 1;
}
```

Now ANY app that keeps rewriting its own layout file (a real business-
logic manager process regenerating its own live view) gets live updates
for free, zero new C in the renderer. This is also *why* a real,
separate manager process, publishing a plain file the renderer re-reads
on a timer, beats "the renderer computes its own content" every time —
see doc 1's own "one manager, everyone else reads" rule. Same idea, one
more layer.

### 2. A real, generic text-input element

Problem: every app that wants to accept typed text either re-implements
its own input-arming/typing/commit logic, or (worse) doesn't bother and
loses real functionality.

Generic fix: a single element "type" (`<cli_io>` in this house's own
vocabulary) with a handful of real, shared semantics:
- click it, or select it via keyboard-nav and press Enter/a digit → it
  becomes ARMED (the one field currently accepting keystrokes)
- typing appends to its own live buffer
- Backspace edits that buffer
- Escape cancels — un-arms, discards the edit, does **not** close the
  window
- Enter COMMITS — writes the buffer somewhere real (a request file, an
  action dispatch) and un-arms

Every one of those five behaviors is written **once**, generically, in
the shared renderer — using the SAME `nav_index`/focus-cursor mechanism
every other clickable element already has (a real armed field shows a
distinct bracket state, e.g. `[^]`, vs. a merely-focused `[>]` or an
inert `[ ]` — reuse whatever cursor-prefix helper your own renderer
already has for nav badges, don't hand-write a second copy of that
string logic for just this one element type).

**One real, non-obvious gotcha this house hit and fixed, worth knowing
up front**: an armed input field needs a **real keyboard grab**
(`XGrabKeyboard`) for as long as it's armed. Without it, typing works
fine *while the mouse is over the window* and then silently stops the
instant the pointer moves off it, under any desktop using focus-
follows-mouse. This is exactly the kind of bug that looks like "it's
broken" when it's actually "it never grabbed focus" — confirmed live
via `XGetInputFocus` returning `None` with the pointer elsewhere.

## The real house rule this produces

> The shared renderer never gains a new `g_is_<project>` global or a
> new per-project dispatch branch, ever again. If a new app needs new
> renderer behavior, that behavior must be expressed as a real, generic
> capability any current or future app could also use — never scoped
> to "just this one app."

If you catch yourself about to write `if (g_is_my_app) { ... }` inside
a shared renderer file: stop. Ask what the real, generic version of
that need is instead. It's usually smaller than the special case would
have been.
