# pc-hq board Interact Mode vs TPMOS fuzz-op — parity study

**Status: RESEARCH + PLAN (2026-09-09).** Direct instruction: pc-hq's
Interact Mode is broken (Esc dead, starts armed, starts with a nav
highlight too, auto-exit gone). Compare against the golden reference
(`1.TPMOS.../projects/fuzz-op` + `pieces/chtpm/plugins/chtpm_parser.c`)
and the last-known-good pc-hq
(`NNEST-11.17/.../@.apps/piececraft-hq`, `run_pchq_board_mode()`), find
every pipeline disparity, plan for parity.

---

## 0. The golden model (fuzz-op / TPMOS `chtpm_parser.c`)

`fuzz-op/layouts/fuzz-op.chtpm`:
```
<panel>
  <module>${module_path}</module>
  <interact src="pieces/apps/player_app/history.txt" />
  ...
  <button label="Control Map" onClick="INTERACT" />
```

**One process — `chtpm_parser.c` — owns the entire Interact state
machine.** It:
- reads `pieces/keyboard/history.txt` (**one input file**, lines
  `KEY_PRESSED: <decimal>`) in a 60 Hz (`usleep(16667)`) loop,
- calls `process_key(key)` for every key,
- `process_key` is a two-mode machine keyed on `active_index`:
  - **`active_index == -1` (nav):** arrows/digits/Enter move
    `focus_index`; Enter on an `onClick="INTERACT"` element does
    `active_index = focus_index`.
  - **`active_index != -1` (active/engaged):** the FIRST check is
    `if (key == ESC_KEY /*27*/ ...)` → walk to the nearest `ACTIVATE`
    ancestor, `active_index = p` (→ -1), `export_active_index()`.
    Otherwise, for an `onClick="INTERACT"` element:
    `inject_raw_key(eff)` (arrows remapped to 1000-1003) to the
    `<interact src=…>` file — that's the ONLY thing forwarded to the
    game.
- `export_active_index()` writes `pieces/display/active_gui_is_typing
  .txt` = `active_index != -1 ? 1 : 0`. **This file is the single
  source of truth for "is Interact engaged".**

**Escape is consumed by the parser, locally, before anything is
forwarded. There is no focus flag, no window manager involvement, no
second state machine, no auto-exit-on-idle. Engage = Enter on the
INTERACT button; disengage = Esc. Both are just keys on the one input
file.**

---

## 1. The last-known-good pc-hq (`run_pchq_board_mode()`, NNEST-11.17)

Deleted from the current tree (2026-09-04) but its design comment is the
spec. Header: *"steal everything, reimplement nothing … forward EVERY
real key/click into board-viewer's own relay files … zero local nav
logic of this file's own."*

Key routing (`khtpm_core_render.c` old, ~line 9239), decided by
**`pchq_interact_on`** — a per-frame fresh read of
`pchq_is_interact_on(bv_session)` = the engine's own
`active_gui_is_typing.txt`:

| Mode | Behaviour |
|---|---|
| dropdown open (File/Desk) | local dropdown nav; Enter → `pchq_append_key(h1,h2,'5'/'6')` (sends `13` first if not engaged) |
| **`!pchq_interact_on`** | **local toolbar nav** (`pchq_focus` over In/File/Desk/Menu/Player/Clock/!/X); digits 1-8 jump; Enter activates. **Never touches the engine relay.** "In" activate → `pchq_append_key(h1,h2,13)` |
| **`pchq_interact_on`** | **keyboard is 100% game input** — every key `pchq_append_key(h1,h2,<code>)`, **including Escape** ("the legacy engine's own native ESC-exit consumes it BEFORE this project's ops ever see it — zero local interception needed here") |

`pchq_append_key(history1, history2, key)` — **dual-format write**:
```c
fprintf(history1, "%d\n", key);              // player_app/history.txt  (bare)
fprintf(history2, "KEY_PRESSED: %d\n", key); // keyboard/history.txt    (prefixed)
```
`history2` MUST be the `KEY_PRESSED:`-prefixed form — that is the only
format `chtpm_parser*.c`'s main loop (`strstr(line,"KEY_PRESSED: ")`)
will parse.

So the last-good pc-hq matched the golden model: **no renderer-side
Interact state machine.** The only local flag, `pchq_interact_on`, is a
read-only per-frame mirror of the engine's `active_gui_is_typing.txt`.

---

## 2. The current pc-hq (`pchq-board.xhtpm` + `khtpm_core_render.c`)

`pchq-board.xhtpm`:
```
<window class="pchq-board-pal database-window managed" vars="state/ui.txt">
  <module src=".../pchq_board_projector.+x" args="piececraft-hq"/>
  <item id="tb-in" class="pchq-tb ${interact_class}" label="In: ${interact_label}"
        relay="${bv_h1},${bv_h2}"
        action="'…/pchq_board_action.sh' '${bv_session}' 'interact'"/>
  ...
  <canvas id="view" sprite="${canvas_raw}"/>
```

Pipeline (5 processes, 3 poll loops in the key path):

```
key ─X11─▶ khtpm_core_render (board window)
             handle_key():  if (g_interact_relay_on && g_x11_window_focused)
                              → write "%d\n" to g_interact_relay_paths[]  (bv_h1, bv_h2)
                            else → local nav
       ┌─── g_interact_relay_on  ← kh_scan_interact_relay() reads PROJECTOR vars
       │                            interact_armed / interact_class
       │
       ▼
   pchq_board_projector (300 ms loop)  reads bv_session/…/active_gui_is_typing.txt
                                       publishes interact_armed, bv_h1, bv_h2, …
       ▲
       │  active_gui_is_typing.txt  ← written by …
       │
   chtpm_parser_pal  board_viewer.chtpm   (60 Hz)  ← reads keyboard/history.txt
                                                     (KEY_PRESSED: only)  owns active_index
   prisc+x  main_module.pal               (~30 ms) ← reads player_app/interact_relay.txt (bare)
```

Plus `74488d53` (grok, 2026-09-08): `g_x11_window_focused` (FocusIn/Out),
`handle_key` gate `g_interact_relay_on && g_x11_window_focused`,
FocusOut(NotifyNormal) → `kh_interact_disengage_engine_if_on()` →
`kh_interact_append_13()`.

---

## 3. Disparities (current vs golden / last-good)

| # | Golden / last-good | Current | Consequence |
|---|---|---|---|
| **D1** | Interact state owned ONLY by the engine parser (`active_index`) | Renderer keeps its OWN mirror `g_interact_relay_on` **and** gates it with `g_x11_window_focused` **and** adds `kh_interact_disengage_engine_if_on` **and** grok FocusOut→13 | a second, lagged, WM-coupled state machine that desyncs from the engine |
| **D2 (root cause of "Esc dead / stuck / starts armed")** | `pchq_append_key` writes `KEY_PRESSED: %d` to `keyboard/history.txt` | `handle_key`'s forward loop **and** `kh_interact_append_13()` write **bare `%d\n` to BOTH** paths | `keyboard/history.txt` gets `27` / `13` with no `KEY_PRESSED:` prefix → the `board_viewer.chtpm` parser's `strstr("KEY_PRESSED: ")` **ignores every forwarded key** → its `process_key(27)` ESC-exit NEVER runs → `active_index` stuck on the INTERACT button → `active_gui_is_typing.txt` stuck at 1 → `interact_armed=1` forever → every relaunch starts armed, no key ever disengages |
| **D3** | engaged-state source = fresh per-frame read of the engine's `active_gui_is_typing.txt` | `g_interact_relay_on` from the projector's `interact_armed`, a 300 ms-lagged copy of the same file, through `kh_scan_interact_relay()` | lag + a feedback loop (renderer forward → parser typing → projector → renderer arm) |
| **D4** | Escape FORWARDED to the engine; engine exits | Renderer intercepts Escape (`g_x11_window_focused` gate; my 2026-09-09 patch reroutes it to send `13`) | when the focus flag is stuck at 0, Escape is not sent at all; even when sent, D2 means the parser never sees it |
| **D5** | no focus flag at all | `g_x11_window_focused`, set 0 on any real FocusOut(NotifyNormal) for a `managed` window; Mutter/XWayland fires these spuriously | key-forward stops while the window still holds focus → "stuck", toolbar still responds |
| **D6** | non-engaged → local toolbar nav, digits/arrows, never the relay | while `interact_armed` is (wrongly, D2) stuck at 1, `handle_key`'s gate eats every key → **no local nav possible** → "starts on nav 1 and can't move" | the "2 navs at once" the user sees = the toolbar's default `g_focus_nav=1` highlight that can never move, next to a stuck ON badge |
| **D7** | there is no "auto-exit on inactivity" — you just leave, the engine keeps its state, `pchq_interact_on` reflects it next time | grok's FocusOut→13 auto-disengage (new concept, `74488d53`) | fine in principle, but D2 makes its `13` invisible to the parser, so it silently does nothing = "we lost the grok fix" |
| **D8** | one input file (`keyboard/history.txt`), `KEY_PRESSED:` format | keys split to `bv_h1` = `player_app/interact_relay.txt` (bare) **and** `bv_h2` = `keyboard/history.txt` (should be prefixed). `interact_relay.txt` ≠ the golden `player_app/history.txt`; the pal-VM camera reads it directly, bypassing the parser | camera moves (VM reads bare from interact_relay.txt) but the parser's INTERACT machinery is starved (D2) |
| **D9** | toolbar + game in ONE chtpm, one parser, `active_index` routes | toolbar in the X11 renderer's xhtpm, game in a separate board-viewer session | the renderer must re-derive "engaged?" to know whether a key is toolbar-nav or game-input — the entire class of D1-D7 bugs |

---

## 4. Parity plan

Goal: the board window is a **dumb frame-blitter + key-forwarder**, exactly
`run_pchq_board_mode()`'s "steal everything, reimplement nothing". The
board-viewer engine parser owns Interact. No renderer state machine.

### PR-1 — fix the write format (the one bug behind Esc/stuck/starts-armed)
`khtpm_core_render.c`, the interact-forward loop **and**
`kh_interact_append_13()`: per target path,
- path ends `/keyboard/history.txt` (or any `…/history.txt`): write
  `KEY_PRESSED: %d\n`
- else (`interact_relay.txt`): write `%d\n`

Mirrors `pchq_append_key`. This alone makes the parser's ESC-exit and
grok's FocusOut→13 actually reach the engine → Esc works, auto-exit
works, and the stuck `active_gui_is_typing.txt` clears on the first Esc.

Also revert the 2026-09-09 "reroute Esc to send 13" patch — forward `27`
plainly (golden behaviour); the parser exits on `27`.

### PR-2 — drop the renderer-side state machine, mirror the engine
- `g_interact_relay_on` / `kh_scan_interact_relay()`: keep as a
  **read-only per-frame mirror** of `active_gui_is_typing.txt` (read the
  file directly, like `pchq_is_interact_on()` did — not the 300 ms
  projector var). It gates ONLY "is a key toolbar-nav or game-input",
  nothing else.
- **Delete** the `&& g_x11_window_focused` half of the `handle_key`
  gate, `g_x11_window_focused`, and grok's FocusOut→`kh_interact_
  disengage_engine_if_on()`. There is no focus-loss auto-exit in the
  golden model; if one is still wanted it belongs as a *deliberate*
  later feature, not load-bearing for basic Esc.
- Keep the `In:` badge projector-driven (display only).

### PR-3 — one input file / correct target
- Projector: publish `bv_h1 = <sess>/pieces/apps/player_app/history.txt`
  (the golden target), not `interact_relay.txt`. If the pal-VM camera
  genuinely needs `interact_relay.txt`, the parser's own
  `inject_raw_key()` already writes there — the renderer should not.
- Ideally the renderer forwards to `keyboard/history.txt` ONLY and lets
  the parser fan out (golden model). Dual-write is a fallback.

### PR-4 (optional, biggest) — collapse the toolbar into the engine
Move In/File/Desk/Menu/Player/Clock into `board_viewer.chtpm` as real
`<button>`s so `active_index` routes everything and the renderer has
zero nav logic — true single-parser parity (D9). Large; only if PR-1..3
don't fully settle it.

### One-time
Reset the currently-stuck engine: write `0` to the live
`<bv_session>/pieces/display/active_gui_is_typing.txt` (and, once PR-1
lands, a single Esc will do it).

---

## 5. KPIs
- Fresh board window opens **not** armed (toolbar nav works, `In: off`).
- Click "In" → armed; arrows move the camera; Esc → `In: off`, toolbar
  nav back — **every time**, no relaunch.
- Click away and back → whatever state the engine was in (no surprise
  auto-changes unless PR-2 keeps a deliberate focus-exit).
- `active_gui_is_typing.txt` never sticks; no relaunch inherits `armed`.
- `khtpm_core_render.c` has **no** pc-hq-specific Interact state beyond a
  read-only `active_gui_is_typing.txt` mirror.

## 6. Sources
- `1.TPMOS.../projects/fuzz-op/layouts/fuzz-op.chtpm`,
  `pieces/chtpm/plugins/chtpm_parser.c` (`process_key` ~2707-3020, main
  loop ~3055-3150, `export_active_index` ~2240).
- `NNEST-11.17/.../@.apps/piececraft-hq` + that tree's
  `khtpm_core_render.c` `run_pchq_board_mode()` (~8643-9450),
  `pchq_append_key` (~8673).
- current: `@.apps/piececraft-hq/pchq-board.xhtpm`,
  `ops/pchq_board_projector.c`, `ops/pchq_board_action.sh`;
  `khtpm_core_render.c` `handle_key` (~7027+), `kh_scan_interact_relay`
  (~4510), `kh_interact_append_13` (~4589), FocusIn/Out (~8369-8420).
- `PC-HQ-FOCUS-AND-INTERACT-ACTIVATE.md` (grok `74488d53`),
  `pc-hq-leg-vs-nu-fix.md`, `PLAN-pchq-interact-camera-pov.md`.

---

# APPENDIX A — deep pipeline / meta-comparison (2026-09-09)

*Direct instruction: "the movement of xelector in pc-hq is still much
slower and laggier than expected … there maybe a more deep insidious
structural issue where the entire x11-hq house deviated from the tpmos
diamond pipeline standard at some point."*

**It did. There are TWO generations of the render/input loop in this
house, and pc-hq's 3D board sits on the OLD one at every layer.**

## A.1 The TPMOS "diamond" pal-VM game loop (the standard)

`101.mutaclsym…19.00/pal/game_module_3d.pal` — the loop mutaclysm-neo /
lpns+map+4 run, the ones the user calls fast:

```
compose_frame          # once, at startup
muta_render_3d
compose_rgb_frame
hit_frame
loop:
  exec ./ops/+x/game_dispatch     # ONE consolidated op
  sleep 16667                     # 60 Hz
  j loop
```

`game_dispatch.c` header: *"One-shot op: read **ALL** keys from relay,
dispatch each, run NPC auto-play, compose frame, signal renderer.
Architecture: read all -> dispatch all -> NPC -> render -> exit."*

So the diamond is: **drain the whole input queue, apply every key,
render exactly once, tick at 60 Hz.** Holding an arrow key advances one
step per key *per tick* (many steps land in one 16.6 ms frame) — smooth.

## A.2 What board-viewer / pc-hq actually runs (the OLD "civ-txt clone"
loop)

`&.widgits/board-viewer/pal/main_module.pal` (byte-for-byte the same
shape as `@.apps/civ-txt/pal/main_module.pal`, `my-chara-txt`,
`piececraft-xyz` — the whole civ-txt clone lineage):

```
loop:
  bv_menu_input x9                       # "tick" (fork+exec+wait)
  read_pos  bv_screen_changed.txt
  beq -> check_key ; else -> render
check_key:
  read_history interact_relay.txt x2,x1  # exactly ONE key
  beq x2,x0 -> no_key
  bv_menu_input x2                        # process that ONE key (fork)
  j render
no_key:
  sleep 30000                            # 33 Hz
render:
  bv_render_3d                            # FULL per-pixel DDA raymarch, fork
  bv_compose_frame                        # fork
  hit_frame                               # fork
  sleep 30000                             # 33 Hz
```

Every deviation from A.1, in one loop:

| # | Diamond (A.1) | board-viewer (A.2) | Cost per xelector move |
|---|---|---|---|
| **P1** | drain **all** queued keys per tick | **one** `read_history` → **one** key per iteration | holding arrow = 1 move / iteration, not N / frame |
| **P2** | `sleep 16667` (60 Hz) | `sleep 30000` (33 Hz), **and** a second `sleep 30000` after render | ≥30 ms floor per move, ~60 ms round trip |
| **P3** | one consolidated `game_dispatch` op/tick | `bv_menu_input` + `bv_render_3d` + `bv_compose_frame` + `hit_frame` = **4 fork+exec+waitpid** per iteration (`prisc+x.c` `run_custom_bin`) | 4 process spawns per move |
| **P4** | `muta_render_3d` / `compose_rgb_frame` run once at boot; loop only signals | **`bv_render_3d` (raymarch) runs EVERY render branch**, unconditionally | a full DDA raymarch (OpenMP, still 10-30 ms) per move |
| **P5** | renderer = compiled C, marker-pulsed, in-process | see A.3 | — |

Net: one held-arrow step ≈ `30 ms sleep + 4×fork + raymarch` ≈
**50-90 ms → 11-20 moves/s**, vs the diamond's smooth 60. This is the
xelector lag, and it is **structural**, not tuning.

## A.3 The x11-hq renderer also deviates

`khtpm_core_render.c` event loop (`hq_run_event_loop`):

```
struct timeval tv = (g_has_canvas || window_is_dock())
                        ? { 0, 33000 }      /* 33 Hz */
                        : { 0, 150000 };    /* 6.6 Hz */
select(...);
...
if (g_has_canvas && !g_quit) g_frame_dirty = 1;   /* repaint EVERY tick */
if (g_frame_dirty && !g_quit) { g_frame_dirty = 0; redraw(); }
```

- **33 Hz, not 60.**
- **`g_frame_dirty = 1` unconditionally every tick** for a canvas window
  → `redraw()` (re-read the `.raw`, XPutImage) 33×/s whether the frame
  changed or not. TPMOS `pieces/display/renderer.c` repaints **only when
  `frame_changed.txt` grows** (append-only pulse marker). This is
  poll-and-repaint, not pulse-driven — the exact "DO NOT set dirty=1
  directly" the TPMOS parser header warns against, ported inside-out.
- Two input paths (direct X `KeyPress` → `handle_key`, **and**
  `poll_agent_history()` on `entity_menu_history/<pid>.txt`) instead of
  the single `keyboard/history.txt` → parser-drain the diamond uses.

## A.4 Other spiritual discrepancies found

- **Compositor bypass.** Diamond: `keyboard → parser → marker →
  renderer → ONE composited `rgb_frame.raw` (via `chtpm_rgb_render` /
  `compose_rgb_frame`). pc-hq's `<canvas sprite="${canvas_raw}">` blits
  `rgb_frame_3d_overlay.raw` **directly** — skips the shared compositor.
  Faster, but it means the text chrome and the 3D view are composed by
  two unrelated code paths (`khtpm_draw_core` vs `bv_render_3d`) instead
  of one.
- **Interact state across 4 processes + a 300 ms projector** (body of
  this doc, D1-D9) instead of one parser owning `active_index`.
- **`interact_relay.txt` vs `player_app/history.txt`.** The diamond /
  last-good pc-hq relays into `player_app/history.txt` (+ `KEY_PRESSED:`
  into `keyboard/history.txt`); current pc-hq relays into a bespoke
  `interact_relay.txt` that only the pal-VM reads, bypassing the
  parser's `process_key` (see D8, and the 2026-09-09 format bug D2).

## A.5 Parity plan — the loop (supersedes nothing in §4; adds P-5..7)

### P-5 — port board-viewer to the `game_dispatch` diamond loop
New `&.widgits/board-viewer/ops/bv_dispatch.c` — one-shot, matching
`101.mutaclsym…/ops/game_dispatch.c`:
1. read **every** pending line of `interact_relay.txt` (advance cursor),
2. apply each to camera / xelector (`bv_menu_input`'s move logic, inlined
   or one `bv_menu_input` call per key but no render between),
3. if anything moved → `bv_render_3d` **once**, `bv_compose_frame` once,
   write the render marker,
4. exit.

`main_module.pal` becomes:
```
loop:
  exec ./ops/+x/bv_dispatch
  sleep 16667
  j loop
```
Shared widget → civ-txt / piececraft-xyz / my-chara-txt inherit the
same speed-up (they run the identical `main_module.pal` shape). Land it
behind a `bv_dispatch` presence check so a project without the new op
falls back to the old loop.

### P-6 — `bv_render_3d` only on real change
`bv_dispatch` skips the raymarch when neither camera nor board changed
this tick (compare a cheap hash / the `bv_screen_changed` size it
already reads). The raymarch is the single most expensive step (P4).

### P-7 — marker-drive the khtpm canvas
`khtpm_core_render.c`: for a `g_has_canvas` window, stat
`<canvas dir>/rgb_frame*_changed.txt` (or the `.raw` size) and set
`g_frame_dirty` **only on growth**, not every tick. Optionally drop the
select timeout to `16667`. Removes 33 redundant `redraw()`s/s.

### Sequencing
P-1..4 (this doc §4, the format/ownership fixes) are landed / small.
**P-5 is the big one for xelector lag** and is a board-viewer refactor
(shared, needs its own test pass across civ-txt/piececraft-xyz). P-6/P-7
are follow-ons. None of P-5..7 touch Interact semantics.

## A.6 KPI for the loop work
- Holding an arrow in pc-hq Interact = smooth ≥ 50 moves/s (parity with
  mutaclysm-neo on the same box).
- `ps`/`perf` during a held move: **one** `bv_dispatch` spawn per 16 ms,
  not 4 ops per 30 ms; `bv_render_3d` not spawned on idle ticks.
- `khtpm_core_render` `redraw()` count while the board is idle ≈ 0/s
  (was ~33/s).

## A.7 Sources (appendix)
- `101.mutaclsym…19.00/pal/game_module_3d.pal`, `game_module.pal`;
  `ops/game_dispatch.c` (header + `run_op` fork pattern).
- `@.apps/civ-txt/pal/main_module.pal` (same shape as board-viewer).
- `&.widgits/board-viewer/pal/main_module.pal`, `default_op.txt`,
  `ops/bv_render_3d.c` (DDA raymarch + `-lomp`).
- `&.widgits/_shared-lib/system/prisc+x.c` `exec_custom_op` /
  `run_custom_bin` (~1012-1105).
- `khtpm_core_render.c` `hq_run_event_loop` (~8524-8610).
- `1.TPMOS…/pieces/display/renderer.c`,
  `pieces/chtpm/plugins/chtpm_parser.c` main loop (marker-pulse
  discipline).

---

## APPENDIX B — P-5 landed (2026-09-09)

`&.widgits/board-viewer/`:
- **NEW `ops/bv_dispatch.c`** — the one-shot diamond op (drain ALL of
  `interact_relay.txt` → `bv_menu_input.+x` per key → `bv_render_3d` +
  `bv_compose_frame` + `frame_changed` marker ONCE; nothing on an idle
  tick). Direct port of `game_dispatch.c`'s shape.
- **`pal/main_module.pal`** → `loop: exec ./ops/+x/bv_dispatch.+x ;
  sleep 16667`. Old loop kept verbatim as `pal/main_module_legacy.pal`
  (rollback = swap the filename back).
- `scripts/build.sh` builds `bv_dispatch.+x`.

Affects every board-viewer consumer (pc-hq, and anyone who opens "View
Board" from civ-txt / piececraft-xyz — those projects' OWN
`pal/main_module.pal` are separate files, untouched, still on the old
loop until ported).

**Measured (fresh board-viewer session, 60 keys injected as one burst):**
- diamond loop: **~150 keys/s drained, 3 renders for 60 keys**.
- old loop: ~11-20 moves/s, one render per key.
- **≈10× throughput**, render count down from 1-per-key to ~1-per-20-keys.

Remaining cost is the per-key `bv_menu_input.+x` fork (60 forks for 60
keys). **P-6** (batch keys into one `bv_menu_input` call, or inline its
~200-line move core) would close most of the rest — its own follow-up.
**P-7** (marker-drive the khtpm canvas repaint) still open.

End-to-end held-key xelector A/B in the live pc-hq window is pending the
`button.sh engine` split (PIECECRAFT-HQ-LAUNCH-STANDARDIZE.md steps 2-3)
— the current `button.sh run` chain is too flaky under the detached
taskbar launch to bring the full game+board stack up reliably for a
clean measurement.
