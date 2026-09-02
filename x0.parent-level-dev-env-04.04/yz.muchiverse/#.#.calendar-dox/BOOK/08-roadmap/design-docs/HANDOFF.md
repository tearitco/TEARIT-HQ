# 🤝 PROJECT HANDOFF — Sessions, Games, Events, and DB

**Version:** 2026-08-28 (db/events layout fill-out queued)
**Updated by:** grok (session with ~18% context remaining — spec only, not implemented this turn)
**Audience:** New agents (any context size), PM reviewing progress

---

## ⚡ TL;DR (30 seconds)

We're building a playable game engine inside livedesk. **Current status:**
- ✅ Livedesk taskbar runs (14 cells), USER cell has real account creation (relay-verified)
- ✅ Two critical bugs fixed (focus steal, arrow nav)
- ✅ Change Gold event runtime works end-to-end (relay-verified) — single page, single trigger only
- ✅ Ops migration done: game ops now live at `xyzfs/bin/<game>/ops/+x/` (shared, house-wide), not
  per-game dev folders
- ✅ Taskbar terminal ASCII mirror + a new capture/dispatch architecture (2026-08-18) - see
  `taskbar-tpmos-parallel-refactor.md` + `taskbar-history-txt-migration-investigation.md`
- ✅ **db-hq list tabs (2026-08-28):** Actors…Tilesets + Items + System + Types
  are superficial PDL+inject (house nav). Default tab is Actors `[1]`. CE `[12]`
  restores via inject. **Terms `[15]` still the old CE-sidebar layout — leave
  until human says rebuild.** Spec: `DB-HQ-HOUSE.md`.
- 🚧 **NEXT (palettes / RMMV img — AWAITING APPROVAL, do not start):**
  Move **all** RMMV img (including **tilesets**) **out of the house**
  to `NNEST-11.17/` (zip parent, not `palettes/assets` — that bloats).
  Point `RMMV-ASSET-SOURCE-LOCATION.pdl` at that folder (path can
  change / win/mac). Then wire non-tileset tabs to **real PNGs** from
  those dirs and **place** them. Spec: `RMMV-IMG-DIR-TABS-PLAN.md` §10.
  Do **not** copy more PNGs into `&.widgits/palettes/`.

**Quick status check for "does X work":** Play/Change Gold works for exactly one case (a single
`page_1`, `on-click` trigger, numeric state change). Nothing beyond that shape has been built or
tested yet — don't assume multi-page, other triggers, dialogue, or entity AI work just because
Change Gold does.

**Key insight:** Everything is **sessions** now (not @.apps/). Games = sessions. Maps = desks. Events live in session folders.

---

## 🗄 DB-HQ MV LAYOUT FILL-OUT + EVENTS OPTIONS (queued 2026-08-28)

**Human:** fill out the db menu with **superficial RPG Maker MV copied layouts**
(not full CRUD). **Terms isn't quite right — do that first.** Leave **Common
Events** alone for now. If time: fill out the **events-hq Options** panel the
same way. Add this to handoff (this section).

### Why Terms looks wrong today

Terms is already a "real" tab (`DB_HQ_TERMS_TAB 14`, `dbhq_tab_is_real()`,
`terms_hq_manager.+x` → `#.desktop/db_hq_terms.state.txt`). The renderer
**reuses the Common Events path**: one-label-per-line sidebar via
`dbhq_load_common_events()` / `dbhq_inject_sidebar_items()`. MV Terms is
**not** a named list of events. It is a **workspace of labeled string fields**
(Basic Status, Parameters, Commands, Messages) — two columns of `label +
text field`, no left ID list like Actors.

So "isn't quite right" = **wrong layout family**, not a missing manager.

**Do not** keep stuffing Terms lines into `g_dbhq_events[]`. **Do not**
change Common Events injection, `khtpm_hq_manager.c`, or CE add-button
behavior.

### Target layout (copy MV, superficial)

Visual source of truth (this directory):

- `rpg-maker-database.html` — tab bar already matches
  Actors / Classes / Skills / Items / Weapons / Armors / Enemies / Troops /
  States / Animations / Tilesets / Common Events / System / Types / Terms.
  The HTML currently **only fleshes Actors**; Terms still needs a real MV
  Terms panel (not an Actors clone).
- `rpgmaker-mv-event-editor.html` — right-rail **Options** (Walking /
  Stepping / Direction Fix / Through), plus Priority + Trigger radios.
  That is the events-hq Options copy target.

**Terms (do first):** MV Database → Terms:

| Block | Typical fields (English MV defaults — copy labels even if values stub) |
|---|---|
| Basic Status | Level, HP, MP, TP, EXP |
| Parameters | Max HP, Max MP, Attack, Defense, M.Attack, M.Defense, Agility, Luck |
| Commands | Fight, Escape, Attack, Guard, Item, Skill, Equip, Status, Formation, Save, Game End, Options, Weapon, Armor, Key Item, Equip2, Optimize, Clear |
| Messages | Always Dash, Command Remember, BGM Volume, BGS Volume, ME Volume, SE Volume, Possession, Exp, Currency unit |

Superficial bar: **visible labeled fields + default strings**, state file
owned by `terms_hq_manager`. Editing/save is nice-if-cheap; **layout that
looks like MV** is the deliverable. Seed from
`&.widgits/db-hq/data/System.json` `terms` object if present.

**Other db tabs (after Terms, still superficial):** each tab should look
like MV's **sidebar ID list + settings workspace**, not `(coming soon)`
centered gray text (`dbhq_render_placeholder_tab`). Actors HTML mock is
the template (Name, Class, levels, Face/Character/Battler, equipment,
parameter bars). Stubs may be empty `0001:` rows. **Common Events tab:
do not touch.**

**Events-hq Options (if time):** copy `rpgmaker-mv-event-editor.html`
Options checkboxes (Walking, Stepping, Direction Fix, Through) onto the
live events-hq page chrome. Priority + Trigger radios if they aren't
already 1:1. Contents/command list is a different task — skip unless
trivial.

### Implementation constraints

- TPMOS: real manager + state file, **not** bash-`printf` chtpm
  (`TPMOS-COMPLIANCE-DEBT.md`). Terms manager already exists — change
  **what it publishes** and **how the renderer draws Terms**, not CE.
- `dbhq_tab_is_real()` is the one-line registry for "this tab has data."
  New real tabs add one `|| tab == …` there.
- Terms needs its **own** workspace renderer (form grid), not
  `dbhq_inject_sidebar_items`. Gate: `g_dbhq_current_tab == DB_HQ_TERMS_TAB`.
- `!.OPEN-2do-events-db-networking-2026-08-28.md` Task 2 asked for
  list-views via existing sidebar injection. **This queue supersedes that
  for Terms** (form, not list). Other tabs can still start as list+stub
  settings.
- Do not edit `khtpm_entity_menu_render.c` concurrently with another
  agent (that OPEN-2do hard boundary). If the file is live elsewhere,
  land manager/state/chtpm first and defer the Terms workspace draw.
- Verify per `_.0.aigent-testing-k9.txt`: relay +
  `db_hq_history.txt` / state dump first; PNG last.

### Out of scope this queue

- Common Events tab, CE runtime, Play, message-command bytecode.
- Full editable RPG database (CSV/JSON round-trip for every domain).
- Networking tab / pal-irc mirrors (OPEN-2do Task 3).

---

**⚠️ STANDING RULE — check before you invent:** Before adding any new CHTPM tag/attribute/state
shape, check (1) local chtpm usage elsewhere in this codebase, then (2) the **grandfather program,
tpmos**, at `1.TPMOS_c_+rmmp.0103.0001/` (sibling dir, one level up from `yz.muchiverse/`, same
NEST-11.17 tree) — CHTPM was ported FROM tpmos and tpmos is often more feature-complete (e.g. tpmos's
`<cli_io>` already supports multiple simultaneous named fields on one screen via `id`+`target_id`;
khtpm's manager-side state only ever tracks one singleton field today — see USER_CREATION.md for the
full writeup of this exact case). Only invent new shape if genuinely absent from both.

---

## 🪟 HOUSE WINDOW STANDARD (2026-08-13, do not regress)

Every popup/dialog/hq window in this house is an **X11 RGB window + CSS** (`khtpm_css_parser`) —
**NOT a GL window** (no freeglut/gl-canvas/GLX mirror; GL is only for `gl_mirror`-style RGB
mirrors in muchi-pals/egg_window/mutaclysm). A reminder popup was once built looking GL-ish and
the user pushed back: *"its still a gl window. just follow standard dont waste time."* The rule:

- Own detached process: `setsid nohup <bin> <house> <payload>` (button.sh / open_*.sh style).
- CSS from a `.css` file (e.g. `reminder.css`), font scale from `#.desktop/hq_ui.pdl`
  `font_scale=1.25`.
- WM-managed but **borderless**: `_MOTIF_WM_HINTS` `decorations=0` — NOT `override_redirect`
  (override_redirect exempts the window from Mutter's focus policy entirely; the 2026-08-12 fix in
  `khtpm_hq_render.c:1189-1229` is canonical). Also `WM_HINTS input=True`, `WM_DELETE_WINDOW`,
  class `MuchiverseLivedesk` (xwayland grab-access allowlist), `XMapRaised`.
- Own drawn chrome bar (`#2b2b2b` + DejaVu Sans bold scaled(10) + `[x]` close), Escape/click/`[x]`
  closes. Copy the helpers from `khtpm_hq_render.c` (`alloc_pixel`/`xft_color`/`font_for`/`scaled`
  `:594-630`, window setup `:1197-1251`) or `xyzfs/bin/livedesk-clock/ops/lc_reminder_popup.c`
  (built to match, 2026-08-13). Full note in `au11-hq/15.clock-design.md §6.2`.

**CORRECTION 2026-08-28 (khtpm_entity_menu_render family, live-verified):**
`XMapRaised` on a **WM-managed** HQ window (chat-hai) stole the human
browser via Mutter. **Override_redirect popups** (Settings / entity
menu) keep `XMapRaised` and do **not** `XSetInputFocus` on map.
**WM-managed HQ** (db-hq / events-hq / chat-hai) map with `XMapWindow`,
not `XMapRaised`. History poll must not require X focus. Source:
`HQ-WINDOW-MAP-AND-AGENT-INPUT.md` + `GROK-RENDER-INPUT-REFACTOR-HANDOFF.md`
(this directory). Caveat: Mutter may still activate a newly mapped
WM-managed window even after `XMapWindow`.

---

## 🎯 Project State

### What's Done
- Livedesk taskbar fully operational (HQ, USER, file, desks, pals, palettes, edit, player, db, plugins, menus, store, network, datetime)
- Entity window bugs fixed (arrow navigation guard, pointer grab conditional)
- Datetime cell publishing live with Chinese/English formatting
- Event-EZ visual editor exists and works (already deployed in production)
- User creation code exists (user-pal) but not wired to livedesk UI yet

### What's In Progress
- **User creation in livedesk** — currently missing from 2.USER cell UI
- **Common events per-session** — needs data structure + storage design
- **Event persistence testing** — verify Change Gold runs + survives session switches

### What's Next (Prioritized)
1. Create claude-0001 user account (test livedesk user flow)
2. Document livedesk user creation gap + plan fix
3. Build Change Gold test harness (event execution + persistence)
4. Document event system architecture
5. Design + build demo games (DSR, desk-civ, muchipal-desk)

---

## 📁 File & Folder Structure (New Standard)

### Session Storage (CRITICAL CHANGE)
```
<house_root>/sessions/<user_id>/<session_id>/
├── session.pdl          (config: desk list, entity spawns, common events)
├── desks/
│   ├── desk_1.pdl       (map 1 — entities, teleports, events)
│   ├── desk_2.pdl
│   └── ...
├── common_events.pdl    (per-session events, runs regardless of desk)
├── save_data/           (player state: gold, items, etc.)
└── harnesses/           (test + dev automation, agent-local copies)
```

### User (xyzfs) Storage
```
<house_root>/xyzfs/users/<uuid>/
├── home/
│   ├── projects/        (where user's games/sessions live)
│   ├── exchange/        (inter-user share)
│   └── net/             (networking)
├── meta.txt             (uuid, user_id, display_name, created_at)
└── harnesses/           (claude-0001/, claude-agent-review/, etc.)
```

### Why This Structure
- **Sessions = games:** Switch via 3.file menu (each session is a game instance)
- **Desks = maps:** Navigate via arrows/teleports within a session
- **xyzfs/users/<uuid>:** Multi-user isolation; agents don't clobber each other's test harnesses
- **common_events.pdl:** Events that run globally (not tied to specific entity)

---

## 🔄 Event System Architecture

### Current Event Storage (event.ir.pdl format)
```
SECTION      | KEY                | VALUE
META         | piece_id           | ava
STATE        | source             | event-ez
NODE         | id=1 type=show_text    | text=ava: hi!
NODE         | id=2 type=on_interact  | text=call ops/+x/chat_relay.+x ava
```

**Format breakdown:**
- SECTION header (3 cols: descriptor | key | value)
- NODE lines: `id=N type=<trigger>` + `text=<data>`
- SOURCE: "event-ez" (visual editor) or "blocks" (hand-coded)
- Triggers: `on_interact`, `on_click`, `show_text`, `on_spawn`, `parallel`, `command`

### Event Commands Reference (3 .txt files → need PDL conversion)
Located: `#.ref/menu/event.commands.{1,2,3}.txt`
- Part 1 (51 cmds): Message, Party, Game Progression, Actor, Flow Control
- Part 2 (48 cmds): Movement, Character, Timing, Screen, Audio & Video
- Part 3 (46 cmds): Scene Control, Map, Battle, System Settings, Advanced
- **TODO:** Reformat to `.pdl` with unique IDs before db-ez uses them

### Two Views (Like events-ez + events-mock)
- **events-ez:** Visual page editor (already working, used for entity events)
- **events-mock:** RPG Maker GUI (future, RPG Maker MZ style database)
- **db-ez:** Common events editor (simple CHTPM nav, START HERE)
- **db-mock:** Database GUI (future, items/skills/actors/states)

### Event Execution Flow
```
1. User creates event in event-ez (visual editor)
   ↓
2. Saved to @.apps/<entity>/event_pkg/event.ir.pdl
   ↓
3. Compiled to event.pal (runnable script)
   ↓
4. On "Play" or entity spawn: player widget loads .pal + executes nodes
   ↓
5. Changes apply to game state (gold, items, switches, etc.)
```

**For common events:** Same flow, but loaded from `sessions/<id>/common_events.pdl` instead.

---

## 🐛 Known Issues & Gaps

> **CORRECTED (2026-08-29 doc-audit pass):** item 1 below (User Creation
> Missing) is stale and contradicts this doc's own TL;DR above ("USER
> cell has real account creation (relay-verified)"). Left here struck
> through rather than deleted, since the rest of this section (2, 3) is
> still real and unresolved.

### ~~1. User Creation Missing in Livedesk UI~~ — RESOLVED, see TL;DR above
- ~~**Status:** Can create users via CLI (`userpal_create_account.+x <id> <name>`)~~
- ~~**Gap:** No "New User" button in livedesk 2.USER cell~~
- ~~**Solution:** Wire userpal_create_account function into ktb_hq_open() for which==2 (USER cell)~~
- ~~**File:** `khtpm_taskbar_manager.c` (ktb_hq_open switch statement, add case for new user)~~

### 2. Event Persistence Test Needed
- **Status:** Change Gold command exists in event-ez
- **Gap:** Unknown if it survives desk switches or session reloads
- **Test plan:** Create simple event → run → switch desk/session → come back → verify state persists
- **File:** Need harness at `xyzfs/users/claude-0001/harnesses/test_change_gold.sh`

### 3. Menus Cell (cell 11) Needs Standards
- **Status:** Button added to layout, not functional
- **Gap:** Need pal plugin system to register custom menus
- **Decision:** Defer until pal plugin architecture designed
- **Reference:** Would scan `#.desktop/user_menus/` for .pdl definitions

---

## 🎮 Demo Games (Our Testbed)

### Why Building Games
These serve three purposes:
1. **Test the event system end-to-end** (common events, entity events, state persistence)
2. **Demonstrate session/desk/event architecture to users**
3. **Build actual playable experiences** (not just infrastructure)

### Game List (Priority Order)

#### 1. **desk-shop** (START HERE — simplest)
- One desk (map)
- Static NPCs with dialogue (on_interact events)
- Shop UI: buy/sell items
- Uses: on_interact trigger, common events for gold/item updates
- Goal: Prove Change Gold + inventory changes work

#### 2. **desk-civ** (SNES Civ style)
- Multiple desks (regions)
- Teleport between them
- Settlers, resource gathering
- Turn-based progression
- Uses: parallel events (passive resource gen), on_spawn triggers

#### 3. **muchipal-desk** (Pokemon-like, uses full RPG Maker features)
- Multiple desks (wild areas, towns)
- Monster catching/leveling
- RPG Maker battles (full event command set)
- Persistent team
- Uses: all event types, complex state management

#### 4. **dsr** (Desk Street Raider — business sim)
- Single or multi-desk
- Shops, NPCs, trading
- Economy simulation
- Uses: complex common events, dynamic NPC behavior

---

## 🔧 Testing & Harnesses (Relay-Based, No Cheating)

### Testing Philosophy: Inject, Not Direct CLI
**Critical:** All testing must go through **relay/inject**, not direct CLI or function calls.
- Why: Testing should match user behavior (keyboard input, UI navigation)
- How: Use existing harness at `#.desktop/harnesses/khtpm-livedesk-taskbar/nav.sh`
- Result: Same code path real users use = confidence testing matches production

### Relay System (livedesk_agent_relay.txt)
```
Agent sends:           Parser receives:
Digits → ASCII codes   nav.sh digits "123" → 49 50 51 each on own line
Keys → ASCII codes     nav.sh key Return → 13
Text → char codes      nav.sh type "alice" → char-by-char
                       nav.sh frame → read current state
```

### Master Harness Pattern
All harnesses follow this shape:
```bash
#!/bin/bash
# <what this tests>
HOUSE="${HOUSE:-$PWD}"
NAV="$HOUSE/#.desktop/harnesses/khtpm-livedesk-taskbar/nav.sh"

# Setup: screenshot before
$NAV frame > before.txt

# Test sequence: use relay commands
$NAV nav 2              # go to cell 2 (USER)
$NAV key Return         # activate cell
$NAV nav 1              # select "New User" option (row 1)
$NAV key Return         # confirm
$NAV type "testuser"    # type username
$NAV key Return         # confirm

# Verify: check frame + filesystem
$NAV frame > after.txt
if grep -q "testuser" users/*/profile.txt; then
  echo "PASS: User created"
else
  echo "FAIL: User not found"
fi
```

### Harness Storage (Per Developer)
```
xyzfs/users/claude-0001/harnesses/
├── test_user_creation.sh        (relay-based user signup + verify files)
├── test_change_gold.sh          (relay-based event creation + execution)
├── test_session_switch.sh       (game state survives desk/session switches)
├── test_desk_shop.sh            (play desk-shop game via relay)
└── README.md                    (index + how to run)
```

### Harness Output Format (PM-Scannable)
Each harness generates:
```
results/
├── frame_history.txt           (frame state before/after each step)
├── summary.md                  (PASS/FAIL summary + key evidence)
├── filesystem_proof.txt        (ls -la of created files)
└── relay_log.txt               (all relay codes sent)
```

### Running Harnesses as Agent
```bash
# As claude-0001 (in proper xyzfs context):
cd xyzfs/users/claude-0001
bash harnesses/test_user_creation.sh
# → generates results/ folder with relay + filesystem proof
```

### Why No Direct CLI
- **Direct CLI example (WRONG):** `userpal_create_account.+x testuser "Test User"`
- **Relay example (RIGHT):** `nav.sh nav 2` → activate USER cell → relay creates account
- **Difference:** Relay tests the UI path, CLI tests the binary in isolation
- **Problem with direct CLI:** UI bug could hide while CLI works, creating false confidence

---

## 📝 Documentation Pattern

### Per-Game Documentation
Each demo game folder contains:
```
desk-shop/
├── DESIGN.md           (game concept, story, mechanics)
├── EVENTS.md           (what events this game uses + how)
├── TEST_CASES.md       (manual + automated test scenarios)
├── session.pdl         (actual session config)
└── desks/
    └── shop.pdl        (map/desk definition)
```

### Architecture Documents
- **HANDOFF.md** (this file) — high-level overview, what's done/next
- **EVENTS.md** (in au11-hq/) — deep dive on event system
- **SESSIONS.md** (in au11-hq/) — storage & session lifecycle
- **USER_CREATION.md** (in au11-hq/) — how accounts work, gaps, fix plan

---

## 🚀 Immediate Next Steps (Priority Order)

### Step 1: Livedesk Test Drive (claude-0001)
1. [ ] Create `claude-0001` account via CLI: `userpal_create_account.+x claude-0001 "Agent Claude 0001"`
2. [ ] Log in via livedesk 2.USER tab
3. [ ] Create first session (via 3.file → new)
4. [ ] Document user creation gap observed in livedesk UI
5. [ ] Create session directory structure proof
6. [ ] Screenshot: livedesk working with claude-0001 account

### Step 2: Change Gold Test Harness
1. [ ] Locate existing Change Gold event in event-ez
2. [ ] Build test script to: create event → execute → check gold value → switch session → verify gold persists
3. [ ] Document event execution flow (what files change)
4. [ ] Store harness at `xyzfs/users/claude-0001/harnesses/test_change_gold.sh`

### Step 3: Event System Documentation
1. [ ] Document user-pal create flow (already researched — in au11-hq/USER_CREATION.md)
2. [ ] Design common_events.pdl format
3. [ ] Map event execution code path (event.ir.pdl → event.pal → runtime)

### Step 4: Desk-Shop Design (Research Phase)
1. [ ] Research event commands needed for shop (Change Gold, Change Items, Show Text, Conditionals)
2. [ ] Document game flow as series of events
3. [ ] Plan session/desk structure
4. [ ] THEN implement (will hand to agent for actual coding)

---

## 💬 Questions Agents Should Ask

If research blocks you, ask:
- "Where should I look for [component X code]?"
- "Is this architectural decision correct, or should I pivot?"
- "Do I have permission to modify [file Y]?"
- "Should I create [new harness/doc]?"

**Don't guess.** Document your question + dead-end in au11-hq/ so other agents learn.

---

## 📚 Key Files to Know

### User Management
- **User creation code:** `0.user-pal👤️/00.login-signup/ops/userpal_create_account.c`
- **User login code:** `0.user-pal👤️/00.login-signup/ops/userpal_login.c`
- **Livedesk user cell:** `khtpm_taskbar_manager.c` (function `ktb_strip_user_activate()`)

### Livedesk Taskbar
- **Layout:** `khtpm_strip_header.chtpm` (button definitions + layout)
- **Manager logic:** `khtpm_taskbar_manager.c` (menu dispatch, cell handling)
- **Parser:** `khtpm_strip_parser.c` (rendering, variable substitution)
- **Config:** `livedesk_taskbar.pdl` (settings like datetime_lang)

### Events
- **Event-EZ visual editor:** `&.widgits/event-ez/` (production code + harnesses)
- **Event commands reference:** `#.ref/menu/event.commands.{1,2,3}.txt`
- **Example entity events:** `@.apps/asa-&-ava/pieces/ava/event_pkg/event.ir.pdl`

### Session Storage
- **Sessions dir:** `#.desktop/sessions/<id>/` (or per-user xyzfs once migrated)
- **Current user:** `#.desktop/current_login.txt`
- **User profiles:** `users/<user_id>/profile.txt`

---

## 🔍 How to Read This Document

**For quick status:** Read TL;DR + Project State sections.  
**For architecture:** Read File Structure + Event System Architecture.  
**For implementing feature X:** Search for X in "Known Issues & Gaps" or "Key Files to Know".  
**For testing:** See Testing & Harnesses section.  
**For building games:** See Demo Games + Documentation Pattern sections.

---

## Version History

- **2026-08-11:** Initial handoff doc created by claude-0001. User creation research documented, event system overview added, demo game list defined.

---

**Last Updated:** 2026-08-11  
**Next Agent:** Review this doc first. Ask questions in au11-hq/ if blocked.
