> **HISTORIC (2026-09-09).** Design conclusions superseded by
> `08-roadmap/design-docs/pchq-vs-tpmos.md` — see `pc-hq-INDEX.md`.
> Kept for archaeology; the investigation notes are still valid.

# pc-hq: legacy (working) vs. current (broken) — comparison + solution plan

**Written 2026-09-08.** The user found a pristine, fully-functioning
pc-hq board in an older clean tree and asked for a thorough diff +
solution doc. **No code changed for this doc** — implementation is
expected to go to grok.

The ask, verbatim:

> "it has a version of pc-hq prior to many edits we have done. its
>  functionality is pure. … it probably predates a refactor but its
>  functioning better. so we want the functionality but keep the
>  x11-hq refactor or w/e it is."

---

## 0. The two trees

| tag | house root | pc-hq board window |
|---|---|---|
| **LEG** (works) | `~/Desktop/🤖️🪤️🏠️/🥡️🪜️/🪜️-00.00/NNEST_CLEAN_PARENT/NNEST-11.17/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz❤️‍🔥️00.17/` | `run_pchq_board_mode()` — ~810 lines of dedicated C in `khtpm_core_render.c` |
| **NU** (broken) | `~/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/` (this repo, `claude` branch) | `pchq-board.xhtpm` (generic template) + `pchq_board_projector.c` + `pchq_board_action.sh`, rendered by the **shared generic renderer** |

Everything below "the current tree" / "NU" means this repo.

---

## 1. The refactor that changed it (git history, `claude` branch)

```
e44431a9  rename khtpm_entity_menu_render.c -> khtpm_core_render.c
f961f5c5  piececraft-hq board: static template + projector + action.sh (pchq refactor steps 2-3)
eff8f11c  renderer: generic Interact Mode (ports tpmos chtpm_parser_pal.c onClick=INTERACT/active_index)
35c1b0b1  khtpm_core_render: delete run_pchq_board_mode(), fully retired   <-- the cut
874c71b5  pchq-board: restore Menu/Player/Fullscreen toolbar items dropped during the xhtpm port
2a7968b6  pchq-board: fix ^ badge; keyboard-grab on Interact Mode (LATER REVERTED, 54b0c564)
6cf14364  scoped feature: remaining event cmds, sample maps, board File→file-hq
ae9e7d14  fix(pc-hq): scoped per-window WM-managed mode + bounded interact grab   <-- 2026-09-08 attempt
c0516dfe  Revert ae9e7d14   <-- reverted same day; see §6
```

**`run_pchq_board_mode()` still exists, verbatim and complete, in LEG.**
It is also recoverable from this repo at
`git show 35c1b0b1~1:x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c`.

---

## 2. What LEG's `run_pchq_board_mode()` actually does

LEG line references are into
`44.xyz❤️‍🔥️00.17/*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c`.

| # | behavior | LEG lines | detail |
|---|---|---|---|
| A | **Its own window, WM-managed** | 8803-8811 | `XCreateSimpleWindow` + `_MOTIF_WM_HINTS` decorations=0. **NOT** `override_redirect`. Its own comment: *"override_redirect windows never get real keyboard/mouse focus routed by Mutter (synthetic XTest input worked, masking the bug)."* |
| B | **`XSetInputFocus` retry — at map AND every frame** | 8826-8832 (startup, 5×), 8943-8948 (per-loop `if (!pchq_focus_ok)`) | Keeps re-asserting `XSetInputFocus(win)` until `XGetInputFocus()` reports `focused == win`. Survives click-away/click-back. |
| C | **Its own event loop, 60 fps cap** | 8931-9210, `usleep(16000)` at 8942 | Fully self-contained. Never enters the generic `hq_run_event_loop`. |
| D | **Board image = direct raw blit, decoupled** | 8969-9000 | Each frame: read `<bv>/pieces/display/rgb_frame_3d_overlay.raw` + its `.receipt.txt` for dims, `XPutImage`. **Independent of `prisc+x` / the projector.** A frozen board-viewer engine → a *stale* image, but the pc-hq window stays fully responsive. |
| E | **Dual-mode key routing** resolved once/frame from `active_gui_is_typing.txt` (`pchq_is_interact_on`, 8406) | 9239-9360 | **Not engaged:** arrows/Tab move local toolbar focus; Enter activates the focused toolbar elem. **Engaged:** keyboard is 100% game input, every key forwarded (incl. Escape). |
| F | **`pchq_append_key` — dual-write** | 8673-8680 | `%d\n` → `player_app/history.txt`; `KEY_PRESSED: %d\n` → `keyboard/history.txt`. |
| G | **Arrow remap to the engine's dialect** | 8658-8668 (`PCHQ_ARROW_*` = 1000-1003), `pchq_map_special_key` | Left/Right/Up/Down → 1000/1001/1002/1003 (matches `bv_menu_input.c`'s `ARROW_*`). Printable keys forwarded as literal ASCII. |
| H | Toolbar: In / File / Desk / Menu / Player / Clock / `!` / `X`; File & Desk open real local dropdowns; `!` = `pchq_toggle_fullscreen`; `X` = `pchq_quit_host_session` (writes `quit_flag.txt`). | 8895-9040, 9340-9430 | |
| I | Mouse always works, even while engaged | 9426-9470 | "Mouse click only while in interact mode" — toolbar clicks reach the local elems regardless of engage state. |

## 3. What NU replaced each with

NU line references are into
`44.xyz.01.00/*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c` unless noted.

| # | NU mechanism | where | gap vs LEG |
|---|---|---|---|
| A | Generic window, **`override_redirect` (house-wide default)** — `#.desktop/livedesk_override_redirect.pdl` = `override_redirect=true`; only `window_is_dock()` opts out. | window-create ~14383; `load_override_redirect` ~186 | **This is the #1 regression.** Mutter/XWayland never routes real hardware keyboard to an `override_redirect` surface — `XGetInputFocus` lies, which is why every `xdotool` test passed while real hardware failed. Proven once already, for this exact window (`git show 35c1b0b1~1`). |
| B | Post-map `XSetInputFocus` retry once (2026-08-29). No per-frame re-assert (deliberately — the flicker regression, ~line 1840). | ~7805 | Focus is not re-taken after a click-away. LEG's per-frame `pchq_focus_ok` loop is gone. |
| C | Shared `hq_run_event_loop` / `hq_dispatch_xevent`. `g_has_canvas` bumps the tick to ~30fps and forces `g_frame_dirty` every tick (1853, 8334, 8349). | | Fine in principle. |
| D | `<canvas id="view" sprite="${canvas_raw}"/>` — `${canvas_raw}` = `<bv>/pieces/display/rgb_frame_3d_overlay.raw`, published by `pchq_board_projector.c`. `kh_draw_canvas` repaints from the raw + receipt each dirty tick. | projector `pchq_board_projector.c:131`; canvas sizing ~4833 | **Decoupled, same as LEG** — the board image is NOT the freeze. A stale board only *feels* frozen because the window itself is dead (A/B). |
| E | Generic Interact Mode: `kh_scan_interact_relay()` (4409) arms `g_interact_relay_on` when the `tb-in` item's `class` contains `interact-active` (projector-published from `active_gui_is_typing.txt`). While armed, `handle_key()` forwards every key to the `relay=` paths and `return`s before local nav (6855). Ports tpmos `chtpm_parser_pal.c`'s `onClick="INTERACT"` / `active_index` model. | 4409-4462, 6855-6890 | Sound design — but **it never fires**, because `handle_key()` only runs on real `KeyPress` events, which an `override_redirect` window doesn't receive (A). |
| F | `pchq_board_action.sh` `append_key()` (shell): `%d\n` → history1, `KEY_PRESSED: %d\n` → history2. The **C** relay path (6855) writes `%d\n` to *both* `relay=` paths. | `@.apps/piececraft-hq/ops/pchq_board_action.sh`; renderer 6883 | The projector points `bv_h1` at **`player_app/interact_relay.txt`** (the file `main_module.pal` actually reads — `read_history pieces/apps/player_app/interact_relay.txt`), `bv_h2` at `keyboard/history.txt` (vestigial). So the C relay writes plain `%d\n` to `interact_relay.txt` — correct — and plain `%d\n` to `keyboard/history.txt` — harmless-but-wrong-format, and unread. Minor; not the bug. |
| G | Same arrow remap 200/201/202/203 → 1002/1003/1000/1001, applied only in the `g_interact_relay_on` block (6857-6866). `kh_key_history_code()` returns literal ASCII for printables. | 6855-6866 | Equivalent to LEG once keys actually arrive. |
| H | Toolbar `<item>`s in `pchq-board.xhtpm` → `pchq_board_action.sh <bv> <verb>`. `!` = `TOGGLE_FULLSCREEN` (generic verb); `x` = `CLOSE`; File/Desk = `show=`-gated `<repeat>` menus from the projector. | `pchq-board.xhtpm` | Feature-equivalent; some items are `action="void"` stubs (Menu/Player/Clock) — matches LEG's stubs. |
| I | Generic mouse click → `handle_mouse` → element activate. | | Same caveat as E: mouse events also under-deliver to `override_redirect` under some Mutter states, though less reliably reproducible. |

---

## 4. Root-cause map — every symptom the user reported

| symptom (user's words) | true cause | LEG immune because |
|---|---|---|
| *"im in interact mode but not able to control camera"* | keys never reach `handle_key()` → `g_interact_relay_on` never forwards → engine gets nothing. Root: **`override_redirect` window (§3-A)**. | LEG window is WM-managed (§2-A) + re-asserts focus every frame (§2-B). |
| *"exit wont leave interact mode"* | same — the `13` toggle is a keypress that never reaches the window. | same. |
| *"refocus issue when i click back to it"* | no per-frame `XSetInputFocus` re-assert (§3-B); `override_redirect` makes even a successful `XSetInputFocus` not route hardware keys. | LEG's `pchq_focus_ok` loop (§2-B). |
| *"used quit to quit tb but its still on screen"* | **separate bug**, already fixed on `claude` (`fix(strip): X.quit … also stops the strip renderer`, 2026-09-08). Not pc-hq. Listed here only so it isn't re-investigated. | n/a |
| board *"freezes"* / stale | `prisc+x`'s `exec_custom_op()` runs custom ops via `popen(cmd,"r")` + read-to-EOF, **no timeout** — a hung/ fd-leaking child (`bv_render_3d`) blocks the whole pal VM (`wchan=pipe_read`). **This bug is byte-for-byte present in LEG too** (`014.wsr-pal💸️📌️+2/system/prisc+x.c` line ~957/968, identical). | LEG's `run_pchq_board_mode` blits the raw frame directly (§2-D), so a frozen engine ≠ a frozen *window*. NU couples the perception: dead focus + stale board = "totally frozen". Fixing A/B alone makes NU tolerate the freeze the way LEG does. A real `prisc+x` fix (muta-neo's `run_op` pattern) is a *separate, optional* follow-up — see §6. |

**board-viewer is identical between LEG and NU** — `pal/main_module.pal`, all of `ops/` (`bv_render_3d.c` / `bv_compose_frame.c` / `bv_menu_input.c`), and the `prisc+x.c` copy board-viewer builds from. The regression is **100% in the pc-hq window layer**, not the board engine.

---

## 5. What to KEEP (the refactor) vs. RECOVER (the behavior)

**Keep, do not undo:**
- `pchq-board.xhtpm` + `pchq_board_projector.c` + `pchq_board_action.sh` as the pc-hq board definition. No return to an 810-line `run_pchq_board_mode()`.
- The generic renderer as the single window engine.
- Generic Interact Mode (`g_interact_relay_on` / `kh_scan_interact_relay`) — it's the correct port of tpmos's `onClick="INTERACT"` model; it just needs keys to arrive.
- The generic toolbar `<item>` / `<repeat>` menus / `<canvas>`.

**Recover (port LEG's behavior into the generic path):**

| recover | how, inside NU's architecture |
|---|---|
| **A. WM-managed window for pc-hq only** | A per-window opt-in that makes *only* the pc-hq board window WM-managed, every other window (dock, dropdowns, db-hq, chat-hai, …) unchanged. Two viable shapes: **(i)** a `<window managed="true">` attribute + a `#.desktop/livedesk_override_redirect.pdl` `managed_windows=1` gate (attempted in `ae9e7d14`, reverted — see §6 for *why it didn't take* and how to finish it); **(ii)** a class check, exactly mirroring `window_is_dock()`'s `elem_has_class(g_window, "dock-header")` — add `elem_has_class(g_window, "managed")` (or reuse the existing `pchq-board-pal` class) at the `dock_managed` site (~14383) so `win_managed = dock_managed \|\| elem_has_class(g_window, "managed")`. Shape (ii) is the safer bet — no new global, no parse-order risk, matches a blessed pattern. |
| **B. Per-frame focus re-assert** | Port LEG's `pchq_focus_ok` loop (§2-B, LEG 8943-8948) into the generic idle tick, gated to `win_managed` (or the pchq window): once per ~N ticks, if `XGetInputFocus() != win`, `XSetInputFocus(win, RevertToParent, CurrentTime)`. Gate it so it does **not** run for override_redirect windows (that's what caused the 2026-09-03 flicker regression) — only a genuinely WM-managed, continuously-interactive window needs it. |
| **C. Board decoupling** | Already present — `kh_draw_canvas` repaints from the raw each dirty tick. **Verify** it never blocks on a projector/receipt read (wrap reads so a missing/short receipt or raw = keep last frame, don't stall or blank; there's a prior `d9afd258` "don't blank on one bad tick" fix — confirm it still covers this). |
| **D. Key format to `keyboard/history.txt`** | Minor. The C relay (6883) writes `%d\n` to every `relay=` path; LEG wrote `KEY_PRESSED: %d\n` to `keyboard/history.txt`. Since `main_module.pal` reads `interact_relay.txt` (not `keyboard/history.txt`), this is cosmetic — but if any consumer ever reads `bv_h2`, either fix the format or drop `bv_h2` from `relay=` entirely (projector already calls it "vestigial"). |
| **E. Escape while engaged** | LEG forwards Escape too while engaged (the engine's native ESC-exit consumes it). NU's `g_interact_relay_on` block forwards *every* key including Escape (6855, no Escape special-case before the forward). Confirm this still holds after A/B land — it's the only in-engine way to disengage. |

**Optional, separate track — the `prisc+x` freeze:** port
mutaclsym-neo (`101.mutaclsym🧟‍♂️️19.00`)'s `game_dispatch.c` `run_op()`
pattern into `exec_custom_op()` — fork + child `stdout/stderr →
/dev/null` + `execl` + `waitpid` with a hard watchdog, replacing
`popen`+read-to-EOF. Attempted as `run_custom_bin()` in
`3eeb5a25`, reverted `4370fc13` (the *pc-hq* symptom was the focus bug,
not the freeze, so the user pulled the risky shared-VM change). It is a
real latent bug in ~10 `prisc+x.c` copies house-wide; worth doing on
its own merits, not as part of the pc-hq fix. See auto-memory
`prisc-x-popen-custom-op-freeze` and `DB-EVENTS-HQ-PORT-DESIGN.md`
context.

---

## 6. Why the 2026-09-08 `managed="true"` attempt (`ae9e7d14`) failed, and how to finish it

`ae9e7d14` added: `apply_attr()` sets a global `g_window_wants_managed`
when it sees `managed="true"` on a `<window>`; `load_override_redirect()`
parses `managed_windows` / `interact_kbd_grab` from the pdl;
`win_managed = dock_managed \|\| (g_managed_windows_enabled &&
g_window_wants_managed)` at the create site.

Live test with `managed_windows=1`: **the pchq-board window never
appeared in Mutter's `_NET_CLIENT_LIST`** (the two strip windows and
co-lab-hai did — `window_is_dock()` forces those managed). So the
window stayed `override_redirect`. Not diagnosed before the revert.
Suspects for grok:

1. **Parse order** — `load_override_redirect()` (main ~14159) runs
   before `parse_chtpm()` (main ~14195), which is correct; and
   `parse_element` *does* route `<window>` attrs through `apply_attr`
   (896 `elem_new(tag)` → 933 `apply_attr`). But `g_window_wants_managed`
   is **never reset** per parse, and a reparse path exists — check it's
   set on the *initial* parse and read at the *initial* create, with no
   intervening reset.
2. **The window may simply not be mapping** — in every diagnostic run a
   large pc-hq board window could not be found in the X tree at all
   (`_NET_CLIENT_LIST` *or* `xwininfo -root -tree`), managed or not.
   Rule this out first: does `pchq-board.xhtpm` produce a visible,
   correctly-sized window on baseline? If not, that's a prior bug that
   masks everything else.
3. **Prefer the class-based shape (§5-A-ii)** and skip the attribute +
   global entirely — it removes both the parse-order and the
   reset-hygiene risk.

The `interact_kbd_grab` half of `ae9e7d14` (a FocusOut-bounded
`XGrabKeyboard`) is a *fallback* if WM-managed focus still proves
flaky under this Mutter/XWayland — cli-io-style grab, released on real
`FocusOut` (the `g_is_cursword` pattern), so it can never lock the
house the way the 2026-09-04 unbounded grab did. Only reach for it if
§5-A + §5-B don't fully fix real-hardware interact.

---

## 6b. 2026-09-08 — §5-A/§5-B attempted by Sonnet, reverted, SECOND bug found

Landed `fb2678ae` (class-gated WM-managed via a `managed` class on
`<window>` + a per-tick `XSetInputFocus` re-assert in `hq_idle_tick()`),
reverted `01f2dbef`. Results:

- **§5-A worked.** With `class="… managed"`, `elem_has_class(g_window,
  "managed")` at the `dock_managed` site, the pchq-board window is
  created WM-managed: `_NET_CLIENT_LIST` includes it, `xwininfo` reports
  `Override Redirect State: no`, `IsViewable`. Strip + dropdowns
  unaffected. **This half of the plan is correct — grok should redo it.**
- **§5-B (focus re-assert) built fine**, no regression observed, but
  couldn't be validated because of the second bug below.
- **The window keeps focus but keys still don't reach Interact Mode.**
  Root cause, isolated live: **`g_interact_relay_on` never arms.**
  `kh_scan_interact_relay()` (renderer ~4430) arms only when a page
  `<item>` has `relay[0]` set AND `elem_has_class(it, "interact-active")`.
  The `tb-in` item's class is `pchq-tb ${interact_class}`, and the
  projector *does* publish `interact_class=interact-active` into
  `state/ui.txt` the moment `active_gui_is_typing.txt` flips to 1
  (verified). **But the live Elem tree keeps `tb-in` = `pchq-tb`
  (no `interact-active`), label `In: off`, indefinitely** — a frame
  dump shows the stale class even 10-20 s after `ui.txt` changed.
  Doing `touch @.apps/piececraft-hq/pchq-board.xhtpm` (forcing a full
  reparse) *immediately* makes the dump show `tb-in` =
  `pchq-tb,interact-active`, `In: ON` — i.e. the render path is fine,
  it's the **live-reparse-on-vars-change that isn't firing** for this
  window.

  `reparse_chtpm_if_changed()` (renderer ~1511) has a `g_vars_path`
  content-hash branch with a two-consecutive-polls debounce
  (`g_vars_hash` / `g_vars_hash_pending`) that is *supposed* to trigger
  a reparse when `state/ui.txt` changes. It is called unconditionally
  from `hq_idle_tick()` (~7771). Consecutive reads of `ui.txt` were
  byte-stable (not per-tick churn defeating the debounce). So one of:
  1. `g_vars_path` isn't actually set to `state/ui.txt` for this window
     (check `kh_find_vars_attr` vs the `vars="state/ui.txt"` attr and
     the `${var}`/`<repeat>` gate in `parse_chtpm` — if the template
     has neither `${` nor `<repeat` at parse time the whole vars
     pipeline is skipped; pchq-board.xhtpm DOES use both, so this
     should be fine — verify).
  2. `database-window` in the class list sets `g_default_persistent=1`
     (~14157) — check whether that (or another db-hq/persistent flag)
     gates the vars-hash reparse OFF anywhere. The reparse comment says
     it's "scoped OFF for db-hq/events-hq/chat-hai"; pchq-board carries
     the `database-window` class and may be catching that exclusion.
  3. The debounce never completes because `g_vars_hash` was seeded
     wrong at startup, or `kh_files_hash` intermittently fails to open
     the file mid-projector-write and the pending hash never matches
     twice.
  4. `pchq_board_projector.c` writes `ui.txt` non-atomically (in-place,
     not tmp+rename) — the debounce exists precisely for that, but if
     the write is *also* changing another field every tick the hash
     never stabilises. (Consecutive reads looked stable in the test,
     but re-check under real interact toggling.)

  **Cheapest correct fix is probably**: make `kh_scan_interact_relay()`
  read the arm signal from the **projector-published var directly**
  (e.g. a `g_vars`-table lookup of `interact_class` /
  `active_gui_is_typing`, or have the projector publish a dedicated
  `interact_armed=1` line) instead of depending on `${interact_class}`
  having been re-substituted into an Elem class by a reparse that isn't
  happening. That decouples "is Interact Mode engaged" from the
  reparse cycle entirely — which is also what legacy did (it read
  `active_gui_is_typing.txt` directly, once per frame, in
  `pchq_is_interact_on()`).

  Either way: **§5-A (WM-managed window) AND this reparse/arm bug both
  have to be fixed** for Interact Mode to work. Neither alone is enough.

## 6c. 2026-09-08 — grok landed both halves (needs hardware confirm)

- **§5-A-ii + §5-B** restored as in `fb2678ae`: `class="managed"` on
  `pchq-board.xhtpm`; `win_managed = dock_managed || elem_has_class(...,
  "managed")`; per-tick `XSetInputFocus` while pointer is over the window.
- **§6b arm:** `hq_idle_tick()` reloads `vars=` every tick and
  `kh_scan_interact_relay()` arms from `interact_class` /
  `interact_armed` / `bv_h1` projector keys, not from a reparsed Elem
  class. Projector also publishes `interact_armed=0|1`.
- Running windows must be **relaunched** to pick up the renderer rebuild.
- Real-keyboard click-away / click-back / arrows / `13` is still the
  owner's check — synthetic XTest is not evidence.

## 7. Suggested sequencing for grok

1. **Confirm the window maps at all** on baseline (`pchq-board.xhtpm`,
   no other changes) — size, position, visible. If not, fix that first.
2. **§5-A (ii)** — class-gated WM-managed for the pc-hq board window
   only. Verify: pc-hq board shows in `_NET_CLIENT_LIST`; strip
   dropdowns / db-hq / chat-hai / co-lab-hai all still work (this is the
   regression the 2026-09-04 house-wide flip caused — test it
   explicitly).
3. **§5-B** — per-frame focus re-assert for that window. Verify on
   **real hardware** (synthetic input masks this whole bug class):
   interact ON → click another window → click back → arrows move the
   camera → `13` / the In toggle disengages.
4. **§5-C / §5-D / §5-E** — verify canvas never blanks/stalls; tidy the
   `bv_h2` format or drop it; confirm Escape still disengages.
5. Only if 3 still flakes on hardware: **§6** bounded `XGrabKeyboard`.
6. Separate PR, own merits: **§5-optional** `prisc+x` `run_op` port.

## 8. File / line index for the diff

**LEG** (`.../NNEST-11.17/.../44.xyz❤️‍🔥️00.17/`):
- `*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c`
  - `run_pchq_board_mode()` — **8709-9520**
  - `pchq_*` helpers — 8366-8707 (`pchq_is_interact_on` 8406,
    `pchq_map_special_key` 8663, `pchq_append_key` 8673,
    `pchq_quit_host_session` 8574, `pchq_toggle_fullscreen` 8511)
  - `g_is_pchq_board` / `class="pchq-board"` dispatch — 938, 17379-17383
- `@.apps/piececraft-hq/pchq-board.chtpm` — the 1-line stub
- `@.apps/piececraft-hq/button.sh` — launches `pchq-board.chtpm`

**NU** (this repo):
- `*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c`
  - `load_override_redirect` ~186; window create + `dock_managed`
    ~14383; `render_managed_wm_hints` call ~14400
  - `kh_scan_interact_relay` 4409-4462; `g_interact_relay_on` forward
    6855-6890; `kh_key_history_code` (~5710 legacy numbering)
  - post-map focus retry ~7805; `g_has_canvas` 1853 / 8334 / 8349
- `@.apps/piececraft-hq/pchq-board.xhtpm`, `pchq-board.css`
- `@.apps/piececraft-hq/ops/pchq_board_projector.c` — `bv_h1`/`bv_h2`
  154-155, `canvas_raw` 131, `interact_class` 184
- `@.apps/piececraft-hq/ops/pchq_board_action.sh` — `append_key`,
  `engage_if_needed`, `restore_interact`, verbs
- `#.desktop/livedesk_override_redirect.pdl` — `override_redirect=true`
- Recover deleted C: `git show 35c1b0b1~1:…/khtpm_core_render.c`
- Prior art / context: `09-appendix/pc-hq-bugs.md` (Bug 2, and the
  2026-09-04 "override_redirect is the deeper root cause" update — this
  doc supersedes its "recommended next step" with the concrete plan
  above), `09-appendix/PLAN-pchq-interact-camera-pov.md` (arrow-code
  remap, Part A — already landed as `eff8f11c`/`cb6aee7b`),
  `03-pitfalls/X11-AND-SESSION-PITFALLS.md` (the house-wide flip
  incident, and the DISPLAY-WIDE grab warning).

## 9. Testing method (mandatory)

Per `pc-hq-bugs.md` and the `khtpm-house-standards` skill: this bug is
**invisible to `xdotool` / XTest** — synthetic input reaches an
`override_redirect` window fine, real hardware does not. Every
checkpoint must be verified with a real keyboard by the user, or via
the per-pid file relay (`#.desktop/entity_menu_history/<pid>.txt`)
*plus* a real `_NET_CLIENT_LIST` / `xwininfo -id` check that the window
is genuinely WM-managed. Do not declare it fixed from a synthetic test.
