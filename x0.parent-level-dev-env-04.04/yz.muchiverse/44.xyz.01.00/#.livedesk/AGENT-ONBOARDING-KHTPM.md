# Agent Onboarding: khtpm (X11 Entity Windows & UI)

**Date Created:** 2026-09-27  
**Purpose:** Prevent repeated mistakes when working on khtpm family apps (entity windows, context menus, UI rendering)

---

## Before You Start: Read This First

**DO NOT start coding without:**
1. Reading CENTROID_GOLD_STD.md (architecture rule: one parsed tree, multiple renderers)
2. Reading this file completely
3. Reading the relevant skill file (khtpm-house-standards) in `.claude/skills/`

**DO NOT try to automate testing without:**
1. Reading _.0.aigent-testing-k9.txt (testing methodology guide)
2. Understanding the relay system (not xdotool, not external screenshots)

---

## The Real Architecture (In One Paragraph)

khtpm apps use a **real, generic shared renderer** (`khtpm_core_render.c`) that walks a parsed `.chtpm` file + CSS rules to build an `Elem` tree (with real x/y/w/h and real CssStyle). This tree is then rendered via:
- **X11/Xft**: pixel-based GUI (db-hq, chat-hai, palettes, stats-hq, entity windows)
- **ASCII**: text-mode representation (future, same tree, different renderer)

**The mistake I made:** Built solutions in isolation, didn't check if the working pattern already existed elsewhere. The house rule is: **find the real, already-working implementation and reuse/refactor it, never write a parallel copy.**

---

## Key Standards (What I Violated & How to Avoid)

### 1. **One Canonical Implementation, Not Parallel Copies**

❌ **WRONG**: "I'll add a text-include of khtpm_ui_common.c to my new binary"  
✅ **RIGHT**: "Let me grep for how this is done elsewhere... ah, khtpm_core_render.c already has it"

**How to avoid:** Before writing code:
```bash
grep -r "what_you_want_to_build" ~/path/to/house/ | head -10
```

If it exists (even 80% of what you need), **stop** and read that implementation fully. Then refactor to share it, don't write a new one.

### 2. **The Right Order: Read Code, THEN Test, THEN Automate**

❌ **WRONG**: Try 5 relay commands, none work, debug blind, fail repeatedly  
✅ **RIGHT**: Read how the code parses relay commands → test manually → automate

**My mistake last session:**
- Started testing relay automation (step 3) before reading relay parsing code (step 1)
- Guessed at key codes instead of reading the constants
- Asked user to test each attempt instead of reading history.txt logs myself

**The pattern that works:**
1. Read the `.c` file where commands are processed (e.g., `if (strcmp(line, "OPEN_CONTEXT") == 0)`)
2. Manually test with real UI (click buttons, watch what happens)
3. Only then automate via relay with correct command format

### 3. **Relay Systems Are NOT All The Same**

Different windows support different relay formats:
- **db-hq/events-hq**: `#.desktop/db_hq_history.txt` — bare decimal ASCII codes
- **Entity windows**: `interact_relay.txt` — supports `OPEN_CONTEXT`, `ACTIVATE_NAV:<n>`, `NAV_KEY:Up/Down/Enter`
- **Taskbar**: `#.desktop/livedesk_agent_relay.txt` — bare decimal codes

❌ **WRONG**: Try bare decimal codes on entity window, expect it to work like taskbar  
✅ **RIGHT**: Read the code handler for this specific window first

**How to find the handler:**
```bash
grep -n "interact_relay\|strncmp.*line" khtpm_entity.c | head -20
```

Then read those lines to see what commands it actually accepts.

### 4. **Animation in X11 is Fragile**

❌ **WRONG**: Call `XMoveWindow()` directly during event processing  
✅ **RIGHT**: Update position files; let the window re-read them on next frame

**The bug I fixed:** khtpm_entity.c was calling `XMoveWindow()` in the middle of its event loop, which corrupted X11 window state (caused disappearance). Moving windows should only happen:
- At window creation time
- On real user drag events
- Via dedicated window manager commands (not mid-event-processing)

### 5. **State Files Are the API**

❌ **WRONG**: Hardcode paths like `"/44.xyz.01.00/..."`  
✅ **RIGHT**: Use relative paths, read config files, let the system resolve paths

**Example I violated:** Originally tried to hardcode paths instead of using the house's path-resolution pattern.

**The pattern:** The entity itself provides `package_dir` → all state files are relative to that.

---

## Standard File Locations for Entity Windows

```
<entity_package_dir>/
  desktop_pos.txt          # Current position (x/y/z in REFERENCE px)
  interact_relay.txt       # Relay for this entity's input commands
  history.txt              # Log of all injected/processed events
  meta.pdl                 # Methods available (Act, Dir, Move, etc.)
  menu.chtpm               # Menu layout (.chtpm format)
  animation_queue.txt      # Temporary: coordinates for animation frames
```

**Key insight I missed:** The animation_queue.txt is **temporary and ephemeral**—don't rely on it existing. Only move_entity_animated.+x should write it, only the entity window should read it, and it should be cleaned up when animation completes.

---

## Testing Methodology (From _.0.aigent-testing-k9.txt)

### For Entity Windows (khtpm_entity.c, tp_desktop_window_rgb.+x)

**Preferred method (in order):**

1. **The relay file (text state audit, not PNG dumps)**
   ```bash
   echo "OPEN_CONTEXT" >> <entity>/interact_relay.txt
   sleep 1
   tail <entity>/history.txt  # See what actually happened
   ```

2. **Text state files** (faster than PNG, no timing races)
   ```bash
   cat <entity>/desktop_pos.txt
   cat <entity>/meta.pdl
   grep "CLICK\|INPUT" <entity>/history.txt
   ```

3. **PNG dump via relay** (last resort for pixel-level proof)
   - Not all windows support this
   - khtpm_entity.c may not have `dump_frame_png()` built in
   - Don't assume it works; check the code first

4. **xdotool / external tools** (ONLY if relay fails)
   - Use `.claude/skills/khtpm-house-standards` as reference
   - Understand why relay is preferred: focus stealing, environment conflicts

### What I Did Wrong

- ❌ Tried relay automation WITHOUT reading the code first
- ❌ Skipped to xdotool/PNG dumps before understanding relay system
- ❌ Asked user to test instead of reading history.txt logs myself
- ❌ Made 5+ attempts at wrong relay commands instead of one careful read

### What To Do Right

- ✅ Read code handler first (grep for command processing)
- ✅ Check history.txt after each test (it logs EVERYTHING)
- ✅ Use text state files (faster feedback than PNG)
- ✅ Only automate after manual testing works

---

## The Move Action Case Study (What Went Wrong)

### What I Should Have Done

1. **Read the code** (3 minutes)
   - Found `Move` is under `Act` submenu, not a top-level method
   - Found relay supports: `OPEN_CONTEXT`, `ACTIVATE_NAV:<n>`, `NAV_KEY:<key>`
   - Found `MOVE_TARGET` handler expects input mode

2. **Tested manually** (2 minutes)
   - Clicked Act → Move → entered coordinates
   - Watched history.txt to see what events fired

3. **Automated the test** (2 minutes)
   - Used correct relay format
   - Checked history.txt after each step

**Total time: 7 minutes. I spent 2 hours guessing instead.**

### What Went Wrong

- ❌ Tried to automate without reading code first
- ❌ Guessed at relay command format (`ACTIVATE_NAV:1` vs actual nav indices)
- ❌ Didn't read history.txt to see why commands failed
- ❌ Skipped manually testing the UI to understand how Move actually works
- ❌ Added animation loop disable without verifying it was the real issue

### What Was Actually Needed

The real fix was simple:
- Add boundary constraint to move_entity_animated.c ✓
- Disable animation loop that corrupted X11 state ✓
- Test that Move works without disappearance ✓

No amount of relay automation mattered until the core issue (X11 corruption) was fixed.

---

## Quick Reference: Standards Checklist

Before submitting code:

- [ ] Did I read CENTROID_GOLD_STD.md?
- [ ] Did I check if this is done elsewhere in the house?
- [ ] Did I read the actual C code where my feature connects?
- [ ] Did I test manually BEFORE automating?
- [ ] Did I commit ONLY my changes, not runtime state files?
- [ ] Did I follow house commit message style?
- [ ] Did I read existing tests/harnesses to understand patterns?
- [ ] Did I check history.txt logs instead of asking user to test?

---

## Deferred Work Exposed This Session

**khtpm_ui_common.c unfactor:** See `#.livedesk/TODO-khtpm_ui_common-unfactor.md`. This text-include pattern violates the standard. Deferred to future session, but documented.

---

## For The Next Agent (And Future Me)

If you're stuck:
1. **Read the skill file** (`.claude/skills/khtpm-house-standards`)
2. **Read CENTROID_GOLD_STD.md** (the actual architecture rule)
3. **Read the relevant C code** (not a summary, the actual code)
4. **Search for working examples** (`grep -r <what_you_need>`)
5. **Test manually first** (click the UI, watch history.txt)

If you still can't find the answer after step 4, THEN ask for help or create a test harness.

---

## Files That Matter

- `#.#.calendar-dox/!.HQ-IQ-BOOK/02-architecture/CENTROID_GOLD_STD.md` — The core rule
- `#.#.calendar-dox/1.^V-hq/_.0.aigent-testing-k9.txt` — Testing methodology
- `.claude/skills/khtpm-house-standards` — House rules for khtpm work
- `*.monads/*.livedesk-taskbar/ops/khtpm_entity.c` — Entity window implementation
- `history.txt` in entity package dir — Your best debugging tool (not user testing)

---

**Last updated:** 2026-09-27 after Move action investigation  
**Reason:** Document mistakes to prevent repetition
