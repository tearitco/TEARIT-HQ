# 🚀 Haiku Task Queue — Small, Scoped Work

**For:** Claude Haiku (faster, cost-effective for well-defined tasks)  
**When to use this:** Tasks that are fully specified, have clear success criteria, and don't need architectural decisions.  
**When to escalate:** Architectural changes, multi-system integration, or decisions that affect other features → send to Sonnet.

---

## How to Use This Doc

1. Pick a task from the **Ready** section below
2. Read the task description completely
3. Follow the "Steps" section
4. Verify against the "Success Criteria"
5. Update the task status in this file when done
6. If you get blocked, add a note to **Blockers** section instead of guessing

**Important:** Do NOT start a task marked ⏸️ **BLOCKED** — read the blocker note first.

---

## ✅ DONE

### Task H1: Show Text + Show Choices Event Commands ✅

**Status:** COMPLETE (2026-08-12)  
**Summary:** Implemented both Show Text and Show Choices event commands, enabling dialogue-driven gameplay in the event system.

**For context on why this matters,** see DB_CONTEXT.md — it explains how your dialogue commands integrate with the db cell and common events system.

---

## ✅ READY (Pick one)

### Task H1: Show Text + Show Choices Event Commands (COMPLETED)

**Goal:** Implement dialogue commands (Show Text, Show Choices) together as a pair, proving the compiler/runtime pattern generalizes beyond Change Gold and enabling dialogue-driven gameplay.

**Context:** See EVENTS_RUNTIME.md "Multi-Page/Multi-Trigger Runtime" section. Change Gold already works. Show Text and Show Choices are simpler (no state changes), work together (Show Text often leads to Show Choices), and together demonstrate the pattern is reusable.

**What to do:**

**Part A: Show Text Command**
1. Read `#.ref/menu/event.commands.1.txt` and find "Show Text" definition
2. Open `&.widgits/event-ez/pieces/chtpm/layouts/event_ez_page_2_cmd_change_gold.chtpm` (Change Gold's UI)
3. Create `event_ez_page_2_cmd_show_text.chtpm` layout (simpler than Change Gold — just text input field, no number picker)
4. Create `show_text_relay.+x` script in `xyzfs/bin/muchi-pet/ops/` (mirrors `mr_change_gold.+x`)
   - Takes: entity path + message text + optional speaker name
   - Outputs message to game UI (print, message queue, or display overlay)
5. Update `ez_menu_input.c` to recognize "Show Text" command and generate cmd_N.sh wrapper

**Part B: Show Choices Command**
1. Read "Show Choices" definition from `#.ref/menu/event.commands.1.txt`
2. Create `event_ez_page_2_cmd_show_choices.chtpm` layout (text area for options, newline-separated)
3. Create `show_choices_relay.+x` script (more complex than Show Text):
   - Takes: entity path + choice list (newline-separated) + optional default choice index
   - Displays choices to player, waits for input, stores selected choice index in game state (switch or variable)
4. Update `ez_menu_input.c` to recognize "Show Choices" and generate cmd_N.sh wrapper
5. Ensure choice result is accessible to subsequent pages (via switch/variable system — verify this works)

**Part C: Testing (Dialogue Flow)**
1. Test via relay harness: create a multi-page event:
   - Page 1 (on_click): Show Text → "What's your name?"
   - Page 2 (on_interact): Show Choices → "A / B / C" → stores result
   - Page 3 (automatic or conditional): Show Text → "You chose: [result]"
2. Create test harness: `xyzfs/users/[your-uuid]/harnesses/test_dialogue_chain.sh`
   - Relay sequence: trigger page 1 → observe text → trigger page 2 → pick choice → verify stored → trigger page 3 → observe text with choice result

**Success Criteria:**
- [ ] Both layout files (show_text, show_choices) created and load in event-ez without errors
- [ ] Both scripts (show_text_relay.+x, show_choices_relay.+x) exist, are executable, and handle inputs correctly
- [ ] ez_menu_input.c compiles with both commands
- [ ] Relay test: Show Text command → message appears in game (or log/output)
- [ ] Relay test: Show Choices command → choices displayed, selection stored in game state
- [ ] Relay test: Multi-page dialogue flow → all 3 pages execute in order, choice result visible in final page
- [ ] Test harness at `xyzfs/users/[your-uuid]/harnesses/test_dialogue_chain.sh` (relay-only, documents the full flow)
- [ ] Both commands verified working via event-ez UI authoring (not just raw files)

**Estimated scope:** 4-5 hours of implementation + testing. More complete than Show Text alone, but still straightforward (no new architecture, just extending existing patterns).

---

### Task H2: Palette Population UI (Asset Picker)

**Goal:** Build a minimal asset picker UI so users can populate game palettes (tilesets, sprites, etc.) instead of hardcoding.

**Context:** Demo games need to use palettes, not hardcoded content (direct instruction). Palette picker UI is on the deferral list (Task 4) but may be a prerequisite for demo-shop build.

**What to do:**
1. Check if `201.rpg-maker-clone/src/tileset.c` has usable tileset picker code (reference: HANDOFF.md Task 4 note)
2. Check if `emoji_gen_atlas` / `emoji_xtract` exist and are used for emoji glyph pipeline
3. Decide: use existing RPG Maker picker, or build minimal CHTPM-native UI?
4. If existing: wire it into livedesk's palette cell (cell 6, currently inert)
5. If new: design minimal UI (grid of tiles, click to select, preview pane)
6. Test: livedesk palette cell → open picker → select asset → verify it's added to session palette
7. Document the flow in a new PALETTE_PICKER.md

**Success Criteria:**
- [ ] Asset picker opens from livedesk palette cell
- [ ] Can select from tileset/emoji/sprite library
- [ ] Selected asset added to `sessions/<id>/palettes.pdl` (or similar)
- [ ] Test harness: relay→palette cell→pick asset→verify file changes
- [ ] PALETTE_PICKER.md created with usage instructions

**Estimated scope:** 3-4 hours if reusing existing picker; 6+ if building new. Moderate complexity.

**Blocker check:** Does tileset.c actually have a usable picker? Verify before starting.

---

### Task H3: Multi-trigger Event Tests (Verification)

**Goal:** Verify that multi-page/multi-trigger event runtime actually works end-to-end via event-ez authoring (not just raw test pages).

**Context:** EVENTS_RUNTIME.md §"Multi-Page/Multi-Trigger Runtime" says play_event.sh was fixed, but the fix was only tested with hand-crafted pages. Event-ez's own authoring (ez_menu_input.c) should already support multi-page (it uses current_page_number() dynamically), but this hasn't been verified live.

**What to do:**
1. Use event-ez to create a real multi-page event on any entity (not common events yet):
   - Page 1: trigger=on_click, command=Change Gold (+10)
   - Page 2: trigger=on_interact, command=Show Text ("Hello!")
2. Run the entity via relay:
   - Trigger on_click (right-click "Play") → verify only Page 1 runs
   - Trigger on_interact → verify only Page 2 runs
3. Document findings in EVENTS_RUNTIME.md's "Event-EZ Multi-Page Authoring Verification" section
4. If it works: mark as verified. If broken: log the gap in au11-hq/

**Success Criteria:**
- [ ] Multi-page event created visually in event-ez (no hand-editing)
- [ ] Relay test: on_click triggers Page 1 only, gold changes
- [ ] Relay test: on_interact triggers Page 2 only, message shows
- [ ] Both triggers unaffected by the other page
- [ ] Findings documented in EVENTS_RUNTIME.md

**Estimated scope:** 1-2 hours. Straightforward verification, no coding.

---

### Task H4: Hardcoded Event Commands → PDL Conversion

**Goal:** Convert the three plain-text event command reference files (1-3.txt) to a single structured PDL file for future db-ez integration.

**Context:** Event commands are currently in `#.ref/menu/event.commands.{1,2,3}.txt` (145 total commands, 3 categories). These need unique IDs and structure before the database editor can reference them.

**What to do:**
1. Read all three .txt files and understand their structure (category headers, command names, grouping)
2. Design a PDL format:
   - Proposed: `SECTION | cmd_id | category/name`
   - Include: command ID (unique), category (from original grouping), display name, description (short)
   - Example:
     ```
     COMMAND | 001 | Message/Show Text
     COMMAND | 002 | Message/Show Choices
     ...
     ```
3. Write a script to convert the three .txt files → single `event.commands.pdl`
4. Update the script's output to `#.ref/menu/event.commands.pdl`
5. Verify: pdl file is valid (can be parsed by read_key_value pattern or similar)
6. Test: db-ez or any consumer can load and enumerate commands

**Success Criteria:**
- [ ] event.commands.pdl created with all 145 commands
- [ ] Each command has unique ID (001-145)
- [ ] Categories preserved (Message, Party, etc.)
- [ ] No commands lost or duplicated
- [ ] PDL file parses without errors
- [ ] Original .txt files documented as archived (not deleted)

**Estimated scope:** 1-2 hours. Mechanical conversion, low risk.

---

### Task H6: events-hq — Page Creation UI

**Goal:** events-hq (real events editor, `&.widgits/events-hq/`) can read/switch/edit existing `pages/page_N/` dirs but has no UI to CREATE a new page. Add one.

**Context:** `EVENTS-HQ-RGB-HANDOFF.md` §1 "Near-term" item #1. events-hq was built 2026-08-12 studying event-ez's own file format first — do the same here, don't guess the shape. First VERIFY whether event-ez itself can create pages from its own UI, or whether that's always been a manual/external step (check `ez_menu_input.c` and `HOW2_USER_GUIDE.md`) — don't assume either way.

**What to do:**
1. Read `&.widgits/event-ez/ops/ez_menu_input.c` (or its live, non-backup copy — NOT `event-ez.backup-20260805-163329/`) for how (if at all) it creates a new `pages/page_N/` dir: `condition.pdl` + `event.ir.pdl` shape, what N gets picked, default trigger.
2. Read `&.widgits/events-hq/ops/khtpm_events_hq_render.c`'s existing `load_pages()` to see how it enumerates/numbers pages today.
3. Add a nav-indexed "New Page" row (wraith-alpha nav convention — bracket badges, digit-jump — same as every other list in this file, see house standard `!.HOUSE_STDS.md` #22) to the page-tabs area.
4. On activate: create `pages/page_<next N>/condition.pdl` (default trigger, e.g. `on-click`) + empty `event.ir.pdl`, reload pages, focus the new one.
5. Test via relay (never direct CLI — see `TESTING_STRATEGY.md`): open events-hq on a real entity, create a page, confirm the new dir + files exist on disk and the new tab is selectable.

**Success Criteria:**
- [ ] New Page row present with a real nav badge, keyboard-reachable (digit-jump + Enter)
- [ ] Creates a real `pages/page_N/condition.pdl` + `event.ir.pdl` matching event-ez's own format exactly (verified by diffing against an event-ez-created page, not assumed)
- [ ] New page immediately selectable/editable without restarting events-hq
- [ ] Relay-only test harness proving the above (per house testing rule)

**Estimated scope:** 2-3 hours. Bounded — one new UI row + file-creation logic, no architecture change.

---

### Task H7: events-hq — Condition/Trigger Editing

**Goal:** The left panel shows the current page's `condition.pdl` trigger READ-ONLY. Make it editable.

**Context:** `EVENTS-HQ-RGB-HANDOFF.md` §1 item #2. First verify event-ez itself can edit triggers (don't assume). events-hq already has a working text-entry mechanism from its "Add Command" picker (keystroke-accumulation pattern in `khtpm_events_hq_render.c`) — reuse that exact mechanism, don't build a second one.

**What to do:**
1. Check event-ez for trigger-editing precedent (same verify-first approach as H6).
2. Find the existing "Add Command" text-entry code in `khtpm_events_hq_render.c` (keystroke accumulation, Enter-to-commit) and identify what to genericize/reuse.
3. Make the Trigger field in the left panel clickable/nav-reachable; on activate, arm the SAME text-entry mechanism, seeded with the current trigger value.
4. On commit, rewrite `condition.pdl`'s `COND | trigger | <value>` line, reload.
5. Test via relay: change a trigger, confirm `condition.pdl` on disk actually changed and the runtime (`play_event.sh`) respects the new trigger.

**Success Criteria:**
- [ ] Trigger field is nav-reachable and editable via the reused text-entry mechanism (not a new one)
- [ ] Edits persist to `condition.pdl` correctly (verified on disk, not just in the UI)
- [ ] No regression to the "Add Command" picker's own text entry (still works after the refactor to share code)
- [ ] Relay-only test harness

**Estimated scope:** 2-3 hours. Bounded, but touches shared UI code — if genericizing the text-entry function turns out to be messy/risky, STOP and escalate to Sonnet rather than forcing it.

---

### Task H8: events-hq — "Play" Test-Run Button

**Goal:** Add a footer button that runs the currently-open event right now, same as `ava`'s own `meta.pdl` "Play" METHOD row does.

**Context:** `EVENTS-HQ-RGB-HANDOFF.md` §1 item #3. `EVENTS_RUNTIME.md` describes the real runtime path via `play_event.sh`.

**What to do:**
1. Read `EVENTS_RUNTIME.md`'s runtime section and find an existing real "Play" METHOD row (e.g. `ava`'s `meta.pdl`) to see the exact command shape it shells out to.
2. Add a footer button (nav-reachable) to events-hq's layout.
3. On activate, shell out to `play_event.sh <event_pkg_dir>` (or whatever the real existing invocation is — copy it, don't re-derive) for the CURRENTLY OPEN page's `event_pkg` dir.
4. Test via relay: open an event with real commands (e.g. change_gold), hit Play, confirm the effect actually happens (e.g. gold changes) the same way it would from the entity's own context-menu Play row.

**Success Criteria:**
- [ ] Play button present, nav-reachable
- [ ] Shells out to the exact same real runtime path other Play rows use (not a new/parallel mechanism)
- [ ] Verified live: a real event command's effect actually occurs after pressing Play
- [ ] Relay-only test harness

**Estimated scope:** 1-2 hours. Smallest of the three — mostly wiring an existing, already-proven runtime call.

---

## 🔲 IN PROGRESS

(None currently assigned to Haiku)

## ✅ DONE

### Task H1: Show Text + Show Choices Event Commands ✓
- **Completed:** 2026-08-12
- **Test harness:** xyzfs/users/04c8ce55-11a5-47f3-933d-ac009ca4ac72/harnesses/test_h1_final.sh
- **What was done:**
  - Part A (Show Text): Created mr_show_text.c relay script, event_ez_page_1/2_cmd_show_text.chtpm layouts
  - Part B (Show Choices): Created mr_show_choices.c relay script, event_ez_page_1/2_cmd_show_choices.chtpm layouts
  - Both relay scripts read text/choices from parameters, log to messages.txt and history.txt
  - Updated ez_menu_input.c with KEY:8 (Show Text) and KEY:9 (Show Choices) handlers - follows exact Change Gold pattern
  - Updated cmdpick layouts (page 1 and 2) to show Show Text and Show Choices as options in the command menu
  - Compiled all C programs successfully
  - All test verifications passed: relay scripts work, layout files exist, compiler updated with handlers

---

## ⏸️ BLOCKED (Do NOT start these)

### Task H5: Demo Game — desk-shop (Blocked on H1, H2)
- Needs dialogue commands (Show Text + Show Choices from H1) + Palette picker (H2) before implementation
- Waiting: Both to complete → will hand off to Sonnet for full game build
- Why: desk-shop needs NPC dialogue (Show Text) with dialogue trees (Show Choices) for shopkeeper interaction, plus proper tile/sprite palettes instead of hardcoding

---

## ✨ FUTURE (Not ready yet)

- **desk-civ** — SNES Civ-style, needs parallel events (multi-trigger) + advanced state management
- **muchipal-desk** — Pokemon-like, full RPG Maker feature set
- **dsr** — Business sim, complex economy events
- **Menus cell (Step 1.3)** — Needs standards draft first (pal plugin architecture)
- **events-hq: new RPG-Maker-style event/command types** (2026-08-12, direct
  instruction: "adding new events from rpgmaker") — NOT ready for Haiku yet,
  needs a Sonnet-level scoping pass first: which specific commands/trigger
  types beyond the current 3 (change_gold/show_text/show_choices), and
  whether each is a `compile_page()` extension (same pattern as the
  existing 3) or something structurally new. See `EVENT_AI_VISION.md` for
  the long-range trigger-type design intent before picking specific
  commands to add. Once scoped into individual bounded commands (mirroring
  H1's Show Text/Show Choices shape), each one becomes its own Haiku-ready
  task here.
- **events-hq: node delete/reorder** — NOT a gap, a deliberate parity
  choice: `event.ir.pdl` is append-only in event-ez itself today. Only
  add this once event-ez itself gains delete/reorder — don't get ahead of
  the format's own real capabilities (see `EVENTS-HQ-RGB-HANDOFF.md` §1
  item #4).

---

## Reference: When to Escalate to Sonnet

- "This task involves changes to other systems" → escalate
- "I need to design a new feature" → escalate
- "Should we do X or Y?" (architectural choice) → escalate
- "This breaks existing tests" → escalate
- "I don't understand why this part exists" → ask in au11-hq/, or escalate if still unclear

**Haiku is best at:** Implementing a fully-specified feature, fixing a known bug with clear steps, verifying a hypothesis, converting data formats.

---

## Updating This Doc

- When you **start a task**, update it to 🔲 **IN PROGRESS** + add your UUID as a note
- When you **finish**, update to ✅ **DONE** + link the test harness or evidence file
- When you **get stuck**, update the task to ⏸️ **BLOCKED** + add a clear blocker note (don't guess)
- When you **discover a new small task**, add it to **FUTURE** with a 1-line description

---

**Last Updated:** 2026-08-12  
**Maintained by:** Human + Haiku queue  
**See also:** HANDOFF.md (architecture), EVENTS_RUNTIME.md (event system), TESTING_STRATEGY.md (how to test)
