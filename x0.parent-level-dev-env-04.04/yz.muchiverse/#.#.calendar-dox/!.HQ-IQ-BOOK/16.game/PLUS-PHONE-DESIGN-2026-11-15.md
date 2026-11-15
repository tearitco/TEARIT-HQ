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
