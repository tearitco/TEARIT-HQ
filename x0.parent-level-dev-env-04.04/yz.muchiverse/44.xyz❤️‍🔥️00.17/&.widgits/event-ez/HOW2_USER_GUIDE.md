# 📘 event-ez — User guide (new players & agents)

**What this is:** a simple **visual event editor** (like a thin RPG Maker MV event list).  
**What it’s for:** teach a desk entity (e.g. a MUCHI_RANCHER monster) *what to do* by clicking screens — not by writing code by hand.

**Related:**
- Design/history: `walk-off-au5.md`, `&.widgits/event-editor/visual-event-compiler-pal.md`
- Ranch open from menu: right-click monster → **Events (ez)**
- Agent k3 how-to (Change Gold): `@.apps/MUCHI_RANCHER/HOW2_event-ez_change_gold_k3.txt`

---

## 1. One-sentence idea

You open an **event** for a character, edit **pages** (each page = one script with its own “when” + command list), save, and later the game **runs** that script.

```
You (visual editor)
    → event.ir.pdl   (readable list of what you meant)
    → event.pal      (runnable script the engine executes)
    → gold / state   (real effect when Play / runtime runs it)
```

The window is your **sanity check**: if the list looks right, the files and the game should match after Save + Run.

---

## 2. How this maps to RPG Maker MV

| RPG Maker MV | event-ez |
|---|---|
| Double-click map event | Open **Events (ez)** on a monster (or launch with `EZ_PKG_*`) |
| Event with **pages** | **Event pages** on the first screen |
| Page **Trigger** (Action Button, Autorun, …) | **When / Trigger** on that page (`on_click`, `on_spawn`, `parallel`) |
| Page **Contents** (command list) | **Event Commands** on the page |
| Insert command (Show Gold, etc.) | **New Event Command…** → pick a type (e.g. Change Gold) |
| OK on the event | **OK — Save Command** / **Save Trigger** |

You do **not** create a new page just to set a trigger.  
**Every page has its own trigger.** Edit the page you care about.

---

## 3. What you should press (happy path)

### A. Open the editor

**From the desk (recommended):**
1. Right-click the monster window.  
2. Choose **Events (ez)**.  
3. An **EVENT EDITOR** window opens for that character.

**From a terminal (dev / agent):**
```bash
export EZ_PKG_NAME=m6_golddeity   # or m8_redhorned, etc.
export EZ_PKG_DIR="<house>/@.apps/MUCHI_RANCHER/entities/<name>/event_pkg"
cd <house>/&.widgits/event-ez
sh button.sh r
```

Close when done: close the GL window, or `sh button.sh kill`.

---

### B. First screen — “Event pages” (the starter list)

You should see something like:

```
EVENT EDITOR — m6_golddeity
What is this? Each PAGE is one event script...
HOW TO: number or arrows, then Enter.
-- EVENT PAGES --
[>] 1. Page 1 - When: on_click (0 cmd)
[ ] 2. Page 2 - (new empty page)
```

| What you see | Meaning |
|---|---|
| **Page 1 - When: on_click** | Page 1 exists; meant to run on “activate”; command count in parentheses |
| **Page 2 - (new empty page)** | Extra blank slot (RMMV-style “always one more”) — only open if you want a *second* script |
| **[>]** | Keyboard focus (like house CHTPM menus) |

**What to do for normal work:**  
→ Focus **Page 1** → **Enter**.

**Do not** open Page 2 unless you intentionally want another page.

**Nav keys (same as the rest of this house):**
- **Arrows** — move `[>]`  
- **Number then Enter** — jump to that row and open  
- **Enter** — open / activate focused row  

---

### C. Event Page screen — one page’s settings + commands

Rough layout:

```
EVENT PAGE 1 — m6_golddeity
When does this page run? (RMMV Trigger)
  Trigger: on_click / on_spawn / parallel
  [Save Trigger]
-- EVENT COMMANDS (runs top to bottom) --
  • Change Gold: 25     (example)
  New Event Command...
< Back to Event Pages
```

#### “When / Trigger” — what is that?

This is **page properties**, not a command in the list.

| Value | Intent (like RMMV) | Fully enforced live today? |
|---|---|---|
| **on_click** | Run when the entity is activated (menu / interact) | **Labeled & saved**; runtime hook-up still incomplete |
| **on_spawn** | Run when the entity window starts | Same |
| **parallel** | Run while the window stays open | Same |

**Default for new users:** leave **on_click**, hit **Save Trigger** only if you changed it.  
Then focus on **Event Commands**.

You are **editing this page**, not inventing a special “trigger event.”

#### Event Commands — what is that?

The **script body** for this page only — top to bottom, like RMMV **Contents**.

| What to press | What happens |
|---|---|
| **New Event Command...** | Open the command-type list |
| **Clear All Commands on This Page** | Wipe every command on this page (keeps trigger). **Working as of 2026-08-06** (`KEY:7` in `ez_menu_input`) |
| A line like **• Change Gold: 25** | Display only for now (edit-in-place not built yet) |
| **&lt; Back to Event Pages** | Return to the page list (starter screen) |

**Back should restore the full page list** (Page 1, empty slot, …). If the list is blank after Back, that’s a bug — see §7.

---

### D. New Event Command — pick a type

```
NEW EVENT COMMAND (page 1)
Only working types are listed.
  Change Gold...
  < Cancel
```

- Pick **Change Gold...** for money.  
- Only **working** types appear (no dead “Show Choices” until it works).  
- **Cancel** returns to the page without adding.

---

### E. Change Gold — set amount and save

```
Change Gold (page 1)
HOW TO: Enter on field, type number, Esc, then OK.
  Gold amount (e.g. 25 or -5)
  OK — Save Command
  < Cancel
```

**Steps:**
1. Focus the amount field → **Enter** (start typing).  
2. Type a number (`25` = add; `-5` = spend, if supported).  
3. **Esc** (stop typing — important).  
4. Focus **OK — Save Command** → **Enter**.  

You should see a message like: *Saved Change Gold 25…*  
Then **Back** / Cancel to the page; the command list should show the new line.

**What Save actually does (so you trust it):**
1. Appends a **NODE** to `event.ir.pdl` (readable).  
2. Rebuilds `event.pal` from all NODEs (runnable).  
3. Makes small `cmd_N.sh` wrappers when needed (`prisc+x` only allows one literal `exec` arg).

---

## 4. After editing — make the game “do it”

The editor **authors** scripts. Something still has to **run** them.

| How | Status |
|---|---|
| Dev: `prisc+x …/pages/page_1/event.pal` | Works (agent-proven for Change Gold) |
| Desk menu **Play / Faucet** | Not fully wired for all monsters yet |
| Automatic “on_click” when you use the entity | **Not fully enforced yet** — trigger is saved as intent |

So: seeing **When: on_click** does **not** mean every desk click already runs Page 1.  
It means *we intend* that page to run on activate once runtime is hooked up.

Check money after a run:
```text
@.apps/MUCHI_RANCHER/entities/<name>/inventory.txt
```
(e.g. `qolq=35`)

---

## 5. Mental model cheat sheet

```
Monster (desk window)
  └─ event_pkg/
       └─ pages/
            ├─ page_1/
            │    condition.pdl   ← trigger for THIS page only
            │    event.ir.pdl    ← readable command list
            │    event.pal       ← what the engine runs
            │    cmd_*.sh        ← helpers for multi-arg ops
            └─ page_2/           ← only if you use a second page
```

| Confused thought | Clear answer |
|---|---|
| “Do I make a new page for the trigger?” | **No.** Trigger is a field **on each page**. |
| “Is on_click the same as Feed?” | **No.** Feed is a menu stub; on_click is page *when*. |
| “Is EVENT-EZ a different game?” | **No.** It’s the editor for this entity’s events. |
| “Why Page 2 empty?” | Optional next page slot, RMMV-style. |

---

## 6. Files you might peek at (optional)

| File | Role |
|---|---|
| `event_pkg/pages/page_N/event.ir.pdl` | Human-readable commands |
| `event_pkg/pages/page_N/event.pal` | Compiled script (`exec …/cmd_N.sh`, not raw 2-arg hacks) |
| `event_pkg/pages/page_N/condition.pdl` | Trigger for that page |
| Session `pieces/display/current_frame.txt` | What the window shows *now* |
| Session `pieces/debug/frames/session_frame_history.txt` | Append-only frame log for debugging nav |

Session path (while editor is open):
```text
&.widgits/event-ez/pieces/sessions/<id>/
```

---

## 7. Troubleshooting

| Symptom | What to try |
|---|---|
| No **Events (ez)** on monster | Monster with `objects.pdl` must list it there (not only `meta.pdl`). Right-click again after update; respawn if needed. |
| Starter page list **empty** after Back | Fixed 2026-08-06 (gallery rows were wiped while on a page). Update `ez_compose_frame.+x`, reopen editor. Check frame history. |
| Save says enter an amount | Field still in type mode — **Esc** first, then OK. |
| Gold file doesn’t change | Did you **run** `event.pal`? Save only writes scripts. |
| Double/confusing numbers on buttons | Editor labels should not bake `1.` into button text; CHTPM draws `[>] N.` itself. |

---

## 8. For agents (k3)

Prefer the same human path:

1. Launch with `EZ_PKG_NAME` + `EZ_PKG_DIR`.  
2. Inject `KEY_PRESSED` into the **session** `pieces/keyboard/history.txt` (not the ranch entity relay).  
3. After each inject, read `current_frame.txt` (and append-friendly `session_frame_history.txt`).  
4. Assert IR NODE + wrapper `event.pal` + inventory after `prisc+x`.  

Full inject sequence for Change Gold:  
`@.apps/MUCHI_RANCHER/HOW2_event-ez_change_gold_k3.txt`

---

## 9. Honest limits (so we don’t oversell)

- **Working command type today:** Change Gold (and growing).  
- **Triggers:** saved & shown; **not** full runtime page selection yet.  
- **Edit existing command in-place:** not yet (Save appends new NODEs).  
- **Show Choices:** designed, not listed until runnable.  

---

## 10. One-screen “I just want gold”

1. Right-click monster → **Events (ez)**.  
2. **Enter** on **Page 1**.  
3. **New Event Command...** → **Change Gold...**.  
4. Enter on amount → type `25` → **Esc** → **OK — Save Command**.  
5. Run the script (dev `prisc+x` until desk Play exists).  
6. Check `inventory.txt`.  

That’s the whole product loop for a new user.

---

*Written 2026-08-06 for hikikomorai/livedesk + MUCHI_RANCHER proving ground.*
