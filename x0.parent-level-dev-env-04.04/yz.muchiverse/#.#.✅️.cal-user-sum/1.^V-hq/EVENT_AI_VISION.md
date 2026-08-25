# Event & Entity-AI Vision — Long-Range Design Intent

**Status:** VISION/INTENT ONLY — nothing in this doc is built. Written to capture direct instruction
before any of it is designed in detail, so scope doesn't drift or get re-explained. Cite this doc when
deciding what a new event type or entity-AI feature should look like; don't build past what it says
without checking back.
**Date:** 2026-08-12
**Owner:** claude-0001

---

## 0. Where We Actually Are (don't overstate progress)

Confirmed working (EVENTS_RUNTIME.md): a single-page, single-trigger (`on-click`) numeric-state event
(Change Gold), triggered via right-click "Play" or `RUN_METHOD:Play` relay injection, verified
end-to-end. That is the ENTIRE proven surface as of this doc's writing. Everything below is intent —
none of it has a line of code yet. Treat "does X work" questions accordingly.

The immediate, concrete blocker standing between today's proven slice and everything below:
**`play_event.sh` is hardcoded to run `page_1` only, unconditionally.** No trigger-awareness, no
multi-page dispatch. This has to be fixed before ANY of the richer event types below can coexist on
one entity.

---

## 1. RPG Maker's Real Trigger Taxonomy (align with this, don't reinvent)

Direct instruction: align our framework with RPG Maker's typical start conditions "as much as
possible for easy scaffolding and assumptions." RPG Maker MV/MZ's actual event page trigger types
(verified against known RPG Maker documentation, not guessed):

| RPG Maker Trigger | Real Semantics | Our Rough Equivalent Today |
|---|---|---|
| **Action Button** | Player faces the event and presses the interact/confirm key | `on-click` (already exists, `condition.pdl`) |
| **Player Touch** | Fires when the PLAYER's sprite steps onto the event's tile | Not built — needs real tile/collision movement |
| **Event Touch** | Fires when the EVENT (NPC) moves into the player's tile | Not built — needs entity movement + collision |
| **Autorun** | Fires automatically the instant its page's conditions become true; BLOCKS all other input/movement until it finishes | Closest to a future `on_spawn`, but must add the "blocks input" semantic — not built |
| **Parallel** | Runs continuously in the background every tick/frame, does NOT block player movement | Not built — needs a real tick loop per entity, not just click-driven dispatch |

**Also relevant — RPG Maker's page-switching model** (directly useful for the decision-tree/FSM ask
in §3): an event can have MULTIPLE pages, each with its own trigger AND its own activation
conditions (switches, variables, self-switches, item/actor state). Only the highest-numbered page
whose conditions are currently true is "active" at any moment — this IS a simple FSM: the "state" is
whichever page's conditions currently evaluate true, and switching game state (a switch flip, a
variable change) is how the "event" transitions between pages/states. **Our own page-based
`event_pkg/pages/page_N/` structure already loosely mirrors this shape** — what's missing is (a) the
condition-evaluation step that picks WHICH page is active, and (b) self-switches (small, per-event
local boolean state, RPG Maker's traditional way of tracking "have I already talked to this NPC").

**Design implication:** don't invent a new state-machine format for entity AI (§3) from scratch —
model it as "which page is active" exactly like RPG Maker does, reusing the page structure we already
have. This keeps event authoring (dialogue, item events) and entity AI (behavior states) in ONE
consistent mental model instead of two.

---

## 2. Message / Input Events — "Self-Bootstrapping" via CHTPM Reuse

Direct instruction: events that show messages and allow user input should be built by essentially
**re-deriving chtpm's own layout/navigation system** — a message box with focus, a `[>]` cursor over
choices, digit-jump, Enter to select — the SAME navigable-layout pattern used everywhere else in this
house, not a bespoke dialogue UI built from scratch.

### What this means concretely
- `Show Text` / `Show Choices` / `Input Number` events (RPG Maker's own Message category, see
  `#.ref/menu/event.commands.1.txt` lines 2-7) should render as a `.chtpm` layout — a `<panel>` with
  `<text>`/`<button>` elements, navigated the exact same way khtpm's own popups are (arrow-equivalent
  digit-jump, Enter to activate, Escape to cancel).
- This is **"self-bootstrapping"** in the sense that the event system, once it needs interactive UI at
  all, ends up re-implementing (a scoped version of) the same parser/layout/navigation machinery
  already proven in `khtpm_strip_parser.c` — not a coincidence, a deliberate reuse. Before building a
  new one from scratch, check whether `khtpm_strip_layout.c`'s existing `LayDoc`/`lay_*()` functions
  can be reused directly (parsing/hit-testing/navigation) rather than re-implemented for events.
- Consistent with the STANDING RULE already in HANDOFF.md (check local chtpm before inventing new
  UI/state shape) — this is that rule applied to the biggest surface yet (a whole new UI feature, not
  a single new attribute).

### Not yet decided (don't guess, flag when this comes up)
- Does a message/choice box render in the ENTITY's own window (`tp_desktop_window.c`), a shared
  overlay, or a new small window type? Depends on whether messages need to work while multiple
  entities are visible at once.
- How does a `Show Choices` event's selected branch map back into `event.ir.pdl`'s NODE structure —
  does a choice fork into different subsequent NODEs, i.e. does event.ir.pdl need real branching, not
  just a flat NODE list?

---

## 3. Entity Independence via AI — Movement, Interaction, Decision-Making

Direct instruction: events covering entity movement, interaction, and decision-making
(trees/behavior-trees/FSMs) that are either self-triggered (autonomous NPC behavior) or user-aware
(reacts to player proximity/action) — usable both as autonomous NPCs and, in tactics-style games, as
player-controllable party members. Eventually should be able to mimic capabilities of **agent-45**
and **SCM** (existing house projects, referenced precisely below, not vaguely).

### What agent-45 actually is (checked, not assumed)
A tool-using autonomous entity with a real tool-calling loop: `list_dir`, `connect_op`,
`send_message`, `switch_model`, `check_response`, `cmd_exec`, `compose_frame`, `edit_file`,
`search_in_files`, `json_parser`, and more (`045.muchi-pal-agent🤖️+1++/`). The relevant pattern for
entity AI: **an entity that can be given a small, well-defined SET of real tools/actions, and decides
which to invoke** — not a single hardcoded behavior. For desk-entity AI, the equivalent "tool set"
would be things like `move_toward(target)`, `say(text)`, `change_state(name)`, `attack(target)` — a
small, closed action vocabulary, not open-ended code execution.

### What SCM actually is (checked, not assumed)
Student-Curriculum Model (`047.scm🎓️+1/`): NOT a language generator — a **selection policy** that
picks which pre-written phrase to say from a small, human-authored "curriculum" bank, biased by
plain-text weights, kept coherent by an **FSM/bandit skeleton**, graded by a "describe, don't
classify" judge (a small model can be trusted to DESCRIBE an outcome, not to directly RATE/classify
it — direct rating silently poisons the reward signal). Locked house philosophy relevant here:
**deterministic routing FIRST, model/judge only as fallback** — never trust a small model's
structured self-output directly.

### Design implication for entity AI
This maps to a very specific, house-consistent shape for autonomous NPCs:
1. **FSM/BT skeleton first** (deterministic, page-based per §1's design implication — "which page/
   state is active" IS the FSM state).
2. **A small, closed action vocabulary** per entity (agent-45's tool-loop pattern) — `move`,
   `interact`, `say`, `change_state`, not arbitrary code.
3. **Phrase/dialogue selection via a curriculum-like bank** (SCM's pattern) for anything resembling
   "what does this NPC say" — pre-authored options selected by policy, not generated per-line.
4. **User-aware vs. self-triggered** is just which trigger fires the FSM transition: `on-click`/
   `Player Touch`/`Event Touch` = user-aware, `Autorun`/`Parallel` = self-triggered (ties directly
   back to §1's trigger taxonomy — no new mechanism needed, just the existing triggers driving FSM
   transitions instead of a flat command list).
5. **Player-controllable mode** (tactics-game party members): the SAME entity/FSM, but with `on-click`
   opening a player-driven choice menu (§2's message/choice UI) instead of the FSM picking
   autonomously — i.e. player control is "which system chooses the FSM's next action," not a
   separate entity type.

### Explicitly NOT yet decided (flag before building)
- Whether entity movement needs real tile/grid collision detection in `tp_desktop_window.c` (a
  genuinely new capability — currently entities don't move on their own at all) or whether "movement"
  starts as a simpler teleport-between-named-positions model.
- Whether the FSM/BT state lives in `condition.pdl` (extended) or a new file — needs a real decision,
  not a guess, once this is actually being built.

### The "ai" header cell (added 2026-08-12) — this vision's future real seat in livedesk

Direct instruction: khtpm's taskbar is getting a new header cell, "ai", inserted between `network`
(cell 13) and `date/time` (bumped from 14 to 15) — cell 14 is now `ai`. As of this writing it's a
bare inert placeholder (no submenu yet), following the exact same "confirmed inert cell" pattern as
`palettes`/`edit`/`db`/`plugins` — see `khtpm_taskbar_manager.c`'s own inert-cell comment for that
precedent. **Its real, stated destination: a capable CLI agent**, drawing on the two house projects
already referenced above — `045.muchi-pal-agent🤖️+1++/`'s tool-calling loop (agent-45) and
`047.scm🎓️+1/`'s deterministic FSM/bandit-driven selection policy (SCM) — not a from-scratch design.
When this cell gets real behavior, look at those two projects' own proven shape FIRST, the same way
every other "check before inventing" decision in this doc works.

### The delegation pattern this task itself demonstrates (direct instruction to record it)

Direct instruction: this is also meant to be documented as **the actual pattern for how users and
agents will build out AI features and delegate subtasks going forward** — not just "we added a menu
button." The concrete, real contrast worth preserving:

- **The palette-picker task (see `a12.opencode-prompt.md`'s own real-result note) failed**: handed to
  a fresh agent as a broad, multi-part, open-ended prompt ("build a picker, handle RPG Maker
  animation, design a stable-ID architecture, design a migration tool, learn the house conventions").
  It burned its ENTIRE usage budget on exploratory research and produced zero deliverable.
- **The "ai" cell task succeeded — confirmed, not assumed.** Handed to a subagent as a TIGHT,
  MECHANICAL, already-fully-specified task: exact files named, exact current state confirmed and
  stated up front (no research needed to discover it), an exact existing pattern to copy (the
  menus/store/network renumbering done earlier the same night), explicit things NOT to touch, and a
  concrete relay-based verification command to run at the end. Real result: 4 files edited exactly as
  scoped, clean build, real relay proof (`nav 14` → new `ai` cell, `nav 15` → `date/time` correctly
  shifted) — **24 tool calls, ~50K tokens, ~4.5 minutes.** Compare the palette task's ENTIRE usage
  budget burned on research with zero deliverable. Same subagent capability, wildly different outcome
  — the only variable that changed was how tightly the prompt was scoped.

**The reusable lesson:** when delegating an AI/feature subtask (to a subagent OR to the future "ai"
cell's own agent, once it exists) — do the scoping and fact-finding yourself FIRST, then hand over a
prompt that reads like a checklist with the research already done, not an invitation to explore. A
prompt that asks an agent to "understand X, then design Y, then build Z" invites the palette failure.
A prompt that says "here is the current state (verified), here is the exact change, here is how to
prove it worked" gets a real deliverable back.

---

## 4. Network / MMO-Participation Events (speculative, future)

Direct instruction: "we may invent some events that deal with network participation, so mmo/network
friendly entities can be made etc." Explicitly speculative and future — not scoped, not designed, no
target file/format yet. Logged here so the intent isn't lost, not because there's a plan. Revisit once
the single-player event/entity-AI surface (§1-3) is actually working; don't design network events
against an unstable foundation.

---

## 5. Palettes — A Hard Requirement for Demo Games, Not an Afterthought

Direct instruction: "when u build the game, i want u 2 use pallets, to make sure users can use it and
that we have autonomous harnesses ready to sell." Two distinct reasons stated:
1. **User usability** — a demo game built entirely from hardcoded content proves nothing about
   whether a REAL user could build their own content with this system. Palettes (tile/asset picker)
   are how a user actually assembles a desk/game, not just how a developer hand-authors one.
2. **Sellable harnesses** — the autonomous test harnesses built alongside each demo game need to be
   polish-grade and demoable, not just internal proof-of-concept scripts.

**Implication:** Task 4 from `2do-au11.txt` ("Palette population UI — asset picker, using
`201.rpg-maker-clone/src/tileset.c` and the emoji glyph pipeline") may need to happen BEFORE or
ALONGSIDE the first demo game, not after. Don't build `desk-shop` entirely by hand-authoring
`.pdl` files if a palette tool should exist to do that authoring instead — check whether Task 4's
scope overlaps with what the first demo game actually needs before starting either.

### ⚠️ Revised, 2026-08-12: RPG Maker tiles ARE the v1 asset source — with a mandatory swap-out path

Direct instruction, corrected from an earlier draft of this doc: real RPG Maker MV tile assets (a
real TSOTS RPGMV asset dump exists on a separate mounted drive, `/media/no/b7ced73c-.../home/jbez/
Desktop/XVS.🕹️xoGames-STICKY]🕹️/RMMV_TSOTS]LINUX=elf.../www/img/` — exact path has unicode
punctuation that doesn't survive manual retyping reliably; locate it fresh via `find` rather than
copy-pasting a remembered path) ARE fine to use as REAL v1 palette content, not just a legally-fraught
placeholder to avoid. The actual requirement is narrower and more specific than "never touch RPG
Maker assets":

**Build the system so RPG Maker tiles can be swapped for same-size generated tiles later, once the
diffusion pipeline (see below) can produce them — the swap must be a data substitution, not a
redesign.** Concretely: the palette/tile data model must reference tiles by a STABLE ID/slot
(independent of which literal PNG currently backs that slot) and must record each tile's exact pixel
dimensions, so a future generated tile of the same size can drop into the same slot with no changes
to anything that references it (desk tile-grids, event triggers tied to a tile, etc.). Also build
(or at least design) the actual migration TOOL as part of this work — not just leave the door open
for a future agent to build one from nothing.

**`201.rpg-maker-clone/src/tileset.c`** (referenced in Task 4) is tile-RENDERING/slicing engine logic
— confirm whether it also bundles any actual tile PNGs (if so, those specific bundled images are the
ones to be cautious about long-term; the real TSOTS asset dump above is the actual v1 content source).

**Animation requirement (direct instruction, 2026-08-12):** RPG Maker tiles include animated tiles
(RPGMV's convention: certain tileset PNGs encode multi-frame strips — e.g. water/waterfall tiles
cycle through a fixed set of frames at a fixed rate) — the picker UI and the desk/map renderer BOTH
need to understand RPG Maker's real animated-tile convention (frame count, frame layout within the
tileset image, playback rate) to render these tiles correctly, especially once "Play" is pressed and
the desk is live. Research RPG Maker MV's actual autotile/animated-tile format (frame strip layout,
typically found in specific tileset sheet positions) before designing the renderer — don't guess at
the frame layout.

**Scope note:** the generative tileset pipeline itself (see `#.ref/2.diffusion.fast]...]cl++/` above)
remains a big, separate future feature — this task's job is to architect the swap-out path and build
the migration tool's design/skeleton, not to finish the generator. The RPG Maker tiles are real,
current, v1 content; the generated tiles are the eventual replacement once that pipeline exists.

### Existing Starting Point — NOT From Scratch (checked, not assumed)

Two real, working, from-scratch C diffusion pipelines already exist under `#.ref/` — this is a real
head start, not a "someday, from zero" feature:

- **`#.ref/2.diffusion.fast]🖼️🖌️🎨️]c4]cl++/`** — image diffusion in C, OpenCL-accelerated (AMD/ROCm).
  `neural_net.c` (shallow U-Net: convolutions, patch-based attention, group norm, GPU-offloaded 3x3
  conv), `noise_schedule.c` (DDIM reverse diffusion, linear beta schedule, 50 timesteps), plus real
  upscaling/sharpening passes (`5.sharper_image]a0.c`, `6.upscale]real]a0.c`,
  `4.output_enhance]a1.c`). **Current mode is conditional image ENHANCEMENT/denoising** (takes a real
  input image + edge map, blends noise predictions) — not yet unconditional "generate a novel image
  from pure noise." Its own `#.image_generation_guide.txt` already documents the exact path from here
  to true generation: add a `generate` mode seeding pure Gaussian noise, add timestep embedding, drop
  the input-image dependency, optionally add class conditioning. This is a real, concrete, already-
  planned next step, not a research question.
- **`#.ref/3.diffusion.mp3🎹️]c4]a0/`** — the same architecture pattern applied to audio (`neural_net]
  mp3.c`, `noise_schedule]mp3.c`, `raw2mp3]a0.c`, trained against real `.mp3` files in its own
  `music/` folder). **Explicitly flagged by its own author note (`wussup🎺️.txt`) as rushed and "not
  doing it right" yet** — a quick copy of the image pipeline's approach, meant to be revisited
  properly later, not currently trustworthy as a finished reference. Relevant here mainly as a second
  data point on the same underlying architecture, and as a reminder that "we already built a
  diffusion pipeline once" extends to audio too (relevant if game music/SFX ever needs the same
  no-external-assets treatment as tiles) — not as something to build the tileset generator FROM.

**Implication:** the tileset generator doesn't start from zero — it likely starts from forking/adapting
the IMAGE pipeline (`2.diffusion.fast]...]cl++/`), following its own guide's documented generation-mode
upgrade path, then adding pixel-art/tiling-specific constraints (seamless edges, fixed small grid size,
limited palette) on top. The MP3 pipeline is a secondary reference, not a template to copy given its
own flagged immaturity.

---

## Priority Order (my own read, confirm before treating as locked)

Given §0's honest state (only one proven event shape exists) and the dependencies above:
1. **Multi-page/multi-trigger `play_event.sh`** — everything else in this doc depends on an entity
   being able to run more than one page/trigger at all.
2. **One new, different event command** (`Show Text`, simplest) proven through the SAME pipeline as
   Change Gold — proves the compiler/wrapper pattern generalizes beyond one hardcoded op.
3. **Message/choice UI (§2)** — needed the moment `Show Text`/`Show Choices` need to actually display
   something, not just log to a ledger file.
4. **Simple FSM via page-conditions (§1's design implication)** — smallest real slice of "entity
   independence" (§3), before movement/collision is attempted.
5. **Palette tooling (§5)** — parallel track, needed before/alongside the first real demo game.
6. Movement/collision, tactics-style player control, network events — later, once 1-5 are proven.

---

**Owner:** claude-0001
**Status:** Vision captured, nothing built. Next concrete step (per priority order above): multi-page/
multi-trigger support in `play_event.sh`.
