# Event/DB Command Registries — Never Hardcode, Always Data-Driven

**Direct standing instruction (2026-08-26):** "we never hardcode stuff,
always keeping things super modular and abstract." This doc is the
concrete example future agents should copy the SHAPE of — not just for
event commands, but for db systems and anything else in this house
shaped like "a fixed vocabulary of dispatchable actions."

## The incident that prompted this

opencode (ox-alpha), mid-handoff on `COMMON-EVENTS-MANAGER-HANDOFF.md`,
was about to add `"control_switch"`/`"control_variable"` to
`khtpm_entity_menu_render.c`'s hardcoded `EVHQ_PICKER_TYPES[]`/
`EVHQ_PICKER_LABELS[]` C arrays — the exact same pattern
`change_gold`/`show_text`/`show_choices` already used. It stopped and
flagged this as a possible Standing Rule 7 violation ("No hardcoded
UIs, ever") before writing the code. Good instinct.

Sonnet's first-pass answer was **too permissive**: it compared these
command types to `prisc+x`'s own opcode enum (`OP_ADDI`, `OP_BEQ`, ...)
— genuinely hardcoded C, and correctly so, since an opcode requires
real interpreter logic that can't be expressed as data. The reasoning
was: "commands are like opcodes, so hardcoding them is fine."

**That comparison was wrong for most of these commands**, and the user
caught it directly: "our parser is general purpose. events are
specific to a game engine mechanism. this creates parser bloat. why
cant parser just read events as methods... and launch them like that?"
— pointing at `#.haiku+/tpmos-re-dox/fo-menu-sys.md`, this house's own
already-documented pattern for exactly this situation.

## The real three-tier distinction

1. **Pure data** (a bookmark's path, a METHOD row's dispatch string) —
   zero logic, a stored value gets substituted and used. Already
   correctly data-driven everywhere in this house.

2. **Generic dispatch templates** (`fo-menu-sys.md`'s own METHOD rows,
   `METHOD | feed | projects/fuzz-op/ops/+x/feed_op.+x xlector` — the
   manager doesn't know what "feed" means, it substitutes and execs a
   stored command string). **Almost every event command belongs here**,
   including all 3 that existed before 2026-08-26 (Change Gold, Show
   Text, Show Choices) and the ones queued next (Control Switch,
   Control Variable, Call Common Event). Each one is really just "exec
   one real op, with N string params substituted in." This is
   data-driven, zero-recompile, and was WRONGLY treated as tier 3
   before this fix.

3. **Genuine compiler logic** (real branch/jump target computation —
   e.g. Conditional Branch, which needs `prisc+x`'s own `OP_BEQ`/`OP_J`
   with computed addresses, not string substitution). This tier
   legitimately requires C code, the same reason `prisc+x`'s own opcode
   set is hardcoded. **This is the ONLY class of command that should
   still live in C** — don't over-correct into thinking EVERYTHING must
   be data now. A command needs real, non-templatable control-flow or
   state-machine logic to justify living in tier 3; "it's inconvenient
   to templatize" does not qualify.

Getting tier 2 vs tier 3 right is the actual skill here — not "always
hardcode" (the original mistake) and not "never write C" (the
overcorrection). Ask: **can this be fully expressed as one exec line
with substituted params?** If yes, tier 2, registry-driven. If it needs
real branching/addressing/state-machine logic that can't be expressed
as a string template, tier 3, and say so explicitly in a comment (see
`compile_page()`'s own header comment for the model).

## The real, working implementation (2026-08-26)

`#.ref/menu/event_commands.registry.pdl` — the tier-2 command registry.
One `COMMAND <type> ... END` block per command, with `LABEL`/`FIELD1`/
`FIELD2`/`PARAMS`/`TEMPLATE`. Read that file's own header comment for
the exact format and `{key}`/`[optional]` substitution syntax.

Loaded by BOTH:
- `khtpm_events_hq_manager.c`'s `compile_page()` — the generic template
  engine (`load_command_registry()`, `parse_params()`,
  `expand_template()`) replaced a hardcoded `if (strcmp(type,
  "change_gold") == 0) {...}` chain.
- `khtpm_entity_menu_render.c`'s Add Command picker — the type list,
  field prompts, and field count are read from the same file
  (`evhq_load_command_registry()`), replacing `EVHQ_PICKER_TYPES[]`/
  `EVHQ_PICKER_LABELS[]`.

Both re-check the registry's mtime on every use — a live edit takes
effect on the very next poll tick, no recompile, no restart, in either
binary.

**event-ez's own `ez_menu_input.c` has the identical hardcoded-if/else
pattern and has NOT been migrated yet** — same fix needed there for
real parity (the house's own PARITY RULE), not done as of this writing.
Whoever picks up event-ez next should port this exact registry/engine
shape, not re-derive a different one.

## Real, live proof this works (2026-08-26, m8_redhorned)

1. Added a real "Take Gold" command (`take_gold`, reuses
   `mr_change_gold.+x` with a negated amount) to
   `event_commands.registry.pdl` **while both the manager and render
   binaries were already running**, unmodified, no restart.
2. Confirmed it appeared as picker option 4 on the very next "Add
   Command" open (registry mtime re-check, live).
3. Selected it, entered amount `8`, submitted via the real relay-driven
   UI — compiled `cmd_5.sh` correctly contained
   `mr_change_gold.+x "$ENT" '-8'`.
4. Played the page for real: gold went 210 → 244 (three existing
   commands' +10/+25/+7, this new one's -8 — the math checks out
   exactly: 210+10+25+7-8=244).

Zero C changes were needed for step 1. That's the actual bar "data-
driven" needs to clear — not "looks like PDL" but "a real new capability
landed with no recompile."

## What NOT to do (the overcorrection risk)

Don't take this as license to make EVERYTHING data-driven reflexively:
- Conditional Branch stays hardcoded C (tier 3) — document why, as this
  file and `compile_page()`'s own comments do, don't silently leave it
  looking like an oversight.
- Don't build a registry for something with exactly one real,
  unlikely-to-grow instance — that's premature abstraction in the OTHER
  direction (see this house's own "don't design for hypothetical future
  requirements" convention).
- If unsure which tier a new command belongs to, ask (or check this
  doc's own three-tier test above) before hardcoding OR before forcing
  a template that doesn't actually fit.

## Where else this same "engines vs. content" question will come up

Any other db/entity system with a fixed vocabulary of dispatchable
actions should get the same three-tier scrutiny before adding a new
action type, especially:
- The plugin architecture (`PLUGINS-ARCHITECTURE-SCOPING.md`) — a
  plugin's own hook TYPE (Provider/Observer/Lifecycle) is closer to
  tier 1/2 (a `.pal` script triggers a real op), consistent with this
  doc's own reasoning; re-verify against this three-tier test when that
  work actually starts, don't assume it's already been checked.
- Any future db-hq tab that grows real per-row actions (Items/Weapons/
  Armors, per `event.commands.remaining.txt`'s own DB COUPLING note).
- Common events' own trigger-type dispatch (Autorun/Parallel), once
  built (`COMMON-EVENTS-MANAGER-HANDOFF.md`) — the manager's own
  per-tick check is tier 3 (real control flow), but what a common event
  actually DOES once triggered should stay tier 2.
