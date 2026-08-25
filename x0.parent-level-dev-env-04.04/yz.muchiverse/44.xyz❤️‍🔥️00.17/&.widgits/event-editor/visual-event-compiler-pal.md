# 🖼️➡️⚙️ visual-event-compiler-pal.md

**What this really is, said plainly**: we are building a **visual compiler
for `.pal`** — a human clicks through screens (pages, conditions,
triggers, commands) and the *output* is a real, executable `.pal` script,
the same real bytecode-ish language `prisc+x` already runs. MUCHI_RANCHER
is **the proving ground** for whether we can do this RPG-Maker-style event
authoring *correctly*, not just "well enough to demo." Direct framing
(2026-08-05): "this is at this point a proving grounds for our ability to
do event management mirroring rpg maker."

This doc exists because the user asked to slow down and get the
fundamentals genuinely right before building monster behavior on top of
them — "always ask questions if ur confused... this will be hard and
confusing, but its worth it to get it write."

---

## 🧠 1. The REAL RPG Maker MV runtime model (source: two reference docs)

Source: `/home/no/Downloads/1.event-menu-flo-grok.md` (editor UI),
`/home/no/Downloads/2.event-play-flo-grok.md` (runtime loop). Read
directly, not paraphrased from memory.

### 📄 An event = a list of PAGES (1..20)

Each page has:
- **Conditions** (Switch, Switch 2, Variable ≥ value, Self-Switch A-D,
  Actor in party) — **AND'd together**. No conditions checked = page
  always eligible.
- **Graphic**, **Autonomous Movement**, **Options**, **Priority**.
- **Trigger**: Action Button, Player Touch, Event Touch, Autorun,
  Parallel Process.
- **Command list** — the actual script that runs.

### 🔁 The runtime loop (this is the part easy to get subtly wrong)

```
Game loop
  → For each map event:
      Find HIGHEST-numbered page whose conditions are ALL true
        → that becomes the active page (graphic/movement/trigger/commands)
      (checked continuously — completely independent of triggering)

  → If the active page's own trigger fires:
      Hand its command list to the Interpreter
      Run top → bottom (branches/loops/jumps can reorder; "Call Common
      Event" temporarily runs a separate list)
```

**The parts most likely to get built wrong if we're not careful:**
- 🔝 **Highest page wins, not first match.** Pages are meant to be
  authored *least-specific-first, most-specific-last* — a more specific
  page needs MORE conditions and a HIGHER number so it overrides a
  general fallback page.
- 🚫 **No conditions passing = the event does nothing** (not "falls back
  to page 1").
- 🔄 **Page selection is continuous**, not a one-time check at load —
  changing a switch mid-game can flip the active page immediately.
- 1️⃣ **Only one page is ever active at a time.** Never a blend/stack.
- 🚷 **No implicit "page common events."** Common Events only run via an
  explicit "Call Common Event" command.
- ⚡ **Autorun** fires the instant its page becomes active and blocks
  other input; **Parallel Process** starts immediately and keeps running
  continuously, independently, in the background.

---

## 📁 2. Our real data shape (designed, partially built)

```
<pkg>/event_pkg/
  pages/
    page_1/
      condition.pdl   # trigger=on_spawn|on_click|parallel, self_switch=A|B|C|D|none
      event.ir.pdl    # human-readable description (NODE rows) - what the editor shows back
      event.pal       # the REAL, executable compiled output
    page_2/
      ...
```

This is genuinely a **compiler pipeline**: the visual editor is the
front-end, `event.ir.pdl` is a readable intermediate form, `event.pal` is
the real compiled target `prisc+x` executes. That's the "visual compiler
for .pal" framing, made literal.

### 🗺️ Mapping RPG Maker's fields onto this house (from `EVENT_SCRIPTING_PROGRESS_AND_GOALS.md`, already-done analysis)

| RPG Maker MV field | This house | Status |
|---|---|---|
| Self-Switch A-D | `state.txt`'s `self_A..D` | ✅ already real, unused by page logic yet |
| Switch/Variable (global) | — | ❌ genuinely new, defer until a real page needs it |
| Actor in party | — | ❌ doesn't apply, drop entirely |
| Autorun | **"On Spawn"** | closest real analog — fires once when the entity's window process starts |
| Action Button | **desktop context-menu click** (`on_click`) | maps directly onto `tp_desktop_window.c`'s real dispatch |
| Parallel Process | **background loop while window is open** | real, needed for "Automate" |
| Player/Event Touch | — | ❌ no shared player-avatar-on-grid concept yet, don't build speculatively |
| Command list | `event.ir.pdl` (description) + `event.pal` (real opcodes) | shape agreed |

---

## 🔧 3. REAL DISCREPANCY FOUND THIS PASS — the stub `event.pal` is not valid PAL

`ee_package_init.c` (the scaffolding every new event package gets) writes:
```
show_text "(empty event)"
ret
```
**These are not real `prisc+x` opcodes.** Grepped the ACTUAL interpreter
directly — both `01.muchi-pals-🥚️-13.01/system/prisc+x.c` and
`101.mutaclsym🧟‍♂️️+18.01/system/prisc+x.c` (the copy `tp_desktop_window.c`'s
own real `Play` method invokes) — neither recognizes `show_text` or `ret`
as a real token. The real, confirmed opcode vocabulary is a **low-level
register-machine ISA**:

```
li, addi, beq, j, jalr, lw, sw, halt, ecall,
exec, read_history, read_pos, read_layout, read_state,
read_env_key, read_active_target, hit_frame, sleep
```

**What this means concretely**: every freshly-`ee_package_init.c`'d
package's own `event.pal` is likely either silently ignored or a parse
no-op today — nobody has actually confirmed `Play` executing a real
non-trivial script end-to-end yet, only that the *file* gets read.

**The practical, real way forward** (matches this house's own established
convention, `feedback_reuse_ops_dont_rename` — reuse ops wholesale, don't
reinvent): a page's `event.pal` should be a **thin wrapper**, almost
always just:
```
exec "<house>/@.apps/MUCHI_RANCHER/ops/+x/mr_feed_op.+x" "<package_dir>"
halt
```
i.e. **real game logic lives in real C op binaries** (one per monster
activity — Feed/Train/Rest/Tournament/Errantry), and `event.pal` itself
stays almost trivial — just an `exec` + `halt`. This is exactly the same
shape the already-designed "ava on-spawn" example uses (`exec` launching
each pet's `button.sh run`). We are NOT hand-writing register-machine
assembly per monster activity — that would be real, unnecessary pain.

---

## 🖥️ 4. The 4-level nested editor flow (already designed, see `EVENT_SCRIPTING_PROGRESS_AND_GOALS.md`)

1. **Event Gallery** (one per pkg) — Event Name, page list at the bottom
   (New Page / Copy Page, RPG-Maker-accurate).
2. **Page screen** (one per page) — Conditions (deferred fields) +
   Trigger (real: on_spawn/on_click/parallel) + a growing command list.
3. **Command Picker** (one shared screen) — categorized real command
   TYPES this house's own `event.pal` actually supports.
4. **Command Parameter screen** (one per command TYPE) — fills in that
   command's real parameters.

Each transition is a real `href` to a real, distinct `.chtpm` file
(confirmed via direct `chtpm_parser_pal.c` source read — not guessed),
using `pieces/display/current_layout.txt`'s own self-identification so a
generated per-page/per-command screen knows which page/command number it
is.

### 🐛 KNOWN, UNRESOLVED BUG (carried over, not yet fixed)

Direct quote from the existing doc: Gallery↔Page `href` navigation does
**NOT reliably land on the clicked page** — confirmed via repeated real
k3 key-injection tests. Digit "1"+Enter landed on page 2; digits "2" and
"4"+Enter both landed back on the Gallery itself. A duplicated nav-
summary block was also observed in `current_frame.txt`. Root cause was
never located — a debug-instrumented copy of `do_jump()` was set up but
the trace was never captured. **This bug lives in the shared CHTPM
navigation plumbing** (`chtpm_parser_pal.c`), so it would affect
MUCHI_RANCHER's own monster pages exactly the same way if built on the
same real editor/event-ez path without fixing it first.

---

## 🐉 5. Mapping onto MUCHI_RANCHER monsters specifically

Each monster's own `event_pkg/pages/`:

| Page | Trigger | condition.pdl gate | event.pal (real) |
|---|---|---|---|
| Feed | `on_click` | none (always available unless away) | `exec mr_feed_op.+x <pkg>` |
| Stats | `on_click` | none | `exec mr_stats_op.+x <pkg>` |
| Rest | `on_click` | none | `exec mr_rest_op.+x <pkg>` |
| Train | `on_click` | none | `exec mr_train_op.+x <pkg>` |
| Tournament | `on_click` | `self_A=0` (not already away) | `exec mr_tournament_op.+x <pkg>` |
| Errantry | `on_click` | `self_A=0` | `exec mr_errantry_op.+x <pkg>` |
| Automate | `parallel` | none | `exec mr_automate_op.+x <pkg>` (real background loop) |

`self_A` = "currently away on Tournament/Errantry" — real self-switch,
already stubbed by `ee_package_init.c`, unused until now. A page like
Feed/Train/Rest could gate on `self_A=0` too (can't feed a monster that's
away) once condition.pdl's real parser exists.

### ✅ event-ez requirement (direct instruction)

"make sure to have an event-ez option also" — MUCHI_RANCHER monsters need
BOTH authoring paths available (same as pets/asa-ava today): the real
CHTPM editor (`Events`) AND event-ez (`Events (ez)`), same
`EE_PKG_NAME`/`EE_PKG_DIR` and `EZ_PKG_NAME`/`EZ_PKG_DIR` env-var wiring
already proven for pets. Not a MUCHI_RANCHER-specific editor — reuse
wholesale.

---

## ❓ 6. Real open questions before writing more code

1. **The Gallery↔Page href bug is unresolved.** Do we (a) root-cause and
   fix it first (the instrumented-`do_jump()` approach the prior session
   already scoped), since MUCHI_RANCHER's own pages would inherit it
   unchanged, or (b) build/prove the DATA model + real running `event.pal`
   scripts first (via direct file authoring + `Play`/relay injection,
   bypassing the nested editor UI for now), and only fix the nav bug once
   we're ready to actually use the visual editor to author monster pages?
2. **Given the `show_text`/`ret` discrepancy**: should I first prove ONE
   real, non-trivial `event.pal` (e.g. `exec`-ing a real op) actually runs
   end-to-end via `Play`/a relay-injected trigger, before scaffolding all
   7 monster pages? (i.e. fix the compiler's own "hello world" before
   writing 7 more programs in it.)
3. **Condition.pdl's real parser doesn't exist yet** (only the *shape* is
   designed) — do you want `self_A`-gating (Tournament/Errantry
   "currently away") built now as part of this pass, or deferred until
   later, with all monster pages unconditionally available for now (like
   pets' current single-page scripts)?
4. Should MUCHI_RANCHER's own event_pkg live under
   `@.apps/MUCHI_RANCHER/entities/<monster>/event_pkg/` (matching the
   pets/asa-ava convention of event_pkg living inside the entity's own
   dir), or under the `pieces/monster/` shared script location? (Pets put
   it under `pieces/<name>/event_pkg/`, referenced by the entity via env
   vars — MUCHI_RANCHER's generic `pieces/monster/button.sh` currently
   has no per-monster pieces dir at all, everything lives in `entities/`.)

**Nothing in this section is built yet** — this doc is the checkpoint
before writing more code, per direct instruction.

---

## 🌳 7. The real proof target: Show Choices + Change Gold (branching commands) — direct instruction, 2026-08-05

Direct instruction, verbatim intent: the ONE real `event.pal` we prove
end-to-end should be a real RPG Maker **Show Choices** command
(confirmed real, `#.ref/menu/event.commands.1.txt`'s own "Message" →
"Show Choices..." entry) feeding into real **Change Gold** commands
(same file, "Party" → "Change Gold..."), for MUCHI_RANCHER's own real
"Feed" activity — "we will show food options that will deduct a certain
amount," plus a debug "faucet" that "just adds 10 qolq" (this house's own
in-universe gold). Direct framing: **"getting this right here is so
profound... i really think u should update documents about this."**

### 🧩 What makes this genuinely different from a flat command list

Real RPG Maker's Show Choices isn't a single command - picking a choice
branches into a **separate sub-list of commands specific to that choice**,
which the engine runs before continuing/ending. Direct instruction on the
EDITOR side (event-ez authoring UX): "user inputs choices, and they spawn
another `[].empty` where u could input more event commands branches" -
i.e. filling in N choice labels should generate N new, real, individually-
editable empty command slots, each its own branch - not one flat list.

### 📁 Real data shape this implies (proposed, confirming before building)

```
pages/page_N/
  condition.pdl        # trigger=on_click (== "context-menu row click",
                        #   already the real, designed mapping - see §2)
  event.ir.pdl          # top-level description
  event.pal             # top-level real script
  branches/
    choice_1/
      event.ir.pdl
      event.pal          # this choice's own real command list
    choice_2/
      ...
```
A `show_choices` command's real compiled form is (proposed):
```
exec "<house>/@.apps/MUCHI_RANCHER/ops/+x/mr_show_choices.+x" "<package_dir>" "<page_dir>/branches" "Feed on what?" "Meat|-5" "Vegetables|-2" "Faucet (debug)|+10"
halt
```
`mr_show_choices.+x` is a real, new op: presents the choices as a real
popup, waits for a real selection, then `exec`s (chains into) that
choice's own `branches/choice_N/event.pal` - a real, if simple, call
semantic (not full call/return - see open question 3 below).

A `change_gold` command's real compiled form (proposed):
```
exec "<house>/@.apps/MUCHI_RANCHER/ops/+x/mr_change_gold.+x" "<package_dir>" "-5"
```
reading/writing a real `<package_dir>/qolq.txt` balance file - same real,
plain-file-state convention every other house mechanic already uses.

### ❓ Real open questions before writing any code (per direct instruction: ask, don't guess, this one's profound)

See the follow-up `AskUserQuestion` in the same turn as this doc update -
repeating the substance here so it's captured in writing, not just chat:

1. **How should the Show Choices popup actually render?** This session
   already built a real, working, shared-nav-claim multi-page menu system
   (KHTPM - `objects.pdl`, `GOTO`/`BACK`, real `[ ]`/`[>]` cursor, taskbar
   jump) in `tp_desktop_window.c`. Reusing THAT mechanism for Show Choices
   (each choice = a real `objects.pdl` `GOTO` target) would be a direct,
   meaningful payoff of the KHTPM work just finished, not a new UI
   mechanism. Confirmed as the real intended answer? Or standalone?
2. **Is "Faucet" its own separate context-menu row/page**, or just one
   more choice inside the same "Feed" Show Choices menu (as sketched
   above)? Affects scope directly.
3. **What happens after a chosen branch's script finishes?** Real RPG
   Maker continues execution after the Show Choices block ends (real
   call/return). Simplest real slice: the branch just runs to completion
   and the page ends there (no return) - matches "prove one real slice"
   ethos already established this session, but is a real, deliberate
   simplification vs. true RPG Maker semantics worth confirming, not
   silently assuming.
4. **Where does `qolq.txt` live?** Proposed: directly in the monster's
   own `entities/<name>/` package dir, matching every other real per-
   entity state file convention (`sprite.csv`, `history.txt`,
   `livedesk_index.txt`, etc.) - confirming before creating a new
   per-entity state file convention.
