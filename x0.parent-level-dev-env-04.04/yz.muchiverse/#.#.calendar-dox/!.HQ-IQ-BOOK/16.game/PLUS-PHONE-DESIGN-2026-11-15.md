# The plus phone: a phone-shaped x11-hq window, one VM per press of `+` (design, 2026-11-15, nothing built yet)

Labels: SAID (owner told an agent), READ (I opened the file), DOC (my reasoning, the owner decides).

## 1. What the owner asked for (SAID 2026-11-15)
- A new top-bar button `+`, also reached by typing `+` in input. Each press starts a NEW "plus phone" with its OWN port, so IP and PvP can be tested between phones on one machine.
- A phone is an x11-hq window shaped like a phone. Its chrome has an `@` button that flips it sideways (landscape) or upright (portrait).
- Inside: a home screen of SQUARE app icons (all the user's apps; this is a platform, with index nav like the rest of the house), and an on-board keyboard that uses nav.
- We emulate most iPhone/Android apps this way, and the design must say so: the phone is built so it can ship later as a native C phone app.
- First app: **True Swords Clash** (TSOTS game 1: Bible-verse Mana challenge plus Miracles, Elo-rated PvP; code in `@.apps/TSC_ELO`, READ listing only, not yet studied). Settings and an app store come later.
- NOT yet: opening the other x11-hq apps inside the phone. Keep it clean: test one app from the bottom up.

## 2. Terms
**Phone** = one window plus one process group plus one port (an instance). **Shell** = what the phone shows when no app is open (home screen, keyboard, chrome). **Phone app** = a folder of data (an icon, a layout, an entry, a manifest) that the shell can open. **Platform contract** = the small, versioned set of files and messages a phone app may use (section 5): this is the part that must also exist in the later native C phone app.

## 3. Shape (DOC)
- **The `+` cell** is one more `<item>` in `khtpm_strip_header.xhtpm` (cells are data there; the bar's manager only maps a nav number to an action). Its action runs one op: `phone_op new`. Typing `+` in input maps to the same action (checked against the existing key handling before building: `+` must not already mean something in a focused cli_io).
- **One window per phone**, started the way the plugz windows are: the shared renderer, one static layout (`plus-phone.xhtpm` + css), the instance-dir hook (`khtpm_core_render <house> <xhtpm> <instance dir>`), and a small C projector writing `<instance>/.hq_manager/ui.txt` only on change. Pattern and traps: `X11-HQ-APP-DESIGN-WISDOMS.md` section 12b. No per-phone C in the renderer.
- **Instance = a folder** `&.hq-apps/plus-phone/instances/<n>/` holding: `phone.txt` (id, port, orientation, running app), `.hq_manager/ui.txt` (projection), a marker file the projector wakes on, and the phone's own state folder. Every started process is registered through `kh_spawn.sh` with owner = the phone (it ends when the phone closes); no new `setsid` sites.
- **Port per phone:** the phone op picks the port with the same retry-scan `palnet_peer` already has (READ: it finds a free port and `PALNET_BIND` defaults to 127.0.0.1, this machine only) and writes it to `phone.txt`; the phone then passes it to its app as `PALNET_PORT`/seed. Two phones on one machine are two peers: that is the PvP test. LAN play later = a bind change, not a redesign.
- **Orientation (`@`):** the chrome button writes `orientation=portrait|landscape` to `phone.txt`; the projector swaps the layout class (two layouts of the same panels, sizes swapped), and the window is resized once. No rotation animation, no second renderer path.
- **Home screen:** a grid of square items from the apps list (`<repeat>` over rows, nav numbers like every other house window). The list comes from ONE reader (an op), not a copy per phone. Today it holds one row, True Swords Clash.
- **Keyboard:** an on-screen key grid whose keys are nav items; a key press appends to the focused field through the same relay append the house already uses (`KEY_PRESSED:` lines). It is a panel of the shell, shown only when a field asks for text, so an app never draws its own.
- **App process:** an app runs as a child of the phone (registered, owner = phone), reads and writes only inside the phone's state folder (`phone state/<app>/`), and talks to other phones through the phone's port. One phone = one user session = one ledger folder, so no two phones can corrupt each other and a deleted phone leaves nothing behind.

## 4. The design-choice test (answered, DOC)
1. **One authority per fact:** the apps list, the port and the orientation each have one writer (`phone_op`); the window only projects them. Game state stays in the app's ledger (no-ghost-data rule); the phone folder holds only config, preferences and projections.
2. **Modular:** a phone app is data plus the shared platform contract; the shell has no per-app C, same rule as the renderer ("zero per-project C").
3. **Scalable:** the 10th phone is one more folder and one more port; the 10th app is one more row. Cost per phone is one renderer plus one projector (about 0.7 percent plus ~0 percent idle, measured on the plugz windows, to be re-measured here); the CPU rule applies, so a phone with no active app must idle near zero, and the number of simultaneous phones is capped (proposal: 4 until measured).
4. **Long term:** the platform contract (section 5) is exactly what the native C phone app will implement; the x11-hq shell is its emulator.
5. **House shapes:** op plus files, a `.pdl`/text manifest per app, marker-file wake-ups, registered processes, a harness case that can fail.
6. **Cost and risk of the better option:** a real VM or container per phone would be heavier and slower to start; process-and-folder isolation is enough to test PvP and is what a native app does anyway. Rejected: one shared window with tabs (cannot test two peers on two ports); rejected: apps drawing their own chrome (breaks the later native port).

## 5. The platform contract (what a phone app may rely on; keep it small and versioned, DOC)
`manifest.pdl` (id, title, icon, entry, min platform version); a state folder it may write; its port and peer seeds; a text-input request (the shell shows the keyboard); an orientation hint (`portrait`, `landscape`, `any`) and a size; a back/home action; the same logs and ledgers rules as every house app. Nothing else is promised. When the native C app exists it implements these same items; an app that stays inside them ports unchanged.

## 6. Build order (each step ends with a harness case that can fail AND a picture opened by a human, per the acceptance rules)
1. `phone_op new/list/close/orient` (instance folder, free port, registry rows) with a case in a scratch house: two `new` calls give two folders and two DIFFERENT ports; `close` leaves nothing running (positive control for each).
2. The window: layout, projector, `@` flip, home grid with one icon; screenshot in both orientations.
3. The `+` cell and the `+` key in input; `phone_op new` from the bar; screenshot with two phones side by side.
4. The keyboard panel (nav keys) typing into a field; case asserts the field value.
5. True Swords Clash as phone app #1: `manifest.pdl`, a portrait layout of its existing screens, two phones playing one rated game through two ports (the existing TSC_ELO harness extended; first read `prog-report-pvp.md` and `QUICK_STANDARDS.md` in `@.apps/TSC_ELO`).
6. Later, not now: settings and an app store (just more phone apps), opening other x11-hq apps inside the phone, LAN peers, the native C phone app.

## 7. Open questions for the owner
1. Phone size: pick a base size (for example 360x780 portrait, an iPhone-like ratio) and let `@` swap width and height?
2. Is `+` the key itself, or Shift+`=` on a US keyboard? (Both should work; confirm.)
3. Cap on simultaneous phones (proposal 4) and whether closing the bar's `+` window closes all phones.
4. Does a phone keep its state when closed (a saved phone you can reopen) or is each press a throwaway? (Proposal: throwaway by default, a named phone keeps its folder.)
5. Should the first slice stop at step 3 (empty phone you can open twice) so you can look at it before any game work?

---
# DEEP DIVE (added 2026-11-15, owner: "intense documentation ... before i approve"). Everything below is DOC (my design) unless marked SAID or READ. Nothing is built.

## 8. The picture: what a human sees
```
 TOP BAR   [HQ][user][file][desks][menu][pals][palettes][edit][player][db][tools][toys][store][network][h-ai][ + ][clock]
                                                                                                            ^ new cell, nav-numbered like the rest; the key "+" does the same

 PORTRAIT (base size, to confirm: 360 x 780)             LANDSCAPE after "@" (780 x 360)
 +-------------------------------+                       +-----------------------------------------------------------+
 | (o)   plus phone 1  :41731  @ |  <- chrome            | (o)   plus phone 1  :41731                              @ |
 +-------------------------------+                       +-----------------------------------------------------------+
 |                               |                       |                                                           |
 |  [11 TSC ]  [12 .. ]  [13 ..] |  <- square icons,     |  [11 TSC ]  [12 .. ]  [13 .. ]  [14 .. ]  [15 .. ]       |
 |  True       (empty)   (empty) |     nav numbers       |  True       (empty)   (empty)   (empty)   (empty)         |
 |  Swords                       |                       |  Swords                                                   |
 |  Clash                        |                       |  Clash                                                    |
 |  [14 .. ]  [15 .. ]  [16 ..]  |                       |                                                           |
 |                               |                       +-----------------------------------------------------------+
 |                               |                       | [ home ]                                                  |
 +-------------------------------+                       +-----------------------------------------------------------+
 | [ home ]   [ back ]   [ kbd ] |  <- bottom row
 +-------------------------------+
 KEYBOARD (portrait, shown only when a field asks for text; every key is a nav item)
 | q  w  e  r  t  y  u  i  o  p |   | a s d f g h j k l |   | shift z x c v b n m del |   | 123 | space | enter |
```
Two phones side by side = two windows, two ports, two ledgers. That is the whole point.

## 9. File layout (one authority per file)
```
&.hq-apps/plus-phone/                    the phone PLATFORM (the shell). Static, tracked.
  plus-phone.xhtpm  plus-phone.css       ONE layout, portrait and landscape as two classes of the same panels
  ops/phone_op.c                         the ONE writer: new | list | close | orient | apps | status
  ops/phone_projector.c                  the <module>: writes <instance>/.hq_manager/ui.txt only on change
  ops/phone_act.c                        the action op the layout's buttons run (select, open app, home, back, key, flip)
  ops/build_phone.sh                     one-line build (house convention)
  apps/                                  PHONE APPS = data. One folder each:
    true-swords-clash/manifest.pdl       id, title, icon, entry, orientation hint, min platform version
    true-swords-clash/icon.*             square icon
  instances/<n>/                         ONE phone (runtime, gitignored, capped)
    phone.txt                            id, port, orientation, running_app, started, owner pid   (CONFIG + PREFERENCE, never game state)
    state/<app>/                         the phone's own app state; the app's LEDGER lives here (no-ghost-data rule)
    .hq_manager/ui.txt marker.txt ...    projection and wake-up marker (projections: safe to delete, regenerate)
&.widgits/_shared-lib/harness/cases/plus_phone.pdl   the case that can fail (section 13)
```
`phone.txt` rows (draft): `PHONE | id | <n>`, `PORT | <number>`, `ORIENT | portrait|landscape`, `APP | <id or ->`, `OWNER | <pid>`, `STARTED | <epoch>`. A file that carries a fact the ledger lacks is ghost data: nothing about a GAME may live here; only what the phone itself is.

## 10. Lifecycle (state machine)
```
 (none) --"+"--> STARTING --window up, port bound--> HOME --open app--> APP_RUNNING
                    |                                  ^  |               |
                    | failure (no free port,           |  +--"@"--> HOME/APP (orientation toggles, state unchanged)
                    | window cap reached)              +--home----------+
                    v                                  |
                  REFUSED (one line, nothing left)     +--close / owner exits--> STOPPING --> (none)  [children ended by pid, instance folder removed unless the phone is named]
```
Rules: STARTING is atomic (folder + port + registry rows, or nothing: a half-made phone is removed). STOPPING ends only this phone's registered pids (never `pkill -f`; the memory note about self-matching patterns applies). A phone whose owner (the bar) is gone exits by itself (lifetime `owner`).

## 11. Ports, peers and the PvP test
- READ: `palnet_peer` scans for a free port and binds `127.0.0.1` by default (`PALNET_BIND`); its seeds are `host:port` entries. The phone op reuses that scan logic (a small shared piece, text-included once if two consumers need it, per the house include rule), picks a free port inside a reserved range (proposal 41000 to 41999), writes `PORT` to `phone.txt`, and starts the app with that port.
- Peering for a test: phone A and phone B each get the OTHER's `127.0.0.1:<port>` as a seed. The phone op writes the seeds for all running phones on every `new`/`close` (one writer, regenerated, so a closed phone disappears from the seed lists).
- LAN later: change the bind (a setting), not the design. Cross-machine networking is a separate, not-started plan (memory: `palnet_peer` was 127.0.0.1-only); this design does not depend on it.
- What this tests: rated PvP (TSC_ELO), the ledger replay per player, and a disconnect (close one phone mid-game: the other must show it, case in section 13).

## 12. Interfaces
**Platform contract (v1, small on purpose).** An app may rely on exactly: (1) `manifest.pdl`; (2) its own state folder; (3) `PORT` and peer seeds; (4) a text request (the shell shows the keyboard and returns the typed line); (5) an orientation hint and its size; (6) home and back actions; (7) the house ledger rules. Anything outside this is not promised, so the native app only has to implement these seven items.
`manifest.pdl` (draft): `APP | id | true-swords-clash`, `TITLE | True Swords Clash`, `ICON | icon.png`, `ENTRY | <house-relative executable under ops/+x>`, `ORIENT | portrait|landscape|any`, `PLATFORM | 1`.
**Projection keys** the layout reads from `ui.txt` (draft): `phone_title`, `port_label`, `orient`, `n_apps`, `app_<i>_label/_icon/_idx`, `app_open`, `kbd_on`, `kbd_target`, `last_result`. The layout contains no logic, the same rule as plugz-hq.
**Actions** (every button maps to one `phone_act` verb): `open <app>`, `home`, `back`, `flip`, `key <ch>`, `submit`, `close`.

## 13. How we will prove it (each step: a case that can fail, a PNG a human opens)
| Step | Case assertion | Negative / control | Picture |
|---|---|---|---|
| phone_op new | folder, `phone.txt`, a free port, registry rows | two news give two DIFFERENT ports; a full port range or the cap gives REFUSED and leaves nothing | none |
| close | no registered pid left, folder gone | a named phone keeps its folder; closing the wrong id changes nothing | none |
| window | `ui.txt` has the title, port and one app tile | an empty apps list shows "no apps", not a blank | portrait PNG |
| `@` | `orient` flips, window size swaps, apps list unchanged | flipping twice returns to the start | portrait AND landscape PNG |
| `+` key and cell | the same `phone_op new` runs from the bar relay | `+` typed inside an armed text field types a plus and opens no phone | PNG with two phones |
| keyboard | typed line equals the keys pressed | a key with no field armed does nothing | PNG with keyboard up |
| TSC on a phone | two phones finish one rated game, both ledgers replay equal | kill one phone mid-game: the other shows the disconnect; a hand-edited ledger is refused | PNG of each phone |
| idle cost | CPU of 1, 2, 4 phones idle, with the producer's state written beside the number (CPU pitfalls 30 to 32: measure children and wakeups, a null needs a control) | the cap refuses the 5th | none |
Acceptance entry goes into the compendium when built: a case that passes while the screen is empty is a failed test.

## 14. Cost and risk
- **CPU:** the plugz windows measured about 0.7 percent per renderer (its own 30 fps tick) and about 0 percent per idle projector. A phone is one of each plus the app. Four phones idle is expected near 3 percent of a core: a guess until measured; the cap (proposal 4) and the CPU gate apply, and a guard kill would be OUR finding.
- **Runtime files:** each phone writes `phone.txt` (a few lines), `ui.txt` (a few KB) and the app's ledger; every writer needs a cap row in `RUNTIME-FILE-GROWTH-STANDARD.md` before it ships, and `hygiene_sweep` learns the instances folder.
- **Risks:** (1) `+` already means something in some text fields: check the key path first. (2) Window size and Wayland: the khtpm windows are X11 under XWayland (READ today), so resize by `@` must use the renderer's own window path; unverified. (3) Two renderers with the same instance hook must not share a folder (the plugz stale-state bug: reset on open). (4) Port reuse after a crash: free by scan, never assume. (5) Scope creep toward a real emulator: the contract stays seven items.
- **Rollback:** the phone is additive: a new folder, one bar cell and one op. Removing the cell removes the feature.

## 15. Alternatives considered (design-choice test, in full)
| Option | Verdict |
|---|---|
| One window per phone, process-and-folder isolation (chosen) | matches house shapes; lets two peers run on one machine; the contract is the native port |
| Real VM or container per phone | heavy and slow to start on the owner's weak CPU for no PvP gain; rejected |
| One shared phone window with tabs | cannot test two ports at once; rejected |
| Each app draws its own phone chrome | breaks the native port and duplicates chrome; rejected |
| Reuse the editor's Play button as the phone | the owner said games run from the editor Play (2026-11-08) but this is a different product surface (a platform with many apps); keep separate, share the instance-dir pattern |

## 16. Native C phone app (the later product, DOC)
The x11-hq phone is the emulator; the native app implements the same seven contract items. Mapping: window and chrome = native view; `ui.txt` projection = native view model; `phone_act` verbs = native intents; the instance folder = the app sandbox; the port = the app's socket; `manifest.pdl` = the app list. Shared logic that is pure (rating math, ledger replay) stays in text-included C so both builds use one copy. Nothing in the x11 shell may use a feature the native app cannot (no per-app renderer branches, no shell scripts in the contract).

## 17. Glossary and FAQ
**Plus phone** the product name for the phone shell; **phone** one instance; **phone app** data the shell can open; **contract** the seven items; **platform** the house (LyfeDesk) that hosts phones.
- *Does the phone replace the bar?* No: the bar's `+` only starts phones.
- *Can an app in a phone see another phone?* Only through the port, like two machines.
- *Can the other x11-hq apps run in a phone?* Not yet (owner SAID keep it clean); later as phone apps with a manifest.
- *Is True Swords Clash rewritten?* No: step 5 only adds a manifest and a portrait layout around the existing TSC_ELO screens (still to be read).

## 18. External references (for other AI agents)
Repo (branch `claude`): this file; `02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md` D82; window pattern `02-architecture/X11-HQ-APP-DESIGN-WISDOMS.md` section 12b and the working example `44.xyz.01.00/&.hq-apps/plugz-hq/`; bar cells `44.xyz.01.00/_.monads/_.livedesk-taskbar/khtpm_strip_header.xhtpm`; peers `044.pal-chat-irc.../ops/palnet_peer.c`; the game `44.xyz.01.00/@.apps/TSC_ELO/` (`prog-report-pvp.md`, `QUICK_STANDARDS.md`); CPU rules `03-pitfalls/CPU-OPTIMIZATION-PITFALLS.md`; lore and IP `22.lore-ip/` (TSOTS definition). Skills: `tsots-game-map`, `lyfedesk-taskbar-driving`, `khtpm-house-standards`.

---
## 19. FIRST SLICE BUILT (2026-11-15, owner: "lets just get something started ... basics nothing crazy")
What exists and was SEEN (I opened a real phone window and read the PNGs): `44.xyz.01.00/&.hq-apps/plus-phone/`.
- `ops/phone_op.c`: `new` (next free number, a free port from 41000 by a real bind test, one registered window; cap 4), `list`, `close`, `flip` (writes ORIENT and relaunches with the other layout through a detached registered worker), `open` (records "not wired yet"), `home`, `closeself`. `ops/phone_projector.c` writes `ui.txt` only on change; the apps list comes from `apps/<id>/manifest.pdl` (`APP`, `TITLE`, `ICON`, `ORDER`).
- Window = the house SWATCH GRID (the palette picker's shape): no sidebar, no side panel, every app a SQUARE `<item class="swatch">` tile with its icon, name and nav badge; chrome = the house `_ ! X` plus a template-declared `@` (flip) and `x` (CLOSE), the way the picker declares its own close button (an in-template `chrome-btn`; the layout draws them in the title strip and REPLACES the native X, so the template must declare `x` itself: I forgot once and the owner caught it). Portrait 360x700, landscape 700x360 (two layout files, `@` relaunches). Apps: True Swords Clash (placeholder icon, not wired) and settings (show only). A `@` click through the window's relay flipped a real window to landscape; the house X closed a real phone; two phones got ports 41000 and 41001.
- **Shared renderer change (`_.monads/_.livedesk-taskbar/ops/khtpm_core_render.c`, 15 added lines, generic, no app names):** on the swatch-grid path (1) an explicit pixel width and height in the window's own CSS now win over the 5/8-screen default width and the content height is a minimum; (2) a swatch whose own CSS asks for a size above 34px sets the grid's tile size and KEEPS its label (a sprite tile normally blanks it). A window or tile without those CSS rules is unchanged. NOT regression-tested live on the palette pickers (only reasoned: the new branches need an explicit px size above the default); open one before trusting it.
- **CSS trap found:** in a rule `border: 1px solid #...;` placed BEFORE `width:`/`height:` the size was silently not applied to the `.swatch` items; the picker's own CSS adds `border-width: 1;` and works. Put the size first and add `border-width: 1;`.
- Case `plus_phone` 29/0 (scratch house, real ops and spawn funnel, stub renderer): ports differ, cap refuses the 5th with nothing left behind, flip relaunches with the landscape layout (checked by a launch log: `pgrep -f` cannot be used in a case, it matches the case runner's own command line), close frees the number, bad and repeated closes are refused.
- **NOT built (honest list):** the top-bar `+` cell and the `0` nav key (the owner asked 2026-11-15 for `+` before `1` as nav 0: the renderer numbers navigation from 1 and the bar's cells are positional, so this is a change to the shared bar manager and its cell-id file, not started); the `+` key in input; the on-board keyboard; True Swords Clash itself (the tile says "not wired yet"); the app name overlaps the icon on a tile (the draw core puts the label at the tile's top-left; wants a below-the-icon placement); real icons (the two are generated placeholders); the apps list is not yet scrollable past one screen; no CPU measurement of 1 to 4 phones; the compendium acceptance entry and a runtime-file cap row for `instances/` are not written yet.

---
## 20. THE `+` / `0` CELL ON THE LYFEDESK TOP BAR (BUILT 2026-11-15, owner: "lets do the +/0 on lyfedesk tb. i just wanna see it")
SEEN on the real bar (restarted three times; PNG `XO/31.Nov/nov-15/4.topbar-plus-cell-0.png`): a cell `[ ]0. +` sits first, before `[>]1. HQ`; every other number is unchanged (1 HQ, 2 user, 3 book, 4 page, 5 menu, 6 pals ...). Typing `0` through the bar's relay opened a phone (port 41000); a real click on the cell opened phone 2 (41001); `1` + Enter still opened HQ's dropdown (regression check) and its cancel row closed it.
How (all generic, no app names in shared code; changes are small and listed so they can be reverted): (1) `_.monads/_.livedesk-taskbar/khtpm_strip_header.xhtpm`: one new first item `id="plus-phone" class="dock-cell nav-zero" action="'${HOUSE}/&.hq-apps/plus-phone/ops/+x/phone_op.+x' 'new' '${HOUSE}'"`. (2) renderer `khtpm_core_render.c`: in `layout_dock_toolbar_row` a `nav-zero` cell gets `nav_index = 0` and is kept OUT of `g_nav`, so no other cell's number moves; a typed `0` (nothing typed before it) dispatches the cell's action (`kh_find_nav_zero`); the dock click path tests the nav-zero cell first (it is not in the nav list the click scan walks) and `click_focus_then_activate` runs its action. (3) draw core `khtpm_draw_core.c`: the badge is drawn for a `nav-zero` element too, as `[ ]0.`, never "focused". The `+` key and its unshifted twin `=` also run it (added later the same day, owner: "should be +, plus optional shift first"): the key handler runs the nav-zero cell only in a window that has one, so no other window's keys change; seen live: relay keys 43 and 61 each opened a phone, an unrelated key opened nothing.
Traps found: the dock draws its badge at THREE places in the draw core (two format sites and the paint site at the `nav_index > 0 && nav_badge_font` guard) and a click is hit-tested against the nav list only, so a cell outside the list needs its own hit test; a `cpu_gate ... sh -c 'cd rel/path && ...'` run from the wrong working directory silently builds nothing (check the binary time); the header xhtpm is read at bar launch, so a restart is required to see a header change.
NOT built: no harness case for the bar wiring (the evidence is the live run above); the shared renderer and draw core changes have not been regression-tested on other windows beyond the HQ dropdown check; closing a phone from the bar (a phone's house X closes it; `phone_op close` closes by number).

### 20b. Hybrid key behaviour (BUILT 2026-11-15, owner: "it shouldn't auto open ... for 0 it should wait for enter press ... a hybrid compromise")
- `+` and `=` (the same key without Shift) open a phone AT ONCE.
- `0` MOVES the bar's focus to the `[>]0. +` cell (HQ's `[>]` goes away) and WAITS; Enter then opens a phone, like every other top-bar option. Typing another number (`0` then `3`) moves focus there and opens nothing. Seen live: three `0` presses opened nothing and drew `[>]0.`; Enter opened phone 1; `0`,`3` drew `[>]3.`; `+` opened phone 2.
- How: `g_focus_zero` (valid only while `g_focus_nav == 0`, so any nav move ends it); Enter runs the cell when it is set; the draw core and `elem_cursor_prefix` draw `[>]` for it.
- **Why this took six builds (for the next agent):** focus index 0 does not exist in this renderer, and at least FIVE places force `g_focus_nav` back to at least 1: three clamps in the layout passes, the dock layout's own clamp at the end of the dock branch (`if (g_focus_nav < 1 || ...) g_focus_nav = 1`), and the poll that copies the manager's `strip_focus_cell` into `g_focus_nav` on every republish (the manager owns the strip focus; the renderer mirrors it). Each needed a `!g_focus_zero` exemption; the poll ignores the manager cursor until it MOVES. Also: the FIRST relay key after a restart is often dropped (send it up to three times and stop on the effect), and `gdb -p` is blocked by `ptrace_scope` here, so tracing was done by temporary `fprintf` checkpoints (removed; `grep -c 'zero_dbg\|DBGZ'` is 0).
- Still not built: a harness case for the bar wiring (the evidence is the live run); regression check of other windows beyond HQ's dropdown and the cells 1 to 4 numbering.

---
## 21. PLACEMENT: phones open beside each other, wrapping like flex (BUILT 2026-11-15, owner: "phones specifically doesn't open on top of other phone ... beside, till flex like wrap", then "we could use the same service for any window, if opted .pdl")
- **Shared service, opt-in by data.** `44.xyz.01.00/&.widgits/_shared-lib/ops/window_place_op.c` (`window_place_op place <app_dir> <instance> <w> <h> [--screen-w N]`) picks a free slot and writes `<instance>/window_pos.txt` ("x y") and `window_size.txt` ("w h"). An app opts in with a `placement.pdl` in its folder (`GROUP | instances`, `MARGIN_X | 20`, `TOP_Y | 110`, `GAP | 12`, all optional); with no `placement.pdl` the op REFUSES and writes nothing. Slot rule: scan left to right from the margin; a spot that overlaps a LIVE sibling (a folder under the group with `window.pid` naming a live process) moves to that sibling's right edge plus the gap; past the screen's right edge it wraps to a row below the lowest overlapping sibling. A dead sibling holds no slot; a window never avoids itself.
- **Renderer hook (generic, 10 lines, `khtpm_core_render.c`, `kh_instance_window_pos`):** a window opened with an instance dir moves to `<instance>/window_pos.txt` when that file's size changes; `g_inst_pos_set` stops the sidebar-panel layout from hard-setting x=80 over it. A window without the file is unchanged.
- **Who uses it now:** `plus-phone` (`phone_op` calls the op before every launch and on every flip, with 364x704 portrait or 704x364 landscape; the size numbers are the only phone knowledge left in `phone_op`) and `plugz-hq` (`plugz_open` in `harness_menu_op.c` calls it with 784x504; this closes the "three windows open stacked at the same spot" gap of spec section 14). Any other window that has an instance dir needs one `placement.pdl` and one call.
- **Seen live:** three phones opened at x=20, 396, 772 (all y=110, 364 wide) and the real windows matched; flipping phone 1 to landscape moved it to x=1148 (704 wide), exactly clear of phone 3; the plugs and tests windows opened at x=20 and x=816 instead of stacking. Case `window_place` 15/0 (first window at the margin, beside a live one, a dead sibling ignored, flex-wrap with a narrow screen, a landscape size, refusal without `placement.pdl`, bad size, usage); `plus_phone` 29/0, `plugz_hq` 64/0 unchanged.
- **Limits (honest):** only windows of the SAME app avoid each other (phones and plugz windows can overlap one another; a `GROUP` shared across apps is not built); a window the user drags is not tracked (its `window_pos.txt` keeps the placed spot, so a later window may be placed over where it now sits: the renderer does not publish its real rectangle); the screen width comes from X, vertical overflow is not handled (a tall row past the screen bottom is placed anyway); sizes are the caller's numbers, not read from the renderer.

---
## 22. PATTERNS WE BORROW FROM XCODE / iOS AND ANDROID (APK) APP DESIGN (owner 2026-11-15: "settings in phone will open a 'view' screen that will let user change size of phone window default and icon size default xy ... we are modeling this after the way ide xcode / apk treat app design, so document how we can reuse those patterns as we build this out")
Label: DOC. The Android and Xcode facts below are general platform knowledge I am confident of, NOT re-checked against their current documentation in this session; before we copy a number (icon sizes, breakpoints) check the platform docs. The point is the SHAPE of each pattern, because those shapes already solved the problems a phone shell has: many device sizes, many densities, rotation, an app declaring what it is, and a Settings place.

### 22.1 The map (their pattern -> what it is -> our piece)
| Pattern | In Android / Xcode | Our equivalent (existing or planned) |
|---|---|---|
| **App manifest** | `AndroidManifest.xml` (package, app name, icon, activities, permissions, min SDK, orientation) / `Info.plist` (bundle id, name, icon, supported orientations, launch screen) | `apps/<id>/manifest.pdl` (BUILT: `APP`, `TITLE`, `ICON`, `ORDER`, `ORIENT`, `PLATFORM`). Grow it with `SETTING` rows (22.4), `PERMISSION` rows, `MINPLATFORM`. The shell reads ONLY the manifest to build the home grid: no per-app code. |
| **Resources by qualifier** | `res/layout/` vs `res/layout-land/`, `res/drawable-mdpi/..xxxhdpi/`, `values/dimens.xml`, `-sw600dp`; the OS picks the best folder for the device | portrait and landscape layout FILES (BUILT: `plus-phone.xhtpm` / `-landscape.xhtpm`); icon folders per size class; `metrics.pdl` for dimens (22.3). The shell picks by device profile and orientation. |
| **Density-independent units** | `dp`/`sp` (px = dp x density/160), iOS points (pt) with @1x/@2x/@3x | ONE unit `u` in all phone data (a tile is 3u, a gap is 0.25u); a profile carries `UNIT_PX` (px per u). Changing the icon size = changing one number, not editing layouts. |
| **Device definitions** | Android Studio AVD / device manager (screen size, resolution, density, system image) and Xcode Simulator devices and runtimes | `profiles/*.pdl` DEVICE PROFILES (22.2): name, window w x h in px, UNIT_PX, default orientation. The `+` press starts a phone from the DEFAULT profile; settings can change the default. |
| **Size classes / window size classes** | iOS compact vs regular width and height (portrait iPhone = compact width, regular height; landscape = compact height); Android window size classes (compact < 600dp, medium, expanded) | `SIZECLASS` derived from the window's u-width (compact below ~ 6.5u... to be calibrated); layouts and the icon grid columns depend on the class, never on a device name. The `@` flip already changes class. |
| **Auto Layout / constraints, safe areas** | constraints, `safeAreaInsets`, Android insets | the swatch grid already flows tiles left to right and wraps (the renderer's own rule); a `SAFE` inset in the profile for the title strip so chrome buttons never cover tiles. |
| **Interface Builder device bar / SwiftUI Previews / Android layout preview** | one layout shown on many device sizes and orientations at once | PREVIEW MATRIX (planned): the harness opens one phone per profile x orientation with `window_place_op` (BUILT: windows tile beside each other) and photographs them; the acceptance entry for the shell is that picture. |
| **Settings.bundle / Android Preferences (`PreferenceScreen`)** | an app DECLARES its settings (key, title, type, default, range) in a file and the OS Settings app renders a generic screen | `SETTING` rows in `manifest.pdl` rendered by the settings app; identical in shape to the plug `PARAM | key | value | type | min | max | help` rows (BUILT: `plug_ref_value_ok` validates them). Settings > View is the first consumer: the shell's own settings. |
| **UserDefaults / SharedPreferences** | key-value store per app | `phone_prefs.pdl` of kind PREFERENCE in the app's `state.pdl` (no-ghost-data rule: a preference never changes an outcome; a GAME fact goes in the ledger). |
| **Activity / app lifecycle and configuration change** | `onCreate/onResume/onPause/onDestroy`; rotation DESTROYS and recreates the activity, so state is saved and restored (`onSaveInstanceState`); iOS `scenePhase` | our `@` flip RELAUNCHES the window with the other layout, which is the same idea: therefore an app's state must live in its state folder (BUILT rule), never in the window. Planned events: `open`, `pause` (phone home), `resume`, `close`, `rotate` as verbs to the app. |
| **Intents / URL schemes / app groups** | one app asks another to do something by a declared name | verbs addressed `<app>:<verb>` through the phone's port (design; needs the contract to grow). |
| **Launcher icon sets, adaptive icons** | a set of sizes per density, or foreground/background layers inside a mask with a safe zone | icon = a FOLDER (BUILT: `sprite.csv` + `name.txt`); an icon set = several sprite folders by size (`icon-s`, `icon-m`, `icon-l`) chosen by tile size; a safe-zone rule so a label never covers the art (the current label-over-icon overlap is exactly the missing safe zone). |
| **Build variants / flavors (Gradle), Xcode schemes and targets** | one source, several builds (debug/release, free/paid) | PROFILES again: `dev` (4 phones, ports 41000 range, debug rows) and later `ship` (native C app). The same manifests feed both. |
| **ADB / simctl** | drive a device from a terminal: tap, key, screenshot | the relay (`KEY_PRESSED`, `MOUSE_EVENT`) plus `phone_op` plus `dump_frame_png_op` (BUILT). A phone is drivable by a harness exactly like an `adb` device. |
| **Permissions** | manifest declares what an app may touch; the OS enforces | the platform contract's 7 items (section 12); `PERMISSION` rows are the next addition, enforced by what the shell gives the app (its state folder, its port). |

### 22.2 Device profiles (the Android AVD / Xcode Simulator idea)
`&.hq-apps/plus-phone/profiles/<name>.pdl` (DESIGN, not built): `PROFILE | name | standard`, `WINDOW | 364 | 704` (portrait px), `UNIT_PX | 40`, `ICON | 3 | 3` (tile width and height in u), `GAP | 0.25`, `ORIENT | portrait`. The shipped set is small (compact / standard / large, plus a user `custom`). A phone instance records WHICH profile it was started from (one line in `phone.txt`, config) and its own overrides, so two phones can be different sizes at once (the PvP test across form factors). The DEFAULT profile name lives in `phone_prefs.pdl`.

### 22.3 `metrics.pdl` (Android `dimens.xml`)
All sizes the layouts use come from ONE file of named values in u (tile, gap, chrome height, keyboard key). Layout CSS is GENERATED from a template plus the profile (the Gradle resource-compile idea): `phone_op` writes `<instance>/plus-phone.css` and `<instance>/plus-phone.xhtpm` with the numbers filled in and launches THAT. Why generated and not read live: the renderer sizes a window from its own CSS (found 2026-11-15), so a per-instance file needs no new renderer feature, only a template. (Alternative considered: teach the renderer variables in CSS; rejected as a larger shared-file change for one consumer.)

### 22.4 Settings > View (the screen the owner described; DESIGN, nothing built)
- **Where:** the `settings` tile (BUILT as a show-only tile) opens a second SCREEN inside the same phone window (a screen stack, like a navigation controller / back stack): home -> settings -> view. `home` pops to the root. Screen = one layout state selected by `screen=` in `phone.txt`; the layout file shows a different panel group; no new window.
- **Rows (typed, from data, validated by the same code as plug parameters):** `PARAM | window_w | 364 | int | 240 | 900 | default phone width in px`, `window_h | 704 | int | 400 | 1400`, `icon_w | 3 | int | 2 | 6` (u), `icon_h | 3`; plus presets (compact / standard / large) that fill the four numbers. Each row is a stepper (- value +) built from the swatch items the shell already draws, because there is no text input for numbers yet (the on-board keyboard is not built).
- **Apply rule:** changing the DEFAULT affects phones started AFTER the change; an open phone can adopt it with an explicit `apply to this phone` row (which relaunches it, the same path as `@`), never silently (a window must not resize under the user).
- **Storage:** `phone_prefs.pdl` (PREFERENCE, written only by `phone_op prefs set <key> <value>` after `plug_ref_value_ok`; `reset` row restores the shipped profile). Never read back as game input.
- **Proof when built:** a case (set a value, start a phone, the generated css and `window_size.txt` carry it; a value out of range is refused and the file is unchanged; `reset` is byte-identical to the shipped default) AND a picture of three phones at three profiles side by side (the preview matrix).

### 22.5 What we deliberately do NOT copy
No emulated OS, no bytecode, no app store signing, no animation system, no gesture recogniser stack: this is a layout-and-contract shell, not a runtime. We copy the data shapes (manifest, qualifiers, profiles, size classes, preference declarations) because they make many sizes and many apps cheap, and because the native C phone app can read the very same files.

### 22.6 Build order for this part (each step: a case that can fail and a picture)
1. `profiles/*.pdl` + `metrics.pdl` + the css/xhtpm GENERATOR in `phone_op` (no UI): case = two profiles give two window sizes; picture = two phones.
2. `phone_prefs.pdl` and `phone_op prefs get|set|reset` with typed validation (reuse `plug_ref_value_ok`).
3. The settings screen stack (`screen=` in `phone.txt`, a second panel group in the layout, `home`/`back`), rows for Window and Icon size, preset rows.
4. `apply to this phone` and "new phones use the default".
5. The preview matrix harness (a phone per profile x orientation, tiled by `window_place_op`, photographed).
6. Later: `SETTING` rows in app manifests, so any phone app contributes a page to the settings app.

---
## 23. BUILT 2026-11-15 (later): toolbars on every phone, settings first, three icon sizes that snap up
SEEN on a real phone (PNGs read; portrait 364x704 and landscape 704x364).
- **Every phone has a top and a bottom toolbar**, empty at first (owner: "top tb and bottom tb on all phones, even if initially empty"; "top tb should snap to top"). Two items per layout, `class="bar-top"` and `class="bar-bottom"`: full-width bands, NO nav number, the top one flush under the title strip, the bottom one flush at the window's bottom edge. Renderer change (swatch-grid path, generic, ~10 lines): the layout places them, reserves the bottom band in the window height, and the header-chip pass skips them. Nothing is in them yet; later they carry the shell's own status and navigation items (the phone-level `tb`).
- **Settings is ALWAYS first in the index** (owner: "settings should always be first in index"): a manifest row `PIN | first` beats every `ORDER`; the projector sorts pinned apps first. Seen: settings is nav 3, True Swords Clash nav 4. Case asserts it.
- **Icon size = three options, and icons grow to fit** (owner: "icons should also make themselves as big as they can (snap up in size) ... just give 3 options small md large"): sizes 96 / 120 / 160 px. A phone uses the LARGEST size, up to the user's cap, that still leaves at least 2 columns in its window: portrait 364 wide -> medium (2 columns), landscape 704 wide -> large. The cap is `phone_prefs.pdl` `ICON_MAX | small|medium|large` (default large; a PREFERENCE file; `phone_op prefs <house> icon_max [value]` validates and writes it atomically; a bad value or an unknown setting is refused and changes nothing). The icon ART scales with the tile (draw-core change: a swatch tile whose own css asked for a size above the default may draw art up to 256 px; every other tile keeps the 64 px cap; the palette pickers are unchanged by construction, not re-checked live). This REPLACES the free x/y icon size sketched in section 22.4: Settings > View will offer the three sizes (and the window presets), not two number fields.
- **Per-instance generated layout (section 22.3, first use):** each launch writes `<instance>/plus-phone[-landscape].xhtpm` and `.css` from the shared templates with the tile size filled in and `${PKG}` made absolute, and launches THAT; `<instance>/icon_size.txt` records the choice. Case `plus_phone` 37/0 now asserts: portrait -> `medium 120`, landscape -> `large 160`, cap small -> `small 96`, a bad value refused with the stored value unchanged, an unknown setting refused. **Bug the case found:** `phone_op`'s `kv()` reads `<dir>/phone.txt`, so the first `icon_cap()` read the wrong file and silently ignored the preference (it always used the default); fixed by reading the preference file directly. Lesson: read a setting back after you write it.
- **Not done / honest:** the app NAME still draws over the top of the icon art (the label needs a below-the-art placement in the shared draw code); the Settings > View screen itself is not built (only the preference verb and the layout generator exist); the `0`/Enter bar cell, placement and everything in sections 19 to 21 are unchanged.

---
## 24. APP-EDIT: the editor that makes phone apps (DESIGN, owner 2026-11-15: "we are actually going to create a new app called app-edit where we will create apps, like xcode's app editor; tsc will be a blank space till we export the real one from this editor onto our device")
Label: SAID for the direction; the rest is DOC (my proposal; I have NOT yet read the existing editors it would reuse, so every "reuse" below is a hypothesis to verify before building).
### 24.1 What changes in what we already decided
- **True Swords Clash is a BLANK space on the device for now.** Its tile stays (manifest `APP | id | true-swords-clash`, `ORDER | 1`) but it has no `ENTRY` and no screens: opening it shows an empty screen (a blank app is a valid app; the tile's current "not wired yet" result line becomes a blank screen). The real TSC is made in app-edit and EXPORTED to the device; it is not hand-wired into the shell.
- **The phone is the DEVICE; app-edit is the IDE.** Same split as Xcode (the editor) and the Simulator or a device (where the app runs), and Android Studio and an AVD. Apps reach the phone ONLY by export/install, never by the phone knowing an app's name.
### 24.2 What app-edit is
A new x11-hq app (`&.hq-apps/app-edit/`, the plugz-hq pattern: one layout, a projector, an action op, instance-dir windows; opened from the top bar's tools or its own cell). A PROJECT is one app package folder, the same shape the phone reads: `manifest.pdl`, `screens/<name>.pdl` (a screen = layout data per size class), `assets/` (icon folders by size, images), `settings.pdl` (declared SETTING rows, section 22.1), `events/` (verbs and event pages, the house's own event-page format), `tests/` (harness cases). The editor never invents a second format: it edits the files the phone runs.
### 24.3 The panes (Xcode / Android Studio vocabulary -> ours)
| IDE pane | app-edit | Reuse (to verify) |
|---|---|---|
| Project navigator (files of the package) | list of the package's files and screens | the book/page tree of the existing editors |
| Canvas / Interface Builder / layout editor | the screen drawn as the phone shows it, selectable elements | `blueprintz` (book, pages, events editor) and the swatch-grid phone layout itself; the PREVIEW is a real phone window, not a drawing |
| Inspector / Attributes | typed rows for the selected element or the manifest | the plug `PARAM` rows and `plug_ref_value_ok` (one validator) |
| Device picker + orientation (IB "View as", Simulator) | choose a device PROFILE and portrait/landscape; show several side by side | device profiles (22.2) and `window_place_op` (windows tile beside each other) |
| Run button ("Run on device") | EXPORT: validate, package, INSTALL to a phone and open it | `phone_op install <package>` (new verb); validation = `harness_menu_op check`-style checks for manifests, screens, settings |
| Debug area / console | the phone's relay log and the app's ledger rows | existing relay and ledger readers |
| Assets catalog | icon sets by size, with the safe-zone preview | sprite folders (BUILT) |
| Maker / templates (File > New) | new blank app, new screen, new setting, new event | `harness_menu_op make` (BUILT shape: data only, refuses to overwrite) |
### 24.4 Export / install (the contract between the two halves)
`app-edit export <package> [--device <phone n>]` runs the checks (manifest valid, every screen file valid, every referenced asset exists, `PLATFORM` version supported, no verb outside the declared permissions), copies the package to `&.hq-apps/plus-phone/apps/<id>/` (or the target phone's own `instances/<n>/apps/` for a per-device install, so one phone can have a build the others lack: the cross-version PvP test), and the phone's projector picks it up on its next republish (the apps list is regenerated, never edited by hand). An exported app that fails a check is refused and nothing is copied. Uninstall = removing the folder. The package folder is also what the native C app will load.
### 24.5 Design-choice test (answered, DOC)
One authority: the package folder IS the app; the editor and the phone read the same files. Modular: the editor composes existing pieces (data formats, validators, window services) and adds almost no new format. Scalable: the 10th app is a new package folder; the 10th screen is a new file. Long term: the native C phone app reads the same packages; app-edit becomes the authoring tool for it. House shapes: x11-hq app on the plugz pattern, ops with a `check`, data in `.pdl`, a harness case that can fail, a picture per step. Rejected: building TSC straight into the phone shell (the exact thing this split avoids), a separate editor format that must be converted (two authorities), a new web UI.
### 24.6 Open questions (before building)
1. Is app-edit a NEW app that opens the existing editors' pieces inside it, or does it embed/extend `blueprintz` or `rpg-edit-hq` (which already edit books, pages, events and have a Play button)? My lean: new shell, reuse their file formats and checks, not their windows. Needs a read of both first.
2. What does a SCREEN contain in v1: only the grid and panels the swatch-grid layout can draw today (tiles, text, buttons), or free positioning? Lean: the existing layout vocabulary only (no new renderer branch), per `khtpm-house-standards`.
3. Where does TSC's real logic live: events in the package (house event pages), or a compiled op the manifest names as `ENTRY`? Lean: events first; `ENTRY` ops later.
4. One project open at a time, or several windows? Lean: one window per project (the instance-dir pattern).
### 24.7 Build order (each step: a case that can fail and a picture)
1. Read `blueprintz`, `rpg-edit-hq`, the layout studio and the plug maker; write what is reusable here (replaces the hypotheses above).
2. Package format + `app_check` op (manifest, screens, assets) with a case; make the blank TSC a real package under `app-edit/projects/` and the phone's blank-app screen.
3. `phone_op install` / `uninstall` and the projector picking installed apps up; case: install a package, the tile appears; a bad package is refused and nothing changes.
4. The app-edit window shell: project navigator + inspector over the package files (no canvas yet); `New app` via the maker.
5. Device preview: open the project on a profile in a phone window (Run on device).
6. The canvas, then export of the real True Swords Clash.

---
## 25. WHAT THE EXISTING EDITORS REALLY ARE (read 2026-11-15; answers step 1 of section 24.7 and open question 1)
Label: READ (files opened today). Not read: the bodies of `rpg-edit-hq`'s ops and `harness_menu_op` beyond what plugz already showed, `IN-GAME-LAYOUTS-PLAN.md` (a prior doc could not find it either).
### 25.1 `layout-studio` (`@.apps/layout-studio`, layers 1-2 built 2026-10-09) = the layouts editor the owner had not seen
It is NOT yet an editor and has no window: it is the DATA + CHECK layer under one. Pieces: `layout_check` (validator for `.xhtpm`: unknown tag or attribute, duplicate id, an item without a label, bad `repeat` binding; prints `ERROR line N: what | fix: how`; `--catalog` regenerates `CATALOG.md`; `--drift <renderer.c>` fails when the renderer reads a tag the tables lack), five starter `templates/` (menu, menu-search, hud-strip, confirm, status-panel) that pass it, `layout_flow` (a game's flow as data: `check`, `navmap`, `press` drives a window by real nav numbers), `pet_gen`. Its strategy (`LAYOUT-STUDIO-FOR-SIMPLE-AGENTS-STRATEGY-2026-10-09.md`) is "constrain first, draw last": catalog, validator, templates, preview, a `layout_op` CLI (new/add/set/move/remove/bind/on/preview/save), manager scaffolds, THEN drag and drop; v2 (`...V2-PAL-EVENTS-DESIGN-2026-10-11.md`) adds three views (layout tree, events as an RPG-Maker command list over the `mr_*` ops, book view). Layers 4-8 (preview, `layout_op`, scaffolds, drag and drop, visual scripts) are NOT built.
**Verified today:** `layout_check` on `plugz-hq.xhtpm` = 0 errors; on my phone layouts = 2 errors + 2 warnings per file, all from the empty-label toolbar items (`bar-top`, `bar-bottom`): the catalog has no "band" concept, so the phone layouts FAIL the house validator. Fix when building app-edit: add the band (an element with no label that is not interactive) to the catalog and the renderer drift table, or the check will refuse every phone screen.
### 25.2 `blueprintz` (`&.hq-apps/blueprintz`, 648-line manager + 93-line layout, ops `bp_*`) = the EVENTS editor and debugger
One window over ONE page's events: a list of events with trigger and state (waiting, queued, running, ran), problem flags, a debugger (PLAY, PAUSE, STEPEV, STEPCMD, ABORT, FIRE), and B4 editing of a selected event's commands (`CSEL EDIT ADD CDEL UNDO`, multi-page, undo stack of 16, 128 command limit). Pattern: a state-file manager (`state/ui.txt`, requests appended to `state/request.txt` by `bp_req`, read by size growth), children by fork+exec with a time limit, `state/target.pdl` points it at another house for tests, `books.pdl` lists extra event sources. It edits event pages only; it knows nothing of layouts, manifests or packages.
### 25.3 `rpg-edit-hq` (`@.apps/rpg-edit-hq`) = the WORLD/BOOK editor with Play
A full app folder: `rooms.pdl`, `catalog.pdl`, `events/`, `books/`, `art/`, `plugs/`, `verbs.pdl`, `time.pdl`, `layouts.pdl` (declared bars: `BAR | id | key | slot | modes | default | group | note` and `SETTING` rows, read by `ui_mode_op`; the editor remembers per mode which bars the user left open), `toolbar.pdl`, `side_style.pdl`. Edit mode shows palettes, Play mode shows the entity list and game chat. This is the shape app-edit's own WINDOW should copy (data-declared bars and modes), not its content.
### 25.4 What this changes in section 24 (the answer to open question 1)
- **app-edit is a NEW shell, not an extension of any of the three**, and it is mostly COMPOSITION of what exists: `layout_check` is its validator for screens; `layout-studio` templates are its "File > New screen"; `layout_flow navmap`/`press` is its device-preview driver; `blueprintz`'s edit verbs and event-page format are its events pane; `rpg-edit-hq`'s `layouts.pdl` BAR/SETTING rows are how it declares its own panes and modes; `harness_menu_op` (plugz) is its checks/tests runner; `window_place_op` tiles the previews; the plug `PARAM` rows are its settings pages.
- **The first layer to build is layout-studio's missing layer 4-5 (`layout_op preview` / `layout_op` CLI) because every other piece sits on it.** That is the same order its own strategy gives (CLI before drawing) and it benefits agents today, so build it as part of app-edit step 2-4 instead of a second tool.
- Revised build order for section 24.7: (1) teach `layout_check` the toolbar band so phone screens pass (small, testable now); (2) `layout_op` verbs `new/set/preview` for phone screens with a PNG and nav list per call; (3) package format + `app_check` (reusing `layout_check` and `plug_ref_value_ok`); (4) `phone_op install/uninstall`; (5) app-edit window (project list, inspector, "Run on device") on the plugz instance-dir pattern with `layouts.pdl` bars; (6) events pane delegating to blueprintz verbs; (7) canvas.

---
## 26. BUILT 2026-11-15 (later): the foundations and app-edit v1 (owner: "do what it takes to finish out the foundations, and actually start working with the editor to build tsc")
SEEN: a real app-edit window (784x504 PNG read), a real mouse click on "export to the phone" (relay) that printed `INSTALLED true-swords-clash`, and a real phone that opened the app screen and went back.
- **Package format + checker** (`&.hq-apps/plus-phone/ops/app_check.c`): a package is a folder named after its app id with `manifest.pdl`, `icon/sprite.csv` and optional `screens/*.xhtpm`; rows `APP | id`, `TITLE`, `ICON`, `ORDER`, `PIN | first`, `ORIENT`, `PLATFORM` (newer than 1 is refused), `SCREEN | name | path`. Unknown keys, a folder not named after the id, a path outside the package, and a screen that fails `layout_check` are all errors with a fix line. A package with no screens is a valid BLANK app. Case `app_package` 22/0.
- **Install / uninstall** (`phone_op install <house> <package>` / `uninstall <house> <id>`): install runs the checker, copies nothing on failure, stages the copy and renames it into `apps/<id>` (an update replaces it), and tells every open phone to republish; `settings` cannot be uninstalled.
- **App screens on the phone:** tapping an installed app writes `.hq_manager/screen.txt`; the projector then shows only that app's tile (as the header) and a `<` back button, `home` clears it. A hidden tile is removed at parse time, so a window with NO visible tile loses the swatch-grid layout (toolbars lose their flush placement and gain nav numbers): the open app keeps its own tile on purpose. `settings` stays show-only. Text lines (`<text>`) are not drawn in the swatch-grid path, so the "blank" state is simply the empty body under the header tile.
- **True Swords Clash is now a BLANK PACKAGE** (`&.hq-apps/app-edit/projects/true-swords-clash/`, no screens) installed through the op, as the owner asked (TSC blank until built in the editor and exported).
- **app-edit v1** (`&.hq-apps/app-edit/`: `app-edit.xhtpm`, `.css`, `ops/app_edit_op.c`, ONE op that is both the window's projector module and its verbs): a project list (title, `[on phone]` mark), the selected project's status (`package OK` / `has problems`, installed or not), the checker's own lines as details, and three buttons: check this app, export to the phone, open a phone. Verbs `open <house>` (starts the window, instance `instances/main`), `select`, `check`, `export`, `phone`. It edits NO file yet. Case `app_edit` 14/0 (projection, selection, a bad package refused and nothing copied, export installs, a non-instance path refused). Regression: `plus_phone` 47/0, `window_place` 15, `plugz_hq` 64, `harness_menu` 60, `tools_menu` 86, `layout_preview` 7.
- **Not done (honest), in build order:** (1) the PHONE does not yet render a package's own screens: an opened app shows its header tile and an empty body; the next step is that opening an app with `SCREEN` rows relaunches the phone window on the app's own layout (same size, back button returns to the shell), or a screen area in the shell layout; (2) app-edit cannot create or edit anything: needs `New app` / `New screen` (copy a `layout-studio` template), then `layout_op preview` of a screen inside the editor, then an inspector for the manifest; (3) no launcher in the top bar's Tools list yet (open it with `app_edit_op open <house>`); (4) events pane (delegate to blueprintz verbs) and canvas; (5) the XO copies of the design doc were refreshed through section 25 only.

### 26b. Placement clears the left GNOME dock (2026-11-15, owner: "make sure the phones don't open over the left ubuntu gnome left tb")
`window_place_op` reads the desktop's `_NET_WORKAREA` (x y w h with the dock and panel already subtracted; on this desktop `140 54 2356 1610`) and starts at its left edge plus 10 (first phone at x=150, y=110 from `TOP_Y`), and wraps at its right edge. Used only when `--screen-w` is NOT given, so the harness stays deterministic (case `window_place` 15/0 unchanged). Seen: two real phones at x=150 and x=526. The dock width is read, not hard-coded, so a wider dock or another monitor is handled. NOT covered: windows that do not use the placement op (app-edit and the other sidebar windows open at the renderer's house default x=80, which is inside the 140 px dock; not changed because the owner asked about phones only).

## 27. SEPARATE BUT EQUAL, and the first real app (owner 2026-11-15)
**Owner:** +phone is "not really part of LyfeDesk": it is the interface for the phone software of **TSOTS**, a separate company (not Tear-it co). LyfeDesk also uses it. The first product is tested through the phone sandbox, not raw rpg-edit. rpg-edit opens as the SECOND phone app; our rpg-edit apps run in the phone later; the text-based Bible app comes first. (SAID)

**Rules that follow (DOC, to keep true in code):**
1. The phone (`phone_op`, `phone_prefs.pdl`, app packages, `app_check`) must not assume LyfeDesk: no taskbar-only paths, no LyfeDesk wording in phone-facing text. LyfeDesk is one HOST of the phone (top-bar `0`/`+`), not its owner. A later native phone shell must be able to use the same packages unchanged.
2. App packages are the contract between product and phone. TSOTS apps are packages; app-edit (the IDE) exports them.
3. App order: 1 settings (pinned), 2 True Swords Clash (text), 3 rpg-edit (opens the existing editor in the phone). Not built: the rpg-edit package.

**First real app = text True Swords Clash (READ the middle-house sources, not yet run):** rules `+.TSOTS-ALPHA-OMEGA/#.RULES/True_Swords_Clash_Rules.txt` (HP 100, Mana 0; turn = Mana Challenge 2-10 sorted verses, or one of 11 Miracles; 4 statuses; win at HP 0 or forfeit), design `@.apps/TSC_ELO/TSC_DESIGN.md` (reuse the TSOTS-OG Mana Challenge engine; master ledger `data/master_ledger.txt`; Elo op). Per the ledger standard, the match state is ledger rows only; screens are projections.
**Plan (build order):** (1) read TSC_DESIGN.md sections 5-12 and TSC_ELO ops, find what runs without GL (`NO_GL=1`); (2) screens as layouts: home, mana challenge (sort verses), miracle list, status/HP, result; (3) one op for the verbs (as `app_edit_op`), calling the existing TSC ops where they exist; (4) the phone renders package screens (open item 1 of section 26); (5) a case that plays a scripted match and checks the ledger.
**Open questions for the owner:** Bible text source (KJV/WEB; the book-stack file is unlabelled); HvC first, then HvH?; where does the Elo rating live for a phone user?

### 27b. What the middle-house TSC really contains (READ the sources 2026-11-15, nothing run)
- **The design is the full game, the code is not.** `@.apps/TSC_ELO/ops/` has 11 compiled ops (tsc_setup, tick, deal, input, ai, compose, elo, miracle, net, ledger_append/peers) but `tsc_deal.c` implements an OLD placeholder duel (strike 3 dmg/0 mana, heavy 6/2, heal +5/2, block; keys 1-4), NOT the rules file: no verse Mana Challenge and no 11 Miracles. `tsc_miracle.c` is a 13-line stub (`return 0`). So the real rules (sorting 2-10 verses, Hail Mary..Holy Sword, Bleeding/Poison/Locust/Blessing) are paper only.
- **What exists and is reusable:** `tsc_elo.c` (273 lines, chess Elo K=32, ratings file `<xyzfs>/home/games/tsc_ratings.txt`), `tsc_tick.c` (referee: status effects at turn start, win check, dispatches the AI), `tsc_ai.c`, the ledger format `timestamp|epoch|player|turn|action|type` (design section 6), `config.txt` shape; the PvP proof run `proof/pvp-20260803-181726` (P2P over `palnet_peer`, 127.0.0.1 only). The verse engine is `+.TSOTS-ALPHA-OMEGA/TSOTS-OG+01.00/` (`tsots_deal.c`, `tsots_input.c`, `tsots_compose.c`).
- **Consequence for the phone app:** do not port the old ops (they are a terminal + GL widget pair with their own PAL loop). Write the phone TSC as ONE new C op implementing the rules file exactly (the rules are 47 lines and small), a ledger as the only state (replayable), reuse the Elo formula and the TSOTS-OG verse dealing logic by reading them. Verses need a text source (owner question open).

## 28. DATA PACKS AND A SERVER: installing apps and data from the net, not only p2p (owner 2026-11-15: "make a link in gh to download data packs like this separately in the future like hugging face... users install apps from a server other than p2p, data, etc.")
**Status: DESIGN only, nothing built.** Test subject: the True Swords Clash verse file (`&.hq-apps/app-edit/projects/true-swords-clash/verses.txt`, 31,102 verses, 4.6 MB). It was committed into git on 2026-11-15 (`2e14ff016`), which is fine for GitHub (soft limit 50 MB per file, hard 100 MB) but does not scale: every future text, image set or sound pack would bloat the clone for everyone.

**Shape (DOC, design-choice test answered):**
1. **Two kinds of install unit.** An **app package** (section 24/26: manifest, layouts, icon; small, text) and a **data pack** (large data an app reads: verses, images, sounds). A package names the packs it needs: `NEEDS | verses-ch2en | 1 | sha256`. Packs live in `<house>/&.hq-apps/plus-phone/packs/<id>/` (a runtime folder: needs a cap row in the growth standard).
2. **The server is just static files.** First choice: **GitHub Releases** of the org's repo (free, a URL per file, up to 2 GB per asset, no new infrastructure), with ONE index file `packs.pdl` (rows `PACK | id | version | size | sha256 | url | title`) published as a release asset or a raw file. Hugging Face datasets are the second choice if size or traffic outgrows Releases (same index, different urls). p2p stays another source of the SAME pack: the sha256 is the identity, not the place it came from.
3. **Verbs (one C op, like phone_op):** `pack list <house>`, `pack get <house> <id>` (download to a temp name, check size and sha256, then rename into place: a failed or tampered download never appears), `pack remove`. `phone_op install` refuses an app whose NEEDS are missing and says which `pack get` to run. The download uses a plain HTTPS fetch (an external `curl` called from the op; no logic in sh).
4. **Safety rules:** a data pack is DATA ONLY (text, images, audio, pdl): never a binary or a script, so installing one cannot run code. App packages from a server are also data and layouts; native ops (`.+x`) are NOT installable from a server until there is signing (open, owner decision). Every download is checked against the index sha256, and the index itself comes from a url the owner/company controls. Never print or send user data in a request; a download is a plain GET with no identity.
5. **Money and rights:** a pack carries its source and licence in its own `pack.pdl` (the verse file's source note is the first example: `verses.source.txt`). Bible text rights are still an owner question (the book-stack file is unlabelled).

**Build order:** (1) move the verse file out of the app package into a pack folder with a `pack.pdl` and a sha256 (still local); (2) `pack_op` with `list/get/remove` against a local `file://` index (testable without a network, with a tampered-file negative test); (3) publish one GitHub Release with the verse pack and the index; (4) `get` over HTTPS; (5) the phone's Settings gets an "Apps and data" screen that lists the index and installs.
**Open questions for the owner:** which GitHub org/repo hosts the releases (TSOTS company vs Tear-it co: this is the "separate but equal" line of section 27), whether to later remove `verses.txt` from git history (it is in `2e14ff016`; GitHub keeps it, a rewrite would need a force-push, so I would not), and the signing rule for installable code.

### 28b. GitHub as the store AND the bootstrap for p2p / the chain (owner 2026-11-15: "i do wanna use github infrastructure for things like store and even p2p blockchain (super node etc)")
**Status: DESIGN (DOC), nothing built, nothing verified against GitHub's current terms (check them before relying on any of this).**
- **What GitHub is good for here (static, signed, versioned files):** the store index (`packs.pdl`), app and data-pack downloads (Releases), the **bootstrap list** of peers and super nodes (`peers.pdl`: address, public key, since), **chain checkpoints** (a periodic signed snapshot of a block height + hash so a new node does not replay from zero), and the release of the node software itself. Every file is identified by its sha256 and, for the peer list and checkpoints, by a signature from a key the company holds (so a changed file on GitHub is detected, not trusted).
- **What GitHub is NOT:** a node. It has no sockets, cannot gossip, mine or validate, and using it as a live database or for compute is the sort of use its terms restrict. So: **GitHub = the front door and the notary; the super nodes are our own always-on machines** (the owner's Mac/debil, later rented boxes) that run the existing chain code and serve peers. A scheduled GitHub Action may refresh `peers.pdl` and publish checkpoints (static output only, within its free limits).
- **Honest state of the p2p code:** `palnet_peer` is same-machine only (127.0.0.1 hardcoded, lines 138/166 per the roadmap) and the pet-chain/exchange pieces are designs plus tested local ledgers: so the first real step is making the peer connect to an address from `peers.pdl`, tested across two machines (an ssh step in a harness case), before any "super node" claim.
- **Build order addition (after section 28 steps 1-3):** (a) `peers.pdl` format + signature check op (local files first, tamper negative test); (b) `palnet_peer` takes a peer address; (c) a super-node role = the same program started with `--serve`; (d) checkpoints; (e) the phone's Settings shows peers/height.
- **Open owner decisions:** which key signs the index and who holds it; which org owns the repo (TSOTS vs Tear-it co, section 27); whether a store takes payments (out of scope until legal is read).

### 28c. Owner answers: cones as payment, apps DOWNLOADED not installed, dedicated repos, periodic trunks (owner 2026-11-15)
**SAID:** payments are in **cones** for now (the house chain's unit, see the pet-chain/economy docs); TSC will be p2p, so this is relevant to it; apps are **downloaded from GitHub if possible** (not "installed"), from dedicated app and data repos under the tearitco / TSOTS GitHub accounts; asked: can GitHub hold periodic "master trunks" of the blockchain and p2p host lists, updated periodically? (not a live database: the live copy is on users' machines or p2p).
**Answer (DOC; limits quoted from memory, NOT checked this session: verify before relying):** yes, that is the right use. Rule of where each file goes:
| File | Size / change rate | Home |
|---|---|---|
| `peers.pdl` (host list), the index `packs.pdl`, checkpoint headers (height + hash + signature) | KBs, hourly to daily | a **git file** in a small dedicated repo (history is tiny; a bot commit per update is fine, but not every minute) |
| the **trunk** (a full chain snapshot) and data packs | MBs to GBs, rarely | **Releases assets** (one asset, up to about 2 GB each; never commit them to git: every old version stays in the clone forever) |
| apps (app packages) | KBs to MBs | one **repo per app** (or per family), versions = tags, a download = the tag's archive or a Release asset |
A new node's join = fetch `peers.pdl` (signed), fetch the latest trunk asset + checkpoint, verify the sha256 and signature, then sync the gap from peers. The trunk is a CACHE: if GitHub is unreachable, p2p still works, it is only slower to start. Periodic = a scheduled Action (best-effort timing, minutes-to-hours late is normal, so never depend on exact times) or the owner's super node pushing with a token.
**"Downloaded, not installed":** a phone app is fetched as a package folder into `apps/<id>/` (the same unit `app_check` validates), so "install" is a verified download; removal = delete the folder. The phone's Settings "Apps and data" screen (section 28 step 5) is where it shows.
**Cones:** the store price is a cone amount; a purchase is a ledger row on the chain, not a file flag (no ghost data). Who receives the cones (TSOTS company vs Tear-it co) is an owner decision; the old chain is 99.98% mined, so cones for the store come from the preferred-cone design in the chain-economy doc, not from mining. Not built, not checked against the chain code this session.
**Repos (proposal, owner names them):** `tsots/apps-<name>` one per app, `tsots/data-<name>` one per data pack, `tsots/net` (peers.pdl + checkpoints index), trunks as Release assets there.
