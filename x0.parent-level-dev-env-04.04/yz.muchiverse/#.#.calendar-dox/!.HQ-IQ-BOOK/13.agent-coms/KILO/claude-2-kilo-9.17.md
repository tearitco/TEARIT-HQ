# 🤖➡️🤖 claude-2-kilo-9.17.md — WSR-CIV momentum + the cursword/IRL watch-layer seed

**From:** Claude/Sonnet (agent_id `sonnet`) **To:** kilo 🫡
**Date:** 2026-09-17/18. **Status:** 🟢 ACTIVE — start here, today.

📌 **This doc's own full, exact, canonical path** (copy this verbatim —
a real prior session mistyped it and lost time to a glob search):
```
#.#.calendar-dox/!.HQ-IQ-BOOK/13.agent-coms/KILO/claude-2-kilo-9.17.md
```

📌 **2026-09-18 UPDATE (read this before §1-9 below, they're still valid,
this adds a second parallel track + one new hard rule)**: user
follow-up conversation added real architecture — DSR now runs
**in parallel** with WSR-CIV (not instead of it, not queued after —
both tracks, kept explicitly separate, see new §10), a new house-wide
strictness rule (§2b), and a scaffolding task for house-specific
context-menu events (§11). Read §10/§11/§2b, they're additive to
everything below, nothing in §1-9 is cancelled.

📌 **2026-09-18 POST-MORTEM UPDATE (read this too — real fixes from
kilo's actual first session)**: kilo's real session
(`kilo-post-mortem-s17.md`, `13.agent-coms/KILO/2026-09-18/
understanding-01.md`) found real, concrete process/testing gaps this
doc had. §3 below is REWRITTEN to fix a dead-relay-file reference kilo
actually hit, plus new hard rules on checkpoint-verification and
process cleanup. Read the new §3 in full even if you read the old one
before — it changed materially, not just additively.

📌 **2026-09-18 DIRECTION SHIFT (user, Grok session — read before §11)**:
event-chat on Cursword *main* is the naive iteration. New bootstrap
path is documented in
`13.agent-coms/GROK/2026-09-17-cursword-file-inventory-chat.md`.
§11's "add AI Chat (events) to Cursword context menu, leave Chat as-is"
is **not cancelled for Chat**, but the *first* new buttons are
**File** (stub/hook only) and **Inventory**, not a second chat item.
Do not start famous/QKV/meta-transformer work this pass.

📌 **2026-09-18 PAUSE (user)**: WSR-CIV + DSR tracks in this handoff
are **paused momentarily**, not cancelled. Todo:
`12.calendar/2026-09-18/2do.md`. Do not start Step A–F or DSR menu
class until that file says resume.

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

### 2b. 🔒 NEW HOUSE-WIDE RULE (2026-09-18, stated directly by the user)

> "we should never write something in C when we could write it as a
> user reusable event, and put it in the events editor"

This is a **strictness rule for the whole house going forward**, not
just WSR-CIV. Read it precisely — there's exactly **one narrow,
explicit exception**, and it's the AI primitives themselves:

- ✅ **Allowed to be new C**: genuinely new AI-category **event-command
  TYPES** added to `44.xyz.01.00/#.ref/menu/event_commands.registry.pdl`
  — e.g. `ai_describe` (calls gemma, DESCRIBE only, never classify —
  §5's law), `ai_fsm_transition`, `ai_goap_plan`. These are base
  primitives, the same tier as existing commands like `show_text` or
  `change_hp` (real, confirmed: the registry currently has **zero** AI
  commands — Flow Control/Message/Character/Party/World categories
  exist, no `ai_*` category yet). A primitive is allowed to be C
  because it's the smallest indivisible unit — there's nothing to
  compose it FROM.
- ❌ **Not allowed to be new C**: anything that CAN be built by
  composing existing or new event commands — a specific NPC's
  behavior, a save/load flow, a context-menu action, a shop system,
  literally anything past the primitive tier. If you catch yourself
  about to write a `.c` file for something an event page could do,
  stop — that's the rule being violated, not a shortcut.
- 🚨 **User-confirmed exception for THIS handoff specifically**: kilo
  is authorized to build the new `ai_*` primitives (the ✅ case above)
  — this is a deliberate, explicit relaxation of §2's "events only"
  rule, scoped ONLY to adding new registry command types + their C
  implementation. It does **not** reopen touching
  `khtpm_core_render.c`, `bv_render_2d.c`/`bv_render_3d.c`,
  `pchq_board_projector.c`, or any renderer/parser — §2's
  parser/render ban stays absolute.

Tax-as-event example the user gave: even something as basic as "collect
a tax" should be authored as a real, reusable Common Event, not
hardcoded logic — because a real event page IS the reusable unit (see
§6).

---

## 3. 🧪 Testing discipline: real relay injection, not xdotool (REWRITTEN 2026-09-18 post-mortem)

**Before writing any new event content**, verify the events-creation
screens (db-hq's Common Events tab / events-hq, whatever the current
real entry point is — check `08-roadmap/OPEN-ITEMS.md` for current
status) actually work, using the house's REAL testing method.

### 3a. The REAL, authoritative doc for this — read it, don't re-derive from this section alone

`08-roadmap/design-docs/TASKBAR-MENU-ARCHITECTURE.md` is the actual
canonical, deep reference for the relay/dispatch architecture — this
§3 is a summary, not a replacement. It also points at
`taskbar-tpmos-parallel-refactor.md` and
`taskbar-history-txt-migration-investigation.md` for real relay-gap
history (two real relay-forwarding bugs already found+fixed, and a
real filename-collision regression already found+fixed). **Read the
real doc before trusting this section's summary of it.**

### 3b. There are THREE relay files, not one — know which applies

A real prior kilo session used all three without the handoff
documenting them, which cost real time:

1. **`#.desktop/entity_menu_history/<pid>.txt`** — per-window, format
   `KEY_PRESSED: <decimal>` / `MOUSE_EVENT: <button> <x> <y> <is_press>`.
   Use this for any specific `khtpm_core_render.c` window instance
   (events-hq, piececraft-hq board, an entity menu) once you know its
   real PID.
2. **`#.desktop/strip_history.txt`** — the taskbar MANAGER's relay,
   bare decimal code per line, consumed by
   `khtpm_taskbar_manager_main.c`'s `poll_strip_history()` →
   `dispatch_code()`. Use this for taskbar-strip-level actions (opening
   the toys menu, cell numbers, HQ-header codes).
3. 🛑 **`#.desktop/livedesk_agent_relay.txt` is DEAD — do not use it.**
   Real, confirmed by direct source read: its consumer,
   `poll_agent_relay()`, was removed when `khtpm_strip_parser.c` got
   folded into `khtpm_core_render.c` on 2026-09-01 (see the header
   comment in `khtpm_strip_keyboard_ascii.c`, lines 10-16, which says
   this explicitly: "RETARGET 2026-09-06... its poll_agent_relay()...
   went with it"). Writing to this file today is a silent no-op — **a
   real prior kilo session wrote to this exact dead path and it's not
   obvious from the symptoms alone that nothing consumed it.** If you
   see this filename anywhere (including in an old understanding-file
   you're resuming from), treat it as stale and use `strip_history.txt`
   (item 2) instead.

### 3c. 🔒 NEW RULE: a checkpoint is not "started"/"in progress" until relay-verified

A real prior kilo session declared "WSR-CIV Step A: IN PROGRESS" after
merely LAUNCHING events-hq, without ever actually injecting a key into
its history file to confirm it responds. That's not a checkpoint, it's
a guess wearing a checkpoint's clothes. **Do not mark any step
started/in-progress/done in your understanding-file until you have
actually written a relay event to that specific window's PID and
observed a real, confirmed state change** (a new frame, a changed
state file, anything real — not "the process is running").

### 3d. 🔒 NEW RULE: after "quitting" anything, `ps aux | grep khtpm` broadly

Real, confirmed architectural fact: **this house has no cascade-kill /
session-tree.** A real prior kilo session quit the entire taskbar and
found two events-hq processes (a manager + a window) plus a defunct
zombie child still alive — because each toy/app manages its own
process lifecycle independently; the taskbar is a launcher, not a
parent. After "quitting" anything, run `ps aux | grep khtpm` (broad,
not filtered to an expected process name — the kilo session's own
mistake was searching for "taskbar"-named processes and missing
`khtpm_core_render.+x`/`khtpm_events_hq_manager.+x`, which don't have
"taskbar" in their name) and kill any real orphans individually. Do
**not** use `pkill -f <pattern>` from a shell whose own command line
could match your pattern — see the `pkill -f self-match footgun` house
rule — kill by exact PID instead.

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
- 📌 **Known, real, tracked blocker as of 2026-09-18**: piececraft-hq's
  board window rendered only a thin ".main" tab with no board content
  (real, hit by a prior kilo session, PID killed without root-cause).
  This is now logged in `04-bugs/BUG-LOG.md` — read that entry before
  re-diagnosing from scratch. Before assuming this needs a C-level
  fix (banned by §2): `pchq-board.xhtpm` has recent, actively-maintained
  fix comments dated 2026-09-15 for a related dead-UI-wiring issue in
  the same file — check `git log`/`git blame` on that exact file for
  anything even more recent before concluding it's a deep unfixable
  renderer bug rather than a launch-arg or stale-binary issue (see
  `HOUSE_CODE_PITFALLS.md` #1, stale-binary, the single most common
  false "still broken" report in this house).

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

## 7. 🚀️ Mystery-menu / save-load taskbar cell — noted, NOT blocking, superseded in spirit by §10

User's idea: a new taskbar cell ("mystery menu") for save/load/new-game,
shown before the Player cell; Player should move behind it eventually.
**This is real but explicitly deferred** ("can do later" — user's own
words). Don't build a new taskbar cell this pass — if Step D (§4) needs
*some* save/load UI, use whatever minimal in-game menu/event-driven
prompt is fastest, not a new taskbar cell.

📌 **2026-09-18 update**: the user has since said new-game/save/load
should themselves be **house-specific reusable events** (§2b's rule
applied directly to this), not necessarily a taskbar cell at all — see
§10's DSR "menu" class. The taskbar-cell idea isn't cancelled, just no
longer the only candidate UI for this — don't build either
speculatively, but if you're forced to pick a shape for Step D (§4) or
DSR's menu class (§10), prefer the events-authored version since it's
consistent with §2b and reusable across WSR-CIV/DSR/future games.

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

## 10. 🏰 DSR track — runs in PARALLEL with WSR-CIV, kept explicitly SEPARATE

**2026-09-18, direct from the user, confirmed: both tracks run
together — DSR is not queued after WSR-CIV, and WSR-CIV is not
paused for it.** But they must stay **legibly separate** — different
file:desks, different understanding-file sub-threads, and every
checkpoint report to the user must say plainly which track it's for.
Do not let event pages silently blur between the two without an
explicit note that a specific page was deliberately shared/reused
(that reuse tracking IS the point — see below).

**Real launch shape, user-specified**: both DSR and WSR-CIV should
open the same way — **from a "toys" toy**, which opens/populates the
game's map, exactly like the existing `file:desk` + `player:play`
convention (§1's "file=dir, desk=map"). Long-term this is meant to
mirror how the pre-house standalone `014.wsr-pal💸️📌️+2/` worked, as a
real in-house incarnation of it — but built fresh, events-first, not
ported code (§1 still applies: that old codebase is domain-idea
reference only).

**Multi-copy reuse-testing is deliberate, not scope creep**: the user
wants **many copies/variants** — DSR, WSR, "MSR", CIV, "Armor of War" —
specifically to see whether event pages and other components can be
reused ACROSS these different game copies. This is the real,
practical test of §6's "bank of event pages" idea — don't build the
bank/train-button yet (§6 still holds), but DO keep a running,
explicit list (in your understanding-files) of which event pages you
wrote for one game and successfully reused verbatim (or near-verbatim)
in another. That list is real evidence for whether the reuse idea
actually works, before anyone invests in the formal Bank/training UI.

**DSR's own menu class** (this is DSR's actual Step A-equivalent,
parallel to WSR-CIV's own Step A in §4): populate a "menu" class with
**New Game / Load Game / Save Game / Save As** — built as a house
specific **reusable event** (per §2b, not a bespoke DSR-only screen;
the fact WSR-CIV and DSR both need this is exactly the kind of thing
that should be ONE reusable event authored once, used by both).

Then DSR's menu leads into a **setup menu** asking, per player:
> "Player 1: Human or Harness? Player 2: Human or Harness?"

"Harness" = that player's turn is driven by the AI relay-driven
FSM/GOAP engine (the `ai_fsm_transition`/`ai_goap_plan` primitives from
§2b) instead of waiting on real human relay input. Architecturally:
this is a per-player flag in session state, and dispatch routes on it
— same dispatch-by-id pattern already used elsewhere in this house
(e.g. this session's own taskbar cid-based dispatch work), not a new
mechanism. **Don't build IRL meta-learning start/stop controls yet** —
the user's own plan puts that in hai-lab's future "game" tab (confirmed
this session: hai-lab's Part 3 viewer is built, but Part 4 — the
game/chat tiers, including any start/stop-learning toggle — is
design-only, not built). Scaffold the human-vs-harness FLAG now; the
toggle UI that turns IRL watching on/off for a harness player is a
later, separate task.

**Weight/session storage — real, confirmed-workable direction**: keep
IRL histories/Bank weights as their OWN portable directory tree
(`<bank-store>/<bank-id>/`), and have each save-game session
**reference** bank ids in a plain `banks_used: <id>,<id>,...` line
(same key=value convention as everywhere else in this house) rather
than physically nesting weights inside the save-game directory. This
gives you both properties the user asked for — traceability (a session
records which banks it touched) and portability (a bank is just a
`cp -r`-able directory, movable independent of any specific save,
matching the existing pal/wsr-pal save-copy convention). Don't build
a drag-and-drop bank-management UI — that's real future work, not this
pass; a plain file move is enough for v1.

---

## 11. 🖱️ House-specific context-menu events — scaffold now, iterate, don't overthink

Real, confirmed fact: **context menus in this house are ALREADY
fully data-driven and zero-recompile** — `meta.pdl` has plain
`METHOD | <label> | <action>` rows, mechanically converted to
`menu.chtpm` by `meta_to_menu_chtpm.py`
(`44.xyz.01.00/*.monads/*.livedesk-taskbar/ops/`). This means "remove
an option, add a new one" already costs nothing architecturally — the
real, new work is making that ADDITION itself a **house-specific
event** (per the user's own framing: "add option to context menu"
should be a reusable event a dev can drop into any entity's context
menu, not a one-off manual meta.pdl edit each time).

**Direct user instruction: don't design this heavily up front — just
scaffold and iterate.** First real, concrete use case, small and
real:

- Cursword's existing "Chat" button (real, confirmed:
  `.../pals/cursword/menu.chtpm`, wired to `chat_button.sh` →
  chat-hai) **stays exactly as-is** — do not remove or replace it.
- Add a **second** context-menu option, "AI Chat (events)" or similar,
  built via: (1) the new `ai_describe` primitive from §2b (gemma call,
  DESCRIBE only, never classify), wired into (2) one small Common
  Event page, exposed via (3) a new, reusable "add option to context
  menu" event mechanism — this is the actual scaffolding task, and it
  doubles as this hack-family's SECOND real house-specific-event
  precedent (WSR-CIV's buy/sell loop in §4 was the first).
- Keep it minimal: one working round-trip (click "AI Chat (events)" →
  gemma describes something real → text shows) is the whole v1. Don't
  build a full chat history/thread UI for this — that's chat-hai's job
  already, this is a proof of the events-authored, auditable path.

---

## 12. 🎬 A new NIGHT script is warranted (user said so directly) — not kilo's job

The user explicitly said this whole follow-up conversation ("warrants
another NIGHT") should get a HARNECIENT.SMOL dramatization — the
self-referential AI-writes-AI-via-events idea (§2b, §11), the DSR/
WSR-CIV parallel-track multi-copy reuse-testing philosophy (§10), and
the new house-wide "never hand-write in C what an event can do" rule.
**This is Sonnet's own task, not kilo's** — noted here only so kilo
doesn't duplicate it or wonder why it's missing from this handoff.

---

## 9. ✅ Summary checklist (copy this into your first understanding-file)

**WSR-CIV track:**
- [ ] Read `014.wsr-pal💸️📌️+2/dox/00-overview.md` + civ-desk precedents (§1)
- [ ] Verify events-creation screen via relay injection; fix if broken (§3, Step A)
- [ ] 🧪 checkpoint: events pipeline verified
- [ ] WSR-CIV file:desk created, launched from a "toys" toy (Step B, §10)
- [ ] Buy/sell loop, events-only, through `ledger_append()` (Step C)
- [ ] 🧪 checkpoint: ledger records a real buy/sell
- [ ] Save/load wired — prefer the reusable-event shape from §10's menu
      class if you're building this before DSR's version lands (Step D)
- [ ] Fight/flight/farm-hiring behavior, events-only (Step E)
- [ ] 🧪 checkpoint: farm-hiring loop feels like real gameplay
- [ ] One entity gets chat via events + gemma DESCRIBE (Step F)
- [ ] 🧪 checkpoint: real chat exchange confirmed

**DSR track (parallel, kept separate — §10):**
- [ ] DSR file:desk created, launched from a "toys" toy
- [ ] "menu" class: New Game / Load Game / Save Game / Save As, as a
      reusable house-specific event (shared with WSR-CIV if timing
      allows — log the reuse explicitly either way)
- [ ] Setup menu: Player 1/2 Human-or-Harness flag wired into session
      state + dispatch (no IRL toggle UI yet)
- [ ] 🧪 checkpoint: user creates a new DSR game, sets one player to
      Harness, confirms dispatch actually routes differently

**Shared / cross-cutting:**
- [ ] `ai_describe` primitive built (§2b exception) — first real use:
      §11's cursword "AI Chat (events)" second context-menu option
- [ ] 🧪 checkpoint: AI Chat (events) round-trips once, for real
- [ ] Relay-watcher skeleton + Synonym Bank first entries, fed by your
      own event-authoring actions across BOTH tracks (§5)
- [ ] Reusable-event-page list kept, across WSR-CIV/DSR/future copies
      (§6, §10) — no bank/button built yet
- [ ] Bank/weight storage: portable `<bank-store>/<bank-id>/` dirs,
      sessions reference by id, no drag-drop UI (§10)
- [ ] Understanding-files written at every checkpoint, in
      `13.agent-coms/KILO/2026-09-18/`, each one tagged WSR-CIV / DSR /
      shared so tracks stay legible

Good luck. Go slow on purpose. 🐢💨
