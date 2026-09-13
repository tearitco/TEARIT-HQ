# h-ai-lab — an inspection/play GUI for every AI this house has built

**Status: BRAINSTORMED + SCOPED, not started.** Direct live request
(2026-09-13): a GUI ("h-ai-lab," shaped like db-hq) to inspect an AI
by its events/ops and corpuses/weights, and "play" with it by
attaching it to a real X11-HQ template window. Full narrative
walkthrough: `1-1.HARNECIENT.SMOL/NIGHT_13_H_AI_LAB_AND_THE_REAL_
REGISTRY.txt`. This doc is the terse, buildable version. Extends
`IRL-BOOTSTRAP-RECURSION-SPEC.md` (NIGHT 12) - h-ai-lab is where that
spec's `irl_bootstrap_fsm` becomes visible and operable, once built.

## Grounding: what's real, today, that this design reuses (zero invention)

- `&.hq-apps/db-hq/dashboard.chtpm` - not a separate manager+projector
  binary. A `<window class="database-window">` MODE of the shared
  `khtpm_core_render.c`: sidebar of categories + panel of content.
  Selecting "Common Events" doesn't open a new window - it INJECTS the
  events-hq editor directly into the panel (`dbhq_ce_open()`/
  `dbhq_ce_inject_panel()` in `khtpm_core_render.c`). h-ai-lab reuses
  this exact sidebar→embedded-panel pattern, not a new architecture.
- `#.Z.HUMAN_LLM/3.stage.llm.tomom@qroq.fame]921🐋️/http_server.c`
  (~lines 187-297) already exposes a real REST API: `GET /api/curricula`,
  `POST /api/chat`, `GET /api/debug`. An attention-net entry's chat/
  viewer panels hit this directly - no new IPC to invent.
- `014.wsr-pal💸️📌️+2/ops/corp_decide.c` - real `decision_mode` dispatch
  (weighted/rule/llm/human) read from a pal's own `state.txt`. A
  decision-pal entry's "chat" tier is a direct call into this dispatch,
  not a generic chat box - different KINDs genuinely take different
  input, the UI must branch on that, not paper over it.
- `*.monads/*.cursword/ops/cursword_fsm.c` - the one real, live FSM in
  the house today. NOT table-driven: `set_state("OFFER")` etc. are
  plain sequential string writes in procedural C, no transition table,
  no trigger/condition struct. Cannot appear in an event-shaped viewer
  as written - real rewrite required (see below).
- `&.widgits/events-hq/ops/khtpm_events_hq_manager.c` (~129-167) - the
  real shape of an "event" here: trigger type (None/on-click/Autorun/
  Parallel) + switch-based condition + a registry-driven command list
  (`#.ref/menu/event_commands.registry.pdl` - adding a simple command
  needs zero recompile). This is the target shape the cursword FSM
  rewrite aims at, and the shape a registry FSM-kind entry's viewer
  renders.
- `#.desktop/livedesk_taskbar.pdl` - the real `14.h-ai` strip cell
  already has a wired dropdown (`strip_btn_14_menu_0/1_label/cmd`:
  "Open h-ai," "Chat-h-ai"). h-ai-lab is a third row in this same
  dropdown, not a new taskbar cell.
- `*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c`
  `livedesk_registry_add()` - the real read-prune-write-rename pattern
  (just root-caused and fixed twice this same week, see `bug_bounty.md`)
  the new AI-instance registry reuses, not a new file-locking scheme.

## Part 1 — the real registry (build this first)

**Why first, not a hardcoded scan:** direct instruction - "real
registry if thats the right way of doing things... this is what we
are doing right now." A hardcoded scan would need editing every time
a new AI/pal gets built; a registry lets each instance register
itself once, the same way every other "many real things, one list"
problem in this house is solved (entity registry, hq-window registry).

**File:** `#.desktop/ai_instances_registry.txt` (house-root scoped,
same tier as `livedesk_open.txt`).

**One line per instance, pipe-delimited, matching this house's
existing convention:**
```
NAME=<display name>|KIND=<fsm|attention-net|decision-pal|other>|PATH=<real path to its own interface>|IFACE=<how to reach it>
```
- `KIND=fsm` → `PATH` = the instance's own state-table file (once it
  has one - see Part 2). `IFACE` = n/a, the viewer just reads `PATH`.
- `KIND=attention-net` → `PATH` = the project dir (e.g. the qroq
  dir). `IFACE` = its `http_server.c` base URL (host:port).
- `KIND=decision-pal` → `PATH` = the pal's own `state.txt`. `IFACE` =
  n/a, the panel calls `corp_decide.c`'s dispatch shape directly by
  reading/writing that same `state.txt`'s `decision_mode` field.
- `KIND=other` → escape hatch for whatever doesn't fit yet; the
  registry's own schema should NOT try to anticipate every future
  shape - add a new KIND when a real instance needs one, don't
  pre-design for hypotheticals.

**Writer:** each real AI instance (or its launcher) calls a small,
shared `ai_registry_add()` function - same read-prune-write-rename
shape as `livedesk_registry_add()`, prune criterion is instance-
specific (an FSM/decision-pal prunes on `PATH` no longer existing; an
attention-net prunes on its `IFACE` URL failing a liveness probe, not
a PID check - these are files/services, not always live processes).

**Reader:** h-ai-lab's own sidebar-population step, direct file read,
no manager process needed for v1 (mirrors db-hq: the generic renderer
reads state files directly, no bespoke IPC).

## Part 2 — cursword_fsm.c rewrite (do in this same pass)

**Why bundled with h-ai-lab, not separate:** two real reasons, both
direct instruction. (1) the registry needs a first real FSM-kind entry
to prove the shape with, and cursword is the house's only real,
live FSM today. (2) rewriting it into a table-driven shape is the
real test of whether "FSM as events" (the NIGHT 12 ask) is honestly
answerable for the rest of the house's FSMs later, before promising
it broadly.

**Target shape**, matching events-hq's own real event (trigger +
condition + command), NOT a full events-hq event (cursword's FSM has
no UI/click triggers, its triggers are conversational/state-internal):
```
STATE | NEW_USER  | on_enter=say(...)   | trigger=user_msg_received -> OFFER
STATE | OFFER     | on_enter=say(...)   | trigger=user_accepts -> ...
```
A real table file (`cursword/fsm_table.pdl` or similar, house `.pdl`
convention), each row a state + its real trigger→next-state edges +
its real on-enter action. `cursword_fsm.c` becomes a real, small
table-walker reading this file, replacing the current inline
`set_state("X")` call sites - not a rewrite of cursword's own
conversational logic, just its state-transition backbone.

**Proof before considered done:** the same live-report discipline
every NIGHT closes on - `cursword_say()`'s real fallback path (horizon
item 3, still separately open) must still work identically after this
rewrite: kill the local model mid-run, confirm the canned line still
plays. A table-driven FSM must not regress the one FSM-first/model-
optional guarantee this house already depends on.

## Part 3 — h-ai-lab itself

**Location:** new `<window class="database-window ai-lab">` (or
similar), launched from a new third row in the existing `14.h-ai`
strip-cell dropdown (`strip_btn_14_menu_2_label = h-ai-lab`, pushing
the existing `cancel` row down one slot in `livedesk_taskbar.pdl`) -
not a new taskbar cell, reusing the real one that already exists.

**Shape:** sidebar = one row per `ai_instances_registry.txt` entry
(name + KIND badge). Selecting a row injects a panel for that
instance's KIND - same embed pattern as db-hq's Common Events tab,
literally the same `*_inject_panel()` shape, new per-KIND injector
functions instead of a new architecture.

**Three real tiers per panel, all present in v1 (direct instruction:
"all of the above, i just wanna see the hooks are there"):**

1. **Chat** - `cli_io` field. `attention-net` → `POST /api/chat`.
   `decision-pal` → direct `state.txt`/`decision_mode` write + read
   back the pal's own decision output. `fsm` → inject a message as
   that FSM's real trigger event, show the resulting state transition.
2. **Viewer** - read-only. `attention-net` → `GET /api/debug`
   rendered as text/values (a full weight-heatmap visualization is
   explicitly OUT of v1 scope - the qroq project's existing
   `visualize_associations` 2D/3D/4D tools are a real, separate,
   later integration, not reinvented here). `fsm` → the state table
   itself, current state highlighted, read directly off `PATH` (cheap
   text read, not a rendered diagram - matches this house's own
   "text state beats decoding pixels" testing convention).
   `decision-pal` → its `state.txt` fields, plain.
3. **Retrain trigger** - a real, VISIBLE, WIRED-BUT-HONEST button:
   "run IRL pass" fires NIGHT 12's `irl_bootstrap_fsm`'s `PROPOSING`
   step. Since that FSM is not built yet (`IRL-BOOTSTRAP-RECURSION-
   SPEC.md`'s own smallest-first-step hasn't happened), this control
   ships DISABLED with a real, honest label ("not yet built") rather
   than hidden - direct instruction was to prove the hook exists, not
   to fake that it works.

## Real blockers vs. non-blockers (direct question, answered)

**NOT a blocker:** the pc-hq event-trigger layer (horizon item 1,
still unbuilt). h-ai-lab talks to events-hq's already-working Common
Events dispatch directly - the same path db-hq's own embedded editor
already uses today. No dependency between the two.

**Real blocker 1:** the registry (Part 1) doesn't exist yet - nothing
for h-ai-lab's sidebar to enumerate until it does.

**Real blocker 2:** `cursword_fsm.c`'s rewrite (Part 2) - not a
blocker for the registry's existence, but a blocker for h-ai-lab's
own smallest end-to-end proof if cursword is meant to be the first
real FSM-kind entry registered (it's the only real FSM in the house
today, so in practice, it is).

## Smallest real first step (do exactly this, nothing bigger, first)

1. Build `ai_instances_registry.txt` + `ai_registry_add()` (the
   read-prune-write-rename writer) + a plain read function. No UI yet.
2. Rewrite `cursword_fsm.c` into a real table-driven shape; register
   it in the new registry as the first `KIND=fsm` entry. Prove
   `cursword_say()`'s fallback path still works post-rewrite.
3. Build ONE sidebar row + ONE embedded panel (viewer tier only,
   reading cursword's new state table) - prove the db-hq-style embed
   pattern works for a real AI instance before promising all three
   tiers for every KIND.
4. Only after 1-3 are proven: add the chat tier, then the qroq
   attention-net KIND (viewer via `/api/debug`, chat via `/api/chat`),
   then the decision-pal KIND, then the disabled retrain-trigger slot.
   Do not build all KINDs/tiers at once.

## Open questions this doc does not resolve

- Exact `.pdl`/table file format for cursword's rewritten FSM - pick
  the smallest real shape that works when Part 2 actually starts,
  don't over-specify here.
- Whether attention-net liveness (for registry pruning) should be a
  real HTTP health-check or a simpler PID-file convention matching
  the rest of the house - decide when Part 1 actually starts.
- Full weight/attention visualization inside h-ai-lab (the qroq
  `visualize_associations` tools) - explicitly deferred past v1.
