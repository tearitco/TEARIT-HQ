# Short-Term Vision Chat
Date: 2026-06-24

This note captures my current understanding of the project direction, the main tradeoffs, the likely short-term priorities, and the questions I still need answered from you.

## What I Think Is Going On

Your long-term vision is not a set of unrelated projects. It is one system with several layers:

- `wraith` is the host shell / visual runtime.
- `chtmgl` is the GL-facing project family that should eventually run under Wraith.
- `xo-pets` is the fast simulation and controller sandbox.
- `op-ed` is the reusable editor core that should eventually support real project authoring, not just one narrow app.
- `gem-xo` / local LLM work is the agentic interface layer, which should become useful only after the surrounding state and ops are stable.
- `p2p-net` / LSR is the eventual network and identity layer for users, trading, chat, mining, and remote coordination.

My read is that you want these to converge into one environment where:

- projects can be created, edited, launched, observed, and networked
- AI can inspect frames, state files, and behavior instead of guessing
- simulations and games can evolve into larger systems without throwing away the earlier work
- reusable behavior lives in ops or PAL instead of being duplicated in managers

The current Wraith / GL track should be treated as the visual platform path, while `chtmgl-alpha` is the current milestone family proving that path.

## What The Recent Tests And Docs Suggest

I reviewed the J2 testing guide and the GL/Wraith docs. The important testing pattern is:

- test the current behavior first
- prove the file/state/frame path
- use key injection and frame review for black-box validation
- verify op-level behavior separately
- then verify a full end-to-end flow

That lines up with your broader architecture preference:

- keep project-specific behavior in the project
- keep reusable behavior in ops
- keep rendering in the parser / renderer bridge
- keep AI and controllers thin around stable contracts

The `op-ed-gl` manager also makes the general direction clearer:

- it writes project/session state
- it writes camera and selection state
- it triggers render updates from file changes
- it shows that editor-like interaction is already moving into the GL lane

That means the immediate challenge is not "invent the future."
It is "choose the smallest slice that proves the future can actually hold together."

## My Current Priority Recommendation

If the goal is to reduce future complexity, I would prioritize in this order:

### 1. Lock the Wraith / CHTMGL bridge

Reason:

- this is the most architectural work
- it sets the runtime surface for future 2D/3D apps
- it reduces the chance that later game/editor work gets trapped in a dead-end UI model

What "done enough" means:

- one or two `chtmgl`-style projects can reliably launch under Wraith
- the frame path is auditable
- state files are consistent
- the visual shell contract is stable enough that future apps can follow it

I do not mean "finish every visual feature."
I mean "prove the shell contract."

### 2. Finish a reusable editor core before making more project-specific editors

Reason:

- editor logic is the kind of code that gets duplicated badly
- once duplicated, it becomes expensive to unify later
- you already know the project family wants drop-in controllers, PAL scripting, and reusable save/load flow

My bias is:

- build the reusable `op-ed`-style core first
- then wrap it with thin project-specific entry points for `xo-pets`, `chtmgl`, or future simulations

This is the safest way to avoid baking game-specific behavior into the wrong layer.

### 3. Make `xo-pets` the proving ground for controllers, PAL, and simulation loops

Reason:

- it is the safest sandbox for experimentation
- it lets you test FSMs, AI control, selection, and generated setup without the business pressure of the larger platform
- it is likely the easiest place to prove drop-in controllers and editor-driven world changes

This is where I would prove:

- drop-in controller modules
- PAL-driven bot behavior
- save/load parity
- map or world mutation
- future AI/NPC behavior

### 4. Keep `gem-xo` and local LLM work narrow until the surrounding contracts are stable

Reason:

- a local agent is only useful if its inputs and outputs are already trustworthy
- if the project state is unstable, the LLM becomes a liability rather than leverage

Short version:

- make the tool useful as a CLI assistant first
- connect it to a local model when the interface boundary is stable
- only then begin training / distillation workflows on another machine

### 5. Build the network layer once the project contract is boring

Reason:

- `p2p-net`, login, mining, trading, and chat all become much easier to reason about once the core project/state model is stable
- if network identity is built too early, you risk locking a broken object model into the social layer

If you want the network effect, you need a compelling first loop that a second user would actually care about.

## My Short-Term MVP Reading

If I had to define a believable MVP for outsiders, it would be something like:

- one visual shell
- one editable simulation/game loop
- one shared state model
- one small networked interaction
- one clear AI-auditable frame trail

In product terms, the sellable wedge is probably not "a giant operating system."
It is more like:

- an editable, AI-auditable simulation platform
- with a visual shell
- that can host games, toy worlds, and eventually collaborative networked systems

That is easier to explain than the full long-term stack, and it is more believable to ship.

## Business / Marketing Viability

My blunt take:

- the full vision is strong but too broad to market directly as an MVP
- the MVP should be a narrow demo with a sharp hook
- the hook is not raw feature count, it is the combination of:
  - editable world
  - visible state
  - AI-auditable behavior
  - eventual multiplayer / network extension

Possible marketing angles:

1. "An AI-auditable game and simulation platform."
2. "A visual shell for editable worlds."
3. "A project runtime where code, state, and frames stay inspectable."

The most credible early path is probably a demo that shows:

- create a world
- edit it
- run a bot or simulation
- inspect the frame trail
- re-open it under the shell

If that is solid, then multiplayer, LSR, and local LLM automation become much easier to pitch.

## My Biggest Concern

The biggest risk is trying to advance too many axes at once:

- Wraith polish
- editor depth
- multiplayer/networking
- local LLM training
- simulation complexity
- 3D physics
- business scaffolding

That can create a lot of visible motion without a stable foundation.

The safer strategy is to choose one "hard spine" and one "showable surface":

- hard spine: Wraith + reusable state/ops/editor contracts
- showable surface: one game/sim/editor slice that works end to end

## Questions I Need Answered

1. What is the first thing you want a stranger to do successfully?
2. Which is more important for the next milestone: visual shell proof, editor proof, or network proof?
3. Do you want `xo-pets` to be the primary proving ground, or should `chtmgl` remain the lead demo?
4. Is your near-term goal a single-player experience that later becomes multiplayer, or do you want multiplayer-native structure now?
5. Should `op-ed` be the reusable core first, with project-specific editors on top, or do you want a dedicated `xo-pet` editor surface first?
6. How much of the AI work should be local CLI usefulness versus actual autonomous control?
7. What is the smallest "ship it" milestone you would be proud to show another person?
8. What part of the system must never be reworked later, even if everything else can move?

## My Suggested Next Step

If you want the lowest-risk path, I would do this next:

1. Keep Wraith / `chtmgl` as the shell target.
2. Finish one reusable editor/core path instead of multiplying project-specific editors.
3. Use `xo-pets` or a similarly small sandbox to prove drop-in controllers and PAL-driven behavior.
4. Keep `gem-xo` narrow and useful, not broad and fragile.
5. Postpone heavy networking and local training until the state/editor/runtime contracts stop changing.

That sequence should minimize future rewrites.

## What `fuzz-op` Teaches Us

`fuzz-op` is not just a simple pet toy anymore. It has already pushed into a fairly deep mechanics stack:

- multiple entity types and piece state
- selector-driven interaction
- z-level / map switching
- per-project session and GUI state
- mirrored UI state
- clock / turn coupling
- ops for actions like scan, collect, place, inventory, and turn progression
- AI-like behavior in the form of zombie chasing and other scripted reactions
- render pulses and frame synchronization

So `fuzz-op` is best understood as a mechanics benchmark. It shows how far the pet loop can go when you keep adding concrete game behaviors around a stable file-backed state model.

How that jives with `xo-pets`:

- `fuzz-op` is the "proven mechanics" reference
- `xo-pets` should be the cleaner next-generation sandbox
- `xo-pets` should keep the same spirit, but avoid inheriting every historical quirk
- `xo-pets` is where drop-in controllers, PAL-driven behavior, and generated setup should become first-class instead of accidental

My suggestion is not to replace `fuzz-op`.
It is to let `fuzz-op` define the upper bound of what the pet/sim loop can already do, then let `xo-pets` become the reusable version of that idea.

In practice, that means:

- keep `fuzz-op` as the "how far can mechanics go?" reference
- keep `xo-pets` as the "how cleanly can we do this again?" target
- avoid copying every old mechanic unless it helps prove the reusable contract
- move repeated behaviors into ops or PAL where possible

If you want, I can also turn this into a simple 3-column comparison table:

- `fuzz-op` = legacy-mechanics baseline
- `xo-pets` = next clean sandbox
- `chtmgl` / Wraith = visual shell and host runtime

## What I Want From You

I want your answer on two things:

- what the first public-facing or shareable demo should be
- whether the next hard push should be Wraith, editor core, or `xo-pets`

Once you answer that, I can turn this into a concrete short-term roadmap instead of a broad strategy note.

## Extra Strategic Lane: Local FSM / Mini-LLM Chatbot

We also discussed a separate but potentially important lane:

- a local chatbot with an FSM-driven interface
- a small local model in `#.standby/` or a similar offline model source
- a `gem-dev` / `gem-xo` style shell for talking to that local model
- the ability to distill recurring themes into curriculum-like "pieces" with words, weights, and structured memory

My read is that this is not the same as the visual/game/runtime roadmap.
It is a parallel intelligence lane that can grow independently if the interface boundary is clean.

That means it may deserve its own early proof-of-concept, because:

- it can be useful even before the visual shell is finished
- it can help summarize and structure project knowledge
- it may become the bridge between raw chat and more formal curriculum / memory pieces

I have not treated it as the main architecture answer yet.
I am recording it here because it could become a major supporting system rather than just a helper tool.
