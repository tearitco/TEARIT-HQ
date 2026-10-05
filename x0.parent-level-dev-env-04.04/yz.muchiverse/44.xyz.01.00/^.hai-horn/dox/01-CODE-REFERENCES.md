# Code references — what exists, where to find it

All paths are absolute. Copy these files verbatim (or adapt minimally) as the foundation.

---

## For HORN_CHAT v0.1

### The renderer (copy verbatim)
**File:** `/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/1.TPMOS_c_+rmmp.0103.0001/projects/gem-dev/ops/chtpm_parser.c`

**Why:** Parses `.xhtpm` (terminal layout files) and renders them to terminal. No changes needed.

**How to use:** 
1. Copy this file into `/ops/horn_chat_chtpm_parser.c` (or keep it named chtpm_parser.c)
2. In your `.pal` main loop, call the parser on your layout file
3. Parser outputs rendered terminal frames; your loop displays them

**Size:** ~2000 lines. No modifications needed for HORN.

---

### The OpenRouter round-trip (copy + minimal tweak)
**File:** `/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/&.widgits/entity-cli/ops/ai_chat_openrouter.c`

**Why:** Proven OpenRouter integration. Direct fork/exec curl, bearer token, auto-cycles models, appends to history file.

**How to use:**
1. Copy into `/ops/horn_chat_openrouter.c`
2. Tweak: entity_dir setup (HORN will have its own, different from robot-chat)
3. Keep the core: curl round-trip, OR_MODELS[] cycle, history append

**What to change:**
- The history file path (change from `<entity_dir>/chat_history.txt` to wherever HORN stores sessions)
- Everything else stays the same

**Size:** ~300 lines.

---

### Layout file (minimal template)
**File:** `/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/&.widgits/robot-chat/robot-chat.xhtpm`

**Why:** Real `.xhtpm` file showing the chat window shape (sidebar + main reply area).

**How to use:**
1. Copy and simplify for terminal (no colors/styles, just the structure)
2. Keep: `<window>` + `<panel>` + `<text>` (for replies) + `<cli_io>` (for input)
3. Minimal example for HORN: just a full-screen window, top half for replies, bottom half for input

**Size:** ~50-100 lines for a minimal version.

---

## For HALO_CHAT v0.1

### DESCRIBE step (copy + no changes)
**File:** `/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/&.widgits/entity-cli/ops/ai_describe.c`

**Why:** Takes chat_history.txt, runs Gemma with a constrained prompt, writes a candidate line to pending_review.txt.

**How to use:**
1. Copy into `/ops/halo_chat_describe.c` (no changes to the core logic)
2. Your `.pal` file calls this op after each chat turn
3. Output goes to `pending_review.txt` (in HORN/HALO's working dir)

**Size:** ~200 lines.

---

### Validator (inspect first, may need minor tweaks)
**File:** `/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/&.widgits/concept-bank/ops/concept_edit_validate.c`

**Why:** Checks that a describe-output edit is valid (target exists, slot exists, weight is safe range).

**How to use:**
1. Copy into `/ops/halo_chat_validate.c`
2. Your `.pal` reads `pending_review.txt`, pipes each line to this op
3. Op returns 0 (valid) or 1 (invalid); your loop decides next step

**Size:** ~300 lines.

**Note:** This validates only the NARROW case (weight nudge on existing concept). NIGHT_30 mentions three other edit types (new concept, new FSM transition, new planner action) that don't have validators yet. HALO v0.1 only handles the one that exists.

---

## For understanding the architecture

### Attrition model overview (required reading)
**File:** `/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/#.#.calendar-dox/1-1.HARNECIENT.SMOL/NIGHT_30_THE_ATTRITION_MODEL.txt`

**Why:** Explains the four-piece model (TEARIT loop, Concept Bank, tomom, decision layer) that HORN/HALO feed into.

**Read time:** 15 min (it's a 4-voice dialogue, not a dense spec).

---

### 2026-09-30 design docs (reference, not required to start)
**Directory:** `/home/no/Desktop/github/work/XO/10.kilo-attrition/kilo-openrouter-docs/`

**Files:**
- `01-ONBOARDING.md` — what kilo + OpenRouter models need to connect
- `02-TESTING-LOOP.md` — how kilo will test this stack
- `03-ATTRITION-VIA-CHAT.md` — how HORN/HALO feed the Concept Bank
- `04-TOMOM-TRAINING-OPEN-QUESTIONS.md` — the IRL/training questions (genuinely open)

**Why:** Ground truth on what's real vs. what's still being designed. Read this when flagging decisions back to the owner.

---

## House rules

### Commit discipline (AGENTS.md)
- Every session ends with a scoped commit (never `git add -A`)
- One commit message per file, clear what changed
- Your branch: `opencode` (for Claude Code agents) — never commit to `main` or `claude` or `grok`
- Example: `git add ops/horn_chat_openrouter.c && git commit -m "feat: OpenRouter round-trip for HORN_CHAT"`

### Code style (if modifying existing code)
- Keep .c files minimal (no new abstractions beyond what's needed)
- .pal files are RISC-V assembly, not shell scripts or Tcl
- Comments only for non-obvious "why," never "what" (the code should say what it does)

### Testing before commit
1. **It compiles** (`gcc -c <file>.c`)
2. **It runs** (actual execution, not just a syntax check)
3. **A real feature works** (e.g., one chat round-trip completes, not just "program starts")

---

## Summary checklist

Before you start coding:

- [ ] Read `00-QUICK-START.md` (this dir)
- [ ] Read `../HORN_CHAT-HANDOFF.md` (main handoff)
- [ ] Read NIGHT_30 (attrition model overview)
- [ ] Browse gem-dev's `button.sh` (shows how the pattern works)
- [ ] Understand `ai_chat_openrouter.c` is your OpenRouter foundation
- [ ] Know your three sprints: HORN v0.1, HALO v0.1, IRL design
- [ ] Ready to build: `/ops/horn_chat_openrouter.c` + `/horn_chat.pal` + `/layouts/horn_chat.xhtpm`

**Go build.**
