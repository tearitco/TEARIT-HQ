# WRAITH RGB Architecture & CLI_IO Rendering Pipeline
**Date:** 2026-07-06  
**Reference:** parsers.txt (canonical source of truth)  
**Purpose:** Map where cli_io elements live, how they render, and why GL differs from ASCII

---

## THE NAV-NUMBERING INVARIANT — read this before touching ANY nav= or SET_ACTIVE: number

**One incident, three regressions, in a single session, before this was fixed
properly.** If you are about to add a new clickable element to the wraith-alpha
desktop shell (a chrome button, a taskbar slot, anything with a `nav=` value
in GL or an auto-numbered position in ASCII), read this whole section first.
Skipping it is exactly how the same bug got shipped three times in a row on
2026-07-06 before anyone stopped to write it down. Full incident history is in
`zest-09.00-handoff.md`'s chrome-button appendices; this section is the
distilled rule, not the story.

### The two renderers number things two completely different ways

- **ASCII** (`wraith_parser_alpha.c`): assigns each interactive element the
  next sequential display number *as it parses*, purely by textual position
  in the composed markup. It has no concept of "chrome" vs "body content" vs
  "taskbar" — everything is just the Nth `<button>`/`<cli_io>` tag it
  encounters, left to right, top to bottom, every single frame.
- **GL** (`wraith-alpha_manager.c`'s `write_semantic_projection_files()`):
  assigns `nav=` values via its OWN arithmetic — hardcoded/derived counts of
  how many chrome buttons, launcher slots, taskbar entries, etc. exist —
  completely independent of anything `wraith_parser_alpha.c` decides.

**These two numbering systems only agree when someone has manually ensured
the ARITHMETIC in `wraith-alpha_manager.c` produces the same number as the
POSITION that element happens to occupy in the composed text stream.** There
is no automatic mechanism that keeps them in sync — every single element that
needs a matching number in both renderers requires this to be true by
construction, not by accident.

### The three ways this broke (all in the same session, fixing one chrome button)

1. **Gave the new element `nav=0` in GL** (a deliberate dodge, to avoid
   touching the "4 fixed chrome slots" arithmetic). Result: ASCII's
   auto-numbering still counted it as a real element and shifted everything
   after it by one; GL's arithmetic, untouched, didn't shift at all. The two
   renderers disagreed about which number activated which button.
2. **Gave it a *trailing* nav slot instead** (`g_max_index - 2`, mirroring
   the ASCII\*/GL debug-selector pattern). Result: ASCII's parse-order number
   for this element was *low* (it sits right after the other chrome buttons,
   near the start of the composed text), while GL's trailing number was
   *high* (near the end of the whole nav range) — they were never going to
   match, because the debug-selector pattern only works when the
   corresponding ASCII text is *also* positioned near the end of the stream,
   which chrome buttons structurally are not.
3. **Fixed the trailing slot's OWN bug** (a stale `g_max_index` caused it to
   collide with the title button's `nav=1`) — a real fix, but for the wrong
   underlying design, which still couldn't produce ASCII/GL parity no matter
   how correctly its own arithmetic was computed.

None of these got a numeric match between the two renderers, because none of
them addressed the actual structural requirement: **the GL number and the
ASCII parse-order position must be driven by the exact same underlying
count, computed once.**

### The fix that actually works: single source of truth, not independent arithmetic

`wraith-alpha_manager.c` now has ONE array, `g_chrome_icons[]` (near the top
of the file, just after `MAX_LAUNCHERS`), that is the sole definition of the
fixed chrome icon row (`o`, `-`, `x`, `&` as of this writing). Both renderers'
code iterates this SAME array with the SAME loop variable to produce their
respective numbers:

```c
/* ASCII (build_desktop_shell_markup) */
for (ci = 0; ci < CHROME_ICON_COUNT; ci++) {
    appendf(raw, ..., "<button ... onClick=\"SET_ACTIVE:%d\" />", 2 + ci);
}

/* GL (write_semantic_projection_files) */
for (ci = 0; ci < CHROME_ICON_COUNT; ci++) {
    int nav = 2 + ci;
    fprintf(objects, "OBJECT | ... nav=%d ... action=SET_ACTIVE:%d ...\n", nav, nav);
}
```

Because both loops compute `2 + ci` from the same array in the same order,
the Nth icon gets the exact same number in both renderers **by construction**
— there is no separate "did I remember to update both places" step, because
there's only one array to update. `CHROME_CONTENT_START` (`2 +
CHROME_ICON_COUNT`) is likewise the single definition of "where does content
after chrome start," consumed by `recompute_nav_bounds()`,
`dispatch_menu_index()`'s `launcher_start`, and
`write_semantic_projection_files()`'s `taskbar_start`/`next_body_nav` — none
of them hardcode "4" or "5" as a separate literal anymore.

**Why ASCII's auto-numbering still works even though it never reads this
array's numbers directly:** it doesn't need to. ASCII's own sequential
parse-order numbering will land on `2+ci` for the Nth chrome icon *as long as
nothing else is inserted into the text stream between the title button and
the icon loop* — which is guaranteed, because both the ASCII buffer and the
GL objects are built from the exact same `g_chrome_icons[]` traversal, at the
same point in each renderer's own composition order. The "coincidence" that
made `o`/`-`/`x` work before this fix (ASCII's 2nd/3rd/4th parsed element
happening to equal GL's nav=2/3/4) is now a **guaranteed property**, not a
coincidence, because there's no way for a 5th element to sneak in between
title and the icon loop in one renderer but not the other — they're driven by
the same array.

### The cost this fix required, and the invariant it leaves behind

Extending `g_chrome_icons[]` (going from 3 icons to 4, `o`/`-`/`x` →
`o`/`-`/`x`/`&`) moved `CHROME_CONTENT_START` from 5 to 6. Every wraith
sub-project with its own hand-authored `session/scene.objects.pdl` — and
there are nine of them, each generating that file from a hardcoded `nav=N`
sequence inside its own `ops/src/wraith_project_input.c` (confirmed via grep,
not dynamically computed): `piececraft-wraith` (two copies — its `manager/`
AND its `ops/src/` both independently write the same scene, kept in sync
manually), `wraith-browser`, `chtmgl-wraith`, `wraith-ed`, `web-cam`,
`chtmgl-video-isolate`, `wraith-3d-cube`, `screen-record` — assumed their own
nav range could safely start at 5. All nine were bumped to start at 6 instead
(`sed`-driven, descending-order substitution per file, verified with a
post-edit grep showing clean contiguous ranges and untouched `nav=0`
entries, then a full `compile_all.sh` pass, 225/225 clean).

**If `g_chrome_icons[]` ever grows again, this same nine-file (ten-generator)
audit must happen again**, because these projects are separate C binaries
that cannot `#include` a shared constant (TPMOS convention: no shared
headers, every `.c` is a self-contained island) and currently have no
file-based mechanism to read `CHROME_CONTENT_START` at runtime instead of
hardcoding their own assumption about it. **This is a real, currently-open
gap, flagged not fixed**: a proper follow-up would have
`wraith-alpha_manager.c` publish `CHROME_CONTENT_START`'s current value to a
well-known file (e.g. `pieces/display/chrome_reserved_nav_count.txt`), and
have every scene-generating project read it at startup instead of hardcoding
"5" (now "6") as a literal — matching the TPMOS mantra "if it's not in a
file, it's a lie" instead of relying on nine separate agents/sessions
remembering to keep nine files in sync with one array by hand, forever.
Nobody should extend chrome again without either doing that follow-up first,
or explicitly re-running this same nine-file grep-and-bump audit and
recompiling everything.

### The rule, stated plainly

**Before adding any new numbered/clickable desktop-shell element:**
1. Does it belong in the fixed chrome row? Add it to `g_chrome_icons[]`.
   Nothing else should need to change by hand — `CHROME_CONTENT_START` moves
   automatically, and both renderers' loops pick it up automatically.
2. Does adding it move `CHROME_CONTENT_START`? If yes, grep every project for
   hardcoded `nav=` sequences (`grep -rn "nav=[0-9]" projects/wraith-alpha/wraith-projects --include="*.c"`)
   and bump every range that assumed the old boundary, in descending order,
   per file. Recompile everything (`compile_all.sh`) and confirm 0 failures
   before considering it done.
3. Never give a new element a *trailing* nav slot (`g_max_index - N`) unless
   its corresponding ASCII markup is *also* textually positioned at the very
   end of the composed stream (only true today for the two debug selectors).
   A trailing GL number and an early-position ASCII number cannot match.
4. Never give a new element `nav=0` in GL "to avoid the numbering problem" if
   it's also being inserted into the ASCII text stream as a real interactive
   tag — ASCII will number it anyway, GL just won't move to match.

### 2026-07-11 update: this pattern generalizes beyond nav numbers

Confirmed the same session, applied to three more cases beyond chrome icons:
the launcher row (`nav_idx = CHROME_CONTENT_START + li`, was a stale hardcoded
`5 + li`), `scene.objects.pdl`-declared controls (`next_scene_nav` counter in
`append_project_scene_objects()`, replacing trust in a project's own
hardcoded `nav=N` literal), and cli_io's live-value display
(`emit_embedded_line_objects()`, generalized from `is_cli_io` to
`has_target_id`). Also closed the nine-hardcoded-`nav=N`-files gap this doc
flagged and left undone since 2026-07-06: `wraith-alpha_manager.c` now
publishes `CHROME_CONTENT_START` to `pieces/display/chrome_reserved_nav_count.txt`
at startup, and all nine projects' `scene.objects.pdl` generators read it
instead of hardcoding their own guess.

**The general form of the rule, not just the chrome-specific one:** this
isn't only about nav numbers. Any time a value needs to agree between the
ASCII builder and the GL builder — a number, a live text value, a boolean
flag — the fix is the same shape: find (or create) ONE place that computes
or holds that value, and make both renderers read it, instead of letting
either side compute or guess its own copy. A project's own hardcoded
literal, a stale cross-process read, or a tag-type-specific special case are
all instances of the same failure: something that should have exactly one
source of truth ended up with two. This is the standing discipline for
every future interactive element type added to Wraith, not a closed,
one-time fix.

---

## CORRECTION (2026-07-06, same day, written after the rest of this doc)

Part 2 below ("The cli_io Gap") describes the pipeline as if
`wraith_parser_alpha.c` reads `window-geom.chtpm` directly as `current_layout`
and parses its `<cli_io>` tags natively for both ASCII and GL. **That's the
wrong mechanism for window-geom specifically.** `window-geom.chtpm` is not
read by either the embedded (Settings → picker → editor) or standalone
(chrome-button) path — both hand-write their own markup directly into
`session/wraith_body.txt`, which reaches the screen through a *different*
mechanism this doc didn't cover at all: `append_project_probe_body()` for
ASCII, and `emit_embedded_line_objects()` for GL, both in
`wraith-alpha_manager.c`, not `wraith_parser_alpha.c`. See
`zest-09.00-handoff.md`'s "Bug B" appendix for the verified, corrected
mechanism and fix (`emit_embedded_line_objects()` not assigning cli_io a nav
slot was the actual GL gap — already fixed there as of this session).

Everything in this doc **is** accurate for a project reached the "real" way —
opened directly via `href`/`current_layout` swap with its own genuine
`.chtpm` layout file (e.g. `terminal.chtpm`, `fs.chtpm`) — where
`wraith_parser_alpha.c` really is the only parser involved, exactly as
described below. It just doesn't describe window-geom's *actual* current
pipeline, which routes through the embedded-body-passthrough mechanism
instead. Left the rest of the doc as-is rather than rewriting it, since the
generic ASCII/GL/rasterizer split it describes (Parts 3-5) is still correct
and useful — just read Part 2's `window-geom.chtpm` specifics with this
correction in mind.

---

## Executive Summary: Two Renderers, One Parser

Wraith has **exactly one UI parser** (wraith_parser_alpha.c) that handles both ASCII and GL output:

```
wraith_parser_alpha.c (SINGLE SOURCE OF TRUTH)
  ↓ reads: window-geom.chtpm, gui_state.txt, keyboard input
  ↓ writes: current_frame.txt (ASCII character grid)
  ├─→ ASCII Renderer: Displays current_frame.txt directly to terminal
  └─→ RGB Rasterizer + GL: Rasterizes current_frame.txt → pixels → GL texture
```

**Important:** GL does NOT have its own parser. Both renderers consume the same ASCII output.

---

## Part 1: Where CLI_IO Lives in the Code

### The Parser: projects/wraith-alpha/ops/wraith_parser_alpha.c

**Lines 2166-2170: cli_io Rendering**
```c
if (display_num > 0)
  asprintf(&line, "%s %s %d. [%s_]  ...", BOX_V, pref, display_num, el->input_buffer);
else
  asprintf(&line, "%s %s [%s_]  ...", BOX_V, pref, el->input_buffer);

const char* display_val = (strlen(el->input_buffer) > 0) ? el->input_buffer : scratch_substituted;
```

**What this does:**
- Renders a **navigation-indexed input box**: `[>] 5. [input_text_here_]`
- If input_buffer is empty, shows `scratch_substituted` (the label text from the layout)
- Every keystroke updates el->input_buffer in memory
- On Enter/Backspace/char input, calls `save_to_gui_state(...)` to persist the state

**Lines 2580-2636: cli_io Input Handling**
```c
case 13:  // ENTER
  if (strlen(el->input_buffer) > 0) {
    save_to_gui_state(el->target_id[0] ? el->target_id : "input_text", el->input_buffer);
    el->input_buffer[0] = '\0';
  }
case 8:   // BACKSPACE
  if (len > 0) el->input_buffer[len-1] = '\0';
  save_to_gui_state(el->target_id[0] ? el->target_id : "input_text", el->input_buffer);
default:  // PRINTABLE CHAR
  el->input_buffer[len] = (char)key;
  save_to_gui_state(el->target_id[0] ? el->target_id : "input_text", el->input_buffer);
```

**State Persistence:** Input is written to `gui_state.txt` with key `el->target_id` (or hardcoded "input_text" if target_id not set). This survives reparse because sync_cli_input_from_gui_state() restores it.

---

### The Rasterizer: projects/wraith-alpha/plugins/wraith_rgb_daemon.c

**What it does:**
- Reads: `/pieces/display/current_frame.txt` (ASCII character grid written by the parser)
- Reads: Font metrics (character size, glyph data)
- Writes: `/projects/wraith-alpha/session/rgb/current_frame.rgba32` (pixel bitmap)
- Writes: `/pieces/display/current_frame.objects.pdl` (hit-box metadata)

**What it does NOT do:**
- Does not parse `.chtpm` layout files
- Does not recognize `<cli_io>`, `<button>`, `<text>` tags
- Does not make UI decisions (which element is focused, etc.)
- Does not touch the input buffer or gui_state

**Rasterization Strategy:**
- Treats current_frame.txt as a pure **character grid**
- Each character position (row, col) maps to pixel (row × char_height, col × char_width)
- Looks up each character's glyph from the font
- Renders glyph pixels at the corresponding (x, y) position
- Metadata (hit-boxes) are created by tracking **which characters belong to which element**

**Critical Point:** Every character in current_frame.txt gets rasterized. This includes:
- `[hi_]` (the cli_io input box with text)
- `[ ]` (button focus indicator)
- Navigation indices (`1.`, `2.`, etc.)
- All text content

If wraith_parser_alpha.c writes `[hi_]` at position (10, 20), wraith_rgb_daemon.c will rasterize it as pixels at (10 × char_height, 20 × char_width).

---

### The GL Renderer: projects/wraith-alpha/ops/wraith_gl.c

**What it does:**
- Reads: `current_frame.rgba32` (pixel bitmap from rasterizer)
- Reads: `current_frame.objects.pdl` (hit-box metadata)
- On mouse click: Looks up pixel (x, y) against objects.pdl to find action
- On keyboard input: Appends to `pieces/keyboard/history.txt` (same file the parser reads)
- Displays: Uploads rgba32 as GL texture, renders to window

**What it does NOT do:**
- Parse UI elements
- Render text (that's already in the rgba32 pixels)
- Make navigation decisions (that's the parser's job)

---

## Part 2: The cli_io Gap (ASCII Works, GL Broken)

### Symptom

**ASCII Mode:**
```
| [>] 5. [hi_]                                                 |
| [ ] 6. [empty_input_]                                       |
```
The input boxes render as `[text_here_]` with actual typed content.

**GL Mode:**
```
X position
Y position
Width
Height
```
Only the **label text** shows; the input boxes are **missing**.

### Root Cause

The layout file (window-geom.chtpm) has:
```xml
<cli_io id="edit_x" label="  X position" target_id="1" />
<cli_io id="edit_y" label="  Y position" target_id="2" />
```

The parser renders:
- If input buffer has text: `[text_]` (the box with text)
- If input buffer is empty: `scratch_substituted` (the label text from the attribute)

**But where are the labels in the GL output?**

The labels ARE being shown in GL. So the question is: where is the **input box itself**?

**Answer:** The input boxes (`[hi_]`, `[empty_]`, etc.) are in current_frame.txt, so the rasterizer SHOULD be rasterizing them into pixels. But they're not showing up in the GL window.

**This means one of:**
1. The rasterizer isn't reading current_frame.txt correctly
2. The rasterizer isn't writing to current_frame.rgba32
3. The GL renderer isn't displaying the rgba32 correctly
4. There's a character encoding issue (e.g., `[` and `]` aren't rasterizing)

---

### How to Debug This (Step-by-Step)

**Step 1: Confirm current_frame.txt has the boxes**
```bash
grep "\[.*_\]" /path/to/pieces/display/current_frame.txt
```
Should see lines like: `[>] 5. [hi_]` or `[ ] 6. [text_]`

**Step 2: Check if rasterizer is running**
```bash
ps aux | grep wraith_rgb_daemon
```
Should show: `wraith_rgb_daemon.+x` running

**Step 3: Check if rgba32 is being written**
```bash
ls -ltr /path/to/projects/wraith-alpha/session/rgb/current_frame.rgba32
stat /path/to/pieces/display/current_frame.objects.pdl
```
Timestamps should be **recent** (within last few seconds). If old → rasterizer is dead or not reading the marker.

**Step 4: Check the marker file (what wakes the rasterizer)**
```bash
ls -l /path/to/pieces/display/frame_changed.txt
```
Size should be growing. Each time the parser writes a new frame, it appends a byte to this marker. The daemon watches it and rasterizes on size change.

**Step 5: Manual test: Force rasterization**
```bash
echo "FORCE" >> /path/to/pieces/display/frame_changed.txt
sleep 1
ls -l /path/to/projects/wraith-alpha/session/rgb/current_frame.rgba32
```
Timestamp should update. If not → daemon isn't reading the marker.

---

## Part 3: Input Handling in ASCII vs GL

Both modes feed into the **same parser state machine**, but through different channels:

### ASCII Mode
```
User presses key
  ↓
Terminal stdin → pieces/keyboard/history.txt (appended by keyboard daemon)
  ↓ (parser reads on next pulse)
wraith_parser_alpha.c: processes key, updates el->input_buffer
  ↓
current_frame.txt rewritten with new content
  ↓
ASCII display shows updated frame
```

### GL Mode
```
User presses key in GL window
  ↓
wraith_gl.c: append_keyboard_event() appends to pieces/keyboard/history.txt
  ↓ (SAME FILE the parser reads)
wraith_parser_alpha.c: processes key, updates el->input_buffer
  ↓
current_frame.txt rewritten with new content
  ↓
wraith_rgb_daemon.c: rasterizes new current_frame.txt
  ↓
current_frame.rgba32 updated
  ↓
GL window displays updated pixels
```

**Key insight:** Both renderers use the **same input file** (pieces/keyboard/history.txt). The parser doesn't care which renderer the key came from. This is why target_id matters — the parser updates gui_state with the right key, and both renderers show the result.

### 2026-07-12 addendum: a universal context-menu hotkey, and why it can't be a bare modifier

Design goal (raised while planning an emoji picker for wrai-text-editor,
but scoped as general infrastructure): ONE key that works identically
on every Wraith screen — any embedded project, or the desktop itself —
and opens a context menu whose contents depend on whatever is currently
focused. This mirrors the existing chrome "&" resize/window-geom icon,
which already scopes itself to whichever window is focused via
`open_window_geom_for_project(window->project_id)` in
`wraith-alpha_manager.c`. The emoji picker would be the first consumer
of this menu, not a special-cased binding of its own.

The first instinct — bind it to a bare Shift or Alt press — doesn't
work, for a reason worth documenting since it'll come up again for any
future hotkey design: **a modifier key pressed alone produces no event
at all in either input path this codebase actually uses.**

- ASCII's live reader (`pieces/keyboard/plugins/keyboard_input.c` — the
  binary `orchestrator.c` actually launches; other similarly-named
  files like `input_capture.c` exist in the tree but are never spawned,
  same "confirm what's live before trusting a filename" trap as
  [[wraith-window-geom-blank-rootcause]]) reads raw bytes off
  `/dev/tty`. A terminal has no channel for "a modifier went down by
  itself" — modifiers only ever change what byte arrives when combined
  with another key.
- GL's `wraith_gl.c` uses `glutGetModifiers()`, which only reports
  modifier state DURING a real key event; freeglut never fires
  `keyboard()`/`special_keyboard()` for a bare modifier tap.

This rules out Shift specifically for any editor context, too: since
there's no separate "shift was held" signal in the ASCII path, Shift+M
just arrives as the byte for `'M'` — indistinguishable from the user
typing a capital letter, which happens constantly while editing text.

**What actually works, already proven live in this codebase:**
Ctrl+letter. `keyboard_input.c` disables `ISIG` in raw mode specifically
so Ctrl+C arrives as a literal byte (3) instead of being trapped by the
OS as SIGINT (`main()`'s `if (c == ((('c') & 0x1f))) break;`), and
`wraith_gl.c`'s `keyboard()` checks `key == 3` the same way — Ctrl+C
already works end to end in both renderers today. Ctrl+letter combos
always produce bytes 1-26, which can never collide with normal
printable typing (32-126) or the existing arrow codes (1000-1003) —
exactly the property a "safe to bind everywhere, including while a
cli_io field is actively being typed into" hotkey needs. Recommended
over Alt+letter (technically closer — GL gets it free via
`GLUT_ACTIVE_ALT`, but ASCII's `editorReadKey()` would need new
escape-parsing work, since it currently only recognizes the 3-byte
`ESC [ A/B/C/D` arrow shape and would misread an Alt+letter's 2-byte
`ESC`+letter shape as a bare ESC and silently drop the letter) purely
because Ctrl+letter needs zero new plumbing on either side.

Not yet implemented — this is a design note, not a landed feature.
Exact letter, menu-content-per-focus-type logic, and ASCII/GL rendering
of the menu itself are all still open. See `jul-12-handoff.txt`
(ZEST-09.00 root) for the full open-questions list.

---

## Part 4: Why target_id Matters (The Phase 2 Bug)

### Current Code (Broken)
```c
save_to_gui_state("input_text", el->input_buffer);  // HARDCODED KEY
```

All cli_io fields on the same screen write to the same key: `"input_text"`. On reparse, sync_cli_input_from_gui_state() restores from this single slot. Collision.

### Fixed Code (Phase 2)
```c
save_to_gui_state(el->target_id[0] ? el->target_id : "input_text", el->input_buffer);
// Fallback to "input_text" only if target_id is not set, for backward compat
```

Now:
- `<cli_io target_id="1" />` writes to and restores from key `"1"`
- `<cli_io target_id="2" />` writes to and restores from key `"2"`
- Typing in field 1 no longer overwrites field 2

**This applies equally to ASCII and GL.** Both renderers parse the same current_frame.txt, both use the same gui_state.txt to persist/restore. The fix in wraith_parser_alpha.c fixes both automatically.

---

## Part 5: Frame Pipeline Checklist

When adding a **new interactive element** (button, cli_io, etc.), verify at each layer:

| Layer | Responsibility | Files | Check |
|-------|-----------------|-------|-------|
| **Parser** | Render element to character grid | `current_frame.txt` | `grep` for the text/box you rendered |
| **Rasterizer** | Convert characters to pixels | `current_frame.rgba32` | Check timestamp on rgba32 is recent |
| **GL** | Display pixels and handle clicks | GL window, `current_frame.objects.pdl` | Visual inspection + test click |

**If ASCII works but GL broken:** Problem is in Rasterizer or GL layer (not the parser).
**If ASCII broken:** Problem is in Parser layer.
**If both broken:** Problem is likely in the layout file or manager state persistence.

---

## Reference: The Three Binary Checkpoints

### 1. wraith_parser_alpha.c

**Entry:** Spawned by wraith-alpha_manager.c in active loop  
**Cycle:** ~60 FPS when active, ~10 FPS when idle (TPMOS pulse throttling)  
**Input:** `.chtpm` layout, gui_state.txt, pieces/keyboard/history.txt  
**Output:** pieces/display/current_frame.txt, pieces/display/frame_changed.txt (marker)

### 2. wraith_rgb_daemon.c

**Entry:** Spawned once at startup, runs continuously  
**Wakeup:** Watches pieces/display/frame_changed.txt size; rasterizes on growth  
**Input:** pieces/display/current_frame.txt  
**Output:** projects/wraith-alpha/session/rgb/current_frame.rgba32, pieces/display/current_frame.objects.pdl

### 3. wraith_gl.c

**Entry:** Spawned by manager in active loop (only when GL mode active)  
**Cycle:** ~60 FPS rendering, event-driven input  
**Input:** current_frame.rgba32, pieces/keyboard/history.txt (same as parser)  
**Output:** GL window, pieces/keyboard/history.txt (keyboard appended), projects/wraith-alpha/session/history.txt (commands)

---

## The Golden Rule: Frame Ownership

- **Parser owns current_frame.txt.** No other code writes to it. The marker (frame_changed.txt) is the **only** signal the daemon needs.
- **Rasterizer owns current_frame.rgba32 and current_frame.objects.pdl.** The GL renderer reads, never writes.
- **GL owns the GL window display and mouse/keyboard capture.** It translates input back into the same history files the parser reads.

This is why cli_io works in both modes: the parser handles all state logic, both renderers just display (or capture input for) what the parser decides.
