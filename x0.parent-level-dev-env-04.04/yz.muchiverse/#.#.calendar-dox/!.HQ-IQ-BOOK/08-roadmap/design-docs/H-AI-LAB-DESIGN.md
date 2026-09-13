# h-ai-lab — an inspection/play GUI for every AI this house has built

**Status: Parts 1-2 + Part 3 (all 4 steps) DONE and live-verified
(`04bfb971`, `e3743313`, `55f1659e`, `236f0bc8`, `93baa822`,
`9959a9cf`, `dd3dac3d`).** Part 3 step 4 (2026-09-13, this pass):
"New State" rebuilt as a real, in-window PICKER overlay - the same
two-stage shape events-hq's own "+ Add Command" uses (pick from a
real list, don't type a comma-separated one by hand) - direct live
confirmation this is the right instinct for Part 4/5 below too
("it should have same kind of setup that add commands from events
has, right?"). Full narrative walkthrough of this pass:
`1-1.HARNECIENT.SMOL/NIGHT_14_THE_PICKER_INSTINCT.txt`. Direct live request
(2026-09-13): a GUI ("h-ai-lab," shaped like db-hq) to inspect an AI
by its events/ops and corpuses/weights, and "play" with it by
attaching it to a real X11-HQ template window. Full narrative
walkthrough: `1-1.HARNECIENT.SMOL/NIGHT_13_H_AI_LAB_AND_THE_REAL_
REGISTRY.txt`. This doc is the terse, buildable version. Extends
`IRL-BOOTSTRAP-RECURSION-SPEC.md` (NIGHT 12) - h-ai-lab is where that
spec's `irl_bootstrap_fsm` becomes visible and operable, once built -
Part 4 below (2026-09-13, second pass) is exactly that: the manual,
button-driven front end for the same states, built before the FSM
automates them.

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
- `014.wsr-pal💸️📌️+2/ops/corp_decide.c` `llm_choice()` (~line 176-214) -
  the real, exact Gemma call shape Part 4 reuses: one-shot
  `POST http://10.0.0.144:11434/api/chat` (LAN Ollama, model
  `gemma3:270m`, no history/tools[]), real deterministic fallback on
  any failure. This is a DIFFERENT real AI from an `attention-net`
  registry entry's own `/api/chat` (that's "famous" itself, the
  from-scratch model this house built; Gemma is the general-purpose
  tool-model used to author/judge - NIGHT 9/11/12's own distinction,
  never conflate the two in the UI).

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

## Part 3 — h-ai-lab itself (DONE, step 1 of 4)

**Location (as built, `55f1659e`):** `<window class="database-window
h-ai-lab">`, launched from the real `14.h-ai` dropdown's `ai_menu_4`
row ("h-ai-lab"), dispatched via a new `livedesk:open-h-ai-lab`
`khtpm_taskbar_manager.c` branch reading its launcher path from
`livedesk_launchers.pdl`'s `launcher_h_ai_lab` row - same generic
indirection as settings/stats, not hardcoded. (The `ai_menu_N`
pdl-key family is the real, live one `livedesk_build_ai_menu()`
reads; `strip_btn_14_menu_N` in the same file is dead/unused - don't
edit it for this cell.)

**Shape (as built):** sidebar = one row per
`ai_instances_registry.txt` entry (name + KIND badge, via
`ai_lab_scan.sh` + a `<module>` refresh loop, NOT a C-side
`*_inject_panel()` - zero new per-project C in `khtpm_core_render.c`,
the generic `<repeat>`/`${var}`/`action=` vocabulary does the whole
job). Selecting a row shows that entry's own real content in an
embedded `<text_area content="${detail_text}">` (multi-line file
content encoded as literal `\n` escapes - `kh_load_vars` is strictly
one `KEY=VALUE` per physical line, this is the one real, already-
supported mechanism for showing multi-line data, not a new one).

**Three real tiers, step 1 (viewer) + a real write path for `fsm`
DONE, chat/retrain next:**

1. **Viewer** - DONE, and upgraded past plain text. `fsm` →
   cursword's `fsm_table.pdl` rendered as real bordered "scratch
   blocks" (one per state, showing its real NEXT= edges, the live-
   running state highlighted), plus a real ADD path: a picker overlay
   lets a human add a new STATE by name and toggle which existing
   states are its NEXT targets - a real write into `fsm_table.pdl`,
   not read-only anymore for this one KIND. `attention-net` →
   `GET /api/debug` rendered as text (a full weight-heatmap
   visualization stays OUT of scope - qroq's own
   `visualize_associations` 2D/3D/4D tools are a real, separate, later
   integration). `decision-pal` → its `state.txt` fields, plain. Real
   next step, not yet built.
2. **Chat** - not yet built. `cli_io` field. `attention-net` → `POST`
   that entry's own `/api/chat` (talking to "famous" itself).
   `decision-pal` → direct `state.txt`/`decision_mode` write + read
   back the decision. `fsm` → inject a message as a real trigger
   event, show the resulting transition.
3. **Gemma-driven action rows** - see Part 4 below (2026-09-13, direct
   follow-up question: "can the chat interface with famous, train
   weights from gemma, be asked to create/score curriculum thru
   button etc") - a real, separate plan, not folded into tier 2's
   chat box.

## Part 4 — Gemma-driven action rows (db-hq inspiration, direct instruction)

**Direct question:** "so the chat can interface with 'famous' (in
house llm) gemma, can it train weights from gemma, be asked to
create/score curriculum thru button etc etc? whats the plan for that?
im thinking dq-hq inspiration [db-hq]."

**The real distinction that decides the whole shape:** "famous" (a
registry `attention-net` entry, e.g. the qroq project) and Gemma
(`corp_decide.c`'s real LAN `gemma3:270m`) are two DIFFERENT real
models doing two different jobs. Chatting with famous is tier 2
(above) - talking to the model you built. Training weights / scoring
or creating a curriculum is never "chat with Gemma" in a freeform
box - it's Gemma performing ONE bounded, single-purpose task ON
famous's own corpus, the exact shape NIGHT 9/11/12 already spec'd
(propose, never auto-merge). A single chat box that has to be phrased
correctly to trigger the right operation is the wrong UI for that -
db-hq's own real shape (structured rows, each a distinct real command,
not a prompt you hope parses right) is the correct instinct, and it's
also literally NIGHT 12's `irl_bootstrap_fsm` states made clickable by
a human before the FSM exists to fire them itself:

| Button (real, bounded, single-purpose) | Real call | FSM state it manually performs |
|---|---|---|
| Propose weights (Gemma) | `llm_choice()`-shaped call: hand Gemma the corpus/term list (NIGHT 9's tool), get back a weight proposal | `PROPOSING` |
| Score curriculum (Gemma) | Same call shape: hand Gemma one real transcript slice, ask corrected-vs-accepted (the inline correction signal, `IRL-BOOTSTRAP-RECURSION-SPEC.md`) | `JUDGING` |
| Create curriculum (Gemma) | Same call shape: propose a new category name + starter weights | `PROPOSING` (new-category case) |
| Review queue | Lists every pending Gemma-authored draft above; a human accepts (writes it into the real `weights.txt`/curriculum file) or rejects (discards) ONE at a time | `PENDING_REVIEW` - never auto-merge, same discipline as every prior NIGHT |

**Why build the buttons before the FSM automates them:** a human
clicking "accept"/"reject" on real Gemma output, one row at a time, IS
the smallest real proof that the review discipline actually works -
prove a human can supervise it before trusting an FSM to run it
unattended. This is not new scope invented for h-ai-lab; it's the
existing `irl_bootstrap_fsm` design, given a UI before it's given
automation.

**Real, separate artifact per action** (never write straight into a
live file): each button appends to a small per-instance review-queue
file (e.g. `<instance PATH's own dir>/pending_review.txt`, one
proposal per line: what it is, Gemma's raw output, timestamp) - the
Review Queue row above reads that file, and Accept is the only code
path allowed to write into the real `weights.txt`/curriculum file.
Exact file format: decide when this part actually starts, matching
the "don't over-specify ahead of building" rule the rest of this doc
already follows.

**Scope boundary, explicit:** this is still design, not started. Real
smallest first step for Part 4 specifically: ONE button (Score
curriculum), against ONE real transcript slice, writing to ONE
review-queue file, with NO accept/merge path yet - prove Gemma's call
+ the draft write before building the review/accept UI on top of it.
Matches `IRL-BOOTSTRAP-RECURSION-SPEC.md`'s own smallest-first-step
(hand-score 5 exchanges) - Part 4's first button is that same proof,
just reachable from a real UI instead of a hand-run script.

## Part 5 — AI bricks as real events-hq commands ("lego" composability)

**Direct follow-up:** "does the events editor consume ai events? do we
have that yet" → confirmed, no: `#.ref/menu/event_commands.registry.pdl`
has zero AI-related commands today (checked directly), and
`fsm_table.pdl` (Part 2) is a separate format events-hq's editor never
reads. **Direct instruction, this pass:** bridge them for real, and
make every AI "trick" this house has discussed (FSM, GOAP, RL policy,
IRL judging, weight/curriculum authoring, attention chat) rearrangeable
and swappable "like legos."

**The real insight that makes this cheap, not a new system:** this
house already has TWO proven, real, zero-recompile "swap the brick"
mechanisms, built for different reasons, that this plan just connects:

1. **events-hq's own command registry**
   (`event_commands.registry.pdl`) - adding a new command TYPE is
   editing this one file, no recompile, no C change, LIVE-PROVEN
   (`take_gold`'s own header comment: added while the manager was
   already running, picked up next poll tick, no restart). A real,
   confirmed constraint: each command gets exactly `FIELD1`/`FIELD2` -
   no `FIELD3` exists anywhere in the file today.
2. **`corp_decide.c`'s `decision_mode` dispatch** - a pal already swaps
   its entire decision STRATEGY via one integer (weighted/rule/llm/
   human today; `goap`/`policy` are horizon item 4, already spec'd,
   not yet built). This is the real, existing precedent for "the same
   pal, a different brick plugged into its decision slot."

**The bridge:** add a new family of AI command TYPES to the SAME
`event_commands.registry.pdl`, each wrapping a real, small, separate op
(same `TEMPLATE exec "$D/.../+x/some_op.+x" "$ENT" '{param}'` shape
every existing command already uses) - not a new registry, not a new
editor, not new C in events-hq itself:

| COMMAND type | FIELD1 | FIELD2 | Real op it wraps |
|---|---|---|---|
| `ai_fsm_transition` | AI instance name (from `ai_instances_registry.txt`) | target state | validates + fires against that instance's `fsm_table.pdl`, same `fsm_validate_transition()` logic Part 2 already wrote - real, existing code, not reinvented |
| `ai_goap_plan` | AI instance name | goal (opt) | horizon item 4's `goap` decision_mode, once built |
| `ai_rl_policy_choose` | AI instance name | - | horizon item 4's `policy` decision_mode, once built |
| `ai_irl_judge` | AI instance name | transcript slice ref | Part 4's "Score curriculum" op, same call, now callable from ANY event, not only h-ai-lab's own button |
| `ai_propose_weights` | AI instance name | - | Part 4's "Propose weights" op, likewise |
| `ai_attention_chat` | AI instance name | message | that instance's own `/api/chat` (tier 2) |

**Why FIELD1 = instance name solves the 2-field ceiling:** every AI
command needs to know WHICH instance and WHAT KIND it is (fsm vs
attention-net vs decision-pal) - instead of cramming that into fields,
the op looks `KIND`/`PATH`/`IFACE` up from `ai_instances_registry.txt`
by name (`ai_registry.sh list` + a grep, same as `ai_lab_scan.sh`
already does). One real field carries everything the op needs; FIELD2
stays free for the ONE thing that's genuinely per-call (a target
state, a message, a transcript ref) - the registry (Part 1) is what
makes this fit the real 2-field constraint instead of running into it.

**The "legos" part, concretely:** an events-hq Common Event's command
list is ALREADY a real, ordered, freely add/remove/reorder-able
sequence in the existing editor - that IS the lego mechanism, already
built, for a completely different reason (game events). Once the AI
command types above exist in the registry, composing `ai_fsm_transition
-> ai_goap_plan -> ai_irl_judge -> ai_propose_weights` as one event's
command list, reorderable and editable live, no recompile, is the real
"try out different architectures/tricks" experimentation surface the
direct ask wants - not a new mechanism, the SAME one every board-game
event already uses, pointed at AI bricks instead of `show_text`/
`change_gold`.

**Real, honest limits, not glossed over:**
- Not every real FSM will fit `ai_fsm_transition` cleanly. `cursword_
  fsm.c`'s own real transitions are gated on polling/`wait_for()`
  side effects (menu-open checks, `is_guest()` polling loops), not a
  pure "trigger arrives, transition fires" shape - an event-fired
  transition works for a STATE CHANGE, it does not replace cursword's
  own procedural waiting logic. Full FSM-as-events was already flagged
  as a real, open, not-fully-answerable question in Part 2; this part
  doesn't resolve it, it just lets the STATE GRAPH be poked from an
  event where that's honestly enough (most non-cursword FSMs, once
  they exist, likely will be simpler).
- `ai_goap_plan`/`ai_rl_policy_choose` wrap horizon item 4, which is
  NOT built yet - these two command types are real registry ROWS that
  can be added now, but their TEMPLATE ops don't exist until item 4
  does. Adding the row and the op are two separate real steps.

**Smallest real first step for Part 5:** ONE new command type,
`ai_fsm_transition`, wrapping a NEW small op (`ai_event_fsm_transition.sh
<house_root> <instance_name> <target_state>` - looks up the instance's
`fsm_table.pdl` via the registry, runs the same validate-and-log logic
Part 2's `fsm_validate_transition()` already proves, writes the result).
Add the ONE registry row, test it fires from a real Common Event
against cursword, before adding any other AI command type. Same
one-real-thing-at-a-time discipline as every prior part of this doc.

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

1. ✅ DONE - `ai_instances_registry.txt` + `ai_registry.sh` (add/
   remove/prune/list), `flock`-locked read-prune-write-rename.
2. ✅ DONE - `cursword_fsm.c` rewritten table-driven, registered as
   the first `KIND=fsm` entry.
3. ✅ DONE - one sidebar row + one embedded viewer panel, live-verified
   against cursword's real table, including a real input-relay click
   proving the refresh action actually re-scans.
3b. ✅ DONE (2026-09-13) - scratch-block FSM view (bordered per-state
   blocks, live-running state highlighted), a real `<tabbar>`
   (Viewer/New State), and "New State" itself rebuilt as a real
   picker (name field + a toggleable list of the fsm's own real other
   states as NEXT targets) - same shape as events-hq's own "+ Add
   Command", not typed free text. Live-verified end-to-end against
   cursword, including the toggle and the final table-append.
4. **Next, not yet started, pick ONE:** (a) the chat tier for
   `attention-net` (viewer via `/api/debug`, chat via `/api/chat`) -
   register the qroq project as the registry's first `attention-net`
   entry first; (b) Part 4's first button (Score curriculum, one
   transcript slice, one review-queue write, no accept path yet).
   Do not build all KINDs/tiers/Part-4-buttons at once - same
   one-real-thing-at-a-time discipline this whole doc has followed.

## Open questions this doc does not resolve

- Exact `.pdl`/table file format for cursword's rewritten FSM - pick
  the smallest real shape that works when Part 2 actually starts,
  don't over-specify here.
- Whether attention-net liveness (for registry pruning) should be a
  real HTTP health-check or a simpler PID-file convention matching
  the rest of the house - decide when Part 1 actually starts.
- Full weight/attention visualization inside h-ai-lab (the qroq
  `visualize_associations` tools) - explicitly deferred past v1.
