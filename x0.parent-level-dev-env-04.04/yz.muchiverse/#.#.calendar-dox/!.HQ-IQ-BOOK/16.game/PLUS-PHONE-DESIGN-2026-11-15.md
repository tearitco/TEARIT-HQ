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
