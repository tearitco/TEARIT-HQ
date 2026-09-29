# Agent Onboarding: Reading Order & Quick Start

**Date:** 2026-09-27  
**Purpose:** Tell new agents (and future me) what to read in what order

---

## Absolute First: Read These (In Order)

**Before you write ANY code or run ANY tests, read these four documents:**

1. **`#.#.calendar-dox/!.HQ-IQ-BOOK/01-orientation/WHAT-IS-THIS-HOUSE.md`**  
   - What is this codebase? What's it built for?
   - ~5 minutes
   - **Required:** Yes, first thing

2. **`#.#.calendar-dox/!.HQ-IQ-BOOK/02-architecture/CENTROID_GOLD_STD.md`**  
   - The ONE core architecture rule: one parsed tree, multiple renderers
   - Why text-includes are bad, why no per-app dispatch tables
   - How khtpm_core_render works
   - ~20 minutes
   - **Required:** Yes, foundational

3. **`.claude/skills/khtpm-house-standards/SKILL.md`**  
   - Concrete rules for khtpm work
   - Read the top MANDATORY section (2026-09-01 incident explanation)
   - Read the "looking backward" section (why old patterns are obsolete)
   - ~15 minutes
   - **Required:** Yes, if you're touching khtpm code

4. **`#.#.calendar-dox/1.^V-hq/_.0.aigent-testing-k9.txt`**  
   - Testing methodology for khtpm windows
   - The relay system (not xdotool)
   - How to avoid focus-stealing bugs
   - Order of preference: relay → text state → PNG → external tools
   - ~30 minutes (skim first, read deep before testing)
   - **Required:** Yes, before you test

---

## If You're Working on Entity Windows

5. **`#.livedesk/AGENT-ONBOARDING-KHTPM.md`** (this repo)
   - Specific to entity windows (khtpm_entity.c, interact_relay.txt)
   - The Move action case study
   - Quick reference checklist
   - ~10 minutes
   - **Required:** Yes, for entity window work

6. **`*.monads/*.livedesk-taskbar/ops/khtpm_entity.c` (the actual code)**
   - Read lines 1-100 (file header + what it does)
   - Search for the specific feature you're working on
   - Read the handler/implementation
   - Don't try to understand the whole file, just your feature
   - ~depends on feature
   - **Required:** Yes, before implementing

---

## Reference Documents (Use as Needed)

- **`#.#.calendar-dox/!.HQ-IQ-BOOK/02-architecture/STATE-AND-PDL-CONVENTIONS.md`**  
  - How state files work
  - PDL format (if you're editing `.pdl` files)

- **`#.#.calendar-dox/!.HQ-IQ-BOOK/02-architecture/X11-HQ-APP-DESIGN-WISDOMS.md`**  
  - X11 window management pitfalls
  - Relay system design philosophy

- **`#.#.calendar-dox/!.HQ-IQ-BOOK/04-bugs/BUG-LOG.md`**  
  - Real bugs and how they were fixed
  - Learn from past mistakes

- **`#.#.calendar-dox/!.HQ-IQ-BOOK/05-faq/HOUSE_FAQ.md`**  
  - Common questions and answers

---

## Quick Decision Tree

**I'm working on:** → **Read these:**

- Entity window (khtpm_entity.c)  
  → CENTROID_GOLD_STD + khtpm-house-standards + k9.txt + AGENT-ONBOARDING-KHTPM

- Adding a method/menu to an entity  
  → Read meta.pdl format docs + menu.chtpm structure + khtpm_entity.c method handler

- Testing/debugging entity behavior  
  → k9.txt (relay not xdotool) + check history.txt not asking user

- X11 window corruption / disappearance issue  
  → X11-HQ-APP-DESIGN-WISDOMS + BUG-LOG (see past X11 incidents)

- Not sure which family of app this is  
  → Read file header comment (first 50 lines) + WHAT-IS-THIS-HOUSE

---

## The Mistake Pattern To Avoid

**This is what I did wrong (don't repeat it):**

1. ❌ Skipped reading the architecture document
2. ❌ Didn't check if the working pattern existed elsewhere
3. ❌ Started testing before reading code
4. ❌ Tried 5+ different approaches instead of one careful read
5. ❌ Asked user to test instead of reading state files myself

**Right pattern:**
1. ✅ Read CENTROID_GOLD_STD (architecture)
2. ✅ Read the actual C code for what you're building
3. ✅ Test manually with real UI
4. ✅ Check history.txt / state files for what happened
5. ✅ Only then automate

---

## How to Use This Guide

**New agent starting work?**  
→ Print this file, read #1-4 in order, then task-specific section

**Stuck on something?**  
→ Read the "Reference Documents" section that matches your issue

**Not sure what you're looking at?**  
→ Use the "Quick Decision Tree"

**Made a mistake?**  
→ Read "The Mistake Pattern To Avoid" and update your approach

---

## Files Not Worth Reading (Yet)

These are interesting but NOT critical for onboarding:

- YOUTUBE-REAL-VS-NATIVE-PLAYER-ROADMAP (future planning, not current state)
- REAL-SPA-SITE-GAP (historical comparison, not current code)
- LEGACY-GL-PIPELINE (old, deprecated, not in use)
- TWO-PARSER-FAMILIES (historical, chtpm_parser_pal is not what khtpm uses)

**Exception:** If you're doing archaeological debugging (investigating old code), read BUG-LOG first to see if it's a known pattern.

---

## Last Resort: Ask For Help

If you've read 1-4 above and still stuck:
- Check BUG-LOG to see if this bug is known
- Check if another entity window does what you need (refactor, don't rewrite)
- Then ask

---

**This guide created:** 2026-09-27 after Move action investigation flailed due to skipping documentation  
**Keep this updated:** When new architecture documents are added, link them here
