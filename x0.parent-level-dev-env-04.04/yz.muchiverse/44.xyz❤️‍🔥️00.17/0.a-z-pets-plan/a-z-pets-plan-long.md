# A-Z Pets Plan: Complete Vision & Implementation Roadmap

**Created**: 2026-07-26  
**Status**: Planning Phase  
**Vision**: Drag-and-drop pet ecosystem with GL windows, mutaclsym-style maps, PAL-driven event editing, and blockchain integration

---

## Executive Summary

This plan covers the full lifecycle of the "Muchiverse" pet ecosystem:
1. **01.muchi-pals**: Desktop pets (GL windows that move autonomously)
2. **002.zoo**: Mutaclsym-style map where pets become living entities
3. **0.user-pal**: Player avatar creation and customization
4. **045.muchi-pal-agent**: AI agent integration
5. **044.pal-chat-irc**: Social/chat layer
6. **041.pal-chain**: Blockchain/P2P layer
7. **Test Harnesses**: AI-first testing infrastructure for all projects

The core mechanic: drag pet windows onto a zoo map → they become entities in the game. Drag them out → they return to desktop mode.

**Testing Philosophy**: Build test harnesses (like 044.pal-chat-irc/test-harness-same/) so AI can test before humans. Reusable ops across all projects.

---

## Phase 1: Foundation (Current Focus)

### 1.1 Conversion Status Review

#### ✅ Completed Projects (Converted to 101.mutaclsym Standard)

| Project | Status | Key Features |
|---------|--------|--------------|
| **01.muchi-pals-🥚️-13.01** | ✅ DONE, live-tested | Orchestrator, kill_all.sh, single-marker renderer, no 'q' quit |
| **014.wsr-pal💸️📌️+2** | ✅ DONE, smoke-tested | Economy simulation, 37 ops |
| **044.pal-chat-irc👥️+2** | ✅ DONE, smoke-tested | IRC-style chat, P2P ready |
| **045.muchi-pal-agent🤖️+1** | ✅ DONE, build-verified | LLM agent integration |
| **101.mutaclsym🧟‍♂️️+18.00** | ✅ CANONICAL REF | Reference architecture |
| **101.ledger-player-npc-simple+3** | ✅ DONE | Word game, ledger-based |

#### ⚠️ In Progress

| Project | Status | Blockers |
|---------|--------|----------|
| **041.pal-chain⛓️** | CONVERTED, smoke test hung | Orchestrator hang (suspected network/lock issue) |

#### ❌ Not Started

| Project | Status | Notes |
|---------|--------|-------|
| **041.pal-forum👥️** | NOT STARTED | Same architecture as pal-chain, needs fix first |
| **0.user-pal👤️** | NOT STARTED | Has login-signup skeleton, needs orchestrator |
| **002.zoo__🦓️🐒️0000** | NOT STARTED | Core drag-drop mechanic, highest complexity |

### 1.2 Architecture Reference (101.mutaclsym Standard)

**Directory Structure**:
```
project/
├── button.sh          # Launcher (compile/run/kill/check)
├── config.txt         # key=value settings
├── project.pdl        # State machine definition
├── debug.txt          # Runtime debug output
├── default_op.txt     # Default operations
├── system/
│   ├── orchestrator.c # Process launcher (fork/exec/waitpid)
│   ├── renderer.c     # Frame display (single-marker pulse)
│   ├── keyboard_input.c # Input handler (Ctrl+C quit only)
│   ├── chtpm_parser_pal.c # CHTPM parser
│   └── prisc+x        # PAL interpreter
├── pieces/
│   ├── os/
│   │   ├── kill_all.sh    # 3-layer cascading kill
│   │   └── proc_list.txt  # PID tracking
│   ├── display/
│   │   └── renderer_pulse.txt # Render trigger
│   └── chtpm/
│       └── layouts/   # UI layouts
├── ops/               # Business logic operations
├── pal/               # PAL scripts
└── data/              # Runtime data
```

**Standing Rules** (from mass-refactor.md):
1. **Never use 'q' as quit** - Ctrl+C (ETX) only
2. **Single-marker renderer** - `renderer_pulse.txt`, not dual-marker
3. **No forced periodic re-render** - Marker-driven only
4. **Skip unchanged frames** - Content equality guard
5. **Pre-sync before first compose** - Avoid stale content

---

## Phase 2: Zoo Core (002.zoo)

### 2.1 Vision

**"Drag pets into a living world"**

- Blank mutaclsym-style map as GL window
- Drag pet windows onto map → they become entities
- Drag pet windows out → they return to desktop mode
- Pet directories move into/out of zoo directory
- Visual event editor (PAL as scratch blocks, RPG Maker style)

### 2.2 Architecture Requirements

**Current State of 002.zoo**:
- Has `zoo_window.c` (GL rendering) ✅
- Has `gl_mirror.c` (window mirroring) ✅
- Has basic button.sh (no orchestrator) ❌
- Missing: orchestrator.c, kill_all.sh ❌
- Missing: pet import/export shim ❌
- Missing: visual event editor ❌

**Required Components**:

#### A. Pet Import/Export System
```c
// ops/pet_import.c - Import pet from desktop
// - Accept drag-drop event (GL window position)
// - Read pet directory from source
// - Copy/move to zoo/pets/<pet_id>/
// - Register entity in zoo state
// - Start pet AI loop

// ops/pet_export.c - Export pet to desktop
// - Accept export command
// - Stop pet AI loop
// - Move directory back to source
// - Signal GL window to detach
```

#### B. Zoo Map System
```c
// zoo_window.c extensions:
// - Render mutaclsym-style tile map
// - Track entity positions
// - Handle drag-drop events
// - Camera/viewport control

// ops/zoo_map_editor.c:
// - Add/remove tiles
// - Place entities
// - Define collision zones
// - Event triggers (scratch-block style)
```

#### C. Event System (Scratch/PAL Hybrid)
```pal
# PAL event blocks visualized as scratch
event:
  when [pet_enters] [zone_a]
    then [play_sound] "welcome.wav"
    then [set_mood] "happy"
  
  when [pet_interacts] [other_pet]
    then [spawn_heart] [particle]
    then [increase_friendship] 1
```

### 2.3 Implementation Steps

**Micro-Steps**:

1. **Convert zoo to 101 standard** (2-3 hours)
   - [ ] Add orchestrator.c (fork from mutaclsym)
   - [ ] Add kill_all.sh + proc_list.txt
   - [ ] Fix renderer.c (single-marker)
   - [ ] Remove 'q' quit, use Ctrl+C
   - [ ] Add config.txt, project.pdl

2. **Implement pet import** (4-6 hours)
   - [ ] Design pet directory structure
   - [ ] Create pet_import.c op
   - [ ] Handle GL window drag events
   - [ ] Register entities in zoo state
   - [ ] Test with muchi-pals pets

3. **Implement pet export** (2-3 hours)
   - [ ] Create pet_export.c op
   - [ ] Signal GL window detach
   - [ ] Move directory back
   - [ ] Clean up entity state

4. **Build zoo map renderer** (6-8 hours)
   - [ ] Extend zoo_window.c for tile map
   - [ ] Implement camera/viewport
   - [ ] Entity position tracking
   - [ ] Collision detection

5. **Create visual event editor** (8-12 hours)
   - [ ] Design scratch-block UI
   - [ ] Map PAL operations to blocks
   - [ ] Event trigger system
   - [ ] Save/load event scripts

### 2.4 KPIs

| Metric | Target | Current |
|--------|--------|---------|
| Pet import success rate | 100% | 0% |
| Pet export success rate | 100% | 0% |
| Zoo render FPS | 30+ | N/A |
| Event editor usability | Drag-drop only | N/A |
| Cross-project compatibility | All pets work | Unknown |
| Test harness coverage | 80% critical paths | 0% |
| AI test execution time | <5 min/scenario | N/A |

---

## Phase 3: Player System (0.user-pal)

### 3.1 Vision

**"Your avatar, your house"**

- Avatar creation/customization screens
- Player's own "zoo map" (house interior)
- Persistent player state
- Integration with pet system

### 3.2 Current State

**0.user-pal/00.login-signup/**:
- Has session isolation ✅
- Has login/signup UI skeleton ✅
- Missing orchestrator ❌
- Missing avatar creation ❌
- Missing house map ❌

### 3.3 FUTURE: xyzfs/ - a real user-filesystem, created at signup (design note, not yet implemented)

Raised 2026-07-27 while working on the egg_window/gl_mirror Xdnd swap
(see 0.a-z-pets-plan/a-z-fix.txt): right now, cross-project handoff
folders like `exchange/` (pet_export/pet_import) and `net/` (P2P
outbox/inbox in 044.pal-chat-irc etc.) just live as ad-hoc sibling
directories next to whichever projects happen to need them. That's
fine for now ("exchange folder is fine for now" - direct user
confirmation) but doesn't scale once there's a real concept of a
signed-up user who owns data across many projects.

**The idea**: when a user signs up (0.user-pal/00.login-signup/), create
a real per-user filesystem tree, `xyzfs/`, styled like a Linux user
home:
```
xyzfs/
├── bin/                    # shared ops - binaries usable across
│                           # projects, instead of each project
│                           # duplicating its own copy
└── user/
    ├── home/               # per-user data - THIS is where exchange/,
    │                       # net/, and similar cross-project handoff
    │                       # folders should eventually live, scoped to
    │                       # the specific user rather than global
    │                       # siblings of the project tree
    └── projects/           # the user's own project instances,
                            # presented as a "desktop" visual metaphor
```

Not scoped or started yet - explicitly deferred until after user login/
avatar creation work begins, since xyzfs/ only makes sense once there's
a real signup flow to create it at. Revisit this section when Phase 3
implementation actually starts.

### 3.4 Implementation Steps

**Micro-Steps**:

1. **Convert to 101 standard** (1-2 hours)
   - [ ] Add orchestrator.c
   - [ ] Add kill_all.sh
   - [ ] Standardize button.sh

2. **Build avatar creation** (6-8 hours)
   - [ ] Character sprite editor
   - [ ] Color/customization options
   - [ ] Save to users/<user_id>/avatar.png
   - [ ] Preview window

3. **Build house map** (8-10 hours)
   - [ ] Room-based map (like zoo but smaller)
   - [ ] Furniture/decoration placement
   - [ ] Pet integration (pets can visit)
   - [ ] Save/load house state

4. **Integrate with zoo** (4-6 hours)
   - [ ] Travel between house and zoo
   - [ ] Bring pets home
   - [ ] Share house with friends (P2P)

---

## Phase 4: AI & Social (045 + 044)

### 4.1 Vision

**"Live agents that do stuff"**

- AI agents that can perform actions in the system
- Chat-based control
- Social features (IRC-style)
- Blockchain integration (041.pal-chain)

### 4.2 Current State

| Project | Status | Notes |
|---------|--------|-------|
| 045.muchi-pal-agent | Converted | Needs live testing |
| 044.pal-chat-irc | Converted | Needs integration |
| 041.pal-chain | Hang issue | Needs debugging |

### 4.3 Implementation Steps

1. **Fix pal-chain hang** (2-3 hours)
   - [ ] Debug orchestrator.c
   - [ ] Check network/lock issues
   - [ ] Test with NO_NET=1

2. **Integrate AI with zoo** (6-8 hours)
   - [ ] Agent can navigate zoo
   - [ ] Agent can interact with pets
   - [ ] Agent can modify map

3. **Social features** (4-6 hours)
   - [ ] Visit friends' zoos
   - [ ] Trade pets
   - [ ] Chat in-game

4. **Blockchain layer** (8-10 hours)
   - [ ] Pet ownership on-chain
   - [ ] Marketplace
   - [ ] Cross-game asset transfer

---

## Phase 5: Test Harness Infrastructure (AI-First Testing)

### 5.1 Vision

**"AI tests before humans do"**

Build reusable, ops-based test harnesses that allow AI agents to:
- Inject keystrokes and drive UI flows programmatically
- Assert frame content contains expected strings
- Run scenarios without human intervention
- Reuse test ops across all projects in the family

Reference implementation: `044.pal-chat-irc👥️+2/test-harness-same/`

### 5.2 Architecture (From 044.pal-chat-irc)

**Directory Structure**:
```
project/
└── test-harness/
    ├── button.sh              # Thin entry point (compile/demo/kill)
    ├── ops/
    │   ├── tk_inject_key.c    # Append one KEY_PRESSED line
    │   ├── tk_type_text.c     # Type string char-by-char
    │   ├── tk_focus_item.c    # Find menu item by label, inject nav-jump
    │   ├── tk_assert_contains.c # Check file for substring (PASS/FAIL)
    │   └── +x/                # Compiled binaries
    ├── scenarios/
    │   └── demo_2user_chat.sh # Reference scenario (2-user chat flow)
    └── README.txt             # Full documentation
```

**Key Design Principles**:
1. **Ops are reusable CLI tools** - Each op is independently callable
2. **Scenarios are op sequences** - Bash scripts calling ops with sleeps
3. **No hardcoded numbers** - Focus items by label text, never position
4. **Assert at every checkpoint** - Cheap, gives PASS/FAIL trail
5. **Save proof before cleanup** - Copy frames to `proof/` directory

### 5.3 Test Ops Reference

```bash
# Inject a keystroke (decimal ASCII code)
ops/+x/tk_inject_key.+x <session_dir> <key_code>

# Type a string (one key per character)
ops/+x/tk_type_text.+x <session_dir> "<text>"

# Focus a menu item by label substring (returns item number)
ops/+x/tk_focus_item.+x <session_dir> <frame_file> "<label>"

# Assert file contains substring (prints PASS/FAIL, exits 0/1)
ops/+x/tk_assert_contains.+x <file> "<expected>" ["<check_label>"]
```

**Example: Interactive Testing**:
```bash
# Find and click "Create Account" button
SESS=$(ls -dt pieces/sessions/*/ | head -1)
ITEM=$(ops/+x/tk_focus_item.+x "$SESS" "$SESS/pieces/display/current_frame.txt" "Create Account")
ops/+x/tk_inject_key.+x "$SESS" 13  # Enter to activate
```

### 5.4 Projects Needing Test Harnesses

| Priority | Project | Complexity | Notes |
|----------|---------|------------|-------|
| **P0** | 002.zoo | High | Pet import/export, drag-drop, map editing |
| **P1** | 0.user-pal | Medium | Avatar creation, house map |
| **P2** | 01.muchi-pals | Low | Pet spawning, menu navigation |
| **P3** | 045.muchi-pal-agent | Medium | AI agent actions, tool execution |
| **P4** | 041.pal-chain | High | Blockchain ops, P2P messaging |

### 5.5 Zoo Test Scenarios (002.zoo)

**Scenario 1: Pet Import Flow**
```bash
#!/bin/bash
# scenarios/pet_import.sh
# Tests: Launch zoo → Import pet from muchi-pals → Verify entity appears

# 1. Launch zoo in background
bash "$PROJECT_DIR/button.sh run --pal" &

# 2. Wait for render
sleep 2

# 3. Focus "Import Pet" menu item
ITEM=$(ops/+x/tk_focus_item.+x "$SESS" "$SESS/pieces/display/current_frame.txt" "Import Pet")
ops/+x/tk_inject_key.+x "$SESS" 13

# 4. Select pet source (muchi-pals)
ITEM=$(ops/+x/tk_focus_item.+x "$SESS" "$SESS/pieces/display/current_frame.txt" "muchi-pals")
ops/+x/tk_inject_key.+x "$SESS" 13

# 5. Assert pet appears in entity list
ops/+x/tk_assert_contains.+x "$SESS/pieces/display/current_frame.txt" "pet_001"

# 6. Save proof
cp "$SESS/pieces/display/current_frame.txt" proof/pet_import_success.txt
```

**Scenario 2: Pet Export Flow**
```bash
#!/bin/bash
# scenarios/pet_export.sh
# Tests: Select pet → Export → Verify removed from zoo

# 1. Select pet on map
ops/+x/tk_inject_key.+x "$SESS" 32  # Space to select

# 2. Focus "Export Pet" menu
ITEM=$(ops/+x/tk_focus_item.+x "$SESS" "$SESS/pieces/display/current_frame.txt" "Export Pet")
ops/+x/tk_inject_key.+x "$SESS" 13

# 3. Confirm export
ITEM=$(ops/+x/tk_focus_item.+x "$SESS" "$SESS/pieces/display/current_frame.txt" "Confirm")
ops/+x/tk_inject_key.+x "$SESS" 13

# 4. Assert pet removed
ops/+x/tk_assert_contains.+x "$SESS/pieces/display/current_frame.txt" "No pets"
```

**Scenario 3: Map Editing**
```bash
#!/bin/bash
# scenarios/map_edit.sh
# Tests: Enter edit mode → Place tile → Save map

# 1. Enter edit mode (F1 or menu)
ops/+x/tk_inject_key.+x "$SESS" 27  # ESC to open menu
ITEM=$(ops/+x/tk_focus_item.+x "$SESS" "$SESS/pieces/display/current_frame.txt" "Edit Map")
ops/+x/tk_inject_key.+x "$SESS" 13

# 2. Select tile type
ITEM=$(ops/+x/tk_focus_item.+x "$SESS" "$SESS/pieces/display/current_frame.txt" "Grass")
ops/+x/tk_inject_key.+x "$SESS" 13

# 3. Place tile (click on map)
ops/+x/tk_inject_key.+x "$SESS" 32  # Space to place

# 4. Save map
ITEM=$(ops/+x/tk_focus_item.+x "$SESS" "$SESS/pieces/display/current_frame.txt" "Save Map")
ops/+x/tk_inject_key.+x "$SESS" 13

# 5. Assert save confirmation
ops/+x/tk_assert_contains.+x "$SESS/pieces/display/current_frame.txt" "Map saved"
```

### 5.6 Shared Test Ops Library

Create a shared library of test ops that all projects can reuse:

```
shared-test-ops/
├── README.md              # Documentation
├── ops/
│   ├── tk_inject_key.c    # Keystroke injection
│   ├── tk_type_text.c     # Text typing
│   ├── tk_focus_item.c    # Menu item focusing
│   ├── tk_assert_contains.c # Frame assertion
│   ├── tk_wait_for_frame.c # Wait for frame to contain text
│   ├── tk_screenshot.c    # Save frame to file
│   └── +x/                # Compiled binaries
├── scenarios/
│   ├── common_startup.sh  # Launch app, wait for ready
│   ├── common_shutdown.sh # Clean shutdown
│   └── common_helpers.sh  # Shared functions
└── button.sh              # Build all ops
```

**New Ops to Build**:

```c
// tk_wait_for_frame.c - Wait until frame contains text (with timeout)
// Usage: tk_wait_for_frame.+x <session_dir> "<text>" [timeout_ms]
// Returns 0 if found, 1 if timeout

// tk_screenshot.c - Save current frame to timestamped file
// Usage: tk_screenshot.+x <session_dir> [output_dir]
// Saves to proof/screenshot_YYYYMMDD_HHMMSS.txt

// tk_drag_to.c - Simulate drag-drop (for pet import/export)
// Usage: tk_drag_to.+x <session_dir> <src_x> <src_y> <dst_x> <dst_y>
// Sends X11 ButtonPress/Motion/ButtonRelease events
```

### 5.7 AI Agent Testing Workflow

**Before Human Tests**:
1. AI runs `button.sh compile` to build test ops
2. AI runs scenario script (e.g., `scenarios/pet_import.sh`)
3. AI checks PASS/FAIL output from `tk_assert_contains`
4. AI saves proof to `proof/` directory
5. AI reports results with frame captures

**Benefits**:
- Catch bugs before human sees them
- Reproducible test cases
- Automated regression testing
- AI can test edge cases (100 pets, rapid input, etc.)

### 5.8 Implementation Steps

**Micro-Steps**:

1. **Create shared-test-ops library** (2-3 hours)
   - [ ] Port existing ops from 044.pal-chat-irc
   - [ ] Add `tk_wait_for_frame.c`
   - [ ] Add `tk_screenshot.c`
   - [ ] Add `tk_drag_to.c` (for zoo drag-drop)
   - [ ] Document in README.md

2. **Build zoo test harness** (3-4 hours)
   - [ ] Copy shared-test-ops to 002.zoo/test-harness/
   - [ ] Create zoo-specific scenarios
   - [ ] Test pet import/export flows
   - [ ] Test map editing flows

3. **Build user-pal test harness** (2-3 hours)
   - [ ] Create avatar creation scenarios
   - [ ] Test house map flows
   - [ ] Test login/signup flows

4. **Integrate with CI** (optional, 2-3 hours)
   - [ ] Add `make test` target to button.sh
   - [ ] Run scenarios on every commit
   - [ ] Generate test reports

### 5.9 KPIs

| Metric | Target | Current |
|--------|--------|---------|
| Test ops reuse rate | 80%+ shared | 0% |
| Scenario coverage | All critical paths | 0% |
| AI test execution time | <5 min per scenario | N/A |
| False positive rate | <5% | Unknown |
| Proof capture rate | 100% scenarios | 0% |

---

## Phase 6: Polish & Release

### 6.1 Visual Polish
- [ ] Sprite sheet standardization
- [ ] Animation system
- [ ] Sound effects
- [ ] Music integration

### 6.2 UX Polish
- [ ] Tutorial system
- [ ] Help documentation
- [ ] Settings menu
- [ ] Save/load system

### 6.3 Testing
- [ ] Cross-platform testing (Linux, Windows via WSL)
- [ ] Performance optimization
- [ ] Memory leak detection
- [ ] Stress testing (100+ pets)

---

## Technical Deep Dives

### A. Pet Directory Structure

```
pets/
├── pet_001/
│   ├── metadata.txt      # name, type, stats
│   ├── sprite.png        # current appearance
│   ├── state.txt         # position, mood, energy
│   ├── ai_loop.pal       # behavior script
│   └── inventory/        # items pet is carrying
├── pet_002/
│   └── ...
```

### B. GL Window Communication

```c
// Window → Zoo communication
// Via shared memory or X11 events
typedef struct {
    int window_id;
    int pet_id;
    float x, y;        // position
    int action;         // 0=idle, 1=dragging, 2=dropped
} PetWindowEvent;

// Zoo → Window communication
typedef struct {
    int window_id;
    int command;        // 0=attach, 1=detach, 2=move
    float target_x, target_y;
} ZooCommand;
```

### C. PAL Event Block System

```pal
# Visual block representation
block: "When pet enters zone"
  trigger: entity_enter_zone
  params: [zone_name]
  
block: "Play sound"
  action: play_sound
  params: [sound_file]
  
block: "Set entity property"
  action: set_property
  params: [entity, property, value]
```

---

## Risk Assessment

| Risk | Impact | Mitigation |
|------|--------|------------|
| GL window drag-drop complexity | High | Start with simple click-to-add, iterate |
| Cross-project pet compatibility | Medium | Standardize pet directory format early |
| Performance with many pets | Medium | Implement LOD (level of detail) system |
| PAL event editor complexity | High | Start with text-based, add visual later |
| Blockchain integration scope | Low | Defer to Phase 6, focus on core first |
| Test harness adoption | Medium | Start with 044 pattern, prove value early |
| AI test reliability | Medium | Extensive assertion checking, save proof |

---

## Success Criteria

### Minimum Viable Product (MVP)
- [ ] Can import pets from muchi-pals to zoo
- [ ] Pets appear on zoo map
- [ ] Can export pets back to desktop
- [ ] Basic map rendering works
- [ ] Ctrl+C quits cleanly
- [ ] Test harness for zoo pet import/export (AI can test)

### Full Release
- [ ] Visual event editor (scratch blocks)
- [ ] Avatar creation system
- [ ] House map for players
- [ ] AI agent integration
- [ ] Chat/social features
- [ ] Blockchain pet ownership
- [ ] Test harnesses for all projects
- [ ] Shared test ops library
- [ ] AI-driven regression testing

---

## Open Questions

1. **GL Window Protocol**: How do we handle drag-drop between X11 windows? (Need to research Xdnd protocol)
2. **Pet State Sync**: Real-time or batch updates between zoo and pet windows?
3. **Event Editor UI**: Scratch-blocks library or custom implementation?
4. **Blockchain Choice**: Which chain? (TON mentioned in directory names)
5. **Multiplayer**: Real-time or turn-based?
6. **xyzfs/ user filesystem**: see Phase 3.3 - should `exchange/`, `net/`,
   and similar cross-project handoff folders move under a per-user
   `xyzfs/user/home/` once signup exists, rather than staying ad-hoc
   project-sibling directories? Deferred until Phase 3 (login/avatar)
   actually starts.

---

## Next Actions (Today)

1. **Verify 01.muchi-pals conversion** ✅ (Done per convert-report-j26.md)
2. **Debug 041.pal-chain hang** (2-3 hours)
3. **Start zoo conversion** (2-3 hours)
4. **Design pet import/export API** (1-2 hours)
5. **Port test ops from 044.pal-chat-irc** (1-2 hours)
6. **Create zoo test harness skeleton** (1-2 hours)

---

## Appendix: File References

- `!.!.convert-report-j26.md` - Current conversion status
- `!.mass-refactor.md` - Refactor plan and rules
- `CHTPM_ARCHITECTURE_GUIDE.txt` - Architecture patterns
- `101.mutaclsym🧟‍♂️️+18.00/` - Reference implementation
- `101.ledger-player-npc-simple+3/` - Ledger-based storage reference
- `002.zoo__🦓️🐒️0000/02.z00.play-plan🗓️.txt` - Original zoo vision
- `044.pal-chat-irc👥️+2/test-harness-same/` - Test harness reference implementation
- `#.haiku+/!.local-ux-testing-ai.txt` - Key-injection interaction model
- `#.haiku+/!.xyzos-pitfalls+1.txt` - Test-related pitfalls (20/21/22)
