RENDER-INPUT-REFACTOR-SUMMARY-2026-08-28.md
Written: 2026-08-28, by Sonnet (this session), summarizing a full
night's real, multi-agent (Sonnet + Grok, one Haiku subagent for
part of one design pass) refactor of `khtpm_entity_menu_render.c`
and the shared Elem/CSS engine. Full blow-by-blow, every claim/
verification/correction, lives in `GROK-RENDER-INPUT-REFACTOR-
HANDOFF.md` (this same directory) - this file is the readable
summary for anyone who doesn't want to read that whole doc.

============================================================
WHAT WAS ACTUALLY DONE (verified against real code, not just
claimed - every item below was independently checked line-by-line
by Sonnet, not taken on trust from Grok's own summaries)
============================================================

**1. Render half - real frame-file-then-paint proof.**
Palettes' tile grid now writes its real layout to
`#.desktop/palettes_frame.txt` BEFORE painting, and paints ONLY from
that file (zero live-tree access during paint) - the real "compose
then render from file" pattern this house's own wraith-alpha/TPMOS
reference uses everywhere else. Same mechanism later extended to the
two popup modes (entity-menu, taskbar-settings/swatch) as a second
proof it generalizes beyond palettes.

**2. Input half - real file-boundary for human/agent input.**
Mouse clicks and key presses in db-hq (then events-hq, then chat-hai)
now go through the SAME real file format mutaclysm already uses in
production (`pieces/keyboard/history.txt`'s own
`MOUSE_EVENT: <button> <x> <y> <is_press>` /
`KEY_PRESSED: <decimal>` convention) - captured to
`<mode>_history.txt`, consumed same-tick. This is a real audit trail,
not decoration: an agent and a real human click produce identical
lines, and the dispatch code cannot tell which wrote them.

**3. Real marker-gated redraw (the piece that was originally
missing entirely).**
db-hq no longer redraws on a flat timer. A real marker file
(`db_hq_frame_changed.txt`) is appended-to (one byte, size-only,
content never read - exact wraith-alpha convention) whenever
something real changes; the main loop only repaints when that file's
size actually grew. Fixed a real user-visible bug in the process (a
Common-Event editor left open would flicker every ~150ms even with
nothing changing) - found several real idle-leak sources causing
false marker growth (mtime-only staleness checks, redundant identical
rewrites) and fixed each one with real content checksums.

**4. One generic event loop, not four hardcoded copies.**
`khtpm_entity_menu_render.c`'s `main()` used to have 4 separate,
hand-duplicated `while (!g_quit)` loops (db-hq/events-hq/chat-hai/
popups). Now there is exactly one (`hq_run_event_loop`/
`hq_dispatch_xevent`), with mode-specific behavior as branches inside
shared dispatch, not four pasted copies of the same event-handling
code. Verified: only one `while (!g_quit)` remains in the file.

**5. Popups converted off their special-case architecture.**
Entity-menu and taskbar-settings/swatch used to be `is_popup=1`
specials with no manager, no state files, in-process click handling.
Per direct instruction ("i want to refactor everything, there should
be no exceptions") they were converted to the same manager-driven,
file-backed, generically-dispatched architecture as every other mode -
including gaining their own real frame-file paint (item 1's mechanism,
extended to a 2nd/3rd consumer). The ONE thing kept, deliberately, is
`override_redirect` + `XMapRaised` at the X11 level - a real, cited
technical need (a context menu must appear on top of what spawned it
with no WM chrome), not an architectural exception. Everything else
that used to ride along with that (no manager, in-process dispatch)
was removed.

**6. Real window-focus-stealing fix.**
Found and fixed a real bug: WM-managed windows (db-hq/events-hq/
chat-hai) were using `XMapRaised`, which Mutter treats as "please
activate this," stealing focus from whatever the human was doing
(confirmed live: chat-hai stole focus, other modes didn't, tracked to
this exact call). Switched to `XMapWindow` (matching open-hai's own
already-correct precedent) for all WM-managed HQ windows. Popups
correctly kept `XMapRaised` (they need to be on top) but never call
`XSetInputFocus` on map either way. Documented honestly: this is not
an airtight guarantee (Mutter can still activate a newly-mapped
window on its own even without `XMapRaised` in rare cases) - the code
does not *ask* to steal focus, but a 100%-can-never-steal claim was
explicitly avoided.

**7. Cross-window Tab-cycling + nav ledger (new capability, not a
bug fix).**
Real Tab-key window-cycling between open HQ windows, driven by a
real file registry (`#.desktop/nav_tab/<pid>`) and a real claim file
(each window focuses ITSELF - no process ever raises or focuses a
window it doesn't own, confirmed by direct code read, no foreign
`XRaise`/EWMH calls anywhere in the path). A companion real audit
file (`nav_master_ledger.txt`/`nav_master_current.txt`) records which
element got which nav number, per window - written honestly (elements
without a real id just say so, `tab`/`item`, rather than inventing
fake ids to look more complete).

**8. LayDoc→Elem capability port - 6 of 8 identified gaps landed.**
The taskbar's own separate rendering system (LayDoc) had several real
capabilities the shared Elem/CSS engine lacked. After a real,
line-cited investigation (not guessed), 6 of the 8 gaps were ported
into the shared engine (`khtpm_render_core.c`/`khtpm_draw_core.c`),
verified live:
   - `elem_flatten()` - flat-index tree export (for future
     serialization use, doesn't change live navigation).
   - `elem_cursor_prefix()` - cursor-state computed fresh at render
     time, not baked into label strings.
   - Real ACTIVATE-scope + BACK-equivalent (`DEACTIVATE` in db-hq,
     to avoid colliding with a pre-existing, unrelated `"BACK"` string
     already used by the popup/entity-menu's own page-stack nav) -
     lets a subtree become the only navigable scope while a popup/menu
     is open, matching wraith-alpha's real convention. Currently
     inert/unused (no real chtpm uses ACTIVATE yet) - proven not to
     regress existing numbering, not yet proven with a live open scope,
     since nothing calls it yet.
   - `cli_io` tag support (navigable only when it IS the active scope) -
     landed, no real consumer yet (future chat-hai composer candidate).
   - `elem_inject_loop()` - a real, reusable helper cutting boilerplate
     out of per-mode list-injection code (piloted on bookmarks' row
     list).
   Two gaps deliberately NOT ported: Gap 7 (header+footer's own
   unified-cursor need) turned out to already have a real mechanism in
   the taskbar's own consumer code (`unified_apply`/`unified_step`) -
   porting a "synthetic Elem root" design would have been solving an
   already-solved problem; correctly not built. Gap 8's cousin (var-
   driven tree shape / `${var}` substitution) was deliberately NOT
   ported at all - per-mode C injection was judged the right long-term
   shape for this house's imperative style, not a gap worth closing.

============================================================
"IS MUTACLYSM-NEO DONE?" - YES, ALREADY, BEFORE TONIGHT
============================================================
Direct answer: mutaclysm-neo does not need this refactor and was
never in scope for it. It is the REFERENCE implementation this whole
night's work has been copying FROM, not a target being brought up to
a standard. Its real, already-in-production `pieces/keyboard/
history.txt` file (the `KEY_PRESSED:`/`MOUSE_EVENT:` format) is
literally the exact format every HQ window's new input-capture code
adopted tonight, verbatim, on direct instruction ("wraith-alpha has
both in some form so u dont need to reinvent the wheel"). Nothing
about mutaclysm-neo's own code was touched or needed touching this
session.

============================================================
KNOWN, REAL, PRE-EXISTING BUGS (not introduced tonight, surfaced
in passing while checking scope - still open)
============================================================

**1. Toys-launch teardown/kill gap.** The taskbar's "toys" submenu
(a real, working feature - scans `house_root` + an apps root for
real candidates: mutaclysm, my-chara, my-lawyer, piececraft, and
launches whichever one is clicked) never records the launched
process's PID anywhere. A real, already-documented consequence
(cited in `khtpm_taskbar_manager.c`'s own comment and tracked in
`au11-hq/TPMOS-COMPLIANCE-DEBT.md`): killing db-hq tears it down
cleanly; killing everything via the house's "kill all" mechanism does
NOT reach a toys-launched app, because nothing ever wrote down that
it exists. Pre-existing, not touched or fixed this session - a real
open item if/when someone works on toys/taskbar-manager compliance.

**2. Open-hai + gemma3 not responding.** Flagged by the user during
this session, investigated briefly: NOT connected to tonight's work.
Open-hai's own build script only pulls `stb_image_write.h` from the
shared-lib directory tonight's Elem/CSS changes live in - it does not
share `khtpm_render_core.c`/`khtpm_draw_core.c` at all, has its own
separate render code, and hasn't even been rebuilt since before this
session started (most recent build report on disk predates this
session by 5 days). Gemma3 routes through open-hai's own
`BACKEND_HARNECIENT` backend path in `khtpm_open_hai_manager.c` - a
local inference-harness integration, unrelated to anything changed
tonight. Real cause not yet diagnosed - most likely something in the
local model-harness/service itself (not pulled, not running, timed
out), not a code regression from this refactor. Queued as a separate,
later investigation per the user's own "we will need to check out the
chat-ai later" - not started yet.

============================================================
WHAT IS STILL OPEN / NOT DONE (real, honest remaining scope)
============================================================
- Gap 7 (taskbar header+footer unified nav) - correctly identified as
  already solved by existing taskbar code, not a remaining port.
- Taskbar's own actual retarget onto the unified Elem/CSS engine
  (the LayDoc port only gave Elem the CAPABILITIES LayDoc has - the
  taskbar strip itself, including the real "toys" button and its
  submenu, still runs on LayDoc today, untouched).
- Dispatch inside the new single event loop is "one loop, mode-ifs
  inside dispatch," not yet "zero mode branches anywhere" (Grok's own
  honest characterization) - a further vtable-per-mode extraction
  would be the next step if that's wanted.
- The two other toys-menu apps besides mutaclysm (my-chara, my-lawyer,
  piececraft) - compliance/refactor status unchecked this session.
- Open-hai/gemma3 root cause - not yet diagnosed, separate follow-up.
