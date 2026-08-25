# TPMOS Vision Onboarding
Date: 2026-06-24

This note is for an agent that needs to understand TPMOS well enough to ask the right questions, choose the right next step, and avoid over-generalizing the codebase.

## 1. Working Model
TPMOS is not one app. It is a filesystem-backed operating environment with:
- managers that own control flow
- ops that own reusable logic
- parsers that render state
- projects that own content and behavior
- PAL as the shared scripting layer when a workflow should be reusable

The default rule is simple:
- keep project-specific behavior in the project
- keep reusable behavior in ops
- keep rendering in the parser
- keep path and project resolution in a resolver/op, not scattered through managers

## 2. Current Priority Lanes

### 1. Wraith
Wraith is the visual shell direction. Treat it as the future GUI path, not a side demo.
It is meant to render TPMOS projects and games through a GL-facing surface while remaining file-backed and auditable.
The long-term vision is that current TPMOS projects can open under Wraith without changing their core behavior, as long as they expose the right state files, frame files, and controller contracts.
The current GL-facing development lane lives in the `chtmgl-alpha` / `chtmgl-beta` family, and Wraith should be treated as the broader shell/platform that can host those projects and their successors.

What it should do:
- mirror TPMOS state visually
- bridge ASCII/project state into RGB frame output
- feed a GL renderer from the RGB bridge
- remain auditable through the underlying state files and frame files
- provide a better GUI substrate for future tools
- stay compatible with the same project/session concepts used elsewhere
- support 2D and 3D project surfaces, menus, and future GL-native variants
- give AI a readable frame trail for debugging, auditing, and agentic development
- provide a consistent launch target for GL-OS-style apps and `chtmgl`-style experiments

What it should not do:
- absorb project-specific gameplay logic
- become a second copy of every manager
- hardcode path logic that should live in shared resolution ops

Current wraith-alpha code already reflects this shape:
- a parser layer that composes the view state
- an RGB daemon that rasterizes frame text into `current_frame.rgba32`
- a GL op that reads the RGBA buffer and presents it in an OpenGL window

This is the bridge pattern that future `chtmgl`-style projects should follow too: file-backed state first, render bridge second, GL view third.

So the contract is not "Wraith draws pixels manually." The contract is:
- state files describe the world
- parser emits the frame view
- RGB bridge rasterizes that view
- GL shows it
- the files remain the audit trail
- AI can inspect the frame/state trail after the fact and reason about what happened
- the same pattern can be reused for menus, simulations, editors, and 3D surfaces
- current GL milestones should be validated against `chtmgl-alpha` first, then generalized back into Wraith/GL-OS

### AI Audit Loop
For agentic development, Wraith should make debugging easier by keeping an inspectable chain of evidence:
- input state files
- parser output
- frame history
- RGB buffer output
- GL presentation

That lets an agent answer:
- what changed
- which file changed it
- what was rendered
- whether the visual output matches the state

This is especially useful for 2D/3D GUI work, menu systems, and future `chtpmgl`-style projects where the frame is part of the debug surface.

### 2. XO-Pets
XO-pets is the sandbox where simulation, crafting, and AI control can be torn apart safely.

Best near-term shape:
- 2D ASCII map with z-level support
- simple creature / tank / faction setup
- generated setup flow for different pet tanks
- fast iteration with minimal UI friction

This is the best place to prove:
- dynamic menus
- parser-driven layout changes
- op-based config generation
- later AI/FSM control loops

### 3. Local LLM Integration
Do not treat local LLMs as a magic replacement for architecture.

Best sequence:
- keep the model interface narrow
- let an op or bridge own the API boundary
- let the manager consume structured output
- only then wire the LLM into decision loops or bot behavior

If the model is not stable, the decision loop should degrade cleanly to non-LLM behavior.

### 4. Network / XO Network / P2P
This is the long-horizon shared infrastructure lane.

Use it for:
- mining / trading
- chat / social coordination
- remote control
- future distributed pet and world state

Do not couple this too early to visual shell work or editor work. It should remain reusable infrastructure.

## 3. Editor Priority
There are two editor concerns that should not be merged too early:

- `op-ed`: the sovereign piece/editor workflow
- project-specific editors or in-game editors: narrow tools for one simulation or one shell

Recommendation:
- finish the reusable `op-ed` core first where the goal is generic piece/project editing
- build thin project-specific editing entry points on top of that
- use PAL for reusable scripted behaviors across apps

If an editor feature belongs in more than one place, it should probably become an op.

## 4. The Lite Setup Path
For a non-TPMOS shell or a tiny bootstrap interface, the goal is not feature parity.

The goal is:
- select or generate a pet tank
- resolve its layout and config
- write the minimal KVP / state files
- launch the matching project cleanly

This should stay small and scriptable. It should be easy to throw away and recreate.

## 5. XO-Pets Drop-In Controllers
The XO-pets format should make controllers, editors, and automated behaviors feel like replaceable modules rather than special cases.

Desired shape:
- a controller is an op or a small bundle of ops
- an editor is a higher-level controller that writes or rewrites state
- a bot brain is preferably PAL-driven, with C used only for the thin execution boundary
- project-specific behavior remains in the project folder, not in the parser

That means a future pet tank could be launched with:
- a default controller
- a replacement editor
- an AI controller
- or a PAL script that drives the same world state without changing the engine

### Example Contract
Think in terms of a stable callable surface:
- `xo_pet::spawn_tank tank_id preset_name`
- `xo_pet::attach_controller tank_id controller_name`
- `xo_pet::set_mode tank_id mode_name`
- `xo_pet::tick_world world_id`
- `xo_pet::render_world world_id`

These names are illustrative only. The real goal is that the editor, the bot, and the game all agree on a small reusable contract.

### Example PAL Flow
Future bot behavior should be expressible as PAL, not hand-wired C loops:

```asm
; future_xo_bot.asm
start:
    OP xo_pet::set_mode "tank_01" "autonomous"
    OP xo_pet::attach_controller "tank_01" "pal_brain"
    sleep 50
    OP xo_pet::tick_world "world_01"
    OP xo_pet::render_world "world_01"
    j start
```

The important part is not the exact opcode names. The important part is the pattern:
- PAL chooses the behavior
- ops do the work
- the manager stays thin
- the parser only reflects state

### Editor Slot
A future editor should be able to plug in the same way:
- it can create a tank
- it can swap controllers
- it can author PAL
- it can export state
- it should not need special parser hacks to exist

## 6. What An Agent Should Ask When The Request Is Ambiguous
Ask these in order:

1. Which lane is this for: wraith, xo-pets, local LLM, network, or editor?
2. Is this a reusable change, or a project-specific one?
3. Should this live in an op, in PAL, in the manager, or in the parser?
4. Does it need to be compatible with current TPMOS only, or also liz / future variants?
5. Is the main goal behavior, rendering, setup, or persistence?

## 7. Default Sequence For New Work
When the user just wants progress and not a full architecture debate:

1. Fix the narrow broken path first.
2. Move repeated logic into an op.
3. Keep the parser dumb unless rendering truly needs new rules.
4. Add a simple smoke test project or layout.
5. Only then generalize.

## 8. Success Criteria
A change is probably correct if:
- the manager gets simpler
- the same logic can be reused from another project
- project/session paths resolve cleanly
- the UI reflects the real state
- the feature can be tested with a small demo project

If a change makes a manager larger, more specific, and harder to reuse, it is probably going in the wrong direction.
