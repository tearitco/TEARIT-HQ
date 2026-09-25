# 🎯 bug_bounty.md — hard-to-pin / recurring bugs, tracked until closed

---

## ⚠️ OPEN 2026-09-23: Co-lab-h-ai cuts off messages so the human cannot read them

**Reported:** live, while approving agent posts in session `1790154594`. Long `@kilo` lines were queued. The window shows a cut-off sentence. The full text is only in `pending.txt` / `conversation.txt`.

**Where:** `&.hq-apps/co-lab-hai/co-lab-hai.xhtpm` draws the pending line and each conversation row as a single `<text label="...">` (`PENDING (${pend_agent}): ${pend_msg}` and `${msg.text}`). Those labels do not wrap. `colab_hai_manager.c` also builds each session sidebar label in a 96-byte buffer (`session_label` → `char label[96]`). The manager keeps a longer `pend_msg` (escaped into 1200 bytes) and reads conversation lines up to 2048, so the files are whole and the window is not.

**Note from `claude` branch's own history**: a DIFFERENT co-lab-hai long-message-clipping bug (the frame-file round trip's two independent 256-byte buffers in `khtpm_core_render.c`) was found and fixed 2026-09-23 - see this file's own "✅ CLOSED 2026-09-23: Co-lab-h-ai cuts off long messages" entry elsewhere in this doc if this looks like a duplicate; check whether that fix already resolves what's described here before doing more work on it.

**Not fixed.** Direction: show `pend_msg` and `msg.text` in a wrapping `<text_area>` (or a row tall enough to wrap), and stop clipping session labels at 96 bytes. Do not shorten agent posts to fit the label.

---

## ⚠️ OPEN 2026-09-22: HQ dropdown/menu lists (pals, palettes, edit, etc.) have no scrollbar at all

**Reported:** direct live report, discovered while investigating a real overlap bug in the "pals" dropdown (see the `khtpm_core_render.c` scroll-boundary entry below, same session) — "drop downs should have a scroll bar (which has navs) if they dont yet. this was an oversight."

**Confirmed by direct code check**: grepped `khtpm_core_render.c` for any scrollbar wiring on the HQ dropdown/menu (`hq_menu`) rendering path — zero hits. The generic scroll-region machinery this house already uses elsewhere (`layout_scroll_region()`/`generic_sbar_register()`, proven in file-explorer/board-viewer/etc.) is real and working, but the HQ dropdown cells (pals, palettes, edit, and any other `which == N` cell using `HQMenuItem[]`) don't call into it at all — a long list (the "pals" dropdown has 140+ real entries) has no visible thumb/track, no click-to-scroll, and the user has no way to know there's more below the last visible row short of scrolling blind.

**Not yet fixed.** Real direction: wire the same `generic_sbar_register()`/`layout_scroll_region()` path every other scrollable list in this house already uses onto the HQ dropdown/menu rendering, rather than inventing a second scrollbar mechanism. Likely related to (may share a root cause with, or may be a separate follow-up from) the boundary-row overlap bug directly below this entry — check both together before considering either fully closed.

---

## ✅ CLOSED 2026-09-23 (all four halves - Cancel row, duplicate row, missing solvent, tax_robot's missing nav badge): "pals" dropdown - real cap, stack_n collision, and a badge-position exclusion gap, all fixed and pixel-verified

**Reported:** direct live screenshot (`/home/no/Pictures/Screenshots/Screenshot from 2026-09-22 17-08-22.png`) - scrolling the "pals" dropdown shows the topmost visible row rendering with just its nav number + selection highlight and no icon/label content, squeezed into a near-zero-height sliver overlapping the row below it. Also reported: no visible Cancel button in this same dropdown ("other dropdowns don't do this").

**Cancel button — real root cause found, fixed, live-verified (commit `4584cc25`).** Not a scroll issue: `KTB_LIVEDESK_DYN_MAX` is 24 (`khtpm_taskbar_manager.h:71`), but the live house has 191 real pal directories — the alphabetical scan in `livedesk_build_pals_menu()` (`khtpm_taskbar_manager.c` ~line 3471) filled all 24 array slots before Cancel could ever be appended, with zero pdl-defined post rows to reserve room. Confirmed live before the fix: dumping `#.desktop/strip_var_hqitems.txt` with the dropdown genuinely open showed exactly 24 rows, the last a real pal (`tax_robot`), no Cancel. Fixed by dry-running the post-row count first and reserving at least 1 slot for Cancel up front; verified after rebuild+restart: 25 rows, last is `Cancel`.

**Boundary row overlap — real finding: the original "140+ entries, scroll boundary" premise was wrong, and the symptom could not be reproduced against the real code.** The pals dropdown's actual render path (a `dropdown-child` repeat block, `khtpm_strip_header.xhtpm` + `khtpm_core_render.c` ~5210-5262/`dock_paint_menu()` ~5431) has **no scroll or clipping logic at all** — every row is drawn unconditionally, sized to fit all of them (and the 24-row cap above means it physically never exceeds 24 rows, so a scroll-boundary bug class doesn't obviously apply here today). Could not get the actual popup window to map live via the relay to capture direct pixel proof either way, and said so rather than guessing at a fix. **Still open** - if this is still visually reproducible, needs a human or an agent with more relay-protocol context (the real relay format has a 5th token, e.g. `hq_win`, not documented in the k9 testing-convention file) driving it interactively.

**2026-09-23, second pass, direct live screenshot evidence (`Screenshot from 2026-09-23 05-10-12.png`) - two real, DISTINCT symptoms, clearer than "double asa":**
1. `terumon_004_solvent` (confirmed on disk as the alphabetically-last of 29 real pal dirs, `pal.pdl`+`glyph.txt` both present, structurally identical to its siblings) was completely ABSENT from the rendered nav sequence - jumped straight from `terumon_003_murmur` to a row labeled `notes-pals`, no directory or hardcoded string with that name anywhere in the codebase (grepped `khtpm_taskbar_manager.c`, the whole `44.xyz.01.00/` tree, and `livedesk_taskbar.pdl`'s pals/palettes sections - zero hits).
2. `tax_robot` (real dir, `glyph.txt` only, NO `pal.pdl` - confirmed live) rendered with its label/icon but no `[ ]NN.` nav badge at all, i.e. `nav_index` stayed 0 for that row while it was still visually placed.

**Real, confirmed root cause for `layout_dock_bar()`'s dropdown-child stacking loop (~5245-5297): `stack_n` used to reset only when `target_id` differed from the PREVIOUS sibling in `page->children[]`** - a silent assumption that every dropdown-child sharing a `target_id` sits contiguously. That assumption is provably false under this house's own incremental reparse (`khtpm_reparse_diff.c`, `kh_diff_match_children()`): dropdown-child elements carry `target_id` (no `id`), and `kh_diff_key()` returns `target_id` as the match key - but EVERY sibling row for the same open cell shares the identical `target_id` value, so the key is non-unique across the whole list, not per-row. A shrink/grow or reorder between two reparses of the same open dropdown (or a leftover row from a previously-open DIFFERENT cell sharing the parent) can leave stale/interleaved children in `page->children[]`, which broke the old adjacency-reset logic directly - two rows landing on the same `y` (the duplicate), later rows losing their slot to the collision (solvent).

**Fix applied (commit pending, `khtpm_core_render.c` ~5245-5297):** replaced the adjacency-based `stack_n` reset with a real per-target running counter (a small fixed `stack_keys[16]`/`stack_counts[16]` table keyed by `target_id`, incremented per matched child regardless of position in `page->children[]`) - removes the contiguity assumption entirely. Built clean (`build_khtpm_strip.sh`, no new warnings), restarted live (`run_khtpm_strip.sh new`, confirmed via `strip_ui.txt`/`strip_var_hqitems.txt` mtimes that the restart landed at 05:12:00, after the 05:10:12 screenshot).

**⚠️ STILL OPEN - fix did NOT resolve the live symptom.** Owner re-tested against the fresh binary and reported "its the same" (still shows the `notes-pals` ghost row before Cancel, still missing/broken the same way) - so the `stack_n` keying fix, while a real and correct hardening in its own right, is **not the actual root cause of the `notes-pals`/tax_robot-badge symptom**, or is not the only cause. Attempted live verification via `nav.sh nav 16` (the pals cell's current on-screen nav badge per the screenshot) to dump the authoritative `strip_var_hqitems.txt` while genuinely open - this opened the WRONG menu (`n_hqitems=6`, not pals' ~30), confirming nav badge numbers are NOT stable cell identifiers (they're global/dynamic per the K9 doc's own warning) and this agent does not yet have a reliable relay recipe for reopening this SPECIFIC cell by cid by digit-jump alone. `nav.sh hqcell 6` (the `which`-based 4000+n direct-dispatch form, bypassing digit-jump entirely) was NOT yet tried this pass and is the next real thing to attempt - untried, not ruled out.

**2026-09-23, third pass - `notes-pals` is NOT a bug, real root-cause narrowing.** `nav.sh hqcell 6` (the `which`-based direct-dispatch form) is a reliable way to reopen a specific cell by identity, independent of the global nav-badge numbers drifting - confirmed live (`strip_ui.txt`'s `n_hqitems` went straight to `31`, matching the real expected count). Dumped `strip_var_hqitems.txt`/`strip_ui.txt`'s `hi_N_*` while genuinely open:

- **`notes-pals` (`hi_29_label`) is real, intentional, WORKING AS DESIGNED** - `khtpm_taskbar_manager.c` ~line 4805 (2026-09-08, direct request): every real header-cell menu automatically gets a generic `notes-<cell>` row one slot above Cancel, opening a per-subsystem dev-note file via `#.desktop/scripts/notes.sh`. For the `pals` cell that genuinely renders as `notes-pals` - this was never a data-layer or render-layer bug, just an unfamiliar (but correct) row nobody had traced back to this feature yet. **Retracting the "ghost row" framing entirely.**
- The full `hi_0..hi_30` list (31 rows: 29 real pals scanned in order, `asa`...`terumon_004_solvent` with `solvent` correctly present at `hi_28`, then `notes-pals` at `hi_29`, `Cancel` at `hi_30`) is clean, correctly ordered, no duplicates, no collisions - **the manager's own data layer has been clean this whole investigation; every real symptom tonight was in the render path.**
- `tax_robot` (`hi_24_label=• tax_robot #`, `hi_24_sprite=.../pals/tax_robot`) is present and correctly labeled in the manager's array - its earlier "missing nav badge" symptom, if still real, is therefore render-side only (`layout_dock_bar()`'s `nav_index` assignment), not a manager/data problem. Confirmed live: `tax_robot` genuinely has no `pal.pdl` on disk (glyph.txt only), which is why its label shows a bare `•` glyph and an empty `#hash` - that part is correct/expected, not a bug; only the missing nav bracket (if it persists) would be one.
- **New, real, distinct testing-methodology gap found this pass** (belongs in the K9 doc, not just here): `nav.sh hqcell <n>` reliably flips the MANAGER's `hq_open`/`n_hqitems` state and even the render's own header-cell marker (`[>]6.pals` visible in a live `dump_frame_png_op` capture of the strip window) - but the window itself never resized/repainted to show the dropdown's rows; `xwininfo -root -tree` showed no separate popup window either, and the strip window's own geometry stayed a flat 45px tall throughout. A REAL mouse click clearly does trigger a resize/overlay draw (every one of the owner's own screenshots proves this), so there is a genuine, distinct gap between "hq_open flips via relay" and "the resize/repaint that makes it visible" - possibly a missing `XResizeWindow`/expose-equivalent step that's only reached from the real X11 click handler, not from `dispatch_code()`'s digit path. This means **pixel-level verification of this specific dropdown still requires a real physical click** - relay can confirm/dump the DATA but not yet the RENDER, a sharper version of the K9 doc's existing relay-vs-real-input caution.

**Net status after three passes:** the duplicate-row/collision part (today's `stack_n` keying fix, commit `5cf4aad5` on `claude`) is believed real and correct, and the manager's data layer is now confirmed clean end-to-end including `notes-pals` (intentional) and `solvent` (present, correctly ordered). What remains genuinely unverified is pixel-level: whether `tax_robot` still loses its nav badge in the actual rendered dropdown, and whether the fix visibly resolved the duplicate-row overlap - both need a real click + fresh screenshot to close out, not another relay pass.

**2026-09-23, fourth pass - ✅ CLOSED (duplicate-row half) / ⚠️ OPEN, narrower (tax_robot nav-badge half) - real pixel-level confirmation, finally.** Direct owner correction ("i am able to navigate using arrows and open pals dropdown with one enter press... your not doing something right according to the K9 doc") pointed at the real gap in the THIRD pass above: `hqcell`/`nav`/`mgrcode` only ever drive the MANAGER's `hq_open` state, never the RENDERER's own `g_default_active_scope_id` that actually creates/paints the popup window - a real mouse click or a **keyboard arrow + Enter** does, and only those. Rebuilt a safe, isolated Xephyr test rig (scratch house root, `#.desktop` a real writable copy, everything else symlinked, bypassing `run_khtpm_strip.sh`'s system-wide kill pattern entirely) and drove it the RIGHT way this time: `KEY_PRESSED: 203` (Right arrow) x5 on the renderer's own `entity_menu_history/<pid>.txt` walked focus `[>]1.HQ` -> `[>]6.pals`, then one `KEY_PRESSED: 13` (Enter) created a real `219x744` popup window (`g_dock_menu_win`) and rendered the full 31-row list. Captured with `dump_frame_png_op` - genuine pixel proof, not a data dump. Full recipe and the earlier wrong "relay is broken" conclusion (retracted) are written up in `06-testing/AIGENT-TESTING-K9.txt`'s 2026-09-23 update.

**Real result, read directly off the screenshot:**
- **Duplicate row (`asa`/nav 18 blank-with-icon) - GONE.** `asa`(118)/`ava`(119) render clean and sequential, no overlap, no collision anywhere in the list.
- **`terumon_004_solvent` - PRESENT**, correctly positioned right before `notes-pals`(146)/`Cancel`(147), matching the manager's own `hi_28_label`.
- `notes-pals`(146) and `Cancel`(147) both render correctly with proper nav badges (confirming the earlier "ghost row" retraction was right - it's real, working, intentional content).
- **`tax_robot` STILL has no nav badge** - renders as `• tax_robot #` with no `[ ]NN.` bracket at all, sitting between `m9_missingno`(141) and `terumon_001_ember`(143) with no number of its own. This is the ONE real symptom still open. `layout_dock_bar()`'s stacking loop (the code this session's `stack_n` fix touched) assigns `nav_index` unconditionally inside the same `if (open && trigger)` branch every other row goes through - since every dropdown-child in this list shares the identical `target_id`/`trigger`, there is no obvious reason this ONE row's `open`/`trigger` check would differ from its neighbors'. Not yet root-caused - a live debug `fprintf` in the loop's `else` branch (dumping `c->label` whenever `nav_index` gets zeroed) is the fastest next step, using the now-proven arrow+Enter Xephyr recipe to reproduce on demand.

**Real, live-confirmed things to stop re-litigating:** the data layer (manager's `HQMenuItem[]`/`hi_N_*`) is clean; `notes-pals` is intentional; the duplicate-row collision this session's `stack_n` fix targeted is genuinely gone.

**2026-09-23, fifth pass - ✅ CLOSED, `tax_robot`'s missing nav badge root-caused and fixed.** The earlier hypothesis ("this ONE row's `open`/`trigger` check must differ from its neighbors'") was wrong - live debug `fprintf`s added at every stage of the pipeline (`layout_dock_bar()`'s stacking loop, `kh_paint_frame_line()`'s frame-file-round-trip parse, `draw_elem()`'s own entry) proved `nav_index=41` survives completely intact end-to-end - layout, serialize, parse, and paint-entry all agree. The bug is neither layout nor parsing; it's in `draw_elem()`'s badge-POSITIONING logic (`&.widgits/_shared-lib/khtpm_draw_core.c` ~1723, the "sprite tile: badge drawn ABOVE the row" branch). That branch already excludes `dock-cell` rows for exactly this reason (2026-09-15 fix, same file, same block - "dock strip packs rows with ZERO vertical gap... every row's own badge bled up into the row above it") - but `dropdown-child` rows (this session's dock-MENU popup, a different, newer row class using the identical zero-gap packing) were never added to that exclusion list. Any dropdown row whose `e->sprite` field is populated (every real pal row, including `tax_robot` - its field points at a real directory even though that directory has no `sprite.csv`/`pal.pdl`, so the FIELD itself is non-empty) hit this branch and had its badge chip shifted up into the previous row's box, where for `tax_robot` specifically it rendered fully hidden/overwritten. Fix: added `!elem_has_class(e, "dropdown-child")` to the same exclusion condition, one line, same reasoning as the existing `dock-cell` exclusion right next to it. **Pixel-verified** via the same Xephyr+arrow-key+Enter recipe (K9 doc, 2026-09-23 update): `tax_robot` now shows `[ ]41.` correctly, matching every sibling row.

**Final status: all four real symptoms this session chased in the pals dropdown are fixed and pixel-verified** - missing Cancel row (earlier session), duplicate/collided rows (`stack_n` keying fix, `khtpm_core_render.c`), and `tax_robot`'s missing nav badge (`dropdown-child` badge-position exclusion, `khtpm_draw_core.c`). `notes-pals` was never a bug. The scrollbar/12-row-cap entry above this one remains separately open.

---

## ✅ CLOSED 2026-09-22 (fixed same day as reported, verified via live `strip_ui.txt` receipts and real timing, commit `60fd7920`): taskbar takes a long time to appear on launch, even though desktop entities (which the user expected to be the slower/bigger thing) appear instantly

**Reported:** direct live report - "it took a long time for tb to populate... doesn't make sense that it took so long when desktop entities, which are larger, were instant." Asked for a "loading" indicator as a possible mitigation, and to track this at minimum.

**Real, evidenced root cause (found by direct code read):**
`khtpm_taskbar_manager_main.c`'s `main()` calls `ktb_init()` synchronously, before the manager writes its pidfile or reaches the event loop that publishes `#.desktop/strip_ui.txt` — the file the renderer polls to draw the taskbar strip at all. `ktb_init()` (`khtpm_taskbar_manager.c` ~line 441) calls `livedesk_ensure_cursword()` then `livedesk_spawn_active_desk()` → `livedesk_spawn_desk()` (~line 2474), which loops over every `DESK` row in the active desk's `.pdl`. For **each entity**, before its (fire-and-forget, `setsid nohup ... &`) spawn, it runs `ktb_find_live_pid_for_pal()` (~line 2768) — a **full scan of `/proc`**, opening and `fread`ing `/proc/<pid>/cmdline` for every process on the machine, to check whether that one entity is already running.

This explains the exact reported shape: each entity's own spawn is genuinely fast once its turn comes (X11 window mapping doesn't wait on anything else), so entities visibly "pop in" quickly one at a time. But the *next* entity's spawn doesn't start until the *current* entity's full `/proc` scan finishes, and the taskbar's own UI can't be published until the ENTIRE loop — one full-process-table scan per entity — completes. A desk with N entities does N full `/proc` scans, fully serialized, before the taskbar can draw anything, on a documented weak-CPU machine (`nice-heavy-background-work` house note).

**Fixed (option 2 of the three considered):** the `/proc` scan is now done ONCE per pass (`ktb_proc_snapshot()`/`_find()`/`_free()`, `khtpm_taskbar_manager.c`), not once per entity, at both real call sites (`livedesk_spawn_desk()`'s startup loop and `ktb_self_heal_active_desk_registry()`'s reconcile pass). Most surgical of the three options considered — no ordering change, no new published state, just removes the redundant re-scanning. Options 1 (placeholder UI) and 3 (defer spawning off the sync path) remain real, not-yet-needed alternatives if this ever regresses at a larger scale.

## ✅ CLOSED 2026-09-20 (verified on the user's hardware: "that actually fixed it"): csv-hq `<grid>` - Enter on the grid nav item doesn't activate it (needs a double click) and no typed input arrives afterward

**FINAL ROOT CAUSE (confirmed on hardware): the Cursword pal's never-released display-wide keyboard grab (`2c1301ab`) - see `03-pitfalls/HOUSE_CODE_PITFALLS.md` #24. The grid re-arm fix (`3895ff77`) was also a real, separate bug. The `managed` class (`17f8da40`), the WM_HINTS/override_redirect theories and the dock were NOT the cause. Fix took effect after restarting the Cursword pal once.**

**Reported:** direct live report, csv-hq (`@.apps/csv-hq/`): the grid is
nav item **10**; pressing Enter on it does **not** arm it (user has to
double click), and once it is in `#` mode nothing typed is taken as
input. User's own note: the grid "wasn't tested very thoroughly, it may
need a bugfix." Also: the user first asked how `#` mode is supposed to
work - answer is in `08-roadmap/design-docs/GRID-ELEMENT-DESIGN.md`
("Three real states") and csv-hq-pal.xhtpm's header: Enter arms (`#`,
navigating), arrows move the cell cursor, letters/digits build a jump
buffer (`a11` or `11a`) resolved by Enter, a second Enter with an empty
buffer enters the cell (`^`, real text input), Esc commits (`SETCELL:`)
and returns to `#`, Esc in `#` disarms. The design doc's own header still
says "DESIGN ONLY", which is stale - csv-hq shows it shipped 2026-09-05.

**Evidence so far (one lead, not confirmed):** `@.apps/csv-hq/
kh_focus_debug.log` ends with repeated
`GRAB key=cell_ attempts=6 rc=1(0=success) real_focus_is_us=1`
(time-of-day 02:36). `rc=1` is `AlreadyGrabbed`: some other X client was
holding the keyboard grab when the grid tried to take it, so cell
editing never received keys. This is NOT the same signature as the
2026-09-14 entry below (text-edit-hq logged `rc=0` success yet got no
keys) - but it is the same *class* ("armed field gets no keyboard"),
and it is the same failure mode the Place-overlay Esc bug had (its
`XGrabKeyboard` result was ignored while another client held a grab).

**Two separate symptoms, possibly two causes:**
1. *Enter doesn't arm.* A double click works, so the mouse path reaches
   the grid's activation but the Enter / `KEY_PRESSED: 13` path on a
   focused `<grid>` nav item may not route to `activate_focused()` for
   the grid tag (Enter path: `handle_key()` `XK_Return` branch in
   `khtpm_core_render.c`; grid state machine: `default_grid_handle_key()`
   ~line 7744).
2. *Armed but no input.* Grab failure above, or keys reaching
   `handle_key()` but the grid handler not consuming letters/digits/
   arrows in `#` state, or the jump-buffer status line not updating.

**Not yet done - next steps:** reproduce with a private Xephyr + house
root via relay: focus the grid nav item, send `KEY_PRESSED: 13`, read
the frame for the `#` badge; then arrows (200-203), `a`,`1`,`1`, `13`
and check `jump: a11_` and the cursor; then a second `13` and typed
text, Esc, and confirm `csv_hq_action.txt` gets `SETCELL:`. Then repeat
with a REAL keyboard - relay injection bypasses X grabs (see
`RELAY-WINDOW-TARGETING-DESIGN.md` and the house rule
`relay-testing-may-mask-real-focus-bugs`), so a relay-only pass proves
nothing about symptom 2. Check who else holds the grab (other armed
cli_io, an open popup, the taskbar) by running csv-hq alone.

**Related:** the OPEN 2026-09-14 entry below (physical keyboard never
reaches an armed field despite rc=0), and the Place-overlay Esc fix
(same ignored/failed `XGrabKeyboard` pattern).

### 🎯 2026-09-20 ROOT CAUSE FOUND (supersedes the override_redirect theory below): a stuck display-wide keyboard grab held by the Cursword pal

Measured on the user's real GNOME/Wayland session, no user action needed:
- **Throwaway-window experiment** (5 windows on DISPLAY=:0: managed with/without
  WM_HINTS, +WM_TAKE_FOCUS, DOCK-typed like the taskbar, override_redirect): Mutter
  activated and X-focused ALL of them (`_NET_ACTIVE_WINDOW` == window) and
  `XGrabKeyboard` returned `AlreadyGrabbed` for ALL of them, both owner_events values,
  with `FocusIn mode=NotifyWhileGrabbed`. So window properties (WM_HINTS, window type,
  override_redirect) are NOT what decides keyboard delivery; some client held the
  keyboard grab the whole time.
- **Who:** X RECORD with the `delivered_events` range (not `device_events`, which
  reports client 0 = the server) while XTest injected one Shift tap: KeyPress/
  KeyRelease were delivered to `id_base=0xc00000`; XRes maps that to PID 28935 =
  `khtpm_entity.+x ...pals/cursword` (started 00:41, the only house process the 03:08
  taskbar reset did not replace). Its history.txt ends `CURSWORD_ARMED ... CURSWORD_PLACED`.
- **Bug:** `khtpm_entity.c` had a file-scope `static Display *dpy = NULL;` and
  `kh_ungrab_kbd()` = `if (dpy) XUngrabKeyboard(dpy, ...)`, but `tp_main()` opens its
  own LOCAL `Display *dpy` (the global stays NULL - see the khtpm tp_main globals
  footgun). So every `kh_ungrab_kbd()` in the pal process (Cursword Esc / placed /
  disarm / focus-lost, close_context_menu) was a silent no-op, while the matching
  `XGrabKeyboard(dpy, ...)` (Cursword's deliberate "stingy focus" armed mode, ~line
  5653) used the local one. Once armed, Cursword owned every keystroke on the desktop
  until its process died: csv-hq's Enter/arrows/Esc, the grid arming, `AlreadyGrabbed` in
  every log, and the placer Esc failure all follow. (The unfactor comment even said
  "no-op ... same as before the split": pre-existing, not caused by the split.)
- **Fix (khtpm_entity.c):** `g_kbd_dpy` set by tp_main; `kh_ungrab_kbd()` releases on
  it (+XFlush). **Verified** in a private Xephyr with a Cursword-like pal (`log_mode=1`):
  old binary - arm: grab held, Esc → `CURSWORD_DISARMED`, grab STILL held; new binary -
  arm: held, Esc → released. Test scripts: /tmp/claude-1000/grabfix/t2.sh.
- **The running Cursword (PID 28935) still holds the grab until it is restarted**;
  a taskbar reset does not restart it. Restart Cursword (or log out) once; new
  binaries release properly.
- **Hardening added in khtpm_core_render.c:** any window whose page has a
  cli_io/text_area/grid is now forced WM-managed regardless of livedesk_override_redirect.pdl
  (`class="unmanaged"` opts out; verified: window comes up `Override Redirect State: no`
  with the PDL set true); `dock_grab_keyboard()` logs a failed grab; every HQ click logs
  `CLICK-FOCUS target x_focus wm_active` into kh_focus_debug.log (evidence next time).
- **Diff table (dock vs generic managed HQ window) - measured NOT to matter for the
  keyboard:** WM_HINTS input=True (dock: yes, HQ: none), _NET_WM_WINDOW_TYPE_DOCK (dock:
  yes, HQ: none), WM_NORMAL_HINTS position (dock yes), click path (dock: XRaiseWindow +
  XGrabKeyboard(owner_events=True) + XSetInputFocus; HQ: kh_raise_and_focus =
  _NET_ACTIVE_WINDOW + XSetInputFocus with real timestamp). All five variants behaved
  identically in the experiment.
- **How to find a stuck grab next time:** `xres`(XRes) client list + XRECORD
  delivered_events probe (sources in /tmp/claude-1000/focustest/rec.c, xres.c) - see
  03-pitfalls/X11-AND-SESSION-PITFALLS.md.

### 🔍 2026-09-20 (later, live on the user's real session): the keyboard never reaches csv-hq at all - override_redirect

The fix below is real but is NOT why the user's csv-hq stayed dead. Live
evidence from the user's real GNOME/Wayland session, with the fixed binary:
- csv-hq log: `GRAB ... rc=1` x5 then `late-retry gave up (another client
  still holds the keyboard)`; a grab probe from a third client also got
  `AlreadyGrabbed` every time, even while a real X window had focus.
- A passive listener (`XSelectInput` KeyPress|FocusChange on csv-hq's
  window, no grab) recorded ZERO KeyPress events while the user clicked in,
  pressed Enter, typed and pressed Esc - only `FocusIn/FocusOut mode=3`
  (NotifyWhileGrabbed). Arrows, Enter and Esc all dead; mouse clicks work.
- `#.desktop/livedesk_override_redirect.pdl` was `override_redirect=true`
  (taskbar "@" always-on-top), and csv-hq's `<window>` had no `managed`
  class, so its window was created override_redirect. This is the DOCUMENTED
  cause: Mutter/XWayland never routes keyboard focus to override_redirect
  windows (`09-appendix/pc-hq-leg-vs-nu-fix.md` §3-A/§4, `03-pitfalls/
  X11-AND-SESSION-PITFALLS.md` 2026-09-05 "arrows control nav broke again",
  and the 2026-09-14 text-edit-hq entry). XWayland also restricts
  XGrabKeyboard for such clients (khtpm_entity.c ~3354 comment), which is
  what the `AlreadyGrabbed` was.
- Fix applied: `class="... managed"` on csv-hq's and text-edit-hq's
  `<window>` (same mechanism pchq-board / export-hq already use; honored at
  window creation, so the window must be closed and reopened). Cheap
  diagnostic without any code: turn the taskbar "@" always-on-top OFF
  (override_redirect=false) and reopen the window.
- NOT verified on hardware yet. Wider follow-up: any HQ window with a
  cli_io/text_area/grid should probably be forced managed regardless of the
  PDL (like the dock windows, `dock_managed`); open-hai/chat-hai/network
  browser share the risk.

### 🛠️ 2026-09-20 root cause + fix (found by reproducing in a private Xephyr + private house)

**What was NOT broken:** the grid state machine. Driven through the
per-window relay it matches the spec end to end: Enter on nav 10 arms
(`[#]10. jump: _`, A1 highlighted), arrows move the cursor, `a` `1` `1`
shows `jump: a11_`, Enter jumps to A11, a second Enter enters the cell
(`[^]`), typed text lands, Esc commits `SETCELL:A11` (csv_hq_ui.txt gets
`cell_10_0=hi`). Nothing in `default_grid_handle_key()` needed changing.

**Root cause 1 (confirmed, both symptoms): the grid was disarmed by every
full reparse.** `reparse_chtpm_if_changed()`'s full-rebuild path re-arms the
armed field by saved key via `kh_find_input_by_key()`, which only matched
`cli_io` and `text_area` - never `<grid>`. So any reparse while the grid was
armed logged `REPARSE key=cell_ NOT_FOUND - ungrabbed`, dropped
`g_default_input_elem`, and released the keyboard grab. That path is taken
on EVERY reparse because `incremental_reparse=0` has been house-wide since
2026-09-15 (`#.desktop/hq_ui.pdl`, pool-leak revert). csv-hq's manager
republishes `csv_hq_ui.txt` on every status/SETCELL change, so the grid lost
`#`/`^` right after arming (or right after the first commit): "Enter doesn't
activate, double click works" (each click re-arms; the log's five arm lines
inside 3 seconds), and typed input "not taken" (armed state already gone).
Reproduced: after the Esc-commit the badge fell back to `[>]10.` and the log
showed the NOT_FOUND line.

**Fix 1** (`khtpm_core_render.c`): `kh_find_input_by_key()` also matches
`<grid>`; before the rebuild the armed grid's cell cursor, edit mode, jump
buffer and cell buffer are captured and restored on the re-found element (the
grab is never released). After the fix the same run ends in `[#]10. jump: _`
with `REPARSE key=cell_ FOUND grid row=10 col=0 edit=0`, matching the spec
(Esc commits and returns to `#`; a second Esc disarms).

**Root cause 2 (reproduced on the log signature; who holds the grab on the
real desktop is NOT confirmed): the grab burst was too short.**
`kh_grab_keyboard_retry()` tried `XGrabKeyboard` 5 times over ~25ms and gave
up, leaving the field armed but with no grab. The user's log
(`GRAB key=cell_ attempts=6 rc=1`) is exactly what a foreign client holding
the keyboard produces - reproduced here with a small holder program. Prime
suspect for the holder: the dock's display-wide grab (`dock_grab_keyboard()`)
whose release only fired on FocusOut/keypress/reparse, so a missed FocusOut
left it stale (see the 2026-09-04 comment in `handle_key()`).

**Fix 2** (`khtpm_core_render.c`): (a) a failed grab now keeps retrying from
`hq_idle_tick()` every ~30ms for up to 3s (`kh_grab_retry_tick()`); with a
holder that releases after ~2s the log shows `GRAB late-retry succeeded`
(with a holder that never releases: `late-retry gave up`, by design);
(b) the dock re-checks live focus every ~100ms and releases a stale grab
itself instead of waiting for a key.

**Verified:** relay end-to-end sequence above; grab contention with a
foreign holder (both outcomes); build clean. **NOT verified on real
hardware:** (1) that the dock (or which client) really holds the grab on the
user's session - I did not probe the live display; (2) the dock's new
proactive release (no taskbar in the private house); (3) real keystrokes
(relay bypasses grabs). **What to check:** in csv-hq press Enter on nav 10
once (badge should become `#` and stay), type `a1` Enter, Enter, text, Esc;
then read `@.apps/csv-hq/kh_focus_debug.log` - expect `REPARSE ... FOUND grid`
lines and no `NOT_FOUND`; `GRAB ... rc=1` followed by `late-retry succeeded`
means a foreign holder was outlasted, `gave up` means something still holds
the keyboard for 3s+ (then find it: run `xdotool`-free probe or close other
armed windows).

---

## ✅ CLOSED 2026-09-17: taskbar HQ header cells drifted after 5.menu's insertion - wrong labels, missing dropdowns, wrong reopen target

**Reported:** direct live report - "i clicked h-ai, and i noticed it
sais 'notes-clock' (instead of notes-hai) then i clicked clock. its
dropdown is gone... and now nav is stuck." Follow-up clarified via a
quick check: the real Clock header cell itself (not a mislabeled row)
failed to show its dropdown - nav correctly focused the cell (the "^"
indicator) but no rows ever appeared.

**Root cause, confirmed by direct code read + a real grep of every
`which == N` / `hq_open == N` site across both taskbar manager files**
(not guessed from the old per-line comments, which turned out to be
part of the problem): 5.menu's own 2026-09-14 insertion shifted every
header cell from position 5 onward by one (`pals` 5→6, `palettes`
6→7, `player` 8→9, `db` 9→10, `network` 13→14, `ai` 14→15, `clock`
15→16 - confirmed against the real, current template,
`khtpm_strip_header.xhtpm`'s own `strip-cell-N` declarations). The
*live dispatch chain* (`ktb_hq_open()`'s own `which == N` chain) was
updated correctly at the time. Five other places, checked one at a
time and found stale, were not:

1. `khtpm_taskbar_manager.c`'s notes-row fallback name-lookup switch
   (only reached when `ktb_cell_id()` has no real declared id for a
   position - true for every cell except "toys" today) still used the
   PRE-5.menu numbering wholesale (`case 5: nm="pals"` should be
   "menu"; `case 6: nm="palettes"` should be "pals"; `case 8:
   nm="player"` should be db's old value on a cell that's really
   inert; `case 13/14/15` for network/ai/clock should be 14/15/16;
   `case 16` for clock didn't exist at all, falling to default "hq").
   This is the direct cause of "notes-clock" instead of "notes-hai".
2. `khtpm_taskbar_manager_main.c`'s `publish_strip_ui()` - the actual
   root cause of the missing Clock dropdown: `if (s->hq_open >= 1 &&
   s->hq_open <= 15)` gated whether `drop_target` got published at
   all. Clock is `hq_open==16`, so this check failed, `drop_target`
   published EMPTY, and the xhtpm template's dropdown-child rows
   (`target_id="${drop_target}"`) had nowhere to attach - they never
   rendered, even though `n_hqitems` itself was correct. The cell's
   own nav focus still highlighted (driven by `hq_open` directly, not
   `drop_target`), which is exactly the "shows '^', locks to the
   number, but no dropdown" symptom reported live.
3. Two separate `is_pals` checks (one in `publish_strip_ui()`, one in
   the ASCII/text-mirror HQITEM fragment builder) both said `hq_open
   == 5` (now "menu") instead of `== 6` (real "pals") - real per-pal
   sprite icons in the Pals dropdown were silently never applied.
4. `khtpm_taskbar_manager.c`'s `livedesk:play-toggle`/`livedesk:play-
   stop` handlers both reopened `ktb_hq_open(s, 8)` after flipping
   Play Mode - `which=8` is a genuinely inert cell today (no menu
   builder dispatches there at all); Player is really 9. Both were
   already wrong the same day they were written (2026-09-14) - 5.menu
   landed the same day and this reopen call was never updated.

**Fixed**: all five sites corrected to match the real, current
dispatch chain (verified against the live template, not assumed).
`publish_strip_ui()`'s own check now uses the real shared
`KTB_STRIP_N_CELLS` macro instead of another hand-counted literal -
the exact bug class every one of these five bugs was.

**Real architecture gap, NOT fixed here** (direct live follow-up:
"numbers arne't supposed to be hardcoded. they were supposed to be
refactored to be dynamic, from .pdl like bottom tb. did we not do that
yet?"): correct - `ktb_cell_id()` (the real, data-driven position→id
lookup via `#.desktop/livedesk_header_cell_ids.txt`) exists and works,
but that file has exactly ONE real entry (`12|toys`) as of this
writing. Every other header cell still resolves through the
positional `which == N` chain this whole bug class lives in. Fixing
the five numbers is a real, necessary, immediate fix - it is not the
same as finishing the migration OPEN-ITEMS.md #14 already tracks
("Strip submenus → data-driven... the four directory-scanning builders
(user/pals/toys/clock) still hardcoded"). That migration is real,
separate, follow-up work, flagged here so the next pass doesn't
mistake this fix for that one.

**Verification**: rebuilt via `build_khtpm_strip.sh` (clean, no new
warnings), restarted the live taskbar via `run_khtpm_strip.sh new`.

---

Different from `BUG-LOG.md` (append real fixed/found entries) and
`03-pitfalls/` (lessons already extracted). This file is for a bug
that's **real, reported more than once, and not yet fully explained**
— so the next person/agent who hits it again doesn't start from zero.
One entry per bug. Update in place as evidence accumulates; don't
re-open a NEW entry for the same symptom.

---

## ✅ CLOSED 2026-09-14: book-stack's verse popup lost CJK glyphs and text color

**Reported:** direct live report, with a real "how it looks now" vs
"how it looked before today" screenshot pair: current showed a dark
background with plain, uncolored text and Chinese characters as tofu
boxes (□□□□□□□); the earlier screenshot showed a cyan background with
bold, colored text and correctly-rendered Chinese.

**Investigation, git-first (per direct instruction to check git diff
before guessing):** traced the real render chain - book-stack's own
`meta.pdl` "Read" method → `pieces/reader/.../branches/bible_text/
run.sh` → `khtpm_show_text.+x` (a thin relay writer) → a
`SHOW_TEXT_FILE:` command in book-stack's OWN `interact_relay.txt` →
handled by that SAME entity's own `khtpm_core_render.+x` process
(`tp_main()` mode). Checked git history for every file in that chain -
`khtpm_show_text.c` (Aug 5), the mutaclysm `system/renderer.c`/
`chtpm_rgb_render.c` (no commits since a Sept 1 path-rename, unrelated)
- all showed zero relevant diffs. Binaries were all newer than their
own source (no stale-build explanation either). Ruled out a live
theme value being the cause too (checked `livedesk_theme.pdl`
directly).

**Real root cause, found by reading `popup_draw_text()`/
`load_popup_fontset()` in `khtpm_core_render.c` and reproduced live**
(a real `SHOW_TEXT_FILE` relay fired at a fresh book-stack process,
frame-dumped via `dump_frame_png_op` - not guessed): this code was
NEVER actually right, not a regression from a recent change -

1. `load_popup_fontset()` requested Xft font family `"monospace"`
   (falling back to `"DejaVu Sans Mono"`) - neither has CJK glyph
   coverage. Every OTHER CJK-capable text path in this same file
   (`font_ui`, via `reload_font_ui()`) explicitly requests `"Noto Sans
   CJK SC"` - this function's own 2026-08-05 header comment claimed to
   do the same thing font_ui does, but never actually matched its real
   font family.
2. `popup_draw_text()`'s own Xft draw call hardcoded the text color to
   `"#000000"` regardless of theme. A 2026-09-09 fix (direct report:
   "after choosing the bible verse / tao it shows a popup with text -
   those are still black and white") themed the popup's own
   BACKGROUND (`g_theme_bg`, at window creation) but never touched
   this hardcoded foreground - silently half-closing that exact same
   complaint, which is presumably why it resurfaced.

**Fixed**: `load_popup_fontset()` now requests `"Noto Sans CJK SC"`
first (matching `font_ui`'s own real convention), with the old
`"monospace"` request kept as a real fallback, then `"DejaVu Sans
Mono"` as a final fallback - not removed, just reordered.
`popup_draw_text()` now uses live `g_theme_fg` instead of hardcoded
black. Verified live: a real `SHOW_TEXT_FILE` relay + frame dump
before the fix reproduced the exact reported symptom (tofu boxes,
uncolored text); the same test after the fix showed correctly-
rendered Chinese characters and theme-colored (cyan) text, both
matching the "how it used to look" reference screenshot.

---

## ⚠️ OPEN 2026-09-14: real physical keyboard input silently never arrives at an armed cli_io/text_area, despite grab+focus both reporting success

🔄 **2026-09-20 LIKELY SAME CAUSE, NOT YET RE-TESTED:** the csv-hq keyboard-dead report was a stuck display-wide grab held by the Cursword pal (`03-pitfalls/HOUSE_CODE_PITFALLS.md` #24, fixed `2c1301ab`). This text-edit-hq entry matches (armed field, focus looks fine, zero keys). Re-test text-edit-hq with the fixed binaries and a restarted Cursword; if it types, close this entry.

**Reported:** direct live report on `text-edit-hq` - "i tried selecting it
didn't work" → (after two separate, real selection-preservation bugs were
found and fixed, see `03-pitfalls/HOUSE_CODE_PITFALLS.md` and commit
`0bbaf435`) → "did u fix it? im still not getting selection highlight" →
"yea its not working. but it seems like a focus issue cuz typing isn't
working also. clicking nav in window still works." Confirmed: not a
selection-specific bug at all - the field never receives ANY real
keystrokes while this is happening, selection included.

**Live evidence, `@.apps/text-edit-hq/kh_focus_debug.log`:**
```
05:39:54.525 GRAB key=editor attempts=1 rc=0(0=success) real_focus_is_us=1
05:39:55.137 GRAB key=editor attempts=1 rc=0(0=success) real_focus_is_us=1
05:39:55.357 GRAB key=editor attempts=1 rc=0(0=success) real_focus_is_us=1
05:39:56.070 GRAB key=editor attempts=1 rc=0(0=success) real_focus_is_us=1
05:39:57.740 GRAB key=editor attempts=1 rc=0(0=success) real_focus_is_us=1
05:39:58.116 GRAB key=editor attempts=1 rc=0(0=success) real_focus_is_us=1
05:40:12.997 INCREMENTAL_REPARSE ok removed=0
```
Six real `XGrabKeyboard` attempts in ~3.5 seconds, EVERY one reporting
`rc=0` (success) AND `real_focus_is_us=1` (a live `XGetInputFocus`
readback also confirms this window holds real X focus) - yet **zero
`KEYPRESS` lines appear anywhere in this window**, despite the user
actively typing during exactly this span. The repeated re-grabs
themselves are real and concerning (something is re-triggering
`activate_focused()` far more often than a single click-in explains -
not yet root-caused, flagged separately in commit `0bbaf435`'s own
message) - but the core mystery is that X11's own APIs report total
success while real keystrokes are provably never delivered.

**Matches a known, documented, historically-recurring house bug
class - NOT a fresh discovery, but the SAME class showing up in a new
place:**
- `09-appendix/pc-hq-leg-vs-nu-fix.md` - "XGetInputFocus lies":
  `override_redirect` windows under Mutter/XWayland can report
  successful focus/grab via the X11 API while real hardware `KeyPress`
  events never actually arrive. Documented as having regressed and
  been re-fixed once already (`git show 35c1b0b1~1`) - a real,
  confirmed recurring-regression pattern, not a one-off.
- `1.^V-hq/_.0.aigent-testing-k9.txt` §F-19: a related instance in the
  taskbar's own popup keyboard focus, same "reports success, doesn't
  actually work" theme, only ever caught empirically (XTest
  injection), never by code review.

**The wrinkle that makes THIS occurrence not a clean match**: both
prior documented cases were specifically about `override_redirect`
windows. This house's `#.desktop/livedesk_override_redirect.pdl` is
currently `false` (**managed** mode, not override_redirect / "always-
on-top") - text-edit-hq is failing in the OPPOSITE state from what the
existing docs describe. Either: (a) this is the same underlying WM/
XWayland quirk showing up for managed windows too, for a related but
distinct reason, or (b) it's a genuinely new, third cause that only
looks similar. Not yet distinguished.

**Real, cheap, not-yet-run diagnostic**: toggle the taskbar's own "@"
button (always-on-top ON = override_redirect=true) and retry typing in
text-edit-hq. If the symptom changes, that confirms override_redirect-
vs-managed is the load-bearing variable here too, same as the
documented cases. If it doesn't change either way, this is likely
cause (b), a new mechanism, and the override_redirect docs are a red
herring for this specific instance.

**Not yet done**: the override_redirect toggle test above; finding
what's actually re-triggering `activate_focused()` so often (the
repeated-GRAB pattern itself, independent of whether keys arrive);
checking whether `khtpm_strip_keyboard_ascii.+x`-style relay delivery
(a separate raw-termios path some other house docs reference for
taskbar input) is involved here too, or whether this is purely direct-
X11-KeyPress delivery failing.

**Live update 2026-09-14, same day - the override_redirect toggle test
above WAS run, and rules out hypothesis (a):** direct report "shift
arrow didn't have focus things are missing focus again." Confirmed:
`#.desktop/livedesk_override_redirect.pdl` now reads `true`
(always-on-top ON) - typing STILL fails in this state too:
```
06:20:03.255 GRAB key=editor attempts=1 rc=0(0=success) real_focus_is_us=1
06:20:04.881 GRAB key=editor attempts=1 rc=0(0=success) real_focus_is_us=1
06:20:15.390 GRAB key=editor attempts=1 rc=0(0=success) real_focus_is_us=1
06:20:15.925 GRAB key=editor attempts=1 rc=0(0=success) real_focus_is_us=1
06:20:16.330 GRAB key=editor attempts=1 rc=0(0=success) real_focus_is_us=1
```
No `KEYPRESS` lines follow any of these. Since it fails identically in
BOTH override_redirect and managed states, this is **not** the same
override_redirect-specific mechanism `pc-hq-leg-vs-nu-fix.md` documents
- override_redirect-vs-managed is a red herring for this specific
instance (cause (b) from the wrinkle above, not (a)).

**New, real, differentiating clue** - the SAME tail of the log also
shows the one real grab FAILURE seen so far:
```
06:16:53.704 GRAB key=editor attempts=6 rc=1(0=success) real_focus_is_us=1
06:16:53.866 GRAB key=editor attempts=6 rc=1(0=success) real_focus_is_us=1
06:16:54.355 KEYPRESS key=editor ks=107 ch=107(k)
06:16:54.390 KEYPRESS key=editor ks=105 ch=105(i)
...
```
`rc=1` here means the grab genuinely FAILED (the "(0=success)" in the
log format string is a static label, not this line's own result) -
yet real keys arrived immediately afterward anyway. This is the
opposite of what "the grab delivers keys" would predict, and is worth
taking seriously: it suggests the exclusive `XGrabKeyboard` itself may
be the unreliable part under this Mutter/XWayland setup - not focus,
not the arm logic - and that plain `XSetInputFocus`-based delivery
(no active exclusive grab) may work MORE reliably here than the grab
this file has always assumed it needs. Not yet tested directly (would
need a real, deliberate "skip the grab, rely on focus alone" trial),
but a real, concrete, differentiating next hypothesis - more promising
than continuing to chase override_redirect.

**Follow-up, 2026-09-14 - selection-highlight investigation, a REAL,
SEPARATE, now-FIXED bug found alongside this one, not the same root
cause**: tasked with checking whether "selection highlight never
appears" is just downstream of this entry's own keyboard-delivery
mystery, or a genuinely separate gap. Confirmed it is separate.
`sel_anchor`/`cursor` selection state is tracked completely generically
in `khtpm_core_render.c`'s key handler (`g_key_shift`-gated, not
tag-gated) - a real `cli_io` gets exactly the same correct
`[sel_anchor,cursor)` range as a `text_area` whenever a real Shift+Arrow
keypress DOES reach it. But `&.widgits/_shared-lib/khtpm_draw_core.c`
only ever drew the selection band (the `sel_lo`/`sel_hi` XFillRectangle
`#2f5f8f`) inside the `<text_area>`-only branch - the separate,
single-line `<cli_io>` draw path (same file, the `draw_label ==
shown_label` block that already draws cli_io's own cursor bar) had NO
equivalent code at all, so even a `cli_io` with a genuinely correct,
non-collapsed selection range would render zero visible highlight.
**Fixed**: added the same sel_lo/sel_hi band draw to the single-line
cli_io path, same `#2f5f8f` fill, same scoping as the existing cursor
bar (armed + unclipped label only). Rebuilt clean via
`build_core_render.sh` in `*.monads/*.livedesk-taskbar/ops/` (pre-
existing snprintf-truncation warnings only, no new warnings, no
errors). **Not independently re-verified against real hardware input**
because of the keyboard-delivery bug documented in this same entry -
could not drive a real Shift+Arrow keystroke into any live or
disposable test window with confidence it would actually arrive, so
this fix is code-reviewed-correct (traced the identical logic pattern
against the already-proven text_area branch) but not yet pixel-
verified live. If the highlight still doesn't appear once the
keyboard-delivery bug above is fixed, re-check this cli_io draw path
first before assuming a third bug.

---

## ⚠️ REOPENED 2026-09-13 (4th occurrence): entities drop off the bottom taskbar after a while, but stay on-screen

**Reported:** 2026-09-11/12, direct live report: "why after a while
entities are dropping from the bottom toolbard (but staying on
screen)... a separate agent caused a crash [since then unable to
verify live]."

**What's confirmed:**
- ✅ A REAL, separate, related leak was found and fixed while
  investigating (2072cc72): `livedesk_hq_windows_<pid>.txt` registry
  files (one per HQ **app** window — chat-hai/db-hq/network-browser,
  gated on `g_default_has_sidebar_panel`) never got cleaned up on a
  crash/SIGKILL — only a clean exit's `atexit()` did. Confirmed 64
  stale files for long-dead PIDs sitting in `#.desktop/`. Now
  self-heals: `ktb_merge_hq_windows()` (the taskbar's own reader,
  `khtpm_taskbar_manager.c`) deletes a registry file the moment its
  own liveness+identity check (`ktb_pid_is_hq_renderer`) proves it's
  stale, instead of skipping past it forever.
- ❌ That fix does **NOT** explain the entity-TILE symptom directly —
  desktop entity/pal tiles (`m8_redhorned`, `self`, `m1_ninjadragon`,
  `asa`, `ava`, `book-stack`, etc.) run through `tp_main()` (entity/
  tile mode), a completely different code path from the
  `g_default_has_sidebar_panel`-gated HQ-window registry above. Not
  yet traced.

**Real next steps, not yet done:**
1. Find the ENTITY/tile equivalent of `ktb_merge_hq_windows()` — how
   does the strip build its list of pal tiles to show at the bottom?
   Likely a separate registry/ledger (see
   [[proc-ledger-consolidation]] memory — `livedesk_proc_list.txt` may
   be the real source of truth here, not `livedesk_hq_windows_*`).
2. Check whether that list is liveness-checked the SAME way (PID alive
   + correct comm), or via a weaker/staler check that could silently
   drop a still-alive entity under some real condition (a reparse
   race, a stale mtime check — see [[prefer-marker-files-not-mtime]],
   or a cap like `KTB_MAX_HQ_WINS` if an entity-tile equivalent cap
   exists and something is filling it with dead entries first).
3. Reproduce for real: leave several entity tiles open for an extended
   period (the report says "after a while" — this may be a slow leak
   or a periodic-tick bug, not an instant one) and watch the relevant
   ledger/registry file(s) directly for the exact moment an entry
   disappears, rather than guessing from code alone.
4. The report notes a **separate agent's own crash** happened around
   the same time — a real, plausible confound. Rule out (or confirm)
   whether that crash's own cleanup (or lack of it) is what triggered
   this specific instance, vs. a structural bug that would recur
   regardless of any one crash.

**Ruled out, 2026-09-12** (direct question: "could that be what got rid
of the good entities? a false positive from the self-healer before?"):
NO — confirmed by code structure, not just plausibility. The registry
file the self-healer above touches is only ever WRITTEN by `redraw()`'s
own generic-HQ-window branch (gated on `g_default_has_sidebar_panel`),
which lives entirely outside `tp_main()` (entity/tile mode's own
separate function, starting well after that write code in the file,
never setting that flag). Entity tiles structurally never had an entry
in this file to begin with, so the self-healer had nothing of theirs
to false-positive delete. The self-healer is confirmed safe and
unrelated to this bounty's real symptom.

**Real root cause found, 2026-09-12 (3e334acc)**: `livedesk_registry_add()`
(an entity's own bottom-bar ledger line) is called EXACTLY ONCE, at
`tp_main()` startup - confirmed via grep, no other call site existed.
The taskbar manager's own registry reader does a real read-prune-write
cycle every tick, dropping any entry whose PID fails a single
`ktb_pid_alive()` check on that one read. Since an entity never
re-registered itself after startup, any single wrong/transient result
from that check, ever, across a long session, permanently erased the
line - while the process itself kept running and rendering, matching
"staying on screen" exactly. This is the opposite of the HQ-window
registry, which every generic HQ window rewrites on EVERY redraw tick
(already self-healing) - the entity path never got that same
treatment. **Fixed**: `tp_main()`'s loop now re-calls
`livedesk_registry_add()` every ~10s, same self-healing shape. Not yet
independently re-observed live over a long real session (the original
report couldn't be reproduced on demand) - if this resurfaces after
the fix, re-open this exact entry rather than starting a new one.

**Real regression this same fix caused, found + fixed same day (eca3c071)**:
the periodic re-call above fires on its VERY FIRST tick, not after a
real 10s wait - its `static struct timespec` timer starts at zero, so
"10s have passed" is true immediately, right alongside the real
startup registration. `livedesk_registry_add()`'s own prune loop only
ever dropped DEAD pids, so re-registering a still-alive pid appended a
second, genuine duplicate line for the same live entity.
`khtpm_taskbar_manager.c`'s `load_tabs()` then saw two live-PID lines
for one entity in a single read and SIGTERM'd the "duplicate" (logic
written for an actual zorder-respawn leftover, not this) - killing
every non-`cursword` entity within 1-2s of every spawn, reproduced via
both a manual launch and the real Player>reset path (`cursword`
structurally exempt from every close/kill sweep, hence the only
survivor). Root-caused live via temporary debug logging in
`load_tabs()` (added and fully removed same pass). **Fixed**: the
prune loop also drops any existing line for the SAME pid being
re-registered, so a re-registration replaces its own prior line
instead of piling up beside it. Verified live: all 6 killed pals
relaunched clean, survived past the 10s checkpoint that used to kill
them. Still worth independently re-observing over a real long session
before this whole entry is considered fully proven, same caveat as
above.

**A THIRD, different regression found + fixed same series, 2026-09-13
(74debf38)** - direct live report: "some entities are missing again,
after your fix... this didn't used to happen." This time nothing died
(all 6 processes confirmed alive, correctly registered, correct
`n_tabs=7` in every backing file) - the BOTTOM BAR ITSELF was visually
frozen showing a stale, out-of-order 4-tab subset, confirmed via a
real PNG dump of the live dock window (`dump_frame_png_op`), not
guessed from code alone. Root cause, found via a second round of
temporary debug logging: `write_small_file()` (writes `strip_ui.txt`,
the file driving the dock's `${n_tabs}`/`${tab.*}` vars) did
`remove(path)` THEN `rename(tmp, path)` - not atomic, opening a real
window where a concurrent reader's `fopen()` gets `ENOENT`. When that
hit `khtpm_core_render.c`'s dock-peer reparse, `parse_chtpm()`
returned NULL and the code unconditionally did
`g_dock_peer = parse_chtpm(...)`, NULLing an already-good tree with no
future retry ever repainting it - worse during rapid entity churn
(more writes = more chances to hit the gap). **Fixed**: dropped the
redundant `remove()` (every other writer in this codebase already
skips it); both dock-peer reparse sites now only adopt a non-NULL
parse result, keeping the last-good tree instead of blanking on any
transient read failure. Verified live: PNG dump of the dock window
during the same staggered 6-entity relaunch that used to freeze it now
shows all 7 tabs correctly. If entities visually vanish from the dock
again with processes/registry confirmed alive, this is the file to
re-open, not a new one - and dump the actual window pixels
(`dump_frame_png_op`) before trusting any backing file's content, since
this class of bug is specifically "data is fine, the render is stale."

**4th occurrence, 2026-09-13** - direct live report: "bottom tb is
missing entities again. no matter what this cant happen. how do we
fix this once and for all?" Confirmed the SAME "data is fine, render
is stale" class as `74debf38` above - not that fix's own specific
ENOENT race (checked directly): `strip_ui.txt` held the correct real
7-tab data the entire time, its content-hash was genuinely STABLE
across 5+ full seconds (ruling out the two-poll debounce too), and
`dump_frame_png_op` on the live dock window still showed it blank
regardless - a fourth, distinct mechanism in this same fragile
mtime/hash/debounce chain, not isolated this pass.

**Real, structural answer this time (`af273699`)**, matching the
direct "once and for all" ask: stopped trying to find and patch the
Nth specific detection gap in this chain. The dock strip now
unconditionally forces a full reparse+relayout+repaint every 3s,
completely independent of mtime/hash/debounce ever agreeing again -
gated to dock windows only (`window_is_dock()`) so every other window
keeps its existing cheaper change-only behavior. A genuinely bounded
worst case now exists: however this next fails, it self-heals within
3 seconds, not "possibly never again until a manual restart." Verified
live: a fully clean restart (every taskbar/entity process killed
first, not a partial one) held all 7 real entities correctly
rendered, with the tab order visibly reshuffling on each heartbeat
tick, across a 35s watch.

**A second, separate thing found the SAME session, worth recording so
it isn't mistaken for this bug's own root cause later**: a test
restart done WITHOUT first killing already-running entity processes
(this session's own repeated manual `run_khtpm_strip.sh new` calls
while testing unrelated code) caused a real entity die-off within
~10s - traced to colliding with `livedesk_spawn_active_desk()`'s
already-guarded (`ktb_pid_alive`-checked) respawn-on-startup logic:
old entities registered under a stale/pruned prior registry snapshot,
a fresh respawn creating a real second live PID per entity, and
`load_tabs()`'s own existing one-entity-one-PID dedup (its real,
intentional job) correctly SIGTERMing the duplicates - working as
designed, just startling to watch happen. Not a bug in the sense this
entry tracks (a normal user session never does a partial restart that
way), but real enough to name: if a future partial-restart workflow
becomes common, `livedesk_spawn_active_desk()` doing its own liveness
check via a fresher registry read (not just relying on the dedup
safety net downstream) would remove the visible flicker.

**Superseded same day (`930fd9ba`)** - direct follow-up: "we dont use
mtime or hash for render, ideally we use marker filesize change only,
or stay with last render (mtime has edgecases); do u see this
instruction in golden rules? thats the final fix." Yes -
`CENTROID_GOLD_STD.md` rule 8 says exactly this ("not on mtime, not
on a hash, not per input event"), and the 3s-timer band-aid above
never actually followed it. The REAL final fix: this exact file
already has a proven, already-wired instance of the real marker
pattern one function away - `khtpm_taskbar_manager_main.c`'s
`publish_state()` writes `strip_ui.txt`/`strip_state.txt` THEN
appends one byte to `#.desktop/strip_frame_changed.txt`
(`touch_frame_changed()`); `dock_poll_strip_state()` in this same
render file already watches that marker's SIZE growth for its own
narrower focus-sync job. `reparse_chtpm_if_changed()`'s vars-changed
gate now watches that SAME marker directly for dock windows,
replacing the hash/debounce chain AND the 3s timer entirely - no
mtime, no hash, no polling interval, growth is the only signal, per
rule 8 to the letter. Non-dock windows are unaffected (most have no
single real "the" writer process the way the strip's manager is one,
so the hash/debounce path stays correct for them). Verified live:
render tracked real entity-count changes in `strip_ui.txt` exactly at
every check across multiple restarts, no lag, no staleness, no timer.

If the ORIGINAL symptom (blank dock, valid backing data) recurs even
with the marker gate in place, re-open this entry again - check FIRST
whether `strip_frame_changed.txt` itself is actually growing on every
real `publish_state()` call (a marker that stops growing is a
materially different, worse bug than this entry's own history: the
manager's publish path itself broken, not a render-side detection
gap).

**5th occurrence, 2026-09-14** - direct live report: "sword and castle
aren't on bottom toolbar... they were there for last 15 minutes. then
vanished. its the same bug we had before." Confirmed the exact same
"data is fine, render is stale" shape as every prior occurrence above:
at report time both `cursword` and `castle` were alive, both correctly
present in `livedesk_open.txt`, and `strip_ui.txt` already published
the correct `n_tabs=2` with both labels - the marker-based gate
(`930fd9ba`) was doing its job for the HEADER window the whole time.

**Real root cause this time, found by reading the code (not yet caught
live via debug log): the marker-gate fix never actually covered the
dock PEER (`g_dock_peer`, the bottom tab bar's own separate parsed
tree from `khtpm_strip_bottom.xhtpm`).** `peer_changed` (the flag
gating both real reparse sites for `g_dock_peer`) is computed from the
peer's own template-file mtime only - a 2026-09-13 "avoid unnecessary
I/O" simplification (same day as the 4th occurrence, unrelated commit)
explicitly decoupled it from `vars_changed` again, reasoning the
template is static and never touches disk after boot. True for the
template BYTES, false for what it RENDERS: `khtpm_strip_bottom.xhtpm`
is entirely `<repeat count="${n_tabs}">` - its actual content is 100%
data-driven, the same data `vars_changed`/the marker already tracks
for the header. So the header self-healed via the marker every tick,
while the peer silently went back to only refreshing on a file mtime
that structurally never moves - the exact bug `930fd9ba` closed,
reopened for one specific window by an unrelated cleanup pass that
didn't know the peer needed the same gate. Fixed (this session, before
`af273699`'s successor commit): `peer_changed |= vars_changed` in
`reparse_chtpm_if_changed()`, restoring the same marker coverage to
the peer the header already had. Verified live via a controlled
`mr_transfer_desk` round-trip (office `n_tabs=8` -> civ-test
`n_tabs=2` -> back), confirmed both header nav-count AND the peer's
own tab list updated together on each switch.

**Belt-and-suspenders addition, same fix pass**: given this exact
mechanism has now produced FIVE distinct root causes across three
days, a bounded 20s periodic force-reparse of dock windows was also
added (`s_dock_force_last`, `CLOCK_MONOTONIC`), independent of
`vars_changed`. Worth being honest about the tension this creates with
rule 8 ("not on mtime, not on a hash, not per input event") and the
3s-timer approach `930fd9ba` deliberately replaced for violating that
exact rule - this is NOT a replacement for the marker-based fix above
(that stays the real, primary mechanism), it's a bounded worst-case
fallback given this chain's own track record of yet another gap
surfacing. Deliberately narrower than every removed timer before it:
it forces the same reparse a real vars change already triggers,
touches no process lifecycle (unlike `ktb_self_heal_active_desk_
registry()`, disabled the same day for the opposite reason - see
`03-pitfalls/HOUSE_CODE_PITFALLS.md`), and is a no-op repaint when
data is already correct. If a 6th occurrence surfaces, treat this
20s fallback as a bug-severity DOWNGRADE signal, not proof the class
is closed - the real question each time is still "why did the marker-
driven path miss it," not "did the fallback eventually catch it."

**6th occurrence, 2026-09-14 (same day, ~1hr later) - a DIFFERENT,
more severe mechanism than every prior occurrence above.** Direct live
report: "the bottom toolbar is completely gone. i think it died again.
we have to prevent this bug." This time NOT the "data is fine, render
is stale" shape at all - confirmed via a raw `xwininfo -root -tree`
dump (real X server ground truth, not a backing-file read): the header
window (`0xa00002`, y=50) existed; the bottom bar's own separate X
Window simply did not exist ANYWHERE in the server's window list,
while the process itself was alive and actively ticking normally
(`DOCK_TICK` firing every ~10s in `kh_focus_debug.log`, correct
`n_tabs=2` data the whole time). Ruled out: process crash (uptime
continuous, no gap), the disabled self-heal (confirmed nothing calls
it), and any `XUnmapWindow`/`XDestroyWindow` touching `g_dock_peer_win`
anywhere in the file while the process is alive (grepped - none
exist). The one-time startup parse-retry (`g_dock_peer` itself, added
for the ORIGINAL 2026-09-13 version of this exact "missing on some
boots" symptom) logged no failure this run either.

**Real root cause: the window CREATION itself, not the reparse/render
path this whole entry has been about until now, is a second, separate
one-time startup decision with zero later recovery** -
`if (g_dock_peer) { XCreateWindow(...) }` runs exactly once, before
the event loop starts. Exact trigger for why it was skipped or lost
this one specific run not pinned down with certainty (both known
upstream causes - the file-parse race, the `dock-header` class check -
tested negative this time), but the SHAPE is the same "decided once,
never re-checked" pattern as every fix above, one level lower in the
stack (the window handle itself, not what gets drawn into it).

**Fixed, structurally**: the real creation code was factored out of
`main()`'s own startup path into `kh_ensure_dock_peer_window()` (a
real, single, reusable function - not a second copy), which the main
loop's own per-tick self-check (`hq_idle_tick()`) now also calls. Cheap
every tick (one `XGetWindowAttributes` liveness probe, a real no-op
when the window already exists and is alive); only actually rebuilds
the window (+ GC/Pixmap/XftDraw) when the probe proves it's genuinely
missing or the server reports it invalid. Deliberately narrow, same
posture as the 20s fallback above: touches ONLY this process's own
window handle, never another process's lifecycle, so it structurally
cannot repeat `ktb_self_heal_active_desk_registry()`'s own "died/
flickered/vanished" incident history. Verified live: relaunched via
the real `run_khtpm_strip.sh new` path, confirmed both bar windows
present in a fresh `xwininfo` dump and a real
`DOCK_PEER_WINDOW (re)created id=0x...` log line at startup.

This is now TWO independent, real self-heal layers stacked on this one
symptom class: the 20s marker-independent reparse (5th occurrence,
covers "window exists, content is stale") and this window-existence
probe (6th occurrence, covers "window doesn't exist at all"). If a 7th
occurrence surfaces, check FIRST which of the two this new instance
actually is (a fresh `xwininfo` dump settles it immediately) before
assuming either existing fix has a gap in it.

**7th occurrence, 2026-09-14 (same day) - the 5th-occurrence fix's own
force-refresh runs and still doesn't repaint. NOT closed by either of
the two self-heal layers above - a real, distinct, deeper mechanism.**
Direct live report: "i just switched to civ test from office, and the
tb bottom is showing the wrong entities? (are we updating tb correctly
yet? needs hardening it seems)." Confirmed via the SAME real evidence
standard this entry's own history insists on (a direct window frame
dump, not a backing-file read): the bottom bar showed `dsr`'s own
buildings (cursword, dsr_castle_a, dsr_bank_a1/a2, dsr_store_a1/a2) -
stale content from a desk visited earlier - while `civ-test` was the
real active desk and `strip_ui.txt` already held the fully correct
`n_tabs=2` (cursword, castle) data.

**This time the window genuinely exists (ruling out the 6th
occurrence) AND the 5th occurrence's own 20s force-refresh was
genuinely firing** - `kh_focus_debug.log` showed real,
repeated `INCREMENTAL_REPARSE ok` / `DOCK_TICK reparse_changed=1` /
`DOCK_TICK layout+redraw_ms=...` entries roughly every 20 seconds for
the full 87-minute life of the process (confirmed via `ps -o etimes`),
proving the reparse gate itself was doing exactly what the 5th
occurrence's fix designed it to do. Re-dumped the live frame twice,
several seconds apart, across two of these real reparse+redraw ticks -
pixel-identical stale content both times. **The reparse is real, the
redraw is real, the on-screen result never changes anyway** - this is
a `redraw()`-level bug (a stale cached frame/diff the repaint path
trusts instead of the freshly reparsed tree), not a
"was a reparse ever triggered" bug, which is what both prior fixes
address. Not yet root-caused inside `redraw()` itself - flagged here
for that real, separate investigation, not guessed at.

**Immediate real fix applied**: a full `run_khtpm_strip.sh new`
restart - confirmed via a fresh frame dump immediately after, correct
content painted right away. This is NOT a fix for the underlying bug,
only for the user's immediate block - a full process restart
trivially side-steps any stale-cache-in-memory bug the same way it
always has. If this occurs again, do NOT stop at another restart:
capture `redraw()`'s own execution around a reparse tick that produces
a `reparse_changed=1` log line but no visible change - the real fix
lives in there, not in the reparse gate (already proven correct this
pass) or the window-existence probe (also already proven correct).

**Bounded, KISS hardening applied same day** (direct instruction:
"harden that code in a safe logical KISS way"). Investigated further
before patching: the write→rename→read frame-file round trip
`dock_paint_peer()` uses to actually paint is byte-identical to the
HEADER's own equivalent block, which never shows this bug - ruling
that pattern out as the differentiator. The `g_dock_in_peer_paint`
reentrancy guard was also checked directly for an early-return path
that could leave it stuck at 1 forever (which would silently skip
every future repaint while `redraw()`'s own outer timing logs kept
looking normal, matching every observed symptom) - none exists, the
function is straight-line code with a single guaranteed reset at the
end. Root cause NOT pinned down after this pass either. Given that,
the safe, bounded answer per direct instruction: on the same real 20s
safety-net tick (5th occurrence), the peer's entire drawing surface
(Pixmap, GC, XftDraw, the X Window itself) is now destroyed outright;
the 6th occurrence's own `kh_ensure_dock_peer_window()` self-heal
(already proven correct, runs every tick) rebuilds it completely
fresh on the very next tick. If the staleness lives in any piece of
that drawing state, this closes it without needing to have proven
which piece; if it doesn't, this is a harmless, once-per-20s rebuild
of one small window's own resources, in-process only - matches the
exact "no process lifecycle, no cross-process effects" posture the
5th/6th fixes already established, not a new pattern. NOT yet
independently re-observed live over a long real session (this exact
7th occurrence took 87 minutes to surface) - if the ORIGINAL symptom
recurs even with this in place, that's real evidence the staleness
lives somewhere this fix doesn't reach (the serialized frame-file
content itself, or something in `assign_nav_and_layout()` shared
between header and peer) - re-open this entry, don't add an 8th patch
blind.

**Real, same-session correction: this fix itself caused visible
flicker.** Direct live report right after it landed: "tb is
flickering sometimes." Real cause, found immediately (not guessed):
the destroy+rebuild ran on the SAME 20-second cadence as the cheap
vars-changed nudge - a real `XDestroyWindow`/`XFreePixmap`/`XFreeGC`
every 20s is a genuine, visible teardown-and-recreate of the whole
window, not a free safety net. Matches this house's own referenced
`PITFALLS_ACTIVE_2026-03-18.txt` §17 ("ONE WRITER RULE... dual
writers/recreations cause flicker/corruption"). Fixed: split into two
independent timers - the cheap `vars_changed = 1` nudge stays at 20s
(no visible cost, just a data re-check), the actual destructive
rebuild moved to its own 5-minute cadence (15x less frequent). Still a
real, bounded worst-case (well under the 87 minutes the 7th occurrence
itself took to surface) without being visible to the eye during normal
use.

---

## ✅ CLOSED 2026-09-13: tab reordering / entities missing after restart - the real architectural cause

**Reported same day, multiple times, same underlying file:** "it just
reshuffled again... how did old codebase accomplish functionality"
and separately "some entities didn't show up on restart... was it
related to restart somehow like before? is there a guard against
that?" and again "bottom tb is missing entities again... this needs 2
stop happening. no patches. research house standards, and a real
solution."

**Real root cause, researched not guessed:** `#.desktop/livedesk_
open.txt` (the file that decides the taskbar's tab list) was written
by EVERY entity process independently, on its own unsynchronized
~10s self-heal timer - exactly the "one hot shared file, many
writers" shape `PROC-LIFECYCLE-CONSOLIDATE-REGISTRIES.md` (this
house's own design doc, §1) explicitly names as the pattern the
house's one-writer rule exists to avoid. This one anti-pattern was
the real, shared cause behind THREE separately-reported symptoms:
tab reordering (each entity's periodic rewrite repositioned its own
line), entities missing after a restart (races between the manager's
one-time startup read and entities' own async writes), and a third,
related PID-reuse class (see below).

**Real, structural fix (`764944ea`, `ec77a18f`)** - not another patch
on the same timer:
1. Entities register ONCE, at their own startup; the periodic re-add
   removed from `khtpm_core_render.c`'s `tp_main()` entirely.
2. The manager is now the SOLE writer - `ktb_self_heal_active_desk_
   registry()` (`khtpm_taskbar_manager.c`, called from `ktb_reload()`
   every tick, internally gated to the same ~10s cadence) restores a
   registry line for any active-desk pal found genuinely alive via a
   real `/proc` cmdline identity scan but missing from the registry,
   and separately re-calls the already-safe `livedesk_spawn_active_
   desk()` so a pal that silently never launched gets a real retry -
   never spawns duplicates (identity-guarded).
3. A THIRD site sharing the same PID-reuse false-positive class
   fixed twice earlier the same day (`ktb_pid_is_this_pal()`, for
   `livedesk_spawn_desk`'s `already_live` check and `livedesk_ensure_
   cursword`) was found and closed: `load_tabs()`'s own dup-kill
   logic - the function that builds the tab list every single tick -
   was still using bare `ktb_pid_alive()`. Live-traced via temporary
   debug logging (added and fully removed same pass): a stale,
   PID-reused registry line alongside a genuinely fresh spawn made
   two "alive" entries look like a real zorder-respawn duplicate, and
   the correct dup-kill logic SIGTERMed one of them - a real,
   plausible explanation for repeated silent spawn failures for one
   specific entity across several restarts.

**Verified live** across multiple clean restarts (full process kill
first, not partial): registry order stable, zero reordering, entities
restored without any manual click, matching the exact "no patches,
real solution" ask.

**Real, honest caveat, not fully closed**: this fix explains and
closes every registry/respawn-skip mechanism found - but one entity
(`book-stack`) kept failing to survive across several of these same
restarts for a DIFFERENT, still-open reason (see the new bounty entry
below) - its own crash, not a registry bug. Don't mistake a future
`book-stack`-specific absence for a regression of THIS fix without
checking that entry first.

---

## ✅ CLOSED 2026-09-13: book-stack dies silently, sometime after a genuinely successful launch

**Reported:** direct live report, same pass as the registry fix
above: "bookstack is missing. it needs to be fault tolerant. (it
appeared later) but thats bugy, janky. know fix?"

**What's confirmed:**
- ✅ NOT a registry/spawn-skip bug - ruled out directly. Temporary
  debug logging (added and fully removed) proved the manager's spawn
  retry correctly, repeatedly attempts to launch book-stack every
  self-heal cycle, and on at least one clean-restart observation
  `already_live=1` was reached almost immediately (a genuinely fast,
  successful self-registration) - the registry/self-heal layer is
  doing its real job.
- ✅ A REAL, confirmed successful launch happens first: book-stack's
  own `history.txt` shows `WINDOW_OPEN` then `ENTITY_PHYMOJI_LOADED`
  (its sprite/voxel atlas load completing) on every attempt - then
  nothing. The process is gone sometime after, silently, with no
  further history entries, no stderr, no exit code ever observed
  (spawned detached via `setsid nohup ... &`, not something this
  investigation could directly `wait()` on).
- ✅ NOT reliably reproducible on demand - inconsistent across
  restarts in the same session (sometimes survives indefinitely,
  sometimes dies within seconds), and a manual foreground/synchronous
  relaunch (bypassing the manager entirely) also succeeded without
  crashing at least once - genuinely intermittent, not a deterministic
  parse/data bug (an earlier "failed to parse" finding this same
  investigation turned out to be a false lead from an incorrect
  manual repro command - argc mismatch - not a real bug in book-
  stack's own package; ruled out and corrected in-session, worth
  recording so a future reader doesn't chase it again).
- ✅ This system routes core dumps through apport
  (`/proc/sys/kernel/core_pattern`), which needs `RLIMIT_CORE` raised
  per-process to even attempt a capture - the default here was 0,
  silently discarding every crash with zero forensic trail. This is
  why nothing could be found: there was never any evidence to read.

**Real fix landed this pass (`7da1ae7b`)**: not a fix for the crash
itself (not yet root-caused) but the fix that makes root-causing it
possible - every entity spawn now does `ulimit -c unlimited` before
the real launch (all 3 spawn sites in `khtpm_taskbar_manager.c`).
Harmless when nothing crashes.

**Major update, same day, live-traced with temporary checkpoint
logging (added and fully removed)** - direct re-report after it kept
recurring: "no bookstack still". Two real findings that change the
shape of this bug:

1. **`/var/crash` stayed empty even with `RLIMIT_CORE` raised**
   (`7da1ae7b`) - `strace -p` also failed outright
   (`ptrace(PTRACE_SEIZE)`: Operation not permitted - `yama.ptrace_
   scope` blocks it in this environment). Neither forensic tool is
   usable here; core-dump capture is a dead end in this environment
   specifically, not a fix that needs more time to pay off.
2. **The real, reproducible signal**: a temporary checkpoint
   (`append_history("...ENTER_MAIN_LOOP")` immediately before the
   main event loop, plus a per-iteration counter immediately inside
   it) showed the loop-entry checkpoint fires on **every single
   launch**, but the first-iteration checkpoint (one line later,
   after nothing but a trivial `while` condition check and an
   integer increment - code that cannot itself crash) **never once
   fired**, across ~15+ consecutive observed launch/death cycles,
   each dying within about one second of reaching the loop. Trivial
   code between two checkpoints, one always logged and the other
   never reached, is a real, strong signal this is an EXTERNAL
   termination (a SIGTERM arriving in that same ~1s window) rather
   than an internal crash - book-stack's own code was never actually
   caught misbehaving.

**Real next steps, not yet done** (superseding the core-dump-focused
ones above - that path is closed off in this environment):
1. Find what's sending book-stack SIGTERM within ~1s of every
   launch. Real candidates, not yet individually ruled out: (a) the
   shared, single-line `#.desktop/.livedesk_last_launch.pid` scratch
   file `ktb_system_recorded()` uses to learn the PID it just spawned
   (header comment, `khtpm_taskbar_manager.c` ~line 94) - if a
   SECOND spawn (the manager launches several entities in a tight
   loop) overwrites this shared file before the FIRST spawn's own
   caller reads it back, a wrong PID could end up registered/reaped
   against the wrong entity; (b) `load_tabs()`'s own dup-kill SIGTERM
   (now identity-verified as of this same day's earlier fix, but not
   re-examined AFTER this specific finding); (c) any other real
   SIGTERM sender in `khtpm_taskbar_manager.c` (`kill_hq_windows.sh`,
   `livedesk_kill_stray_entities()`, the proc-registry reaper) that
   could be matching book-stack's fresh PID by an unintended pattern.
2. Add a REAL (not temporary-debug) `signal(SIGTERM, ...)` log line
   in `tp_main()`'s own handler (`handle_shutdown_signal()`) if it
   doesn't already log which signal/when - the fastest way to
   confirm (1) directly instead of narrowing by elimination.
3. `book-stack`'s own `history.txt` was unusually large (~468KB)
   compared to other entities' - still untested as a factor, lower
   priority now that (1) points away from book-stack's own code
   entirely.

**A real, honest caveat about this investigation's own methodology**:
reproducing this required repeated live restarts, which was directly
disruptive to the user's own concurrent session ("i was on tb but
dissapeared" - a live report of collateral disruption from this same
debugging). Any future continuation of this investigation should
prefer passive observation over forced restarts wherever possible.

**Real root cause found and fixed (`b5443a67`)**, direct follow-up:
"theres nothing external to house quiting booktstack it must be in
house. it must be researched and fix. also i keep seeing weirdly
that it redraws then quickly dissapears." Upgraded `tp_main()`'s
signal handler to `SA_SIGINFO` (`handle_shutdown_signal_info()`,
`khtpm_core_render.c`) - a real, permanent diagnostic, kept - so it
writes the real sender PID via async-signal-safe `write(2)` before
exiting. Caught live on the very next occurrence: the sender was
`khtpm_taskbar_manager_main.+x` itself, confirming the user's own
instinct - not anything external.

Traced to the exact site: `ktb_self_heal_active_desk_registry()`'s
registry-restore half (landed earlier the same day, `ec77a18f`) only
checked the stale `s->tabs[]` snapshot from that same tick's earlier
`load_tabs()` call before appending a "restore" line for a pal found
alive via `/proc`. If book-stack's own process self-registered (its
real, separate, one-time startup write) in the narrow window between
that snapshot and this check, self-heal had no way to see it and
appended a SECOND, genuine duplicate line naming the exact same live
PID. `load_tabs()`'s own dup-kill (real, correct logic for an actual
zorder-respawn leftover) then saw "book-stack" twice on its very next
read and SIGTERMed the second occurrence - which, since both lines
named the same PID, meant killing the only real process there was.
This is exactly "it redraws then quickly disappears": the window
opens and paints for real, then dies to a real signal about one
manager tick later.

Fix: the registry-restore write now happens as a single pass inside
the one real registry lock, re-checking the LIVE file directly (by
real cmdline identity, not just a name match) instead of trusting the
stale snapshot. Also fixed a related correctness bug found while
writing this: the original patch would have called
`livedesk_read_open()` (which itself acquires/releases the same
shared, process-wide lock fd) from inside an already-held lock,
silently dropping protection the instant it returned.

Verified live: book-stack alive and stable for 30+ seconds with zero
forced restarts after the fix, all 7 entities present. If this exact
symptom (opens, paints, dies within ~1s) recurs for ANY entity, the
`SA_SIGINFO` handler left in place should immediately name the real
sender via that entity's own `last_signal.txt` - check that first.

---

## ✅ CLOSED 2026-09-13: taskbar HQ menu gets permanently stuck on nav 1, no key/click moves it

**Reported:** 2026-09-13, direct live report: "tb has an issue now,
its stuck on 1.hq no matter what is pressed" - then, after a full
strip restart cleared it: "ok, good... but it must happen later?"
(a real ask for the recurring root cause, not just relief that a
restart cleared it once).

**What's confirmed:**
- ✅ NOT caused by any of this session's other taskbar-adjacent
  commits (`dfedf360`, `5cf91818`, `5b584ada`, `d941a7d4`) - none
  touch the HQ-menu/scope-confine code path or either dock window at
  all; `5b584ada` only touches the separate bottom "pals" row's
  startup window creation, confirmed unaffected (that window,
  `g_dock_peer_win`, was present and fine throughout).
- ✅ A real, concrete gap found by code audit and fixed (`3e87e11f`):
  the reparse scope-restore block (`khtpm_core_render.c`, runs on
  EVERY reparse - the dock's own projector rewrites its state every
  ~400ms tick, so this is constant) only ever **sets**
  `g_default_scope_confine` to 1 in its two match branches
  (target_id container, or a `<tab>` locking onto `<sidebar>`) - it
  never reset it to 0 when the current scope trigger matches neither,
  which is exactly the case for a plain header cell (`strip-cell-1`
  "HQ": bare `onclick="ACTIVATE"`, no target_id, not a `<tab>`). If
  confine was ever left at 1 from an earlier real scoped interaction,
  it would survive every later reparse regardless of what got clicked
  afterward - `kh_elem_in_scope()` has no other match for a
  target_id-less item, so nav (arrows, digit-jump) permanently locks
  to just the current trigger. Fix: explicit reset to 0 before the
  two conditional re-sets, mirroring `activate_focused()`'s own
  ACTIVATE branch (already correct - unconditional reset before
  conditionally setting).
- ❌ **Not independently reproduced live.** Multiple clean-restart +
  keyboard-only (`xdotool key --window <id>`, real X KeyPress events)
  open/arrow-nav sequences all worked correctly, both before and after
  the fix - the exact sequence that leaves `g_default_scope_confine`
  stuck at 1 for THIS window (which structurally has no `<tab>` and no
  target_id'd trigger of its own anywhere in `khtpm_strip_header.xhtpm`
  - the only two paths that ever set confine=1 at all) was not
  isolated. The fix closes a real, legitimate staleness gap, but
  whether it's the actual mechanism behind the live report is unproven.

**Real next steps, not yet done:**
1. The synthetic-event testing here (`xdotool key --window`) sends
   real X `KeyPress` events directly to the render process - but the
   real user's physical keyboard may reach this dock window through a
   DIFFERENT path: `khtpm_strip_keyboard_ascii.+x` (a separate, raw
   termios-reading binary per the house's own ASCII-relay convention).
   If that relay's key→code mapping or delivery timing diverges from
   direct X KeyPress handling, the real bug may live there instead,
   invisible to any test that only sends synthetic X events to the
   window directly. Check that binary's own code path next.
2. If it resurfaces, do NOT just restart to clear it - first read
   `#.desktop/strip_state.txt` (hq_open/hq_n_menu/hq_focus) AND, if a
   live process attach is possible, the actual runtime value of
   `g_default_scope_confine`/`g_default_active_scope_id` before doing
   anything else, to finally catch it in the stuck state instead of
   only ever seeing it cleared.
3. Re-open this exact entry if the symptom recurs post-`3e87e11f`,
   rather than starting a new one - and note whether it was triggered
   by real physical keyboard/mouse input or another synthetic test, to
   start narrowing the input-path question in (1).

**Recurred, 2026-09-13, same day** - direct live report: "nav is
stuck at 1 again." This time genuinely reproduced live (not just
suspected): a real, GENUINELY FRESH strip process (a brand-new PID
from an always-on-top respawn) already showed the stuck state on its
very first frame dump, before this investigation sent it ANY input.
Confirmed via direct testing: a digit-jump (`5`) worked fine (landed
on nav5, proving the earlier `g_default_scope_confine` fix from
`3e87e11f` was NOT the active mechanism here), but the VERY NEXT
arrow-key press snapped straight back to nav1 - a different code path
entirely (`assign_nav_and_layout()`'s own drop-zone clamp, gated on
`g_dock_drop_lo && g_default_active_scope_id[0]`, checked on every
layout pass, independent of `g_default_scope_confine`).

Root mechanism for HOW a brand-new process ends up with
`g_default_active_scope_id` already non-empty before receiving any
real click **was not conclusively found** despite a real attempt
(exhaustively grepped every assignment site - all three are
click-handler code, none can run before the process's own event loop
starts; ruled out cross-process leakage, a var-driven class seed).

**Real, structural fix anyway (`67aacbe4`)**: rather than keep
chasing the mystery initial value, a freshly-started dock window now
explicitly zeroes `g_default_active_scope_root`/`id`,
`g_default_scope_confine`, and `g_dock_drop_lo`/`hi` once, right
after its own template parses, before the first real layout pass ever
runs - a fresh process cannot possibly have a real, current nav scope
yet, so whatever these globals happened to hold becomes irrelevant.
Belt-and-suspenders on top of `3e87e11f`'s own narrower fix. Verified
live: digit-jump then arrow-step composed correctly (`5` then `Right`
→ nav6) on a fresh post-toggle process, no snap-back.

**Real, honest gap this entry leaves for a future reader**: since the
exact seeding mechanism was never caught in the act, if this recurs a
THIRD time, the new zero-at-startup guard would only mean something
is setting this state DURING the process's life (post-startup, a real
click-path bug) rather than pre-seeding it - re-open this entry and
check for that distinction specifically, not just "does the bug still
happen."

---

## Research pass 2026-09-14 (haiku) — keyboard-input-at-text-edit-hq bug

**1. Related docs audit (pc-hq-leg-vs-nu-fix.md + §F-19):**

`pc-hq-leg-vs-nu-fix.md` documents TWO prior instances of the SAME failure
class ("XGetInputFocus lies") — override_redirect windows under Mutter/XWayland
reporting successful focus/grab while real hardware KeyPress events never
arrived. Both were fixes (git show 35c1b0b1~1 for one commit history). The
§F-19 reference in `_.0.aigent-testing-k9.txt` points to a third, related
taskbar popup-keyboard-focus bug where "reports success, doesn't work" was
only ever caught empirically via XTest injection, never by code review alone.
**Neither prior fix was ever actually applied to the CURRENT codebase** — the
legacy code's per-frame XSetInputFocus re-assert loop (LEG 8943-8948, "survives
click-away/click-back") is gone from NU; a 2026-09-08 attempt (`ae9e7d14`,
reverted) to add managed-window support never made it to final form. No
XTest-injection workaround code exists in production paths (XTest tools exist
only in tile-picker testing suite for diagnostic use, not as delivery path).

**2. Window creation + grab/focus code audit (khtpm_core_render.c):**

- **Window creation (line 16770-16772)**: `win_managed = dock_managed || elem_has_class(g_window, "managed"); swa.override_redirect = win_managed ? False : (Bool)g_override_redirect;` — text-edit-hq lacks the "managed" class, so gets created with `override_redirect = g_override_redirect` (true by default), matching the EXPECTED state from the bug report.

- **XGrabKeyboard call site (line 9181)**: `kh_grab_keyboard_retry()` function attempts grab up to 5 times (line 9180: `for (a = 0; a < 5; a++)`) with XSync + usleep between attempts — retries do exist and match the repeated GRAB log entries in the evidence.

- **XSetInputFocus retry (line 16835-16842)**: A 5-attempt post-map focus retry loop EXISTS, but **only for popup windows inside the generic window-creation code** — NOT called for the text-edit-hq main window itself (condition check at line 16805 gates it to non-dock, but no equivalent wrap for the retry). The comment at line 8264-8273 explicitly notes this retry pattern as "2026-08-28 fix, popups are no longer getting nav/index focus" — but it's a per-map-time fix, not the per-frame re-assertion LEG had.

- **Existing fallback mechanisms**: No per-frame XSetInputFocus re-assertion exists in the general idle-tick path. Comments at line 2557-2559 explicitly mention "XSync + XSetInputFocus storm on every idle redraw = the flicker regression. Not set for override_redirect windows (2026-09-03 flicker)." Line 7066-7069 references this same reasoning: "per-frame `pchq_focus_ok` loop is gone" (a real deletion from the refactor, not a forgotten wire-up). The retry at line 7395-7396 (`kh_grab_keyboard_retry()`) is called from `activate_focused()` (when user clicks), not from idle ticks.

**3. XWayland/Mutter override_redirect documented behavior (broad grep):**

- `pc-hq-leg-vs-nu-fix.md` §3-A, top comment (from LEG's own code): *"override_redirect windows never get real keyboard/mouse focus routed by Mutter (synthetic XTest input worked, masking the bug)."* — a real, proven, documented WM quirk, not a hypothesis.

- §4 root-cause map: "keys never reach `handle_key()` ... Root: **`override_redirect` window**."

- **2026-09-03 flicker regression** (comment line 2559) proves this has recurred before: a per-window per-frame focus re-assert was added, then removed because it caused flicker — suggesting Mutter itself rejects rapid repeated XSetInputFocus calls on override_redirect windows (the "SetInputFocus storm" comment).

- **No XTest-delivery workaround anywhere**: XTest injection tools (`tp_test_send_key.c`) exist only in testing suite (`tile-picker/ops/`), never wired into production paths. The testing guide (`_.0.aigent-testing-k9.txt` SCOPE ADDENDUM, §F-19) explicitly names XTest as a DISCOVERY method ("only ever caught empirically") not a DELIVERY workaround.

**4. Root-cause candidates (static analysis only, not live-tested):**

- **(a) Managed window state conflict**: text-edit-hq IS created WM-managed (override_redirect=false per the debug evidence from livedesk_override_redirect.pdl = false), yet the bug report says it's "not a clean match" because documented cases were override_redirect. If this is genuinely managed (WM should route keys normally), the bug class may be different — OR the managed-window focus retry mechanism is incomplete (the post-map retry only fires on a narrow condition).

- **(b) Repeated re-grabs indicate state churn**: The log shows 6 grab attempts in ~3.5 seconds ("something is re-triggering `activate_focused()` far more often than a single click-in explains", per the bug report itself). This suggests the window is losing focus between attempts, or a different code path is repeatedly calling grab. No retry-trigger mechanism was caught in static reading.

- **(c) Mutter+XWayland managed-window focus routing is also unreliable**: The documented cases covered override_redirect specifically, but this bug may be the SAME family showing up for managed windows too. The fact that XGetInputFocus reports success (line 9189, `real_focus_is_us=1`) despite no KeyPress events arriving suggests a WM-level async delivery failure, not a local bug.

**Concrete diagnostic tests NOT yet run (require live hardware):**

1. **Override_redirect toggle test** (already proposed in bug entry): Switch livedesk_override_redirect.pdl between true/false, restart text-edit-hq, retry typing. If symptom changes, override_redirect state is load-bearing (same as documented cases). If it does NOT change, this is likely cause (b)/(c), a new mechanism.

2. **Repeated-grab root cause**: Enable `g_default_input_elem` tracking at a finer granularity (e.g. log the caller of `activate_focused()` each time) to identify what's repeatedly re-triggering the grab attempts every 0.5-1s. This requires either instrumentation or live gdb attach.

3. **Real X11 event delivery confirmation**: Run `xinput test <device>` during typing in text-edit-hq to confirm whether the X server itself is receiving KeyPress events from hardware. If it is (other windows get them), but text-edit-hq doesn't, the bug is X11-level focus routing. If it isn't, the issue is earlier in the input stack.

4. **WM-managed focus test on a different app**: Create a temporary override_redirect-false test window (same window-creation path as text-edit-hq, different app name), attempt typing. If it works fine, the bug is text-edit-hq-specific (not the managed-window code path itself). If it also fails, the managed-window focus logic is broken for this entire Mutter version.

---

## ✅ CLOSED — bottom-bar mouse click "jumps ahead" to next nav (2026-09-15)

**Report**: "it keeps jumping ahead when mouse clicks bottom tb, to next nav, whenever nav moves... are we using same strategy as top tb?"

**Investigation path** (real evidence at every step, not guessed):
- Reproduced live via real `xdotool` click at exact coordinates read from the peer window's own serialized frame file (`entity_menu_frame_<pid>_bot.txt`) — a click squarely inside item 18's real box (x=214-388) consistently focused item 19 instead.
- Ruled out: digit-key relay, bare numeric relay, and even a raw `MOUSE_EVENT:` relay injection at the same coordinates — none reproduced it, which briefly pointed at a real-X11-delivery-only race.
- Directly tested the header (top tb) with the identical method — it did NOT reproduce, answering the user's own question: no, top and bottom were not using "the same strategy" at the point that mattered.
- First hypothesis (event-loop ordering: `hq_idle_tick()`'s reparse racing a real ButtonPress already queued) was implemented and verified live — bug still reproduced identically. Ruled out; kept the reorder anyway as real, harmless hardening (dispatch-pending-before-idle-tick can never make numbering staler for an in-flight event).
- Added a temporary debug log at the actual hit-test in `popup_handle_click()` and reproduced once more: the log proved the click's own hit-test was matching the CORRECT Elem (`MATCHED i=17 nav=18 label=dsr_castle_a`) every time. The bug was never in the renderer's click handling.

**Real root cause**: `khtpm_taskbar_manager.h`'s `KTB_STRIP_N_CELLS` was hardcoded to `15`, stale since before the header template (`khtpm_strip_header.xhtpm`) grew a 16th real nav-numbered cell (`strip-cell-16`, `${datetime}`). Every click relays `6000 + g_focus_nav` (`dock_relay_focus_code()`) to the manager so its own `strip_focus_cell`/`tab_focus_idx` stay in lockstep; `dispatch_code()` decoded that using the stale `15` (`t = nav_n - KTB_STRIP_N_CELLS - 1`), landing the manager's own focus one bottom-bar tab ahead of the real click. The renderer's next reparse pulled that wrong value back in over the click's own correct focus — a manager/renderer desync, not a click or a race.

**Fix**: introduced one real source-of-truth macro, `KTB_STRIP_N_CELLS_MAX 16`, defined once at the top of `khtpm_taskbar_manager.h` (before `KtbState` needs it to size `cell_id_pos`/`cell_id_str`); `KTB_STRIP_N_CELLS` now just equals it. Replaced every other place carrying its own duplicate `15` literal (`ktb_load_cell_ids()`'s loop bound in `khtpm_taskbar_manager.c`, the notes-menu `which <= 15` range check) so this exact drift class can't happen again — bump the one macro if the header template ever gains/loses a real cell, nothing else to update.

**Verified live**: same real xdotool-click reproduction that failed twice before now lands correctly (`[>]18 dsr_castle_a`, no jump).

**Files**: `khtpm_taskbar_manager.h`, `khtpm_taskbar_manager.c` (the manager-side fix); `khtpm_core_render.c` (event-loop reorder hardening, kept though not the root cause).

---

## ✅ CLOSED — dock nav "jumps index after a while" / pager vanishes on extended navigation (2026-09-15)

**Report**: "navigating the bottom tb and pager, it jumps index after a while and also the pager elements disappeared. its very buggy see it? pc-hq's same bottom tb pager work fine. whats going on?"

**Investigation**: reproduced live by walking the dock's nav all the way right (40 arrow-Right relay codes) then all the way back left (40 arrow-Left) - exactly what the user described. Added a temporary debug log at `layout_dock_bar()`'s row-computation and at the arrow-key step handler, and watched `kh_focus_debug_log`'s own `g_n_elems=` trail in real time: it climbed continuously, tick after tick, from ~700 to a hard **1024** - and then stayed pegged there. `1024` is `MAX_ELEMS`, the fixed size of `g_pool[]` (the shared bump-allocator array every `Elem` in the process comes from). The user's own follow-up ("ok, u activated the bug yourself. see?") confirmed this live run was the real reproduction, not a coincidence.

**Real root cause**: `incremental_reparse=1` (`#.desktop/hq_ui.pdl`) was live and on. Every `INCREMENTAL_REPARSE` log line read `removed=0` - the diff/match step never once recognized the dock's own repeated elements (same tags/ids/content, just a changed nav_index or focus state) as "the same element, just changed" across a reparse. Instead of patching in place, every reparse tick bump-allocated a **fresh full copy** of the dock's own ~21 elements into `g_pool[]` and never freed the stale ones (`kh_pool_free()` exists and is wired for exactly this, but only fires when the diff correctly identifies a removal - which it never did here). The dock relays a focus-echo (`dock_relay_focus_code()`) to the manager on **every single arrow key**, which round-trips back into a vars change and triggers another reparse - so the dock reparses far more often, and far more densely under active navigation, than any other window in the house. That's exactly why **pc-hq's own footer pager never showed this**: it doesn't churn vars on every keystroke, so it never reparses often enough to hit the pool cap in practice, even though it shares the same underlying (currently-broken) incremental-reparse machinery.

Once the pool is exhausted, every subsequent `elem_new()` call for the dock either returns null or (worse) reuses/aliases pool state that no longer means what the tree thinks it means - explaining both reported symptoms as ONE real failure, not two: nav numbers "jumping" and the pager cells "disappearing" are both just what a corrupted/exhausted element tree looks like once rendered, not independent bugs.

**Fix**: `incremental_reparse` set back to `0` in `#.desktop/hq_ui.pdl` (falls back to the pre-existing, always-full-rebuild reparse path - `g_n_elems=0` every single reparse, so the pool can never accumulate past what one frame's worth of real content needs, leak impossible by construction). Verified live: re-ran the exact same 40-right/40-left stress sequence that pegged the pool at 1024 before - `g_n_elems` now stays flat (43, matching one frame's real content) and `INCREMENTAL_REPARSE` never fires again; the pager and full row-2 content render correctly through and after the same sequence.

**Not yet done, flagged for later**: the incremental reparse diff/match algorithm itself (`CHTPM-INCREMENTAL-REPARSE-DESIGN.md`, `khtpm_reparse_diff.c`) has a real bug - it should be recognizing same-shape elements across a reparse and patching them in place, and it currently doesn't, at least for the dock's own repeat-driven content. Do not flip `incremental_reparse` back to `1` anywhere until that's actually fixed and re-verified under the SAME kind of rapid-reparse stress this bug was found with (a slow/occasional reparse window - most of the house - would never have caught this; the dock's own unusually chatty focus-echo is what made it visible at all).

**Files**: `#.desktop/hq_ui.pdl` (the fix). `khtpm_core_render.c` untouched (debug probes added and removed, net zero diff).

---

## ✅ CLOSED — dock focus "skips back" and never reaches the last 2 pager slots (2026-09-15)

**Report**: "nav skips back after tb is opened (wont go to last 2 +- pagers) why is that?"

**Investigation**: reproduced live (walk to the pager, open row 2, walk further right toward the new farthest pager button). Added temporary debug logs at every real candidate: the arrow-key step (`kh_nav_step`), the layout pass, the paint routine, `activate_focused()`, and `dispatch()`. All of them showed the renderer's own logic behaving perfectly - `g_focus_nav` correctly reached 34, `g_n_nav` stayed a correct, stable 35 the whole time, no clamp in the renderer ever fired. Yet `g_focus_nav` kept silently reverting to 33 between one paint and the next, with zero logged cause - meaning the change wasn't coming from inside `khtpm_core_render.c`'s own nav logic at all.

**Real root cause**: found in the manager/renderer round trip. `dock_relay_focus_code()` echoes the renderer's own on-screen highlight to the manager (`6000 + g_focus_nav`) so the manager's `strip_focus_cell`/`tab_focus_idx` and the renderer's `g_focus_nav` stay in lockstep (needed for the terminal/ASCII mirror). The manager's decode (`khtpm_taskbar_manager_main.c`'s `dispatch_code()`) only ever understood real tabs/hq-window cells - its own bound check (`t < s->n_tabs + s->n_hq_wins`) had no concept of the renderer's own 2 synthetic pager slots (`dock-page-minus`/`dock-page-plus`, appended only when content wraps to more than one row), so any focus landing on them was silently rejected, leaving the manager's own `tab_focus_idx` stuck at its last valid (real-tab) value. Separately, `dock_poll_strip_state()` (the renderer's own reverse-sync, feeding terminal-driven input back into the X11 highlight) reads the manager's `tab_focus_idx` back on essentially every republish and overwrites `g_focus_nav` with it - so the moment focus reached the pager, the very next manager republish silently dragged it back to the last real tab. Not a race: 100% reproducible, confirmed by a debug log that caught the exact revert with no other event in between.

**Fix**: gave the manager a real, named margin (`KTB_TAB_FOCUS_PAGER_MARGIN = 2`, `khtpm_taskbar_manager.h`) and widened BOTH places `tab_focus_idx`'s valid range is bounded by the same underlying concept (`dispatch_code()`'s accept-check in `khtpm_taskbar_manager_main.c`, and `ktb_reload()`'s own post-load clamp in `khtpm_taskbar_manager.c` - these two had already drifted apart once before, see pitfall #22 two entries up; not repeated here, same macro used in both). The manager still has no idea what the extra 2 slots mean - it doesn't need to - it just no longer rejects/clamps away a value the renderer legitimately sent it, making the round trip lossless.

**Verified live**: same reproduction (walk to pager, open row 2, walk to the new farthest pager button) - focus now reaches and holds at nav 35 (`+`), confirmed stable across several additional ticks/presses, no revert.

**Files**: `khtpm_taskbar_manager.h` (the new macro), `khtpm_taskbar_manager_main.c`, `khtpm_taskbar_manager.c`. `khtpm_core_render.c` untouched (debug probes added and fully removed, net zero diff).

---

## ✅ CLOSED — bottom tb flicker, ~once every 5-10 min, never top bar/entities (2026-09-15)

**Report**: "i do see a flicker on bottom tb every once in a while... never on top bar so i know we can fix it. we can use marker filesize file monitor when frame changes like diamond and golden standard says or is there something else? entities never flicker either so surely we can fix bottom tb (its very rare, once ever 5-10 min)"

**Root cause**: the dock peer's own 300s "safe, KISS" destroy+rebuild (this bounty's own 7th occurrence fix, 2026-09-14) was a **blind elapsed-time timer** - every 5 minutes, unconditionally, it tore down and rebuilt the peer's real Pixmap/GC/XftDraw/Window, whether or not anything was actually wrong. That's exactly why the symptom was bottom-bar-only, on a 5-10 minute cadence: no other window (header, any entity) has an equivalent destructive timer at all. The 7th occurrence's own root cause was explicitly logged as "not pinned down with certainty" at the time - a speculative safety net stacked on a guess, not a real fix, and this house's own DIAMOND standard is exactly "react to real, observed state change, never a blind timer" - the direct report named the correct standard to hold this code to.

**Why it's safe to remove now, not just silence**: this same session found and fixed several concrete, confirmed root causes of real dock staleness/desync since that 7th occurrence was logged - the incremental-reparse element-pool leak (g_pool exhaustion), the manager's stale `KTB_STRIP_N_CELLS` focus round-trip, and the pager's own missing `tab_focus_idx` margin. Any of these could plausibly have been the real, still-unidentified cause behind the 7th occurrence's own "genuinely running, yet the painted pixels stayed frozen" symptom - real condition-based bugs, now fixed, not timer-shaped problems. The one remaining REAL condition-based self-heal for "the window itself is genuinely gone" is `kh_ensure_dock_peer_window()`'s own per-tick `XGetWindowAttributes` liveness check (the 6th occurrence's own fix, already live, already proven) - a real state check, not a blind timer.

**Fix**: removed the blind 300s destroy+rebuild block entirely. Kept the harmless 20s `vars_changed=1` nudge (a real reparse trigger, not a window teardown - no visible cost, never caused this flicker). Did not replace it with a marker-file check, since a marker can only prove "a reparse ran," not "the paint surface itself is corrupted" - the one failure mode the removed timer was guessing at - and no live evidence since (including this whole session's own heavy dock stress-testing) has shown that failure mode recurring.

**If this specific symptom (stale paint despite a genuinely running, correctly-ticking process) ever resurfaces**: root-cause it for real with the `kh_focus_debug_log` targeted-probe technique this session proved out repeatedly (the nav-jump and pager bugs above), not another blind timer.

**Files**: `khtpm_core_render.c` (removed code only - net negative diff).
