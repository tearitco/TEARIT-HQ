HOUSE_FAQ.md 🏠️❓️
Started: 2026-08-28
Purpose: real answers to real questions the user has asked mid-session,
so a future "wait, why does it work that way again?" gets answered
here first instead of re-deriving or re-asking an agent from scratch.
Each entry is a real question asked in a real session, answered
straight, with a pointer to the doc/code that has the FULL depth if
this summary isn't enough. Append new Q&As at the bottom, dated -
don't rewrite old answers if they go stale, add a 🔄️ CORRECTION note
under the original instead (same append-only convention as every
other doc in this house).

============================================================
🧩️ ARCHITECTURE
============================================================

**❓️ Why are there 7 different "modes" (db-hq/events-hq/chat-hai/
palettes/bookmarks/stats-hq/taskbar-settings) instead of one generic
layout/CSS engine with managers doing the heavy lifting?**
🏗️ Historically: `khtpm_entity_menu_render.c` is a MERGED binary - it
used to be several separate standalone renderer files, squashed into
one during an earlier merge stage, and the per-mode `if`/`else`
branches are the leftover seams from that merge, not a deliberate
design. 🎯 The real end-goal (being actively worked toward, not yet
100% there) IS "one generic engine, all app-specific behavior in
manager processes" - see `RENDER-FRAME-HISTORY-DRIFT-ASSESSMENT.md`
for the full real-vs-ideal gap analysis.
✅️ Good news: each mode still runs as its own SEPARATE PROCESS of the
same binary (mode flags set once at startup, never change) - so a
generic mechanism (like the scroll code, or the Phase 4 marker) can
safely be shared/reused across modes without collision, one process
at a time. 🔗️ See `RENDER-REFACTOR-2DO-PROGRESS.md`.

**❓️ Entity-menu (and every other mode) - do they have their own
layout parser, or do they use "the main parser"?**
✅️ They ALL use the same shared parser already - `khtpm_css_parser.c`/
`khtpm_render_core.c` (parses `.chtpm` into the Elem tree) +
`khtpm_draw_core.c` (paints it). This was never duplicated per-mode.
🔀️ What LOOKS like "its own parser" per feature is actually a
different, smaller thing: reading a manager's live DATA (state files
like `page.state.txt`'s `CMD|...`/`SCRATCHBLOCK|...` lines) is
naturally per-feature, because a bookmark line and a scratch-block
line are shaped nothing alike - same way one web app has ONE shared
HTML/CSS renderer but a different parser per JSON API it calls. Not a
gap the refactor needed to close - layouts are unified, live-data
readers are naturally feature-specific.

**❓️ What's the difference between LayDoc (taskbar) and Elem/CSS
(every hq window)?**
🎭️ Two real, separate tree/render systems in this house. LayDoc
(`khtpm_strip_layout.h`/`.c`) is the taskbar's own engine - flat-array
tree with `parent_index`, `${var}` substitution at render time, a real
ACTIVATE-scope nav mechanism. Elem/CSS (`khtpm_render_core.c`/
`khtpm_draw_core.c`) is every hq window's engine - pointer tree,
concrete strings after parse, CSS box model + `hit_test()`.
🏆️ An earlier investigation found LayDoc genuinely AHEAD of Elem for
some real capabilities (var substitution, ACTIVATE scope, cli_io tag).
2026-08-28: 6 of those 8 real gaps got ported INTO Elem/CSS (ACTIVATE
scope as `dbhq_activate_scope()`/`onclick="ACTIVATE"`/`"DEACTIVATE"`,
`cli_io` tag support, a flatten helper, cursor-prefix helper, inject-
loop helper) so the two systems converge over time. 📄️ Taskbar itself
hasn't been RETARGETED onto Elem yet - that's a separate, later step.
See `LAYDOC-ELEM-PORT-IMPLEMENTATION-PLAN.md`.

============================================================
📁️ FILES / COMPLIANCE
============================================================

**❓️ "If it's not in a file, it's a lie" - why so strict about
real files vs in-memory state?**
🕵️ Compliance/audit reasoning: an in-memory flag dies with the process
and leaves zero trace to prove after the fact what happened or when -
even if it's technically sufficient for correctness, it's NOT
auditable. 📎️ This house's real standard (wraith-alpha/TPMOS) always
uses a real file for anything that needs to be provable later - e.g.
`frame_changed.txt` (one-byte append, size-only, content never read)
gates every real redraw. Phase 4 of tonight's refactor
(`db_hq_frame_changed.txt`) matches this exactly, after an early
in-memory-flag proposal got directly, correctly rejected.

**❓️ What's a "manager" and why can't a feature just be C code
embedded in the renderer?**
🏭️ A manager is a real, SEPARATE compiled binary (`<name>_manager.c`)
that owns a feature's own logic/state and publishes a real state file
the shared renderer's generic injection code reads - NOT
bash-`printf`-generated `.chtpm`, NOT logic baked into the shared
renderer beyond generic injection. `khtpm_hq_manager.c` (Common
Events) and `palettes_manager.c` are the two proven, current
examples. 📋️ Standing rule, see `TPMOS-COMPLIANCE-DEBT.md`.

============================================================
🎮️ NAV / INPUT
============================================================

**❓️ Why does nav_index restart at 1 in every window instead of
continuing across windows/header+footer?**
🐛️ Real bug the user caught: taskbar's own header+footer nav
(`unified_apply`/`unified_step`) walks FOCUS across both, but never
continues the NUMBERING - it restarts independently, unlike
wraith-alpha's own real convention. 🔧️ Resolved design: don't force a
shared/global counter - give each window its own unchanged local
`nav_index` (zero regression), and add a SEPARATE window-level
address: `Tab<N>` prefix (e.g. Tab1, Tab2), cycled with the literal Tab
key. `^` marks which window currently has Tab-cycle focus (window-
level, moves only on Tab); `>` stays the existing local cursor (moves
on digit-jump inside whichever window has `^`) - same real split
LayDoc's own `active_index`/`focus_index` already models, one level up.
🤖️ Agent-drivable for free once Phase 3b (KeyPress capture) exists,
since Tab is then just another key going through the same
`KEY_PRESSED:` file mailbox as everything else - no Tab-specific
capture code needed.

**❓️ How does a human click/keypress actually reach the renderer -
is there a real file boundary like wraith-alpha's?**
📬️ Yes, as of 2026-08-28 (Phase 3a/3b): real capture-then-consume,
same house format mutaclysm's own `pieces/keyboard/history.txt` already
uses (`MOUSE_EVENT: <button> <x> <y> <is_press>` /
`KEY_PRESSED: <decimal>`), written to `<mode>_history.txt`, consumed
same-tick by `poll_agent_history()`. An agent and a real human produce
byte-identical lines - the dispatcher can't tell which wrote them.
⚠️ One real per-mode focus gate exists: if two windows of the SAME mode
share one history file, only the X-focused one reads/dispatches (the
other skips to EOF) - matching wraith-alpha's real "one file, one
reader" shape, not a second parser joining an existing stream.

============================================================
🖼️ ASSETS
============================================================

**❓️ Why do RPG Maker asset paths live in a `.pdl` file instead of
being hardcoded in C?**
🔌️ So the path can change (different drive letter, Windows vs Mac,
files physically moved) WITHOUT a C rewrite - the C code reads one
real pointer key (e.g. `img_root` in `RMMV-ASSET-SOURCE-LOCATION.pdl`)
and never hardcodes a path itself. 📦️ 2026-08-28: all RMMV `img` assets
(tilesets + every other category) moved OUT of `&.widgits/palettes/`
(which gets zipped/shared as "the house" - was bloating it with real
PNGs) to `NNEST-11.17/#.NNEST_ASSETS/rmmv-www-img/`, a general
container ABOVE the house meant to hold future non-RMMV asset types
too as siblings. The PDL's `img_root` key was the ONLY thing that
needed to change - zero C edits, since `rmmv_img_root()` already read
that one key instead of a hardcoded path.

**❓️ Does mutaclysm-neo need this whole render/input refactor too?**
🎓️ No - it's the opposite direction. Mutaclysm-neo is the REFERENCE
implementation this whole refactor has been copying FROM all night
(its real `pieces/keyboard/history.txt` format is what every hq
window's new input capture now uses verbatim). It was never in scope
because it's already the standard being matched, not a target
catching up to it.

============================================================
🐛️ REAL BUGS FOUND THIS SESSION (not exhaustive - see
RENDER-INPUT-REFACTOR-SUMMARY-2026-08-28.md for the full render/input
list)
============================================================

- 🔥️ **open-hai + gemma3 "not responding"**: not a model/server issue
  (gemma3 replied fine when hit directly). Real cause: every send path
  in `khtpm_open_hai_manager.c` silently dropped a new message if a
  previous request was still pending, with zero feedback - switching
  models mid-flight didn't help, the drop happened anyway, and the
  OLD request's eventual (possibly erroring) result showed up instead,
  looking like "the new model didn't respond." 🔧️ Fixed 2026-08-28:
  dropped sends now post a real `[dropped: ...]` message; switching
  models auto-cancels the stale pending request (`SIGTERM` + reap) so
  the new send goes through immediately - no freeze, no silent loss.
- 🧟️ **toys-launch teardown gap** (pre-existing, NOT fixed yet): the
  taskbar's real "toys" menu launches real apps (mutaclysm, my-chara,
  my-lawyer, piececraft) but never records the launched PID anywhere -
  "kill all" doesn't reach a toys-launched app. Tracked in
  `TPMOS-COMPLIANCE-DEBT.md`.
