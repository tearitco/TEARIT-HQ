# Decisions to make, testing strategy, handoff gates

---

## Critical decisions (raise these with owner before coding)

### Decision 1: HALO_CHAT edit promotion — review gate or direct?

**The question:** Once a DESCRIBE-step output validates, where does it go?

**Option A: Review file (safer, slower)**
- Validated edits land in `pending_review.txt`
- A human (or future automation) reads and approves
- Only approved edits go to the Concept Bank
- Matches NIGHT_30's current design exactly

**Option B: Direct to bank (faster, higher error risk)**
- Validated edit goes straight into the bank
- Faster feedback loop for the learning side
- Risk: wrong edits spend longer in the bank before noticed

**What the house currently does:** Option A (review file, no auto-promotion yet). This is the "honest ceiling" NIGHT_30 names.

**Recommendation:** Start with Option A (safer, matches current design). IRL harness can experiment with Option B later once we have real data on error rates.

**Raise this:** Before you start HALO_CHAT, flag which option you're building. Owner will confirm.

---

### Decision 2: IRL grading — who scores the models?

**The question:** When HORN and HALO both answer the same prompt, how do we decide which is "better"?

**Option A: Same model self-judges (fast, but risky)**
- Run both responses through the same OpenRouter model
- Ask "which is better, A or B?"
- Fast: one extra API call per comparison
- Risk: model is grading its own outputs, self-bias

**Option B: Separate judge model (slower, more trusted)**
- Keep a dedicated, larger model in reserve for judging only
- Run all comparisons through it
- Slower: extra API call, maybe a different model
- More trustworthy: external judge, not self-scored

**What the house hasn't decided:** This is genuinely open. Per `04-TOMOM-TRAINING-OPEN-QUESTIONS.md`, this was flagged as a real question, not yet answered.

**Raise this:** Before IRL sprint (Sprint 3), flag which approach you want to try. The owner may have thought about this since the docs were written.

---

### Decision 3: Session persistence — chat history stored or ephemeral?

**The question:** Should HORN/HALO remember past conversations, or start fresh each run?

**Option A: Ephemeral (simpler to start)**
- Each session is a new, isolated conversation
- No persistent `chat_history.txt` between runs
- Simpler for v0.1, matches the "prove it works once" goal

**Option B: Persistent (needed for later audits)**
- Each session writes to a history file
- Next run can read that history and continue
- Needed for IRL learning (scores are based on history)
- Needed for later audit/review of the model's reasoning

**Recommendation for v0.1:** Persistent. Even for the simplest case, history is useful. Code it in from the start (no extra work; ai_chat_openrouter.c already does this).

**Raise this:** Not a blocker, but flag if you have a strong reason to go ephemeral.

---

## Testing strategy for each sprint

### HORN_CHAT v0.1 testing

**Definition of "done":**
1. Compile without errors
2. Run `bash horn_chat.sh`
3. You type: "hello"
4. Program sends to OpenRouter
5. Program displays the reply in the terminal
6. Program is still running (not crashed)
7. Text completion works: type "@" and see file/directory suggestions (mirrors gem-dev)

**How to test:**
```bash
cd /home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/^.hai-horn
bash horn_chat.sh
# Type "hello" and press Enter
# (wait for reply)
# Read the reply in the window
# Type "quit" or Ctrl+C to exit
```

**What counts as "working":**
- One full round-trip (prompt → response) with real text displayed
- No crashes, no silent hangs
- Text completion responds (doesn't need to be perfect, just responsive)

**What does NOT count as "working":**
- Compiles but doesn't run (fix the runtime bug)
- Runs but crashes on input
- Hangs waiting for a response that never comes
- Only a partial reply displays

---

### HALO_CHAT v0.1 testing

**Definition of "done":**
1. Inherits HORN's feature set (chat, replies, completion work)
2. After each chat turn, something lands in `pending_review.txt`
3. You can read `pending_review.txt` and see a validated edit candidate
4. No hang while banking (UI stays responsive)

**How to test:**
```bash
cd /home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/^.hai-horn
bash halo_chat.sh
# Type "what is two plus two?"
# (reply displays)
# (check pending_review.txt in the background)
# Type another prompt
# (reply + another review line appears)
# Type "quit"

# Inspect the result:
cat pending_review.txt
# You should see one line per turn that passed validation
```

**What counts as "working":**
- HALO works exactly like HORN (chat ↔ reply)
- Validated edits appear in the file
- File grows with each turn (no duplicates, no losses)

**What does NOT count as "working":**
- HORN-like chat doesn't work yet (fix HORN first)
- Edits land in the file but never validate (read concept_edit_validate.c to debug)
- File grows but edits are corrupted/malformed

---

### IRL harness Sprint 3 (design only)

**Definition of "done":**
- A document (not code) that answers or explicitly flags:
  1. How models grade each other's output (self-judge vs. separate judge)
  2. What the weight signal is (how do we score a good concept vs. a bad one)
  3. How we collect enough data to retrain tomom
  4. How to generate curricula from the weights

**How to deliver:**
1. Write a design doc (`IRL-HARNESS-DESIGN.md` in this dir)
2. Flag each section as "decided" or "genuinely open"
3. If genuinely open, propose a first experiment to settle it
4. Raise it with the owner before implementing

**What counts as "working":**
- Document is clear about what's known vs. unknown
- Owner reads it and confirms the direction
- No code written yet (design only, per the handoff)

---

## Commit gates — when to commit

### After HORN_CHAT v0.1 works

```bash
# From /home/no/Desktop/github/work/NNEST-12.00/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/^.hai-horn

git status  # See what changed
git add ops/horn_chat_openrouter.c ops/horn_chat_chtpm_parser.c
git add horn_chat.pal horn_chat.sh
git add layouts/horn_chat.xhtpm
git commit -m "feat: HORN_CHAT v0.1 — terminal OpenRouter chat harness

- Copy of chtpm_parser for rendering
- OpenRouter integration via fork/exec curl
- Main loop in RISC-V assembly (.pal)
- Text completion mirrors gem-dev behavior
- One full chat round-trip tested and working"

git push origin <your-branch>  # Assuming you're on opencode or similar
```

### After HALO_CHAT v0.1 works

```bash
git add ops/halo_chat_describe.c ops/halo_chat_validate.c
git add halo_chat.pal halo_chat.sh
git commit -m "feat: HALO_CHAT v0.1 — OpenRouter + Concept Bank validation

- Inherits HORN_CHAT UI and chat feature
- DESCRIBE-step on each turn (ai_describe.c)
- Validator gates edits before promoting (concept_edit_validate.c)
- Validated edits land in pending_review.txt
- Edit approval flow: TODO (design decision pending)"

git push origin <your-branch>
```

### After IRL design doc is written

```bash
git add dox/IRL-HARNESS-DESIGN.md
git commit -m "docs: IRL harness design (Sprint 3)

- Flags open questions: grading signal, weight definition, data collection
- Proposes first experiment for each open question
- Ready for owner review and decision on next sprint"

git push origin <your-branch>
```

---

## Handoff gates — when you're done and ready to hand off

### Gate 1: HORN_CHAT compiles, runs, chats
- [ ] One full prompt ↔ reply round-trip works
- [ ] Text completion responds to "@"
- [ ] No crashes or silent hangs
- [ ] Committed and pushed to your branch

### Gate 2: HALO_CHAT validates and banks
- [ ] HORN features still work
- [ ] DESCRIBE-step runs after each turn
- [ ] Validator accepts valid edits, rejects invalid ones
- [ ] Pending edits visible in `pending_review.txt`
- [ ] Committed and pushed to your branch

### Gate 3: IRL design doc is reviewed
- [ ] Document flags all open questions honestly
- [ ] Owner has read it and confirmed the sprint 3 approach
- [ ] No code written yet (design only)

---

## Blocker resolution

If you hit a blocker:

1. **Compiler error:** Check the existing file syntax (e.g., read ai_chat_openrouter.c line by line if you copied it). C errors are usually in the copied code, not your new code.
2. **Runtime crash:** Revert to a known-working commit, add the change back more carefully. Save breakpoints in text (comments in your .pal file showing what state you expect at each step).
3. **Silent hang:** Likely waiting on OpenRouter forever. Add a timeout to the curl call. Check if your OR_MODELS list is valid (see ai_chat_openrouter.c for current list).
4. **Design question:** Flag it here. Don't guess and build on the guess.

---

**Ready to start?** Go build HORN_CHAT v0.1. Report back when it chats.
