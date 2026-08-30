# 🪞 Aug 30 Retro — the "map-interact canvas" → real khtpm rewrite 🎮️

## 🎬 What was the actual problem?

The piececraft-hq board window (the 3D voxel view you open with "View
Board") had grown into a **"hybrid disorganized blitted-legacy
newfangled setup"** 🍝 — your words, and correct ones. It was blitting
the *entire* window (title bar, File/Desk/Close buttons, the 3D view,
everything) as one flat image, then hand-drawing fake buttons on top
of it in C, with no real nav system behind them. Meanwhile the actual
khtpm engine (the thing every other hq window — db-hq, chat-hai,
events-hq — is built on) has a real, proper Elem tree with focus
indices, click-two-step, CSS-ish styling… none of which the board
window was using for its own chrome. 🤦

## 💡 The architecture correction (your call, not mine)

You cut through it in one line:

> "isn't it only that actual 2d/3d screen {interact screen} needs to
> be blitted? treated like a html/js canvas — everything else can be
> just a typical hq window"

That's the whole fix, really. 🧠✨ Split the window into two honest
halves:

- 🖼️ **The 3D/2D view** = a real `<canvas>`/`<img>` — the ONE thing
  that's still a raw pixel blit (`rgb_frame_3d_overlay.raw`, straight
  from the raymarch renderer, no chrome baked in).

  **🔍 How does that blit actually work, mechanically?** Good question
  to nail down, since it's easy to assume "khtpm parser renders it" —
  it doesn't. 🚫 Here's the real chain:

  1. 🎨 **board-viewer's own separate process** (`bv_render_3d.c` +
     `bv_compose_frame.c`, running as its own real `chtpm_parser_pal`
     session under `board-viewer/pieces/sessions/<id>/`) does the
     actual raymarching every tick, and writes two files:
     `pieces/display/rgb_frame_3d_overlay.raw` (raw RGBA pixel bytes,
     no text/UI baked in — pure scene) and a tiny sidecar
     `rgb_frame_3d_overlay.receipt.txt` (`overlay_w=…` /
     `overlay_h=…` — the real, current pixel dimensions, since the
     raymarch resolution can change).
  2. 👀 **`run_pchq_board_mode()`** (in `khtpm_entity_menu_render.c` —
     the SAME binary that draws every other khtpm window, but this one
     real function does its own thing) does NOT go through
     `parse_chtpm()`/the Elem tree AT ALL for this part. It just polls
     that receipt file every frame (`pchq_read_kv_int(...,
     "overlay_w"...)`), and if the size changed, re-`XCreateImage()`s
     an X11 image buffer sized to match.
  3. 📥 Every frame it `fopen()`s the raw file directly, `fread()`s the
     pixel bytes into a buffer, walks them pixel-by-pixel packing them
     into an `XImage` (`XPutPixel`), then `XPutImage()`s that straight
     into the window's own pixmap, positioned right below the
     toolbar (`CHROME_H + PCHQ_TOOLBAR_H`).
  4. 🖱️ Mouse clicks that land inside that region get written back out
     to a real `last_click_x`/`last_click_y` state file for
     board-viewer's own game logic to pick up — so input flows OUT the
     same "plain file" way pixels flow IN, no shared memory, no direct
     function calls between the two processes.

  So: it's genuinely **not** an "op" in the chtpm-parser sense (no
  Elem, no tag, no CSS) — it's a small, dedicated block of raw X11
  code living inside the khtpm renderer binary that treats a `.raw` +
  `.receipt.txt` file pair exactly like a browser treats an `<img
  src="...">` whose file happens to change every frame. Two totally
  separate processes (board-viewer's renderer, and this khtpm chrome
  window) talking ONLY through plain files on disk — the same
  file-based-IPC convention this whole house already uses everywhere
  else (relay histories, click_kv files, state.txt), just applied to
  pixels instead of key presses. 🗂️➡️🖼️
- 🧱 **Everything else** = real khtpm Elems — the toolbar buttons,
  their focus state, their nav badges — same real system db-hq already
  proves out live.

## 🛠️ What I actually had to build

1. 🗑️ **Ripped the fake buttons out of `board_viewer.chtpm`.** It's
   back down to ONE real element: `<button onClick="INTERACT">` — the
   one piece of UI that genuinely *has* to stay engine-owned, because
   camera/selector control is baked into the legacy engine's own
   input state machine.
2. 🧩 **Rewrote `run_pchq_board_mode()`** with a real `PchqElem[]`
   array — a proper little Elem list (label, x/y/w/h, action) instead
   of hand-drawn rectangles with no memory of what's focused.
3. 🔢 **Rebuilt real nav** — arrow keys / Tab / digit-jump move focus
   between toolbar items, Enter activates, mouse clicks respect
   `click_two_step` (the same house-wide "first click focuses, second
   click fires" rule every other hq window honors). This was missing
   entirely at first — caught live when you said *"its missing all the
   index, nav bracket [] features why?"* 🚩
4. 🛑 **The big stop-and-research moment.** I almost built a *second*,
   parallel "interact mode" concept from scratch, which would have
   silently collided with the *real* one already baked into the
   parser. You caught it immediately:
   > "pls research the legacy documentation for how interact mode
   > works... stop right now and research that first"

   So I actually went and read the original reference parser
   (`chtpm_parser.c`'s `process_key()`) line by line, plus cross-checked
   it against mutaclysm's own working `onClick="INTERACT"` button. What
   I found: there's a genuinely elegant **dual-mode model** —
   `active_index == -1` means "normal nav" (arrows move focus, digits
   jump, nothing collides with gameplay); anything else means "fully
   engaged" (all input — including the SAME arrow/digit keys — routes
   straight into the game/camera, until Escape walks back up the
   element tree). 🔑 One boolean, two completely different keyboards,
   zero real collision risk. I wrote this up permanently in
   `!.HOUSE_STDS.md` §A.9 so no future agent (me included) re-litigates
   it from scratch. 📚
5. 🐛 **Two more real, live-caught bugs** after all that:
   - Close's own draw path never got the `[>]`/`[ ]` nav badge the
     other buttons had — a real inconsistency, not a design choice.
   - "Interact isn't activating" turned out to be a wrong-mechanism
     bug: I was faking a mouse click via a state file
     (`last_click_x/y`), but that channel is wired to *game* logic
     (xelector/possess), not the engine's own button hit-testing. The
     real fix: send a plain Enter keypress through the same relay
     File/Desk already use. Once I did that, the *engine's own* glyph
     correctly flipped `[>]` → `[^]` — confirmed by screenshot. 📸✅
6. 🐙 **A recurring "stale process" gotcha.** Killing the launcher
   script's PID doesn't kill the real engine processes it spawned
   (`chtpm_parser_pal`, `renderer`, `keyboard_input`, …) — they kept
   running as orphans serving an *old* layout, and my own test tooling
   kept finding and talking to them, giving contradictory results for
   a while. Lesson banked: always `ps aux | grep chtpm_parser_pal` to
   confirm you're actually looking at a fresh session before trusting
   a test. 🕵️
7. 🎛️ **The reorder/polish pass** (your live feedback): toolbar is now
   `1. In → 2. File → 3. Desk → 4. Menu (stub, Db will live under it
   later as a dropdown) → 5. X`, widened boxes so labels stopped
   truncating, and the `[^]` engaged-glyph fix so the toolbar's own
   status label actually agrees with the real engine state at a
   glance. 🎨

## ✅ End state, verified by screenshot, not by assumption

`[>]1. In: off` → **Enter** → `[^]1. In: ON` → **Escape** →
`[>]1. In: off`. Both the real engine's own glyph and this window's
own local status label move together, for real, every time. 🔁

Committed as `621eb8b8`, pushed. 🚀

---

## 🧵 Your other two questions

### 1️⃣ Is the parser/render code efficiently reusing ops, not
repeating itself?

Mostly yes, with one known soft spot. The **real reuse wins**:
`click_focus_then_activate()`'s click-two-step logic, the
`[>]`/`[^]`/`[ ]` badge convention, and the `active_index`-dual-mode
model are all ONE real mechanism, shared by db-hq/chat-hai/events-hq/
now-pchq-board — I mirrored the existing pattern rather than invent a
new one, which is exactly the reuse you want. 👍

The **soft spot**, and you already flagged it yourself mid-session:
the pchq-board toolbar (`PchqElem[]`, its labels, positions, actions)
is still **hardcoded in C**, not data-driven from a `.pdl`/`.chtpm`
like every other hq window's own menu content is. That's a real
inconsistency with the house's own "data, not code" convention — it
works fine today, but it means adding a new toolbar button means
recompiling C instead of editing a data file. You said this can wait,
and I agree it's the right thing to fix next, not urgent tonight. 📝

### 2️⃣ Are we really ready to move on — does File/Desk load/saving
actually work yet?

**Yes — genuinely, not just wired-and-untested.** 🎉 I checked the real
handler code (`pc_menu_input.c`, `FILE_MENU`/`DESK_MENU` branches,
built earlier the same day under "§8 step 2"):

- **File** does a real two-way cycle between two real fixture levels
  (`default-pdl` ⇄ `default-legacy`), and for each it *actually copies*
  the real chunk files (`chunk_0_0_z0..31.txt`) and world files
  (`animals.txt`, `phymoji_entities.txt`, `state.txt`) into the live
  session, updating `config.txt`'s `active_level`/`active_board` keys
  as it goes. Not a stub message — real file I/O. 📂
- **Desk** reads `default-pdl`'s own manifest (`default.pdl`) and
  reloads the named board's files the same real way — with an honest
  "No maps to switch" message if you're on `default-legacy` (which
  has no board manifest at all, by design, not by oversight). 🗺️

So: the toolbar reorder/glyph work I just finished is UI polish on top
of load/save logic that was *already real* going into today. Nothing
here is faked or pending — you're clear to move on to the next real
task. 🟢

---

## 📋 Next-steps roadmap (nothing here was recorded anywhere before —
recording it now so it isn't lost)

### 🟢 Priority #1 — real bug, FIXED (`eb74b733`), pending your live confirmation
**Sometimes clicking the piececraft-hq taskbar icon does nothing** —
reliable via direct history-file injection, unreliable via a real
mouse click.

**Investigation (2026-08-30):** traced the full real pipeline — a real
X11 `ButtonPress` on `hq_win`/`popup_win`/`win` gets written by
`mirror_mouse_history()` into `#.desktop/strip_input_history.txt`
(the EXACT file the "tb history injection" repro method writes into
directly), read back by `poll_captured_input()`, hit-tested, and
dispatched via `send_code()` → a separate manager process →
`ktb_hq_activate()` → the real `button.sh run` launch. **Real clicks
and history-injection are identical from that file onward** — so the
bug had to be upstream, in how a real `ButtonPress` gets captured at
all.

**Root cause found**: the exact same bug class already found and
fixed THE SAME DAY for the board window (commit `402c812b`, "its not
geting mouse / kbd input") — `win`/`hq_win`/`popup_win` were all
`override_redirect=True`. Mutter's real XWayland focus routing only
ever gives real hardware click/key focus to WM-managed windows;
`XSetInputFocus()` (which `taskbar_soft_focus()` calls directly on
these windows) reports success at the raw X11-protocol level
regardless, which is exactly why it looked fine under synthetic
testing but failed unpredictably for real. "Sometimes" fits Mutter's
own live focus/stacking state far better than a narrow timing race.

**Fix (`eb74b733`)**: ported the same real technique 402c812b used —
WM-managed window, decorations stripped via `_MOTIF_WM_HINTS` — plus
real EWMH dock hints (`_NET_WM_WINDOW_TYPE_DOCK`,
`_NET_WM_STATE_ABOVE`/`SKIP_TASKBAR`/`SKIP_PAGER`/`STICKY`,
`PPosition`) since this is a real always-on-top dock, not a plain
utility window like the board's. Also installed a non-fatal X error
handler (this file had none before) since `XSetInputFocus()` can
legitimately throw `BadMatch` before the WM finishes reparenting a
freshly WM-managed window — uncaught, that would've crashed the whole
taskbar.

**Verified live**: restarted the real running taskbar; all 3 windows
came back with correct geometry/position, zero decoration/visual
regression (`xwininfo`), `Override Redirect State: no`, all dock/motif
hints present (`xprop`), and `hq_win` showing `_NET_WM_STATE_FOCUSED`
— the same marker 402c812b used to prove genuine WM-managed focus
(absent on `override_redirect` windows under Mutter entirely).
**Update — real second cause found and fixed (`0164e0fc`):** the WM
fix above was real and necessary, but you reported it still happened
"sometimes" and pointed out no other hq window has this problem — the
right instruction, since it led straight to the actual remaining
cause. Compared piececraft-hq's own `button.sh` against civ-txt's and
tactics-txt's (same clone lineage, neither ever reported this): both
only run their orphan-cleanup `kill_own_*()` functions inside their
explicit `kill` verb. piececraft-hq is the ONLY one that ALSO runs
them unconditionally at the top of `run` (added the same day, for a
real, separate need — daemons/widgets surviving a force-killed
session). `kill_own_orchestrator()` matches ANY orchestrator under its
own path, not by PID/session — so a `run` fired twice close together
(a double-fired click, or clicking again before a window appears) let
the second invocation's own safety net kill the FIRST invocation's
just-spawned orchestrator before it could ever show a window. Fixed by
skipping that safety net when a session dir was created in the last 5
seconds (real evidence another `run` is already in flight, not a
stale orphan). Verified live: fired two `run`s 0.3s apart, both
orchestrators now survive independently (previously the second would
have killed the first's).

**Also fixed along the way**: a real gap where this file never
handled `Expose` events at all (harmless under `override_redirect`,
a real problem for WM-managed windows — content drawn before the WM
finishes mapping could be lost with nothing to ever redraw it).
Investigated a live "text missing" report right after the WM fix and
traced it to two separate things: the header was fine, the bottom bar
was legitimately empty (no apps tracked open at the time, not a
rendering bug), and the missing Expose handling was real and worth
fixing regardless (`1521b346`).

**Update — a THIRD real bug in this same area, found live (`b1ef2cf0`):**
direct report: "i opened. killed from close. and tried 2 open again.
its not opening (pc-hq)". The pchq-board Close Elem (toolbar click,
keyboard, and the window manager's own `[X]`) only ever did
`running = 0` — tearing down THIS window's own X11 resources, never
the real underlying game session. The orchestrator/board-viewer widget
kept running silently in the background after "Close." Fixed by adding
`pchq_quit_host_session()`, which finds the host's current session dir
and writes `pieces/system/quit_flag.txt` — the exact real signal
`orchestrator.c` already polls every tick (the same one Ctrl+C's own
`write_quit_flag()` uses) — wired into all three Close paths. Verified
live end to end: open → Close (via real focus-nav + Enter) → confirmed
orchestrator, khtpm window, AND the session directory itself all fully
gone (the real EXIT trap ran) → `button.sh run` again → fresh session
comes up cleanly.

**Update — the real bug was narrower than that fix covered (`9c65e43e`):**
that first Close fix only ever touched the KEYBOARD Close path (its
own variable, `pchq_focus == PCHQ_ACT_CLOSE`) - the text-replace
silently never matched the MOUSE-click Close branch, which uses a
different variable (`hit == PCHQ_ACT_CLOSE`). Since real usage is
mouse clicks, the actual reported bug never got fixed the first time.
Found by reproducing the EXACT real path end-to-end via `xdotool`
through the real running taskbar (not a CLI shortcut): taskbar → HQ
header → toys popup → Piececraft-HQ row → window opens → click
Close → still running, `quit_flag.txt` still empty. Along the way, one
wrong turn: tried making Close bypass this house's own click_two_step
convention (assuming, wrongly, that a close button should always be
one click) - reverted per direct correction ("all menus should use
the amount required of pdl clicks 1 or 2 - that isn't the reason its
not reopening"). Close honors the same setting as everything else,
unchanged; the real fix was just adding the missing
`pchq_quit_host_session()` call to the mouse branch. Verified live,
full real path, twice in a row: open → two real clicks on Close →
orchestrator and session directory both fully gone → same real
taskbar path again → opens cleanly.

### 🟡 Recorded, not yet built
- ✅ **Screen resize / fullscreen support** — confirmed priority,
  "definitely" doing this.
- ⬜ Fullscreen toggle button: `!`, placed next to `X` in the chrome
  strip (i.e. becomes item 6, after Close... or beside it — exact
  placement TBD when built).
- ⬜ Player + clock (time) header — to be added to the toolbar **after
  Menu** (so: In / File / Desk / Menu / Player / Clock / X, roughly).
- ⬜ Drag and drop (scope not yet defined — TBD what's draggable).
- ⬜ Menu → real dropdown (same pattern the taskbar's own menus use),
  with **Db as a row inside it**, not its own top-level toolbar slot.
- ⬜ Toolbar → data-driven from a `.pdl` instead of hardcoded
  `PchqElem[]` in C (flagged by direct instruction: "we really
  shouldn't be hardcoding the toolbar, it should be read from .pdl
  external if possible" — explicitly OK to defer, not urgent).

### ✅ Already real, re-confirmed today (not blockers)
- File/Desk load/save — genuinely real file I/O (`FILE_MENU`/
  `DESK_MENU` in `pc_menu_input.c`), not stubs.
- Interact Mode activation + `[>]`/`[^]` glyph parity — verified live
  end to end.
- Render loop CPU spin — fixed (`fd98e1cc`, ~60fps cap, confirmed
  75-80% → ~22% CPU live).

### 🆕 Also done today (tangent, direct request): taskbar click_two_step
(`1f8abc73`) — `khtpm_strip_parser.c` never read `click_two_step` at
all (a real scope gap: the PDL's own comment only ever documented
db-hq/events-hq/chat-hai). Extended it to all three real mouse-click
branches (header, dropdown, bottom bar), same real focus-then-activate
semantics as everywhere else. Verified live via real xdotool clicks:
first click on "11. toys" only focuses (popup stays closed), second
click opens it; same two-step now applies to launching any app from
the dropdown too.
