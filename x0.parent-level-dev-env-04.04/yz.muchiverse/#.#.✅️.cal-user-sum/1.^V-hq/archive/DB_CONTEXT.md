# 📊 What is "DB" and How Does Your Work Fit In?

**For:** Haiku agents who completed H1 (Show Text + Show Choices) and want to understand the bigger picture.

---

## The Quick Version

**"DB" = Database cell** in livedesk (cell #9, the taskbar header).

It's where players/developers manage **session-level game data:**
- **Common Events** — events that run globally for a session (not tied to one entity)
- **Items, State, Switches** — future additions (not built yet)

**Your H1 work (Show Text + Show Choices) powers dialogue in common events.**

---

## The Bigger Picture

### Game Architecture (Sessions → Desks → Entities)

```
Session (s1, s2, etc.)
├── Desks (maps: shop_1, forest_2, etc.)
│   └── Entities (NPCs, objects: shopkeeper, door, treasure_chest)
│       └── Entity Events (entity-specific: "shopkeeper sells stuff")
└── Common Events ← YOUR H1 WORK GOES HERE
    └── Session-wide events (any entity can trigger these)
```

### Two Types of Events

1. **Entity Events** (per-NPC or object)
   - Stored: `@.apps/<entity>/event_pkg/`
   - Example: "When shopkeeper is clicked, open shop UI"
   - Trigger: right-click entity → "Events (ez)" menu

2. **Common Events** (per-session, shared)
   - Stored: `sessions/<id>/common_events/`
   - Example: "When any NPC is talked to, play dialogue system"
   - Trigger: livedesk → db cell → "Common Events" menu ← **You helped wire this**
   - **Can use Show Text + Show Choices** (H1) to build dialogue trees

---

## What You Built (H1)

Show Text and Show Choices are **event commands** — actions that run when an event plays.

### Before H1
Only one command worked: **Change Gold** (adds/removes gold from player)
- Proved the event runtime works
- But events were limited to state changes only

### After H1 (What You Just Did)
**Show Text** (display message) + **Show Choices** (dialogue menu) both work
- Events can now have **dialogue flows** ✓
- Example flow:
  ```
  NPC: "What's your name?" [Show Text]
  → Player picks: "Alice" / "Bob" [Show Choices]
  → NPC: "Welcome, [Alice/Bob]!" [Show Text with stored choice]
  ```

### Why This Matters for "DB"
- **Before:** Common events could only change game state (boring)
- **After:** Common events can **talk to the player** (interactive, fun)
- **Demo game impact:** desk-shop NPC dialogue now works

---

## The DB Cell Flow (What You Enabled)

```
1. User opens livedesk
2. Clicks "db" header cell (#9)
3. Menu appears: "Common Events"
4. User selects "Common Events"
5. event-ez launches pointing at sessions/<id>/common_events/
6. User authors events using:
   - Change Gold (command)
   - Show Text (command) ← YOU BUILT THIS
   - Show Choices (command) ← YOU BUILT THIS
   - [Future: Items, conditional branches, etc.]
7. Event saved and runs in game when triggered
```

---

## What's Still Missing for "Full DB"

The db cell *could* eventually have more menu rows:

```
"Common Events"      ← working now (thanks to you + earlier work)
"Items"             ← not built yet (would edit item database)
"State Variables"   ← not built yet (define custom game variables)
"Switches"          ← not built yet (define boolean flags)
"Actors / Enemies"  ← not built yet (define game units)
```

But **common events alone is enough for demo games** (desk-shop doesn't need items database yet).

---

## How Your Work Flows Into Demo Games

### desk-shop (Next Demo Game After H2)

```
Shopkeeper NPC
  ↓
  Entity event triggered (on_click)
  ↓
  Common event runs: "Show shop dialogue"
    ↓
    NPC: "Buy or sell?" [Show Text]
    → Player chooses [Show Choices: Buy/Sell]
    → [Future: Show Items, process transaction]
```

**Your H1 makes the dialogue part work.** ✓

---

## Next Steps (Not Your Problem)

After H1 is done and verified:
- **H2 (Palette UI):** Let users pick game assets instead of hardcoding
- **H3 (Event Tests):** Verify multi-trigger events work via event-ez authoring
- **H4 (PDL Conversion):** Organize 145 event commands into structured format
- **H5 (desk-shop):** Build the first playable demo game (uses your H1 + H2)

---

## TL;DR

You built **Show Text** and **Show Choices** event commands.

They let game designers **create dialogue in the event editor**.

The db cell is **where you manage session-level events** (like dialogue trees).

Your work made dialogue-driven gameplay possible. Cool! 🎮

---

## Troubleshooting: "DB Cell Click Did Nothing"

**Issue:** User clicked the db cell in livedesk, but nothing opened. Expected: menu with "Common Events" option.

### Debug Answers

1. **Is livedesk running?** 
   - YES — the db cell wiring is already complete (done 2026-08-12)
   - Check: `ps aux | grep khtpm` should show khtpm_strip_parser and khtpm_taskbar_manager running
   - If not running, restart: `cd *.monads/*.livedesk-taskbar/ops && sh run_khtpm_strip.sh new`

2. **What happens when clicking db?**
   - **Expected:** Menu appears with one row: "Common Events"
   - **If nothing:** Check if there's an active session (livedesk_root_read() needs active session in session.pdl)
     - Verify: `cat xyzfs/users/<uuid>/home/livedesk/sessions/session.pdl` should show `active_session | <id>`
     - If empty: Create a session first via "file" cell → "new session"
   - **If menu appears:** Select "Common Events" → event-ez should launch in ~2 seconds
     - Check: `pgrep -f 'event-ez|button.sh'` should show process

3. **What is the user trying to do?**
   - **Test that H1 (Show Text + Show Choices) works in Common Events?** → See section below "Testing H1 Work Via Relay"
   - **Verify the db cell wiring?** → Click db cell, should get menu (see #2 above)
   - **Move on to H2 (Palette Picker)?** → H2 is independent, start whenever ready
   - **Something else?** → Describe what you're trying and check EVENTS_RUNTIME.md for event execution details

### Testing H1 Work Via Relay (Verification for Haiku)

To verify your Show Text + Show Choices commands work correctly, test via relay without waiting for full db cell GUI:

1. **Create a test entity** with multi-page event:
   - Page 1 (trigger=on_click): Show Text → "What's your name?"
   - Page 2 (trigger=on_interact): Show Choices → "Alice / Bob / Carol"
   - Page 3 (trigger=on_click): Show Text → "You picked: [choice]"

2. **Save event via event-ez GUI:**
   - Use your new Show Text layout (`event_ez_page_2_cmd_show_text.chtpm`)
   - Use your new Show Choices layout (`event_ez_page_2_cmd_show_choices.chtpm`)
   - Save event via event-ez (File → Save or equivalent)

3. **Trigger via relay:**
   - Right-click entity → "Play" (executes on_click trigger, should run page 1 + 3)
   - Check: Do messages appear in game UI or logs?
   - Expected: "What's your name?" then "You picked: [selection]"

4. **Verify output:**
   - Check entity's event_pkg directory for compiled event.pal or cmd_N.sh logs
   - Look for SHOW_TEXT and SHOW_CHOICES entries in execution log
   - If using terminal output, check `/tmp/event-*` logs

5. **What success looks like:**
   - Page 1 (Show Text) runs and displays message
   - Page 2 (Show Choices) runs, displays options, lets player select
   - Selection is stored in game state
   - Page 3 (Show Text) runs and shows the stored choice
   - No crashes or silent failures

### If H1 Verification Fails

1. **Show Text doesn't display?** → Check show_text_relay.+x implementation, verify it's in `xyzfs/bin/muchi-pet/ops/`
2. **Show Choices doesn't let you pick?** → Check show_choices_relay.+x, verify it handles input and stores result
3. **ez_menu_input.c doesn't recognize commands?** → Verify you updated the switch statement to handle both command types
4. **Event doesn't compile?** → Check ez_compose_frame.c, ensure new layouts are referenced correctly

**When in doubt:** Add a `test_h1_debug.sh` relay harness that traces each step and logs output. Simpler than guessing!

---

**Questions?** Check EVENTS_RUNTIME.md for technical details on how events compile and run.
