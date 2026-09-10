# Frame-history receipts for x11-hq windows — eye-less visual debugging

**Status:** DESIGN (2026-09-10). No code changed yet. This extends the
already-shipped terminal-mirror surface with the two halves it never
grew: a durable per-window **frame history**, and the TMOS/wraith-style
**receipt** that packs a rendered frame as data (structure + geometry +
nav + checksum) so an agent with no eyes can diff two moments instead of
decoding pixels.

## Problem

A human can debug an x11-hq window with eyes: point at the screen, click,
look. An agent cannot read PNGs (`dump_frame_png_op` output is a dead end
for this family). What the renderer already gives us is a text-shape of
"what the pixels showed" — but it is **one-shot**:

- the live per-window mirror `#.desktop/ascii_frames/<pid>.frame.txt` is
  overwritten every redraw and **unlinked at exit** — no time-ordered
  record, nothing to diff after the fact;
- `dump_frame_png()` + its `.receipt.txt`/`.frame.txt` fire only on the
  `'p'` key / `--dump-and-exit` — a manual poke, not a stream;
- the dock is the *only* window family with a history append
  (`strip_ascii_frame_history.txt`) — nothing like it exists for the
  window rooms (network-browser, events-hq, irc-chat, chain, …);
- none of it is packed as a **receipt** (serials + geometry + checksum),
  so there is no way to say "this frame ≠ that frame" that a blind agent
  can trust.

## What already exists (prior art — all live today)

### khtpm side (`khtpm_core_render.c`)

| thing | where | what it gives |
|---|---|---|
| per-window live ASCII mirror | `kh_write_ascii_frame()` `:6298` | every NON-dock window writes `#.desktop/ascii_frames/<pid>.frame.txt` (indented Elem-tree via `dock_ascii_walk()`) + `<pid>.pulse.txt` (DIAMOND size-growth marker, rotates >64KB) on every `redraw()`; `atexit(kh_ascii_frame_unregister)` `:6329` |
| Elem-tree → readable text walk | `dock_ascii_walk()` `:6147` | indented labels with `[>]` real-highlight, `[ ]`, `*` active/tab-active, `(hidden)` offscreen, nav numbers |
| dock live+history mirror | `dock_write_ascii_frame()` `:6222` | `strip_ascii_current_frame.txt` (live) + `strip_ascii_frame_history.txt` (append; timestamped `--- <tag> <ts> ---` blocks, rotate past 512KB) — the ONLY history today |
| one-shot dump receipt | `dump_frame_png()` `:6723` | `'p'`/`--dump-and-exit`: PNG via external `dump_frame_png_op`, plus `<png>.receipt.txt` (one-line `ok= png= w= h= t= nav= n_nav= page= vars=`) and `<png>.frame.txt` (`kh_serialize_frame_elem/subtree`) |
| headless parity | `--headless` `:1862`, TERMINAL-MIRROR-PARITY steps 1-4 BUILT | same `redraw()` hook writes the same mirrors with **no** X connection — CI/agent surface already proven (irc/chain from many ports) |

The strip's history append logic at `:6242-6282` is a directly reusable
template (fopen `"a"`, `fstat` size > 512KB → reopen `"w"`, timestamped
header block).

### TMOS / wraith-alpha side (the receipt reference)

- `1.TPMOS_c_+rmmp.0103.0001/projects/wraith-alpha/session/rgb/current_frame.receipt.pdl`
  — `rgb_presenter_audit` receipt: `generated_by`, `generated_at_epoch` +
  `generated_at_iso_utc`, `receipt_generation_key`, source files
  (`source_frame_txt`, `source_objects_pdl`, `source_meta_pdl`), the
  derived artifact (`output_rgba32`), viewport/cell/glyph dims, and a
  `loaded_rgba_checksum_fnv1a64`. Serials make the receipt a statement of
  **what produced what from which source**.
- `scene.objects.pdl` — one `OBJECT …` line per on-screen element:
  `tag= id= role= x= y= w= h= z= nav= source_ref= fg= bg= border= action= label=`.
  This is the exact "frame as data" packing: geometry + nav + label +
  state, diffable field-by-field without a human looking at pixels.
- `gl_display.receipt.pdl` — per present/swap step: event, source rgba32,
  expected/loaded byte counts, checksum. A step-audit trail.
- `pieces/config/receipts.conf` — the policy keys we map 1:1:
  `max_entries` (snapshot-history trim), `only_on_change` (skip
  byte-identical re-renders — the main bloat source), `max_log_lines`
  (append-only ledger trim).

## Gap (what this doc adds — all in the shared renderer, no new binaries)

1. **L1 — per-window frame HISTORY.** Every non-dock window appends the
   frames it actually renders to `<pid>.frame_history.txt`, rotating past
   512KB exactly like the dock. Written only when the frame **changed**
   (compare the fresh serialization to the previous one in memory /
   FNV1a digest) — `receipts.conf only_on_change=1` semantics.
2. **L2 — RECEIPT packing.** For each appended frame, write a
   TMOS-shaped `<pid>.receipt.pdl` serial-linking the frame text + a
   geometry/nav `objects` dump + a checksum, so one receipt alone is
   proof of a specific moment and two receipts are `diff`-able.
3. **L3 — LEDGER + policy.** `ascii_frames/index.txt`, one line per
   snapshot; bounded trail (`max_entries 50` default, oldest trimmed).
4. **L4 — discoverability without the PID.** Stamp the window label and
   chtpm basename into the frame header (already printed) so an agent can
   find a window by `grep -l '<label>' ascii_frames/*.frame.txt` before
   it ever knows the PID.

## Design

### L1 — history writer (model on the dock, lines 6242-6282)

Alongside `kh_write_ascii_frame()` in `redraw()`, but only for non-dock:

- append the already-serialized readable frame (the same buffer that goes
  to `<pid>.frame.txt`) to `ascii_frames/<pid>.frame_history.txt`,
  prefixed by the same `--- <window-label> pid <pid> <ts> ---` header
  the dock history uses;
- rotate at 512KB (fstat → reopen `"w"`);
- **only on change**: keep one `uint64_t` digest of the last appended
  frame in memory; if the new digest matches, append nothing (and skip
  the L2 receipt too). Byte-identical re-renders (idle mouse-move
  repaints) are noise — the `only_on_change` rule.
- current-frame overwrite + pulse behavior, and every strip `strip_ascii_*`
  file, stay untouched (the strip is the already-shipped special case).

### L2 — receipt writer

`kh_write_frame_receipt()` — one TMOS-style PDL per appended frame,
written to `ascii_frames/<pid>.receipt.pdl` (latest) **and**
`ascii_frames/<pid>.<seq>.receipt.pdl` (locked snapshot):

```
receipt_type=kh_x11_framesnapshot
generated_by=khtpm_core_render
generated_at_epoch=<ts>
generated_at_iso_utc=<iso>
receipt_generation_key=<pid>@<epoch>
window=<window label from <window ...> tag>
chtpm=<basename of g_chtpm_path>
page=<g_current_page|->
vars=<g_vars_path|->
source_frame_txt=ascii_frames/<pid>.frame.txt
source_history_txt=ascii_frames/<pid>.frame_history.txt
frame_checksum_fnv1a64=0x...
viewport_w=<w>  viewport_h=<h>
focus_nav=<g_focus_nav>  n_nav=<g_n_nav>
hidden_elems=<count over the laid-out walk>
png=<path when a dump_frame_png() snapshot exists, else ->
objects_pdl=ascii_frames/<pid>.<seq>.objects.pdl
```

Plus `ascii_frames/<pid>.<seq>.objects.pdl`, one line per laid-out
on-screen element (reusing the same serializers as
`kh_serialize_frame_subtree` but as **data**, TMOS `OBJECT` shape):

```
OBJECT tag=… id=… class=… x=… y=… w=… h=… nav=… active=0/1 hidden=0/1 label=…
```

The `hidden` flag is the `e->y <= -1000` parked/offscreen test
`dock_ascii_walk()` already applies; nav numbers are the real display
nav (unified across windows), so a blind agent sees exactly the numbers
a human's `[1]` `[2]` badges show.

### L3 — ledger + policy

- `ascii_frames/index.txt`, append-only, one line per appended receipt:
  `<iso> <pid> <window-label> <seq> <checksum>` — trimmed to the last 50
  entries (`max_entries`, houses the trim policy; a future `receipts.conf`
  read is optional — defaults built in, no new config file required).
- Line shape mirrors the wraith `receipts.txt` ledger so any existing
  ledger tooling treats both the same way.

### L4 — find-without-PID

Keep the existing header line `--- <chtpm-basename> pid <pid> <ts> ---`
in both frame and history files and explicitly document it as the
look-up key:

```
grep -l 'network-browser-hq' ascii_frames/*.frame.txt   # -> the pid
```

### Blind-agent debug loop (the recipe this design exists for)

1. **Open** a window: `button.sh <house>` (live) or
   `khtpm_core_render.+x --headless <house> <window.xhtpm>` (no X).
2. **Find** it: `grep -l 'network-browser-hq' ascii_frames/*.frame.txt`
   or read `ascii_frames/last_headless.pid`.
3. **Drive** it: relay `KEY_PRESSED: <code>` lines into
   `entity_menu_history/<pid>.txt`, or the app's own request file
   (e.g. `nb_write_go.sh go <url> <pkg> <house>`).
4. **Read** the result as text:
   - `cat <pid>.frame.txt` — structure, labels, `[>]` focus, active `*`;
   - `diff` the last two entries of `<pid>.frame_history.txt` — what
     *changed on screen* across the drive step;
   - `diff` two `<pid>.N.receipt.pdl` / `.objects.pdl` — nav/geometry/
     label deltas as data, field-by-field;
   - `index.txt` tail — the ordered trail of every real render change.
5. **If pixels ever matter**, `--dump-and-exit` / `'p'` still writes the
   PNG; the receipt carries its path, so a sighted human can pick it up
   from the same receipt a blind agent already validated.

## Acceptance criteria (paste-in check after implementation)

- **Live route:** open network-browser on the taskbar, drive
  `go: http://127.0.0.1:8126/t.html` → `ascii_frames/<pid>.frame_history.txt`
  grows by ≥1 timestamped entry; a `<pid>.<seq>.receipt.pdl` +
  `.objects.pdl` land; `diff` of pre/post receipts shows only the content
  rows that actually changed (the `ghost=…` result line), nothing else.
- **Change discipline:** sending the same input twice appends *zero* new
  entries (only_on_change).
- **Headless parity:** the same drive under `--headless` (no `DISPLAY`)
  produces the same history/receipts.
- **No regressions:** strip mirror + `strip_ascii_*` untouched; `make check`
  (55 PASS + 4 binaries) and the existing windowed E2E still green.

## Non-goals

- No TUI / panes / scrollback — the passive presenter stays a dumb
  reprint (DIAMOND: the marker is the clock).
- No pixel reconstruction, no OCR, no rgba32 dumps from the live path —
  PNG remains a sighted-human artifact; receipts are the machine answer
  to "what was on screen".
- No new per-app code, no new per-app globals — all writers are FILE*
  helpers in the shared renderer reusing existing serializers; managers
  keep publishing their own `_ui.txt` vars (that's the app receipt, this
  is the renderer's).
- No config-file plumbing: the `receipts.conf` policy keys are adopted as
  built-in defaults so the change is zero-friction; a later read of an
  optional `receipts.conf` is explicitly *not* required.

## See also

- `TERMINAL-MIRROR-PARITY-all-windows.md` — the surface this extends
  (steps 1-4 built 2026-09-06: `<pid>.frame.txt`/`.pulse.txt`,
  `khtpm_render_ascii` presenter, `khtpm_kbd_ascii` relay, `--headless`).
- `02-architecture/reference/TPMOS-DIAMOND-render-chain.md` — the marker
  discipline (`pulse`, `frame_changed`) everything here inherits.
- `1.TPMOS_c_+rmmp.0103.0001/projects/wraith-alpha/session/rgb/*.receipt.pdl`
  and `…/wraith-projects/*/session/scene.objects.pdl` — the receipt schema
  this doc adopts.
- `1.TPMOS_c_+rmmp.0103.0001/pieces/config/receipts.conf` — policy keys
  mapped 1:1 (`max_entries`, `only_on_change`, `max_log_lines`).
- `khtpm_core_render.c` `:6147` (`dock_ascii_walk`), `:6222`
  (`dock_write_ascii_frame`), `:6242-6282` (dock history append/rotate),
  `:6298` (`kh_write_ascii_frame`), `:6723` (`dump_frame_png`).
- `HQ-WINDOW-MAP-AND-AGENT-INPUT.md` + `_.0.aigent-testing-k9.txt` — the
  per-PID relay and the agent-input rules.