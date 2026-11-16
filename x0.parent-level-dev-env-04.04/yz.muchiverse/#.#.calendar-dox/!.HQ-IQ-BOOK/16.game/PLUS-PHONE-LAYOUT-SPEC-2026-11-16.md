# +phone layout spec: the phone is a meta window, apps nest inside it (2026-11-16)

> Owner 2026-11-16 (SAID, verbatim): "There is always chrome 'invisible' for the phone. It's meta. Then the phone gets persistent top and bottom tb. Those never go away (home is the app view). Then each app opened goes in the phone's bottom tb for minimize etc. Also apps get a chrome plus their own top and bottom tb. So there will be 2 bottom tbs (phone and app) and 3 top bars: 1. chrome, 2. phone top tb, 3. app top tb. Get it?" And: "in the bottom tb for TSC right now there is 'player1' and that's it for now, a show of life. Eventually the phone will operate much like LyfeDesk, so just use that logic. It's more like a display surface that is compliant with our setup / house."
> Labels: **SAID** = the owner told me; **READ** = I opened it; **DOC** = my reasoning, not decided. Written by Claude Sonnet 5.5. Related: `PLUS-PHONE-DESIGN-2026-11-15.md` sections 27 to 33.

## 1. The stack (SAID, drawn by me)
One X11 window per phone (the meta window). Top to bottom:
```
 +--------------------------------------------------+
 | 1 CHROME   (meta, invisible; the phone's frame)  |  never drawn as a bar; the window's own frame/title
 +--------------------------------------------------+
 | 2 PHONE TOP TB   HQ v | phone : <page> v         |  persistent: never goes away, on home or in an app
 +--------------------------------------------------+
 | 3 APP TOP TB     (the open app's own top bar)    |  only while an app is open; the app owns its content
 +--------------------------------------------------+
 |                                                  |
 |   VIEW  = the app view                           |  HOME = the app picker (grid of apps) IS this view
 |   (home: grid; app: the app's body)              |  with no app open
 |                                                  |
 +--------------------------------------------------+
 | APP BOTTOM TB    (the open app's own footer)     |  only while an app is open
 +--------------------------------------------------+
 | PHONE BOTTOM TB  one entry per OPEN app          |  persistent; minimize/switch; never goes away
 +--------------------------------------------------+
```
Count: **3 top bars** (chrome, phone top, app top) and **2 bottom bars** (app bottom, phone bottom). The order of the two bottom bars is DOC (phone outermost = lowest); the top order is as the owner numbered it.

## 2. Rules (SAID unless marked DOC)
1. **Chrome is meta and invisible** for the phone; it never goes away. DOC: the "chrome" the owner counts first among the three top bars is the phone's own; each APP also "gets a chrome" (its own window-style frame inside the view). Whether the app's chrome is an extra title strip (a 4th top row) or the same one as bar 3 is **OPEN** (question 1).
2. **The phone's top and bottom bars are persistent.** Home (no app open) is just the app view showing the picker; the phone bars do not change between home and an app.
3. **Every opened app is an entry in the phone's bottom bar** (the phone's window list, like the LyfeDesk bottom bar): activate, minimize, close. Minimized apps stay running (data and ledger are kept; only the view hides). **TSC today has one entry, "player1"; one cell is enough for now ("a show of life").**
4. **Each app brings its own chrome, top bar and bottom bar.** The phone never draws app content into its own bars and the app never re-draws the phone's bars (today `match.xhtpm` re-draws the phone's top tab bar and footer: that is the wrong layer and is removed by this spec).
5. **One window.** Opening, switching, minimizing or going home must not close or create an X11 window (that is the defect this spec fixes: `phone_op open` relaunched the renderer). Only rotate (`flip`, the window size changes), restart and quit may relaunch.
6. **LyfeDesk logic, not new phone rules** ("eventually the phone will operate much like LyfeDesk"): the phone bottom bar behaves like the desktop bottom bar (entries, nav badges, activate by number), tab bars and dropdowns use the same `<tabbar>`/`dropdown-child` shapes as rpg-edit, colors follow Settings "2 colors" (TSOTS black/red for TSC; Tear-it yellow/black for the picker/system), and the phone is "a display surface that is compliant with the house setup": no per-app code in the shell.
7. **Nav numbering** is global per window like every other khtpm window; the persistent phone bars take the low numbers, the app's bars and view follow. A cell typed by number is focused first and fired on Enter or a second click (standard; also fixed for the top bar's `+` on 2026-11-16).

## 3. What an app package declares (DOC, derived from the rules; to be fixed in `manifest.pdl`)
```
APP     | true-swords-clash
TITLE   | True Swords Clash
SCREEN  | view    | screens/match.xhtpm            # the app VIEW only (body); no phone bars
SCREEN  | top     | screens/match-top.xhtpm        # the app's own top bar rows (optional)
SCREEN  | bottom  | screens/match-bottom.xhtpm     # the app's own bottom bar rows (optional)
SCREEN  | view-landscape | ...   (and -top/-bottom for landscape when different)
ENTRY   | player1                                  # the label of its cell in the phone's bottom bar
CHROME  | title   | True Swords Clash              # the app's own chrome/title (open question 1)
```
Today the manifest has `SCREEN | home` and `home-landscape` (whole layouts); those stay readable until the packages are migrated (`app_check` accepts both; a package with only `home` is shown as before, which is the compatibility path).

## 4. Composition: how the one window is built (DOC)
The phone window's layout is **composed, not swapped**: `phone_op` writes the phone's own layout file (`<inst>/plus-phone[-landscape].xhtpm`) from three parts: the phone template (bars 2 and the phone bottom bar with an entry per open app), the active app's top/view/bottom parts (or the picker view for home), and the phone variables. Opening an app = rewrite that file + republish `ui.txt`; the renderer's live reparse updates the same window. **Status 2026-11-16: the in-place rewrite did NOT reach the running window in an experiment (a label edit was not shown either); the cause is being investigated before the composition is built** (design section 33).
**Why compose at write time:** there is no layout include mechanism in the renderer (design section 32); composing in `phone_op` keeps the shared renderer unchanged (zero per-project C in `khtpm_core_render.c`, the standing house rule).

## 5. Phone bottom bar entries (DOC)
`ui.txt` variables for the window: `n_apps`, `app_N_id`, `app_N_label`, `app_N_state` (`open`|`min`), `app_active`. Click/number on an entry: activate (restore from minimized); a small `_` control minimizes the active app (back to home); `x` closes it (the app's data stays; it is just no longer running in the view). The list is a UI preference of that phone instance (never game state; the app's own ledger is the authority for the app).

## 6. Open questions (new)
1. **Chrome:** is the app's "chrome" a title strip of its own (so an open app shows a 4th top row), or is it the same row as the app top tb (bar 3)? 2. Minimize: back to home, or leave the view blank until an entry is tapped? 3. Several apps open at once: only one view visible (like a phone) with the others as entries (my default), or tiles? 4. Does the phone bottom bar also hold system cells (home, back) next to the app entries? 5. Landscape: do the bars stay full-width and the view shrink (my default)?

## 7. Acceptance (a harness case that can fail, DOC)
`phone_layers`: open a phone; open TSC; assert the window pid and X window id are UNCHANGED (negative control: `flip` DOES change the window size and relaunch); assert the dumped picture differs from the picker picture (open the PNG and look); assert the phone top and bottom bars are present in both pictures (pixel strips equal); assert the bottom bar shows the `player1` entry; minimize returns to the picker with the same window; the TSC ledger is byte-identical across open/minimize/open (the app's data is untouched).

## 8. The app chrome row (a consequence of rule 3, owner 2026-11-16: "not a new spec, use logic: I cannot minimize TSC if it does not have its own chrome")
Owner: "the individual app will also get a chrome (under the tb for phone hq). There should be a stack of 3 in the phone before we worry about entities."
**So the top stack inside the phone is THREE rows:** (1) the phone's chrome (meta window frame/title), (2) the phone top tb (`HQ v | phone : <page> v`), (3) the APP's chrome row directly beneath it (the app's name and its own window controls: minimize `_` back to home, close `x` = `closeapp`). This resolves question 1: the app's chrome is a separate row (not the same row as the app's own top tb). The app's own top tb (bar 3 in section 1) and bottom tb come after the chrome and are optional per app. Entities (a hotbar, cursword, ...) wait until this stack of three works.
**Implementation shape (DOC):** token `${APP_CHROME}` in an app screen, built once by `phone_op` from the open app's manifest `TITLE` (like `${PHONE_FOOTER}`); the app never draws the phone's rows and the phone never knows the app's name except through the manifest.
**Built 2026-11-16:** token `${APP_CHROME}` (portrait TSC screen); `phone_op` `app_chrome()` writes a tab bar `appchrome` directly under the phone tab bar: the app NAME as a display-only cell (class `no-nav`: no number, not a control; owner "the top app chrome does not need nav for the app name") plus `[_]` (= `phone_op home`, minimize) and `[x]` (= `closeapp`). Seen live: the name shows with no badge, typing `3` then Enter minimized TSC (same pid, `minimized.txt` set). Renderer: the existing opt-in class `no-nav` is now also honored by `<tab>` elements (display-only tab, narrower, not in `g_nav[]`; precedent: the dock strip and swatch grids already honored it). The landscape TSC screen still has its own bars (not migrated). Harness: `phone_layers` 40.
