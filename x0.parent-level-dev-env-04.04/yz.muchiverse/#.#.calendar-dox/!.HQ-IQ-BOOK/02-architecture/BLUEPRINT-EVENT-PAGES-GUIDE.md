# Blueprints: event pages, pal, prisc and C ops (guide for devs and future agents)

Written 2026-10-21 by the `claude` engineer (Sonnet 5.5), from building the game verb pages and the blueprintz editor (16.game/EVENT-VERB-PAGES-DESIGN.md, BLUEPRINTZ-DESIGN.md 12.16-12.22). Companion to XHTPM-WINDOW-BUILD-GUIDE.md (how a window is built); this one is what the window edits. Everything below was read from real files or run; paths are given.

## 1. The one-paragraph model

A **blueprint is an event package**: a folder of pages. Each page has an editable **IR** (a list of command nodes), a **trigger**, a **compiled `event.pal`** and one generated **shell wrapper per command**. A trigger makes `play_event.sh` start the **prisc VM** on `event.pal`; the pal `exec`s the wrappers in order; each wrapper runs a **C op** (a compiled program) that does the real work. The pal is *not* made by prisc and C ops: it is the compiled program, and prisc plus the C ops are what run it. Editing a blueprint means changing which op runs, with which arguments, in what order. It never means writing C.

## 2. The files of a page (a real one: `@.apps/game-dev-hq/events/contract/event_pkg/pages/page_1/`)

| File | Role | Real content |
|---|---|---|
| `event.ir.pdl` | the SOURCE; the only file a human or editor changes | `NODE         \| id=1 type=game_dev_hq_verb \| verb=contract\|arg=` |
| `condition.pdl` | when the page runs | `COND \| trigger \| ui` |
| `event.pal` | COMPILED from the IR, regenerated on every save | `exec cmd_1.sh` then `halt` (header: "real prisc+x opcodes, COMPILED from event.ir.pdl by khtpm_events_hq_manager.c") |
| `cmd_1.sh` | one wrapper per node, generated | finds the house (walks up to `xyzfs`), then `exec "$D/@.apps/game-dev-hq/ops/+x/game_dev_hq.+x" 'contract'` |

An **event** is the folder `events/<name>/` with `event_pkg/pages/page_N/` inside. A page list may have several pages (the old RPG-maker events do, e.g. `page_1:hi0,page_2:hi1,...`); the verb pages have one. Blueprintz sources name the event folder (`EVENT | name | <event dir>`), and the editor works in `<event dir>/event_pkg/pages/page_1`.

## 3. The chain at run time

1. A **trigger** fires: `ui` (a window button), `on-click`, `on-touch`, `Autorun`, `clock`, ... (the page's `condition.pdl`).
2. `&.widgits/events-hq/ops/play_event.sh` picks the page and starts the **prisc VM** on its `event.pal`.
3. **prisc** is a RISC-V VM (`101.mutaclsym*/system/prisc+x`); `.pal` is its assembly (see PRISC-OPS-ARCHITECTURE.md). `exec cmd_N.sh` runs the wrapper, `halt` ends the page. There is a debugger hook (blueprintz Play/Pause/Step uses it, BLUEPRINTZ-DESIGN.md 12.12).
4. The wrapper `exec`s the **C op**, which reads and writes the game's state files. The op is where all behaviour lives.

## 4. Where command types come from: the registry

`#.ref/menu/event_commands.registry.pdl` (135 commands at 2026-10-20) is the vocabulary. One row set per command type:
```
COMMAND game_dev_hq_verb
  LABEL game-dev-hq: Run Verb
  FIELD1 Verb:
  FIELD2 Arg:
  PARAMS verb,arg
  TEMPLATE exec "$D/@.apps/game-dev-hq/ops/+x/game_dev_hq.+x" '{verb}'[ '{arg}']
END
```
The manager (`&.widgits/events-hq/ops/khtpm_events_hq_manager.c`) fills `{verb}`, `{arg}` from the node's `key=value|key=value` params to make the `exec` line of `cmd_N.sh`. To add a capability: write (or reuse) a C op, add one `COMMAND` row, and it shows up in events-hq and blueprintz with no recompile of the manager. A node whose type is not in the registry compiles to a wrapper **with no exec line**; the page then silently does nothing (see pitfalls).

## 5. Verb pages: how a game's buttons become blueprints (2026-10-20)

Gameplay verbs used to be C code that the window called directly, so blueprintz saw nothing. Now each verb is a page and the window goes through it:
- **Manifest** `@.apps/<game>/verbs.pdl`: `GAME | app | command | engine | source` and `VERB | name | yes|native|no | trigger | args | summary`. `yes` = the window plays a page for this verb.
- **Generator/checker** `verbgen` (`&.widgits/events-hq/ops/verbgen.c`): `check` fails when the engine source, the manifest, `events/` and the window files disagree; `gen` writes the packages (stamp `META | generated | verbgen`), never overwrites an edited one, and rewrites nothing that is identical.
- **Engine** `<engine> fire <verb> [args]`: plays `events/<verb>/` through `play_event.sh` (trigger `ui`), passes the args as env words and a one-time token; the page runs the registered command (`game_dev_hq_verb`, `rpg_pvp_hq_verb`) which is the engine called with the plain verb (the **leaf**), which writes the token. Token seen = the page ran.
- **Safety net:** no package, an error, or no token: the engine runs the verb itself and logs it in `state/fire_log.txt` (`ts|verb|page:<args>`, `fallback:<reason>`, `direct:<reason>`, `refused:bad-arg(...)`). A verb never runs twice. Args are limited to `[A-Za-z0-9_:.,-]`.
- **Window actions** are `fire <verb>`; the renderer appends the house root and app dir to every action, so the engine strips trailing absolute-directory args.

Apps done: rpg-pvp-hq (13 page verbs), game-dev-hq (17). INT keyboard bindings still call verbs directly (not through pages).

## 6. Editing a blueprint (three ways, one write path)

All edits end at the events-hq manager, which rewrites `event.ir.pdl` and recompiles. Nothing else writes an IR.
1. **events-hq** window: writes `edit:<id>|<type>|<params>`, `append:<type>|<params>`, `delete:<id>` into `<pkg>/.hq_manager/action.txt`.
2. **blueprintz** window (editing panel, BLUEPRINTZ-DESIGN.md 12.22): select a command, type `key=value|key=value` or `type|key=value`, add, delete, undo (16 steps). Runs `ops/bp_edit_op` in the background. Refuses characters outside letters/digits/`_ - . : , = | / space` and any type not in the registry.
3. **Headless / harness:** `ops/bp_edit_op <house> <event_pkg> <label> edit|append|delete|show ...` and the phase-table rows `BP | <event> | edit|append|delete|show | args` and `BPWIN | REQ:... | EXPECT:...` (harness README). A table can be saved and versioned.

Proof it is the real logic: with the contract page edited to `advance` the same button advanced no weeks and said "no game"; edited back it played again; an emptied page logged `fallback:page-did-not-run` and the verb still ran (`play/phases/game_dev_hq_blueprint.phases.pdl`, PASS).

## 7. Pitfalls (each one was hit)

| Symptom | Cause | Fix |
|---|---|---|
| A page edited in events-hq/blueprintz does nothing | the manager read only 48 of the registry's commands and stopped silently; later commands compiled to a wrapper with no exec line | cap 512 plus an overflow message (2026-10-20); check `cmd_N.sh` has an `exec` line after any edit |
| Editor writes to the wrong folder / "no-pages" flag | source rows name the **event dir**; the package is `<event dir>/event_pkg` | build `<event dir>/event_pkg/pages/page_1` |
| Window click refused as bad-arg, items placed at 0,0 | the renderer appends house and app dir to every action | strip trailing absolute-dir args |
| A hand-typed value runs shell code | values land inside a single-quoted word of `cmd_N.sh` | whitelist characters, check the type against the registry, before writing |
| An edit in the window changes the **live** game | verb pages are the real pages the game plays | point `state/target.pdl` at a scratch house when experimenting; the harness uses a scratch house under /tmp |
| Compiled ops missing on another machine | `+x/` is not in git | `sh ops/build.sh` (blueprintz), `sh build_harness_ops.sh` (harness), the app's own build script |
| Registry/manager changed but old behaviour persists | the manager and ops are loaded at start | restart the window/manager |

## 8. What a blueprint can and cannot do

- **Can:** reorder, swap, add or remove whole commands; change a command's parameters; make a trigger run something else; be debugged (pause, step, trace) and be tested headless.
- **Cannot:** change what happens *inside* a C op (e.g. the contract's payroll math is C; a page only chooses to run it). Logic that needs to be editable has to be split into smaller ops and a registry row.
- **Not yet:** events with several pages are listed but only `page_1` is edited in blueprintz; INT key bindings bypass pages; there is no graph view (BLUEPRINTZ-DESIGN.md B5); compiling a whole book to one `book.pal` (B4b) is designed, not built.

## 9. Checklist: make a new game's verbs blueprints

1. List the verbs in `verbs.pdl`; register one `COMMAND <game>_verb` row (section 4).
2. Add `fire` and the leaf (token, args, fallback log) to the engine (copy rpg-pvp-hq/game-dev-hq `fire_main`).
3. `verbgen check`, then `verbgen gen`; commit the generated packages.
4. Change the window actions to `fire <verb>`; strip trailing absolute-dir args in the engine.
5. Add a phase table with `EVENTS | yes` and a `BP` edit that changes behaviour, plus its negative test.
6. Open the game in blueprintz (`Load <game> verbs` buttons exist for the two done) and edit one page through the window.
