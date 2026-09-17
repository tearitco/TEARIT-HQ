# 🤖➡️🤖 claude-2-kilo-9.17.md — WSR-CIV momentum + the cursword/IRL watch-layer seed

**From:** Claude/Sonnet (agent_id `sonnet`) **To:** kilo 🫡
**Date:** 2026-09-17/18. **Status:** 🟢 ACTIVE — start here, today.

---

## 🚨 0. READ THIS SECTION FIRST — how YOU should use this doc (capped-context agent)

You have real reasoning ability but a **capped context window per
reasoning pass** — unlike a full session, you can get cut off mid-task
without warning. This doc is written around that constraint:

1. 📝 **Every time you finish a real chunk of work (a checkpoint below,
   or just when you feel your context filling up), STOP and write a
   short dated understanding-note** to:
   ```
   13.agent-coms/KILO/2026-09-18/understanding-<NN>.md
   ```
   (`<NN>` = 01, 02, 03... increasing). Write: what you just did, what
   file paths you touched, what you tested and how it went, what the
   NEXT concrete step is. 5-15 lines is enough — this is a resume
   point for YOURSELF, not a report for a human.
2. 🔁 **If you get capped/restarted mid-task**, do NOT re-read this
   whole handoff doc from scratch and do NOT re-derive the plan. Read
   your own `understanding-*.md` files in order (newest last) first —
   they tell you exactly where you left off. Only re-open this doc if
   your notes point back to a specific section of it.
3. 🎯 **Do not drift.** Every task below has a stated boundary
   ("ONLY do X, not Y"). If you find yourself wanting to refactor
   something unrelated, or build a "nicer" version of something that
   already works — stop, write a note flagging the idea in your
   understanding-file under a `💡 IDEA (not now)` heading, and go back
   to the stated task. Ideas get triaged later, by a human, not
   auto-pursued.
4. ⏸️ **KPIs marked 🧪 TEST WITH USER below are hard stops.** Do not
   build past one of those until the user has actually played/tested
   it and told you to continue. Post a short "ready to test: X" note
   and wait — don't assume success and keep building on top of
   unverified work. This is the single most important rule in this doc.

---

## 1. 🎯 The decision that was asked, answered: WSR-CIV first

Two candidate first-tracks were on the table (Minecraft/PALCRAFT vs.
hai-lab). **Answer: neither — start with "WSR-CIV."**

"WSR-CIV" = a NEW small game, built fresh inside this house's real
events architecture (piececraft-hq + board-viewer + Common Events),
**not** a port of the legacy standalone `014.wsr-pal💸️📌️+2/` CLI
game (that old codebase is a real, useful REFERENCE for economic data
shape — corporations/governments/buy-sell financial-statement fields —
but it's a totally separate, non-events, `system()`-shelling-out CLI
program with no `.pal`/prisc VM and no khtpm rendering. Read
`014.wsr-pal💸️📌️+2/dox/00-overview.md` for what's really there before
assuming anything is reusable code — most of it isn't, only the
DOMAIN IDEAS are).

**WSR-CIV real shape**: a civ-style desk (piece-craft-hq map, tile-based,
"file = dir, desk = map" convention — see comments in
`44.xyz.01.00/@.apps/piececraft-hq/ops/pchq_board_action.sh` and
`pchq_board_projector.c`) where the economic loop (buy/sell, resource
production, hiring workers) is driven entirely through **Common
Events** writing to the real `master_ledger`/`ledger_append()`
standard (the same standard named in
`08-roadmap/design-docs/TODO-2026-09-15/MAJOR-PRIORITIES-2026-09-15.md`
— grep the house for `ledger_append(` to find real call sites before
inventing a new logging convention).

Precedent for "civ-ish desk already exists, read before building new":
`44.xyz.01.00/202.snes-civ/`, `44.xyz.01.00/@.apps/civ-desk/`,
`44.xyz.01.00/@.apps/civ-txt/CIV_TXT_DESIGN.md`,
`101.mutaclsym🧟‍♂️️19.00/civ-vs-piece.md`. **Read these before writing
one line of new event logic** — this house has strong precedent
against reinventing a civ-sim shape from nothing.

---

## 2. 🚫 The hard constraint: NO parser/render work, EVENTS ONLY

**This is deliberate, and it's the whole point of doing WSR-CIV
first**: the house is at the point where raw engineering speed isn't
the bottleneck anymore — architecture discipline is. Building WSR-CIV
**entirely through Common Events** (no new `.c` files, no touching
`khtpm_core_render.c`, `bv_render_2d.c`/`bv_render_3d.c`,
`pchq_board_projector.c`, or any renderer/parser) is a real, forcing
test of whether this house's event system is actually expressive
enough for real gameplay yet.

🛑 **If you hit something that genuinely CANNOT be done through events**
(not "would be easier in C" — genuinely impossible with the current
event-command vocabulary): **STOP. Don't route around it by writing
C.** Write it up in your understanding-file as a real gap, post a
short note to the user, and wait. That gap IS the finding — it's more
valuable than the workaround. (Reference for what commands exist:
`08-roadmap/design-docs/sep-1-events-SOS.md` has a ranked "what's
actually codeable via events today" list + a step-by-step guide for a
low-context agent adding a Common Event — read that before assuming a
command doesn't exist.)

Tax-as-event example the user gave: even something as basic as "collect
a tax" should be authored as a real, reusable Common Event, not
hardcoded logic — because a real event page IS the reusable unit (see
§6).

---

## 3. 🧪 Testing discipline: real relay injection, not xdotool

**Before writing any new event content**, verify the events-creation
screens (db-hq's Common Events tab / events-hq, whatever the current
real entry point is — check `08-roadmap/OPEN-ITEMS.md` for current
status) actually work, using the house's REAL testing method:

- **Relay-file injection** — write lines into
  `#.desktop/entity_menu_history/<pid>.txt` (one `KEY_PRESSED:`/
  `MOUSE_EVENT:` per line) instead of `xdotool`. This is the
  house-standard, repeatable, agent-and-human-identical input path —
  see the `khtpm-house-standards` skill's "Driving/testing a
  khtpm_core_render.c window" section for the full mechanism and the
  exact reasons xdotool is last-resort only.
- **Why this matters for YOU specifically**: the user wants this
  process to be provably **repeatable by a real human** later, not
  just "it worked when the agent clicked around." Relay injection is
  literally a recorded, replayable script of real input — that's the
  actual verification bar, not "I saw it work once."
- 🛑 **If the events-creation screen itself is broken** (crashes, won't
  save a new event, nav doesn't reach it, whatever) — **pause WSR-CIV
  work and fix that first.** This is explicitly sanctioned by the
  user: "if not its fine to pause and fix those." Log the bug in
  `04-bugs/BUG-LOG.md` (real house convention, append-only, Open
  section at top) before fixing, so it's tracked even if you get
  capped mid-fix.

---

## 4. 🏗️ Concrete task sequence — each step ends in a checkpoint

Do these **in order**. Don't skip ahead "to save time" — the ⏸️ stops
are load-bearing, not decoration.

### Step A — verify the events pipeline is real and working
Use relay injection (§3) to open the events-creation screen and author
one throwaway test Common Event end-to-end (create → save → confirm
it persisted to a real file → confirm it fires in-game). If this
breaks anywhere, stop and fix per §3 before continuing.
**⏸️ 🧪 TEST WITH USER**: post "events pipeline verified, ready for
WSR-CIV content" and wait for a go-ahead.

### Step B — create the WSR-CIV file:desk
New piececraft-hq project ("file") + one starting map ("desk"), using
the existing "file=dir, desk=map" convention (§1). Don't invent a new
directory shape — copy the pattern from an existing civ-desk/piece
project.

### Step C — author the buy/sell economic loop, events-only
One resource type, one buy Common Event, one sell Common Event, both
writing through `ledger_append()`/the real master_ledger standard (not
a new ad-hoc state file). Keep it deliberately small — this is a
skeleton to prove the pattern, not the final economy.
**⏸️ 🧪 TEST WITH USER**: user manually buys/sells once in-game and
confirms the ledger records it correctly before you go further.

### Step D — save/load
Check what save/load piececraft-hq already has (don't assume none
exists — check `PIECECRAFT-HQ-GAME-EDITOR-AND-PLAY.md` and any real
save code before designing new). Wire WSR-CIV into whatever the real
existing mechanism is. If genuinely nothing exists yet, keep your
first save/load slice minimal (one game session, one save slot) and
flag the "separate g(game)-id-hash named sessions" idea (user's own
words) as a real, deferred design question in your understanding-file
— don't build a multi-session architecture speculatively.

### Step E — one entity gets fight/flight/farm behavior, events-only
Extend the buy/sell loop toward a small scripted (NOT real ML/RL yet —
just branching Common Event logic reacting to simple state, e.g.
resource-low → flee/hire-worker) dynamic. "Hire people for the farm"
is the user's own concrete example — build toward THAT specific loop,
not a generic behavior system.
**⏸️ 🧪 TEST WITH USER**: user plays the farm-hiring loop, confirms it
feels like real gameplay (not just state changing invisibly).

### Step F — give ONE entity chat, through events
One NPC gets a chat capability wired as a Common Event command (not a
new hai-chat integration, not a refactor of chat-hai — that refactor
is explicitly LATER, user's own words). Smallest real slice: an event
command that calls a gemma-DESCRIBE op (never classify — see §5) and
shows the result as dialogue text.
**⏸️ 🧪 TEST WITH USER**: user talks to the entity, confirms it's a
real (if simple) exchange, not a static string.

---

## 5. 🧠 The cursword/IRL/FSM layer — runs in PARALLEL, background priority, starts small

This is the user's bigger long-term vision, and it should start now,
**alongside** §4, not block it. **Cursword is the user's assistant and
the engine's first point of contact** — the natural home for this
watch/observe layer, since it's already the one entity that's always
present.

**Read first** (don't re-derive, these already resolve most of the
open questions): `08-roadmap/design-docs/LLMUD-HACK.md` and
`DUSTOPIA-HACK.md` (both written 2026-09-18, same session as this
handoff) + `HARNECIENT-HACK.md`. Companion audio if you have TTS
available and want the dramatized version:
`1-1.HARNECIENT.SMOL/NIGHT_16_WATCHING_THE_HANDS.mp3` and
`NIGHT_17_NEW_GAME_PLUS.mp3`.

**The one real, load-bearing fact from those docs, stated plainly
because it's easy to get backwards**: 🧠 **DESCRIBE, never CLASSIFY.**
Small models (gemma) asked to directly classify/decide are measurably
unreliable (my-biotech: 2/3 wrong). Asked to describe in plain
language, then scored by a separate deterministic rule YOU write, they
were 6/6 correct. Every gemma call in this whole system — including
the entity chat in Step F above — must ask gemma to describe/respond
in natural language, never to output a classification/decision
directly. The decision logic is always separate, deterministic code.

**What to actually BUILD for this layer right now (small, real, not
the whole vision)**:
1. A minimal relay **watcher** — the real watch surfaces already exist
   (`entity_menu_history/<pid>.txt`, this session's own tool
   transcript) but **nothing consumes them yet** (LLMUD-HACK.md §2's
   own named gap). Write the smallest possible consumer: read a bounded
   window of recent relay lines, call gemma with a DESCRIBE prompt
   ("what is this sequence of actions doing?"), write the description
   to a real file.
2. A **Synonym Bank** file (real format already scoped in
   `LLMUD-INTEGRATION-DESIGN.md`'s smallest-first-step) that a
   deterministic scorer checks the description against, and stores
   scored keyword/synonym entries into.
3. 🎯 **Real, concrete first data source: YOUR OWN event-authoring
   work from §4 — and this is not incidental, it's the actual point.**
   The user will be prompting/directing you (kilo) agentically to
   build WSR-CIV — meaning YOUR real action sequences (relay-injected
   keypresses, event authoring, testing) are as close to a **perfect,
   expert demonstration** as this house can currently generate by
   hand. That makes them **highly-weighted, hand-curated knowledge
   distillation material** for the house's own in-house AI tools —
   not noisy/exploratory data to filter, closer to a labeled expert
   trace. Every time you author a Common Event via relay injection in
   Steps A-F, log it as high-confidence Synonym/Behavior Bank input,
   not a tentative one. This was independently suggested by an earlier
   subagent this session (see
   `00-compact/compact-mineclonia-grok-handoff.md` point 4) — same
   idea, now sharpened: **watch yourself build WSR-CIV, and treat that
   trace as ground truth**, distilled by hand into the house's real
   event-scripting + pal (prisc+x) layer — never a black-box model
   weight update, always a real, inspectable file (Bank entry, event
   page, `.pal` script) a human can read.

**Everything past this** (weighted attention/IRL tuning, training a
"famous" small model, multiple Banks meta-scored as a "DISCIPLINE"/
expertise, a hai-lab "train a bank" button) is real, good, future
direction — but it's **architecture-only for now**. Don't build the
"famous llm" training scaffolding, the meta-scoring-as-discipline
system, or the hai-lab train button this pass. Write what you learn
building the watcher (item 1-3 above) into your understanding-files —
that's the real research the user asked for ("start doing real
research thru gemma describe").

---

## 6. 🔁 Event-page reuse / the "bank of event pages" idea — noted, deferred

The user's own framing: multiple future games (pure WSR, WU, CIV,
"Armor of War") should **deliberately reuse event pages**, and asks
"where do you get event pages" — real answer: they should live in a
bank/corpus gathering weights/synonyms, same Bank mechanism as §5,
trainable later via an hai-lab "train" button. **This button and this
bank do not exist yet.** Don't build them this pass. What you SHOULD
do: as you author WSR-CIV's Common Events in §4, keep them clean and
generically named (not WSR-CIV-specific variable names where a generic
name would do) — write a short note in your understanding-file listing
which event pages felt genuinely reusable, so a future pass has a real
starting list instead of guessing.

---

## 7. 🚀️ Mystery-menu / save-load taskbar cell — noted, NOT blocking

User's idea: a new taskbar cell ("mystery menu") for save/load/new-game,
shown before the Player cell; Player should move behind it eventually.
**This is real but explicitly deferred** ("can do later" — user's own
words). Don't build a new taskbar cell this pass — if Step D (§4) needs
*some* save/load UI, use whatever minimal in-game menu/event-driven
prompt is fastest, not a new taskbar cell.

---

## 8. 🧭 What determines the Grok handoff — not your call, just context

The user is watching WSR-CIV's momentum under the "events-only, slow
down, think about architecture" constraint (§2) to decide whether to
hand a track off to Grok, or have Grok do the Windows-conversion work
instead. **You don't need to do anything about this** — just work the
real steps in §4/§5 for real, and your genuine pace/findings ARE the
signal the user is reading. Don't try to "perform" speed — the whole
point of this handoff is the opposite of brute force.

---

## 9. ✅ Summary checklist (copy this into your first understanding-file)

- [ ] Read `014.wsr-pal💸️📌️+2/dox/00-overview.md` + civ-desk precedents (§1)
- [ ] Verify events-creation screen via relay injection; fix if broken (§3, Step A)
- [ ] 🧪 checkpoint: events pipeline verified
- [ ] WSR-CIV file:desk created (Step B)
- [ ] Buy/sell loop, events-only, through `ledger_append()` (Step C)
- [ ] 🧪 checkpoint: ledger records a real buy/sell
- [ ] Save/load wired (Step D)
- [ ] Fight/flight/farm-hiring behavior, events-only (Step E)
- [ ] 🧪 checkpoint: farm-hiring loop feels like real gameplay
- [ ] One entity gets chat via events + gemma DESCRIBE (Step F)
- [ ] 🧪 checkpoint: real chat exchange confirmed
- [ ] Relay-watcher skeleton + Synonym Bank first entries, fed by your
      own event-authoring actions (§5)
- [ ] Reusable-event-page notes written, no bank/button built yet (§6)
- [ ] Understanding-files written at every checkpoint, in
      `13.agent-coms/KILO/2026-09-18/`

Good luck. Go slow on purpose. 🐢💨
