# cursword — the user's "SOUL" — vision doc (2026-08-27)

Direct instruction: "the cursword is the users first entity, free and
always there, unkillable, tied to their account as their assistant and
'SOUL' so thats an important feature that deserves its own document."
This is that document — vision/design, most of it NOT built yet (see
Status column per capability). Cross-reference:
`MARKETABLE-FEATURES.md` (current real state: text chat works, TTS/
dedicated chat UI don't yet) and `GAME-READINESS-GAP-ANALYSIS-2026-08-27.md`
(the message-box/turn-suspension design this doc's §3 resolves).

---

## 1. Identity — what makes cursword different from every other pal

- **The user's FIRST entity** — exists from account creation, not
  spawned/bought/crafted like other pals.
- **Free** — no cost to have or keep, unlike Store-purchased pals
  (once the Store is real — see `MARKETABLE-FEATURES.md`'s honest
  "empty scaffold" status today).
- **Always there** — not closeable the way a normal entity window is
  (`CLOSE` via `interact_relay.txt` removes a normal pal's DESK row and
  registry entry per the per-entity relay contract — cursword should
  NOT support that same destructive path, or should intercept it).
- **Unkillable** — immune to whatever kill/reset mechanisms apply to
  other entities (e.g. the taskbar's `player` cell's real
  `livedesk:reset-entities` — "kill entities then relaunch them fresh,"
  built this session — cursword must be explicitly excluded from that
  sweep, not just skipped by accident).
- **Tied to the user's account** — one cursword per account/xyzfs user
  (`xyzfs/users/<uuid>/home/livedesk/pals/cursword/` already IS this
  structurally — the account-binding is already real by directory
  layout, not aspirational).
- **The user's "SOUL"** — the framing distinguishes cursword from a
  utility pal: it's the one entity meant to represent/assist the user
  themselves, persistent across every file/desk/session the user has
  (see `OWNER-REPORT-2026-08-27.md`'s file/desk-switching finding —
  cursword's own state today lives in a fixed pal directory, not
  session-scoped, which already happens to match "always the same
  cursword no matter which file/desk you're on" — worth confirming this
  is deliberate, not accidental, given how much of this house's other
  state IS session or common-event scoped).

**Real status today**: the identity/binding is structural and correct
(fixed pal dir per account) but the "unkillable"/"can't be closed"
protections above are NOT confirmed built — this doc flags them as
required, not yet verified present in `tp_desktop_window_rgb.c`'s
`CLOSE` handling or `khtpm_taskbar_manager.c`'s reset-entities sweep.

---

## 2. Capability roadmap

| Capability | Status | Notes |
|---|---|---|
| Text chat | **Real, working** | Per `MARKETABLE-FEATURES.md` — text-based interaction confirmed real today. |
| Speech-to-text (user talks, cursword hears) | **Not built** | No STT pipeline found anywhere in this house's code this session. |
| Text-to-speech (cursword talks back) | **Not built** | Already flagged as a known, honestly-disclosed gap in this session's marketing video and `MARKETABLE-FEATURES.md`. |
| Image generation | **Not built** | No image-gen backend/call found. |
| Sound generation | **Not built** | No sound-gen backend/call found. |
| Automation for the user (UI navigation + actions) | **Not built — see §3, this is the new idea from this session** | The Gemma+FSM/BT+RL concept below. |

None of the unbuilt rows above were independently re-verified with a
fresh code search in this pass beyond what this session's own
`MARKETABLE-FEATURES.md` survey already covered for chat/TTS — treat
"not built" for image/sound-gen and STT as the honest default absent
evidence, not as an exhaustively re-confirmed negative.

---

## 3. Automation: Gemma + FSM/BT + RL driving the real UI (NEW idea, confirmed undocumented elsewhere — see below)

Direct instruction: "we also wanna teach cursword using gemma, to
navigate the interface, and take actions, even if its just triggering
fsm's/bt's and training the rl that feeds information to the gemma
harness." A dedicated research pass this session confirmed this exact
combination (cursword + Gemma + FSM/BT + RL, operating the house's own
UI) is **not documented anywhere else in this repo** — the individual
pieces exist separately (Gemma LAN inference via `net/qwen.sh` and the
`connect_op.+x`/`json_parser.+x` pattern used throughout my-biotech/
my-lawyer/chat-hai; FSM/BT design language in `EVENT_AI_VISION.md`,
scoped there to game NPC event AI, not UI navigation; the `decision_mode`
chassis in `%.harnesses/xo-human.md`, scoped to NPC/game-entity
decisions, not UI operation) — this is the first place they're
connected to cursword specifically.

**Shape, as described, to be scoped further before building**:
- Gemma is the decision layer: given a goal/instruction, it decides
  WHAT to do next in terms the house already understands.
- It doesn't need to emit raw relay codes directly — "even if it's
  just triggering FSMs/BTs" means the real, safe starting scope is
  Gemma selecting from a small menu of pre-built Finite-State-Machine
  or Behavior-Tree actions (e.g. "open db-hq," "navigate to Common
  Events sidebar," "click item N") rather than free-form UI control —
  same "deterministic dispatch, not free-form LLM output" doctrine
  already proven necessary elsewhere in this house (my-biotech/
  my-lawyer's own design docs: "gemma3:270m can't reliably follow a
  structured format," "deterministic tool detection before any LLM
  call").
- The FSM/BT layer is what actually calls the REAL relay/history-file
  mechanisms this session tested extensively (`db_hq_history.txt`,
  `interact_relay.txt`, etc.) — Gemma picks the node/action, the
  FSM/BT executes it through the house's own real, already-proven
  input surfaces, not a new bypass mechanism.
- The RL component "feeds information to the Gemma harness" — read
  literally, this is a context-selection/scoring layer (which
  observations/state facts get put in front of Gemma for its next
  decision), similar in spirit to my-biotech's own "lightweight,
  non-trained weighted-random selection" RL framing (`MY_BIOTECH_
  DESIGN.md`, explicitly NOT IQABOD's heavier trained-embedding
  system) rather than a full trained policy from day one. Training a
  genuine learned policy later is a separate, much larger undertaking
  once the basic FSM/BT-driven loop is proven.

**This is scoping, not a build plan** — real next step is a dedicated
design pass (own doc or a section added here) picking: which FSM/BT
action library to start with, how Gemma's decision gets validated
before dispatch (same "hand-written scorer over real output" doctrine
as `mylawyer_judge_worker.c`'s `classify_comparison()`), and what the
RL layer's real, minimal first version looks like (a scoring heuristic,
not a trained model, matching every other "RL" in this house so far).

---

## 4. Message-box / turn-suspension design (resolves gap #0 from `GAME-READINESS-GAP-ANALYSIS-2026-08-27.md`)

Direct answer, from the user, to this session's own flagged open
question ("where does a message box render, and does it need to
suspend the Parallel-trigger tick loop?"):

**Two real play styles, different suspension rules:**
- **Continuous** (the game clock ticks in real time, e.g. Autorun/
  Parallel-trigger-driven play): a message box/blocking popup SHOULD
  stop the time loop while it's showing, UNLESS it's explicitly marked
  a **non-blocking popup** (a deliberate exception style, matching
  this house's own existing game-clock ticker concept —
  `khtpm_taskbar_manager.c`'s real `livedesk:clock:*` commands already
  support pause/resume — a blocking message box pausing that same
  ticker is the natural mechanism, not a new one).
- **Turn-based** (play advances by discrete player turns): a message
  box is simply PART of the player's current turn, not a separate
  suspension state — and should carry a real, author-settable variable
  for whether showing it CONSUMES a turn or not (some messages are
  "free" flavor text within a turn already in progress, others might
  deliberately cost a turn, e.g. a forced dialogue that ends the
  player's turn immediately).

**Practical effect on `common_events_manager.c`'s real tick loop** (the
Parallel-trigger cooldown mechanism built this session): the pause
mechanism should reuse the EXISTING clock pause/resume commands rather
than inventing a second, message-box-specific pause flag — a blocking
message box in continuous mode is just another reason to call
`livedesk:clock:pause`, the same real command already used for other
pausing needs. Turn-based mode doesn't need this at all, since there's
no real-time ticker to suspend — the message box's turn-cost variable
is the only new piece of state needed there.

This is a real design decision, not yet implemented — `common_events_
manager.c` does not currently check any clock-pause state before its
Parallel cooldown logic runs; wiring that check is real follow-up work
once Show Text/Show Choices themselves are being built.

---

## 5. Tiles/palettes correction (per direct user note: "there is already some documented guidance on 'palettes'")

`GAME-READINESS-GAP-ANALYSIS-2026-08-27.md` §1 said tile/map-authoring
was "0% built... no tile/tileset/chipset concept... needs a fresh
design." That undersold real, existing groundwork, found on a follow-up
check:

- `&.widgits/palettes/pallets.pdl` already has a real `rmmv` (RPG Maker
  Tiles) category, a real chemistry-tiles category (see
  `MARKETABLE-FEATURES.md`), and several other tile-family categories
  (piececraft/cdda/df/kenney) — the palette system's category shape
  already anticipates tile/tileset content, it's not purely UI-theme-only
  as the gap analysis implied.
- `&.widgits/tile-picker/ops/tp_rmmv_character_extract.c` is a real,
  compiled tool for extracting RMMV-style character tiles.
- `&.widgits/event-editor/gl_mock/RMMV_EVENT_EDITOR_GUIDE.md` is a
  real, dated (2026-07-28) design/status doc for an RMMV-style GLUT
  event editor shell: **UI chrome is built** (layout, nav, digit-jump,
  Commands/Scratch toggle, page tabs) but its own status table is
  explicit that **product logic is mostly not wired** — "Edit
  event.pal/IR/switches for real: NO," "event_run (runtime of
  scripts): NO," Save/Load/Import/Export are stubs.

**Corrected framing**: this is not a blank-slate gap needing invention
from nothing — there's a real design precedent (the RMMV guide), real
extraction tooling, and a real palette category shape already pointed
at tile content. What's still missing is the same as the gap analysis
already said: actual map-grid/tile-placement authoring and the
event_run wiring to make any of it playable. Read
`RMMV_EVENT_EDITOR_GUIDE.md` directly before scoping new tile/map work
so it builds on this existing chrome rather than starting over.
