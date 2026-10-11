# Layout studio for simple agents - strategy (menus a "dumb" AI can make)

2026-10-09. Written by claude from an owner conversation. **Strategy only; nothing here is built.** It sits on top of two docs that already exist and does
not repeat them: `18.pc-hq/IN-GAME-LAYOUTS-PLAN.md` (what an in-game layout is, phases 0-8, the `layout_op` CLI in its part 6) and
`08-roadmap/design-docs/HQ-LAYOUT-STUDIO-DESIGN.md` (the seed). Read those first.

## 1. What the owner wants

1. **Layouts and their managers can be made with scripting, visual scripts and drag and drop.**
2. The point: **future agents, including weak ones, make layouts easily.** "Dumber agents" must be able to produce a working in-game menu for a player
   (a game's start menu, shop, inventory, dwarf-fortress style job screens) without understanding the renderer.
3. We (claude's lane) build the studio. Grok builds data: books, levels, screens (see the Grok brief written the same day).

## 2. What exists (read, not all re-run today)

- A layout is a **fragment**: `<overlay src>` splice, `canvas-overlay-*` placement, generic chrome (minimize, slide), nav numbers on every interactive element.
  Phase 0 and 0b are done (2026-10-05/06); the sandbox is `@.apps/layout-studio/sandbox/` (a board template copy + `test-menu`). `@.apps/layout-studio/` has a
  `toy.pdl` and `button.sh`; **there is no `layout_op` yet.**
- The renderer's tag vocabulary is small and fixed. Counted in `khtpm_core_render.c` today: `item, cli_io, text_area, text, row, tab, tabbar, bar, canvas,
  scrolllist, module, sidebar, panel, page, window, title, grid, footer`, plus `<repeat count= bind=>` and `<overlay src=>`. A menu is mostly `item` rows
  whose `action=` is a command line, `text`, and a `cli_io` field.
- A manager is a separate process that publishes a plain `ui.txt` (key=value, `d_0_label=` style rows) and answers actions with small commands. pc-hq's
  Desk menu is the worked example: rows are `<repeat>`ed from `n_desk_opts`, a search field filters them through the same file (done 2026-10-09).
- Games already have a start/stop pattern: the **conductor** entity (`&.widgits/eden/`): menu = METHOD rows (Setup, Start, Pause, Resume, Save, Load, Stop,
  Reset), each an event page running one op; rules in `.pdl` files. Harness-proved (eden_loop 305 checks); the GUI path is **not** verified.

## 3. The gap: why a weak agent fails today

An agent asked for "a shop menu" must currently know: the tag list, that ids must be unique window-wide, that `target_id` doubles as a state key for `cli_io`,
that every interactive element needs a nav number, that a running window keeps its old binary, that windows have no X name, that `dropdown-child`
groups pin their last row, and how to prove it on screen. Each of those cost real time this week. They are tribal knowledge, not checks.

## 4. Strategy: constrain first, draw last

Make the **safe path the only path**, in this order. Each step ends with something an agent (and the owner) can see.

| # | Layer | What it is | Done when |
|---|---|---|---|
| 1 | **Catalog** | one page: every allowed tag, every attribute, every class the CSS knows, with a 3-line example each. Generated from the renderer/CSS where possible, not hand-written | a weak agent can build a menu from the catalog alone |
| 2 | **Validator** (`layout_check`) | refuses a layout that uses an unknown tag/attribute, a duplicate id, an interactive element with no label, a `repeat` bound to a key the manager never publishes, a `target_id` collision. Prints the fix, not just the error | a deliberately broken sample fails with a one-line cause; the fixed one passes |
| 3 | **Templates** | 5 starter fragments that already pass: menu (list + cancel), menu with search (the Desk menu shape), HUD strip, confirm dialog, status panel. An agent copies one and edits labels/actions | each template renders in the sandbox and has nav numbers |
| 4 | **Preview harness** | `layout_op preview <layout>`: launch the sandbox window, dump a PNG, list the nav numbers it found, close it. One command, no pixel reading by the agent | a PNG + a nav list come back from one call |
| 5 | **`layout_op` CLI** | `new / add / set / move / remove / bind / on / preview / save / load / list` exactly as IN-GAME-LAYOUTS-PLAN part 6; it edits fragment files and nothing else, and always runs the validator before saving | an agent builds and saves a shop menu from the command line (plan phase 5) |
| 6 | **Manager scaffolds** | `layout_op manager <layout>` writes a stub manager that publishes the `ui.txt` keys the layout binds, plus the action scripts as empty verbs with `exit 0` guards (an empty id must never do harm: the desk search needed exactly this) | the stub runs, the layout shows its rows, a click reaches a verb |
| 7 | **Drag and drop** | the studio window (plan phase 6): a thin editor over `layout_op`; every gesture = one CLI command | a human edits the same file with live preview |
| 8 | **Visual scripts** | event commands as blocks (the old Scratch/Blueprints idea, `#.Zarchive-2-trash/11.brainstorm/2026-09-14/`) writing event pages | a block graph compiles to the same event page an agent would write by hand |

Rule: **a layer is never started before the one above it can be used by an agent.** Drag and drop is last because it only helps if the files underneath are
already agent-proof.

## 5. Menus for players, made by a weak AI

Use case: a game's author-agent says "add a shop menu with three items". Target flow: copy template 1, `layout_op set` three labels, `layout_op bind` rows to
an items feed, `layout_op on` a click to a verb, `layout_op preview` (PNG + nav list), `layout_op save`. The agent never edits renderer C. The manager
scaffold supplies the feed. The validator refuses mistakes. The player sees a menu inside the game with nav numbers (house accessibility standard, plan 4h).

## 6. Hard rules carried over (do not re-decide)

- No new renderer, no per-layout C in `khtpm_core_render.c` (standing house rule). A layout is data.
- Every interactive element gets a nav number; a search/text field opens empty and disarmed (owner 2026-10-09).
- Only claim "works" with a fresh build, a fresh run and a PNG (AGENTS.md).
- Windows are found by size, not name; kill the 3D daemon with all siblings in one command; a rebuilt renderer needs a window restart.

## 7. Risks

- The catalog drifting from the renderer: generate it, and make the validator read the same list. Otherwise it rots in a month.
- "Weak agent" is not "no agent": layers 1-4 assume the agent can run a shell command and read a short file. Test with a fresh agent that has only the
  catalog, a template and the CLI help, and record where it fails (the delegation flywheel's locked-harness method, `project-delegation-flywheel`).
- Over-building the editor before the CLI exists: plan phases 5 then 6, not the reverse.

## 8. Questions for the owner

1. Is "dumber agent" the Groq/free-model class (the flywheel's cheap workers)? That sets how short the catalog and CLI help must be.
2. First real consumer: a **game start menu** (conductor Start/Stop), the **dwarf-fortress job screen**, or the **shop**? One of them should drive the templates.
3. Per game or per house when both define a layout named `hotbar` (plan open question): who wins?
4. Do you want the validator to block saving, or only warn at first?

## 9. Build order (proposed, claude's lane)

1. Catalog + validator (steps 1-2) - one session, with a broken/fixed sample pair as the proof.
2. Three templates + `layout_op preview` (steps 3-4) - PNG evidence.
3. `layout_op` CLI + manager scaffold (steps 5-6) - a fresh cheap agent builds the first real player menu unaided, and we write down where it stumbled.
4. Then the studio window (7), then visual scripts (8).
