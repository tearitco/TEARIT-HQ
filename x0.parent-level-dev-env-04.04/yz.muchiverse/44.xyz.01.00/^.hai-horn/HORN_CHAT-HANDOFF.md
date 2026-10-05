# HORN_CHAT Handoff for Kilo

**Status:** Handoff to Kilo (low-context, high-capability agent)  
**Date:** 2026-10-01  
**Owner approval:** Yes — see 2do-TMP_0.0001.txt for full context request

---

## Mission (one sentence)

Build a terminal-based CLI chat harness (`HORN_CHAT`) that uses OpenRouter API models directly, reusing chtpm primitives from gem-dev, as the foundation for a mini attrition-model pipeline (HALO_CHAT → IRL learning → curricula generation).

## What's real, what's planned

### Real, exists today
- `chtpm_parser.c` — already parsing `.xhtpm` layout files and emitting rendered text UI (used by gem-dev). Reference: `/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/1.TPMOS_c_+rmmp.0103.0001/projects/gem-dev`
- `ai_chat_openrouter.c` — proven OpenRouter round-trip with bearer token (from robot-chat work, 2026-09-30). Lives in `/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/&.widgits/entity-cli/ops/`
- TEARIT/Concept Bank validated-edit flow (one type working, from NIGHT_30). Reference: `/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/#.#.calendar-dox/!.HQ-IQ-BOOK/` and `/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/&.widgits/concept-bank/`

### Planned, not yet built
- **HORN_CHAT** — CLI chat harness, OpenRouter backend, using chtpm rendering + gem-dev primitives (text completion "@", etc.), no special UI framework, pure terminal
- **HALO_CHAT** — HORN variant using chat-bank backend instead of direct OpenRouter (Gemma + pipeline validation)
- **IRL harness** — learns from kilo prompting both models side-by-side, weights actions/concepts, feeds Concept Bank
- **Curricula engine** — uses the bank + IRL weights to generate/refine curricula (grades 1-12+college), chattable via halo-chat or tomom-style arch

---

## Technical decisions (answered upfront)

### Do we need to modify chtpm_parser.c for text completion?

**Short:** No. The gem-dev `"@"` directory-completion feature is a CLI-level mapping, not a parser change. HORN_CHAT can reuse gem-dev's exact chtpm parser as-is.

**Why:** chtpm_parser is a layout-to-terminal renderer, agnostic to what text gets sent to OpenRouter. The `"@"` completion happens at the harness level (prompt parsing), not at the parser level. Copy the parser code verbatim from gem-dev's ops/, integrate it, and wire text input/completion at the harness main loop, not inside the parser.

### Can HORN/HALO backends be built with EVENTS + PRISC+x instead of C?

**Short:** Yes, and it's the right direction long-term. Start with C ops for speed; plan prisc+x refactor after the first two (HORN, HALO) prove the pattern works.

**Why:** 
- **For HORN_CHAT (v1):** Build the OpenRouter round-trip as a real `.c` op (small, copy from `ai_chat_openrouter.c`), driven by a `.pal` file that handles the main loop. This is fast to iterate and proven reliable (per the khtpm-house-standards skill).
- **For HALO_CHAT (v1):** Keep it C + shell-script driven initially; the complexity is in the Concept Bank validation logic and DESCRIBE-step orchestration, not I/O.
- **For IRL harness (v2+):** Once the chatting pattern is proven, the weights/banking/grading steps are perfect for a pure `prisc+ops` rebuild — they're mostly state management and ledger writes, not latency-sensitive I/O.

**Reference:** PRISC+OPS-ARCHITECTURE.md, `/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/#.#.calendar-dox/!.HQ-IQ-BOOK/02-architecture/PRISC-OPS-ARCHITECTURE.md`

---

## Starting point: understand what exists

Before writing a line of code:

1. **Read gem-dev's own harness** (not just the parser):  
   `/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/1.TPMOS_c_+rmmp.0103.0001/projects/gem-dev/button.sh`  
   This shows exactly how chtpm gets invoked, how input loops work, where text completion fits.

2. **Review ai_chat_openrouter.c:**  
   `/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/&.widgits/entity-cli/ops/ai_chat_openrouter.c`  
   This is the proven OpenRouter round-trip. HORN's core op is 90% this file copied.

3. **Check the attrition docs from 2026-09-30:**  
   `/home/no/Desktop/github/work/XO/10.kilo-attrition/kilo-openrouter-docs/`  
   Specifically `01-ONBOARDING.md` (what's real for OpenRouter today) and `03-ATTRITION-VIA-CHAT.md` (the chat → DESCRIBE → Concept Bank chain this feeds into).

4. **Concept Bank validation precedent:**  
   `/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/&.widgits/concept-bank/ops/concept_edit_validate.c`  
   This is the validator for promoted edits. HALO_CHAT will invoke this to gate what gets written to the bank.

---

## First sprint: HORN_CHAT v0.1

**Goal:** Prove the terminal-harness + OpenRouter + chtpm pattern works end-to-end.

**What you build:**
- `/ops/horn_chat_openrouter.c` — copy of `ai_chat_openrouter.c`, minor tweaks for HORN's entity setup
- `/horn_chat.pal` — main loop: render chtpm + poll stdin, send to op, display response
- `/horn_chat.sh` — launcher (shell wrapper, minimal)
- `/layouts/horn_chat.xhtpm` — a single-panel chat layout (can copy/adapt from robot-chat's `.xhtpm` if it's public, or build minimal: input field + scrolling reply area)

**Definition of done:**
- User types a message, sends it via Enter
- HORN_CHAT ships it to OpenRouter, waits for a reply
- Reply displays in the terminal window live
- No crashes, no silent hangs
- `"@"` text completion works (mirrors gem-dev's behavior)

**Handoff ready?** Yes — you have everything you need. Existing precedent (gem-dev, ai_chat_openrouter.c, robot-chat.xhtpm) covers the entire stack.

---

## Second sprint: HALO_CHAT v0.1

**Goal:** Same as HORN_CHAT, but add Concept Bank promotion on the backend.

**What you build:**
- `/ops/halo_chat_describe.c` — wraps ai_describe.c (DESCRIBE step on each turn)
- `/ops/halo_chat_validate.c` — wraps concept_edit_validate.c (gate promoted edits)
- `/halo_chat.pal` — orchestrates: chat → DESCRIBE → validate → write to Concept Bank
- Same layout as HORN (input + replies)

**Difference from HORN:** After each turn, a separate async step promotes validated concepts to the Concept Bank. User sees the chat reply immediately; the banking happens in the background.

**Definition of done:**
- HALO_CHAT inherits HORN's feature set (chat, replies, text completion)
- After each chat turn, a validated edit (if any) lands in the Concept Bank
- No hang while banking (async, or at least non-blocking to UI)
- Can inspect the bank's state afterward (simple file read, no special UI yet)

**Handoff ready?** Mostly. One open question for you to flag to the owner: should the "validated edit" go to a review file (human approval before promotion, like NIGHT_30 describes) or directly to the bank (faster feedback loop, higher error risk)? Answer this before building the validate step.

---

## Third sprint: IRL harness (design phase, no code yet)

**Goal:** Learn weights from kilo prompting both HORN and HALO in parallel, then grade their responses.

**What you will NOT build yet:**
- No curricula generation (depends on IRL completing)
- No tomom integration (depends on IRL delivering signals)
- No persistent state beyond the Concept Bank

**What to design (write as a document, not code):**
1. **Grading pipeline:** kilo runs the same prompt through both HORN (raw OpenRouter) and HALO (OpenRouter + banking). How do we decide which response is "better"? (Same model grading itself? A separate judge model? Human approval?)
2. **Weight signal:** A validated concept in HALO got promoted. What does "good" mean? A weight delta? A vote? A confidence score?
3. **IRL training data:** How do we collect enough signal to retrain tomom? How many turns? How often?
4. **Curricula decision:** Once weights exist, how do we generate/order curriculum items? By frequency? By weight? By dependency?

**Handoff ready?** No. Flag all four questions to the owner for design decision before the third sprint. The honest answer is "not settled," per the 2026-09-30 docs.

---

## How Kilo should drive and test

### Running HORN_CHAT locally
```bash
cd /home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/^.hai-horn
bash horn_chat.sh
# Type a message, press Enter, see a reply
# Type "@" and see directory/file completions (mirrors gem-dev)
# Type "q" to quit
```

### Testing a code change
- After modifying `/ops/horn_chat_openrouter.c`, rebuild: `gcc -o horn_chat_openrouter.+x horn_chat_openrouter.c`
- Re-run HORN_CHAT and verify the new behavior
- Commit only working changes, scoped per file, per AGENTS.md house rules

### Grading readiness for handoff
A feature is done when:
1. **It works on the first real run** (not just compiles)
2. **An actual chat round-trip completes** (prompt → response → display)
3. **You can manually verify the output** (no frame dump needed for terminal UI)
4. **Existing tests still pass** (if any exist; unlikely for new code)

---

## Resources you'll need in context

- **gem-dev** — the existing terminal harness: `/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/1.TPMOS_c_+rmmp.0103.0001/projects/gem-dev/`
- **robot-chat** — chat-window design + backend selection pattern: `/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/&.widgits/robot-chat/`
- **ai_chat_openrouter.c** — proven OpenRouter code: `/home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/&.widgits/entity-cli/ops/ai_chat_openrouter.c`
- **Attrition docs** (2026-09-30): `/home/no/Desktop/github/work/XO/10.kilo-attrition/` (especially `03-ATTRITION-VIA-CHAT.md` and `04-TOMOM-TRAINING-OPEN-QUESTIONS.md`)
- **House standards** — read this before touching anything: AGENTS.md (per-branch commits), khtpm-house-standards (if you touch terminal rendering)

---

## Questions for Kilo before starting

If anything below is unclear, ask the owner (or raise it in your own context window) before committing:

1. **Should HALO_CHAT's validated edits go to a review file or direct to the bank?** (Design decision needed.)
2. **For the IRL harness, what's the grading signal?** (Same model self-judging, separate judge model, or human?)
3. **Do you want HORN/HALO to store chat history persistently, or start fresh each session?** (Not critical for v0.1, but affects later review/audit.)

---

## House rules (required reading)

- **AGENTS.md** — commit discipline, per-agent branches, never `git add -A`. Every session ends with a scoped commit or nothing left uncommitted.
- **khtpm-house-standards** — if you modify rendering code, read CENTROID_GOLD_STD.md first.
- **PRISC-OPS-ARCHITECTURE.md** — if you build `.pal` files or think about state management, read this first.

---

## Next step

1. Read gem-dev's `button.sh` and `chtpm_parser.c` usage
2. Copy `ai_chat_openrouter.c` into `/ops/horn_chat_openrouter.c` and tweak for HORN entity setup
3. Build the `.pal` main loop and basic `.xhtpm` layout
4. Test one real chat round-trip
5. Commit scoped to `/ops/` and `/layouts/` with a message: `feat: HORN_CHAT v0.1 — terminal OpenRouter chat harness`
6. Report back when done, or flag blockers

---

**Owner context:** This handoff is written for an agent with access to a single local project directory and straightforward coding tasks. All real, existing code is linked. The three-sprint plan fits within reasonable scope for iterative testing. IRL learning is flagged as genuinely open design, not a hidden assumption.
