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
