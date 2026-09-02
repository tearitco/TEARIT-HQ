# MARKETABLE-FEATURES.md — real, current state of every taskbar app/feature + entities

Written 2026-08-27, research-only pass. Purpose: the marketing video and
owner-report video made earlier this session only covered a curated subset
(Common Events, db-hq, some palette/bookmark fixes) and the user flagged
real, working things that got missed. This doc catalogs EVERY taskbar cell
plus entities/pals/Mutaclysm/h-ai/emoji/files-desks/chemistry-palette, each
with a real/partial/stub verdict, what actually works today, and the real
file path(s) backing the claim — so future marketing/onboarding material
can draw from ground truth instead of a partial memory of one demo.

Per `_.0.aigent-testing-k9.txt`'s "UPDATE 2026-08-27" rule (added this
session): a presentation that only shows BEFORE/AFTER state looks dead to a
human viewer. Any future demo of a UI-visible feature below must capture
the actual navigation/typing — sidebar with nothing focused → focus
stepping frame-by-frame as each nav code lands → for typed text, a
mid-typing frame with a live partial string + caret, not just
empty-then-final. If every frame in a presentation could have been
produced by hand-editing a state file, the presentation hasn't proven the
UI works. This does not apply to headless/no-window processes (ledger-only
tools) — those are proven by state dumps, no navigation exists to show.

---

## Taskbar cell inventory

Real cell source: `44.xyz.01.00/#.desktop/livedesk_taskbar.pdl` (labels/
commands, editable without recompile) dispatched by
`44.xyz.01.00/*.monads/*.livedesk-taskbar/ops/khtpm_taskbar_manager.c`.
Launcher targets for HQ-menu rows resolve via
`44.xyz.01.00/#.desktop/livedesk_launchers.pdl`.

### HQ
**Verdict: real.** Restart/hide-show/dir/quit/settings/stats/cli/cursword/
kill-hq rows all have real `_cmd` targets in `livedesk_taskbar.pdl` (e.g.
`hq_menu_1_cmd | sh .../run_khtpm_strip.sh new`). Demo: step through the
HQ submenu with `ACTIVATE_NAV`/`FOCUS_NAV` relay codes per entity, showing
the `[>]` highlight moving row to row (see k9 doc's HANDS-FREE MENU
WORKFLOW).

### file (file:pre-design cell)
**Verdict: real, structural — sessions ARE the "files."** `strip_btn_0`
submenu: new/save/save-as/load, all wired to `livedesk:new` /
`livedesk:save` / `livedesk:save-as` / `livedesk:load` commands
(`livedesk_taskbar.pdl` lines under `strip_btn_0_menu_*`). Backed by
`resolve_session_root()` in
`44.xyz.01.00/&.widgits/events-hq/ops/khtpm_events_hq_manager.c` — a
session is a real, resolvable root directory, not a cosmetic label. Real
demoable use today: saving/loading distinct named sessions (different
in-progress projects/layouts) and switching between them via the file
menu's load row — this is possible today, not just structural, but a
future demo should actually show two different saved sessions being
loaded in sequence (not just the menu opening).

### desks:office (desks cell)
**Verdict: real.** `strip_btn_1_cmd | livedesk:desks`; live desk name is
read by `livedesk_current_desk_name()` in `khtpm_taskbar_manager.c` (line
~1177). Desks are a sub-concept living inside a session (a session/file
can contain multiple named desks, each a distinct desktop layout of
entities). Real demoable use: switching the active desk within one session
and showing the desktop repopulate with that desk's own entities.

### pals
**Verdict: real.** `strip_btn_2_cmd | livedesk:pals` — opens the pal/desk-
pal picker; see Entities section below for what a spawned pal can actually
do.

### palettes
**Verdict: real, UI color-theme/emoji picker — NOT a tile/map system.**
Per this session's own `GAME-READINESS-GAP-ANALYSIS-2026-08-27.md` §1
finding (reused here, not re-derived): "'Palette' today (`&.widgits/
palettes/`) is a UI color-theme/emoji picker — no tile/tileset/chipset
concept, no map grid." Categories are real and clickable (my-pallet,
emojis, chemicals+compounds, rpg-maker-tiles, piececraft, cdda, dwarf
fortress, kenney-3d, paint colors, generate) — see
`44.xyz.01.00/&.widgits/palettes/pallets.pdl` and
`palettes_menu.sh`/`palettes-elements.chtpm`/`palettes-emojis.chtpm`. A
real numbered-scroll-arrow fix landed this session (see commit
`21299b2`). Demo: show a category window open, scroll arrows actually
moving content, and a tile click landing.

### chemicals+compounds ("chemistry palette") — flagged explicitly this session
**Verdict: real code, genuinely minimal today ("not doing much yet") —
but real precedent for domain-specific pickers beyond UI theming/RPG
tiles.** Backed by a real, checked-in 50-row CSV of actual chemical
compounds (formula, color hex, melting/boiling point, density, toxicity,
reactivity columns) at `44.xyz.01.00/#.ref/menu/palletes/
chemistry_tiles_expanded🏆.csv`, rendered via `publish_elements()` in
`44.xyz.01.00/*.monads/*.livedesk-taskbar/ops/palettes_manager.c`
(line ~194, `g_source_path` set at line ~244) into
`44.xyz.01.00/&.widgits/palettes/palettes-elements.chtpm`. Today this
is click-to-place emoji/glyph tiles from real chemistry data (same
mechanism as the emoji palette, not a periodic table UI, not reactions/
simulation) — honestly scoped as early/minimal, not a chemistry app. The
marketing value is precedent, not capability: it's concrete evidence the
palette system already extends past UI theming and RPG tile chips into
domain-specific data, which sets real expectations for future
science-audience material — don't oversell what's there today.

### edit
**Verdict: stub/placeholder.** `strip_btn_4_label | edit` has only one
submenu row defined (`strip_btn_4_menu_0_label | copy`) with no `_cmd`
target found in `livedesk_taskbar.pdl`. No wired action confirmed.

### player
**Verdict: real, small.** `strip_btn_5` (play/pause/reset). Reset is wired
to `livedesk:reset-entities` (`ktb_menu_player()` around line 2510–2524 of
`khtpm_taskbar_manager.c`) — a real, new feature this session ("kill
entities then relaunch them fresh"). Play/pause rows exist as labels with
empty commands in the C fallback — not confirmed wired beyond reset.

### db (db-hq)
**Verdict: real, working.** Confirmed extensively earlier this session
(see `COMMON-EVENTS-MANAGER-HANDOFF.md`) — db-ez/db-hq submenu rows are
real (`ktb_menu_db()` ~line 2737–2747 of `khtpm_taskbar_manager.c`),
dispatching to `*.monads/*.muchi-pet/ops/open_db_hq.sh`
(`launcher_db` in `livedesk_launchers.pdl`).

### plugins
**Verdict: stub/placeholder label only.** `strip_btn_7_label | plugins`
has no submenu rows and no `_cmd` anywhere in `livedesk_taskbar.pdl`. No
plugin-loading code found wired to this cell. A design doc exists
(`PLUGINS-ARCHITECTURE-SCOPING.md` in this same directory) but it is
scoping/planning, not an implementation behind this cell.

### store
**Verdict: stub — reused finding from this session's marketing-video
pass.** `strip_btn_8_label | store` has no submenu/cmd defined. Backing
directory `44.xyz.01.00/@.app-store` is an empty scaffold with only
data-stub `.pal` files, confirmed by this session's own prior pass — no
user-facing store feature exists behind this cell today.

### network
**Verdict: stub label / infra only, no user-facing feature.** `strip_btn_9
_label | network` has no submenu/cmd defined in `livedesk_taskbar.pdl`.
Real networking DOES exist in the house (the LAN Ollama/qwen ladder,
`44.xyz.01.00/net/ollama-lan.pdl` + `net/qwen.sh`) but it is
infrastructure consumed by h-ai/chat-hai, not something this taskbar cell
itself opens or exposes to a user.

### menus / datetime / tools / (reserved)
**Verdict: labels only for the bare header cells** — `strip_btn_10`
through `strip_btn_13` have no `_cmd` rows in `livedesk_taskbar.pdl`
beyond their bare labels (`strip_btn_10_label | menus`,
`strip_btn_13_label | (reserved)` explicitly an unused slot). **Real
correction (2026-08-27)**: one of these header cells' dropdown reaches a
genuinely real, live "Toys" feature — see dedicated section below. This
was MISSED entirely in this doc's first pass; the exact submenu row that
dispatches `cid="toys"` was not fully re-traced this pass (no
`strip_btn_10_menu_*` rows exist in the checked-in `.pdl` either, so the
dispatch path is reached some other way in `khtpm_taskbar_manager.c` —
flagged honestly rather than guessed at further).

### Toys (real, live-scanning menu — corrected 2026-08-27, previously
missed entirely)
**Verdict: real, not a stub.** `livedesk_build_toys_menu()`
(`khtpm_taskbar_manager.c` ~line 3048) live-scans, every time the cell
opens, for real `toy.pdl` identity files (`META|title`, `META|launch`)
at the house root's own top level AND under `@.apps/`'s direct children
— opt-in by file presence only, so it can never false-positive. Real,
currently-registered toys (confirmed on disk right now):
- **Mutaclysm-Neo** (`101.mutaclsym🧟‍♂️️19.00/toy.pdl`) — see the
  dedicated Mutaclysm section below; real, drivable via injection.
- **my-chara-txt** (`@.apps/my-chara-txt/`) — the base CHTPM game-shell
  template my-biotech/my-lawyer both copy from.
- **piececraft-xyz** (`@.apps/piececraft-xyz/`) — see its own dedicated
  section below; real, with an honest partial-capability split.
- **my-lawyer** (`@.apps/my-lawyer/`) — see its own section above;
  reachable BOTH standalone (`button.sh run`) and via this Toys menu.

Each entry dispatches `livedesk:open-toy:<path>/<launch>` (defaults to
that project's own `button.sh` if no `META|launch` override is set) —
confirmed real, not a placeholder command string.

### piececraft-xyz — real two-phase toy, honest partial capability
**Verdict: real, two real phases, one real capability gap disclosed in
the code's own comments (not discovered by us — it says so itself).**
1. **Terminal setup phase**: `@.apps/piececraft-xyz/button.sh run` in a
   real terminal — world generation, `END_TURN`, `TOGGLE_AUTOTICK`,
   `CYCLE_TICK_SPEED`, all real (`ops/pc_menu_input.c`).
2. **Map-view phase**: a SEPARATE real GL window from the generic house
   widget `&.widgits/board-viewer/` (own `chtpm_parser_pal`/
   `keyboard_input`/`gl_mirror` binaries, same family as Mutaclysm/
   my-lawyer/my-biotech). Reads its own real `pieces/keyboard/
   history.txt` (`[TIMESTAMP] KEY_PRESSED: N` convention), gated by an
   "INTERACT engaged" state — same class of gotcha as Mutaclysm's own
   `interact_mode` (k9 doc Rule 10). Real camera control confirmed:
   `ops/bv_render_3d.c`'s `camera_mode` (0/1/2), yaw/pitch, look-down
   angle.
3. **Cross-project bridge, real**: board-viewer writes the xelector's
   position directly, then notifies piececraft-xyz's own terminal-side
   process via `widget_cmds/inbox.txt`, which dispatches real `MOVE`/
   `JUMP`/`MINE`/`BUILD` commands (`ops/pc_menu_input.c` ~line 798-850).

**The honest capability split** (from the code's own comments, not
inferred): **MOVE is fully real** — arrow keys move the xelector, write
position cross-project, advance the shared world tick. **JUMP/MINE/
BUILD are explicitly "real plumbing, honest stub mechanic"** — the full
key→inbox→dispatch path works and produces a real ledger entry + tick
advance, but there is NO actual jump physics, block removal, or block
placement yet ("physics not implemented yet" — the code's own message
string). A future demo of this must frame around what's real (MOVE, the
ledger/tick proof for JUMP/MINE/BUILD) and never imply the stubbed
actions have a visible in-world effect — see `HARNESS-AUTHORING-GUIDE.md`
§2 for the camera-director guidance this game specifically needs
(frame the xelector/terrain meaningfully, don't rely on the default
camera angle).

### h-ai (cell 14) — see dedicated section below
Real, two distinct modes, both confirmed. See "h-ai" section.

### h-ai (cell 14) — see dedicated section below
Real, two distinct modes, both confirmed. See "h-ai" section.

### datetime cell (clock)
**Verdict: real.** Cell 15 (clock) is a real ACTIVATE cell since
2026-08-13 (`khtpm_taskbar_manager.c` comment ~line 2965), with real
submenus: clocks & cals, reminders, game clocks, new game clock, gamedate,
endturn, ticker on/off, rate, pause/resume, delete — all with real
`livedesk:clock:*` commands (`~line 2682-2724`).

---

## Entities / desk-pals ("chara etc... work to some extent")

**Verdict: real, and genuinely interactive — with an honest caveat.**
Every spawned pal is its own process
(`ops/+x/tp_desktop_window_rgb.+x`, one per pal) polling its own relay
file, per `_.0.aigent-testing-k9.txt`'s "PER-ENTITY RELAYS" section
(2026-08-24 update, line ~887):

- Real command vocabulary written to `<pal>/interact_relay.txt`:
  `OPEN_CONTEXT` (open right-click METHOD menu), `RUN_METHOD:<label>`
  (dispatch a `meta.pdl` row directly, e.g. `RUN_METHOD:Dir`,
  `RUN_METHOD:Chat`), `CLOSE`, `RAISE`, `FOCUS_NAV:<n>`,
  `ACTIVATE_NAV:<n>`, plus bare decimal codes.
- A full hands-free menu workflow was proven live 2026-08-24 driving
  cursword's Dir row end-to-end with no mouse/XTest at all.
- Dragging/window management works via `RAISE` (raise-to-top is the whole
  observable focus model for these override_redirect windows).
- Dir browsing and Chat are real per-entity `meta.pdl` methods
  (`RUN_METHOD:Dir`, `RUN_METHOD:Chat` cited directly above).

**Honest "to some extent" caveat:** the command vocabulary and dispatch
loop are solid and proven, but not every pal necessarily implements every
`meta.pdl` method — capability is per-pal (`meta.pdl` row set), so "chara
etc. work to some extent" is accurate: the entity *framework* (spawn,
relay, context menu, raise/close, per-entity history logging) is fully
real, while individual pal feature completeness varies pal-to-pal and
wasn't exhaustively re-verified for every pal in this pass.

---

## Mutaclysm (3D game space)

**Verdict: real, working, relay/history-file-driven** — not a concept.
Located at `44.xyz.01.00/101.mutaclsym🧟‍♂️️19.00/` (an active dir; an
older `+18.0G` variant also exists). Real C sources include
`camera_control.c`, `move_player.c`, `muta_render_3d.c`,
`compose_rgb_frame.c`, `game_dispatch.c`, `generate_map.c`,
`craft.c`/`eat.c`/`pickup.c`/`examine.c`/`end_turn.c`, `tick_monsters.c`.

Confirmed real per `SIMLINK_PITFALL.md` (2026-08-20, status: DONE):
"Camera mode switching (and rendering generally) confirmed working after
this pass, both via direct testing and by the user live in the GL
window." Real relay/history mechanism: `interact_relay.txt` drives
`game_dispatch.c`, gated by a ported `is_active_layout()` check against
`pieces/display/current_layout.txt` (fixed 2026-08-18, per
`muta-remaining-bugs.txt` Bug 1) so keys only affect the game when its own
`main.chtpm` layout is actually showing — not a leaky global key sink.

What works: camera control, player movement, interact-mode gating,
terrain texture rendering (confirmed live: "i see the textures now. and
it looks like the old map"). What's flaky/incomplete per
`muta-remaining-bugs.txt`: additional bugs beyond Bug 1 were still
being researched/prioritized as of 2026-08-18 (documented as
research-only, not yet implemented at that writing) — treat anything past
Bug 1 in that file as not yet confirmed fixed without re-checking it.

Demo note: this is a live GL window with real camera/movement input — a
future presentation should capture actual movement/camera frames
stepping through a sequence, not a single before/after screenshot.

---

## h-ai (taskbar cell 14) — TWO real modes, both LLM-backed

### Mode 1 — "Open h-ai": single chat, backed by a REAL local LLM
**Verdict: real, not a stub or mock.** `strip_btn_14_menu_0_cmd` launches
`44.xyz.01.00/&.widgits/open-hai/ops/launch_h_ai.sh`, which runs
`khtpm_open_hai_render.c` + `khtpm_open_hai_manager.c`. The manager makes
a REAL local inference call: `send_to_ollama()` in
`44.xyz.01.00/&.widgits/open-hai/ops/khtpm_open_hai_manager.c`
(~line 469) POSTs to a real local/LAN Ollama server —
`g_ollama_host = "10.0.0.144:11434"` (line ~343), hitting
`http://<host>/api/generate` (line ~502) via `execlp("curl", ...)`
(line ~507). This is a genuine local-LLM chat backend, not a mock — it
also supports OpenRouter/TokenRouter as alternate backends
(`send_to_openrouter()`/`send_to_tokenrouter()`), selectable via
`state/sessions/model.txt`, with real tool-call plumbing (`read_file`/
`write_file`/`edit_file`/`search_in_files`/`cmd_exec`, ~lines 1035-1354).
Prior nav-index gotcha (relay index >9 unreachable) is not evidenced as
still open in this cell's current code — not re-derived further here.

### Mode 2 — "Chat-h-ai" (chatroom): 4 agents talking to each other
**Verdict: real — genuinely a multi-agent conversation loop, not just
multiple saved 1:1 sessions.** Corrects an earlier weaker assumption.
`strip_btn_14_menu_1_cmd | livedesk:open-chat-hai` opens
`44.xyz.01.00/&.hq-apps/chat-hai/`, whose real engine is
`ops/chat_hai_loop.sh` — "round-robin chat scheduler for chat-hai...
THIS shell decides who speaks next... the model only generates text via
the shared `net/qwen.sh` wrapper" (file header, lines 1-9).

- Real personas (4 regular chatters + 1 moderator) live as `.pdl` files:
  `44.xyz.01.00/&.hq-apps/chat-hai/pieces/personas/{bravo,moxie,pip,
  sage,conductor}.pdl`. The boot line literally says so:
  `"chat-hai online - 4 smols chatting on the ladder"` (line 596).
  `conductor` is tier `manager` and only speaks every `MODERATOR_EVERY`
  (3) rounds (lines 156, 611-614) — a synthesist/moderator on top of the
  4-agent round-robin, not a 5th equal chatter.
- Real turn-taking/dispatch loop: `main round-robin loop` (lines 600-622)
  iterates every persona `.pdl` in `personas_dir()` each round, calling
  `speak()` (line 418) for each — which builds a prompt from shared
  context, dispatches via `bash "$QWEN" ask "$tier" "$question"` (line
  531, the same local Ollama/qwen ladder as Mode 1), then appends the
  reply.
- Real shared history file: all 4 (+moderator) agents append to and read
  from the SAME per-session ledger, `state/sessions/<session>.ledger`
  (`ledger_path()`, line 48) — `recent_context()` (line 317) feeds the
  last N ledger lines to whichever persona speaks next, so each agent
  sees the others' prior turns. This is the real shared-state mechanism
  making it an actual conversation, not 4 isolated one-shot calls.
- Real supporting mechanisms confirmed in the same file: per-persona
  append-only memory files (`memory_dir()`/`write_memory()`/
  `recall_memory()`), a deterministic anti-repeat word-overlap gate
  (`word_overlap()`, ≥55% overlap with a persona's own last message drops
  the reply), an instruction-leak detector, and a co-occurrence
  "relationship" tracker (`relations.pdl`/`bump_relation()`) — all pure
  harness logic, no model asked to self-judge (documented house
  convention, "Harnecient Way").
- User input is genuinely folded into the loop: `last_user_msg()` (line
  329) promotes the most recent user ledger line to the top of each
  persona's prompt so replies actually address it.

---

## Emoji rendering — current state

**Verdict: real, used both as entity glyphs and as clickable palette
tiles — static/curated, not a live picker with search.** Emoji glyphs are
used directly as desktop entity identifiers (per the taskbar screenshot:
🗡️👨📚🥷🤖👩👺) and as palette tiles via the "emojis" category
(`livedesk:open-palette:emojis`, `palettes-emojis.chtpm`). Per
`palettes_manager.c`'s own header comment: "Real tiles: emojis (starter
grid, `palettes-emojis.chtpm`)... both STATIC, checked-in files now,
never regenerated." A real limitation is already documented in that same
file: some compound-label glyphs don't render as chemistry glyphs and
fall back to text — "Confirmed live: 46 of 49 compound emoji [...]" (line
~116) — i.e. glyph coverage gaps are known and partially worked around,
not silently broken.

---

## my-lawyer / my-biotech — real LLM-driven text games (NOT taskbar-wired, missed by this pass initially, flagged directly by the user)

These live at `44.xyz.01.00/@.apps/my-biotech/` and `@.apps/my-lawyer/`,
outside the taskbar/entity path entirely — no `livedesk_taskbar.pdl` or
`livedesk_launchers.pdl` row references either (confirmed via grep,
zero hits), each launched standalone via its own `button.sh run`. This
is exactly why the taskbar-cell-driven survey above missed them; they
are real and substantial, just reachable a different way.

### my-biotech — chemistry research game, REAL local-LLM loop, DONE through P3
**Verdict: real, compiled, LIVE-VERIFIED.** A `my-chara-txt`-shaped CHTPM
game (`system/orchestrator.c`/`chtpm_parser_pal.c`/`renderer.c`, same
family as Mutaclysm's underlying shell) where the player buys chemical
elements, researches compounds via a REAL `gemma3:270m` call over LAN
Ollama (`ops/mybiotech_research_worker.c`, async/PID-tracked background
worker — not blocking), and an FDA-style Gemma verdict (`ops/
mybiotech_fda_verdict.c`) APPROVES or REJECTS each compound, appended
to a real, player-readable `dossier.txt`. `MY_BIOTECH_DESIGN.md`'s own
status table: P2 "DONE, LIVE-VERIFIED, ASYNC-FIXED" (7/7 harness pass),
P3 "DONE, LIVE-VERIFIED" (11/11 harness pass) — real dossier creation,
real FDA verdict, real catalog/ledger entries confirmed via `test-harn-
same/scenarios/demo_research_and_end_turn.sh` and `demo_fda_review_
discrimination.sh`. Real compiled binaries exist for every op
(`ops/+x/*.+x`). Live game state confirmed on disk right now:
`data/discovered_compounds.txt` has a real entry (`Chlorine|chlorine|
REJECTED|1`). Honest known limitation, documented in the design doc
itself: `gemma3:270m`'s research output quality is weak ("the model
often just echoes the element name back") — a real ceiling of using a
270M-param model, not a bug.

### my-lawyer — legal case-building/argument game, same async pattern, judge classifier IMPLEMENTED
**Verdict: real, compiled, further along in code than its own design
doc admits.** Same CHTPM shell family. Player picks up lawsuits from a
real docket (`data/docket.txt` has real live entries, e.g. "State v.
Adam Chen"), chooses settle or go-to-court, and if going to court, a
background worker (`ops/mylawyer_case_worker.c`) builds a real,
player-readable case document turn-by-turn via Gemma + deterministic
tool calls (search corpus/precedent/cite law). A Gemma judge
(`ops/mylawyer_judge_worker.c`) reads BOTH real case files and picks a
winner. The design doc's §4 flags the plaintiff-vs-defendant comparison
scorer as "not yet designed in detail" — but the actual code has moved
past that: `classify_comparison()` (`mylawyer_judge_worker.c` ~line
236) is a real, implemented hand-written keyword+argument-density
scorer (explicitly NOT a direct Gemma self-classification — a code
comment at the top of the file explains why: self-classification was
"wrong 2/3 times on an obvious case"). Less thoroughly harness-proven
than my-biotech though: only `test-harn-same/scenarios/demo_ping.sh`
exists (a basic smoke scenario), no equivalent to biotech's full
research-and-end-turn or FDA-discrimination scenarios yet — real and
working per the code, but not yet exercised by as complete a test
suite. The office/election/bias mechanic (§5 of the design doc) reads
as designed-not-yet-confirmed-built; verify directly before claiming it
in a demo.

**Demo note for both**: these are real turn-based CHTPM games with a
real LLM in the loop on every research/judge action — a future
presentation should show the actual turn sequence (pick a case/research
target → real "⏳ Researching..." progress polling → real result
appended to a document the player then opens and reads), not just a
before/after diff of the data file, per this doc's own navigation-rule
framing above.

---

## Not yet covered in any presentation

- **Mutaclysm** — the 3D game space (`101.mutaclsym🧟‍♂️️19.00/`), real
  camera/movement/interact-mode, relay+history-file driven.
- **h-ai chat AND h-ai chatroom** — both real: single chat is a genuine
  local-LLM (Ollama) backend; "Chat-h-ai" is a real 4-agent (+moderator)
  round-robin conversation loop sharing one ledger, not just saved
  sessions.
- **Entities/desk-pals** — real per-entity relay/context-menu/RAISE/CLOSE/
  Dir/Chat dispatch, proven hands-free end-to-end; capability varies
  pal-to-pal ("work to some extent" is accurate, not a knock).
- **Files/desks switching** — sessions (files) and desks (a sub-concept
  inside a session) are both real and switchable today, not just
  structural — a demo should show an actual load-a-different-session or
  switch-desk sequence.
- **Current emoji state** — real static curated glyph sets for both
  entity identity and palette tiles, with a known/partially-mitigated
  glyph-coverage gap for compound labels.
- **Chemistry palette** — real, minimal, but real precedent for
  domain-data pickers beyond UI theming/RPG tiles (flagged explicitly by
  the user as under-covered).
- **Toys menu + piececraft-xyz** — the real "Toys" live-scanning menu
  (Mutaclysm-Neo/my-chara-txt/piececraft-xyz/my-lawyer) was missed
  entirely in the first pass. piececraft-xyz specifically needs an
  honest demo: real MOVE + real camera control, but JUMP/MINE/BUILD are
  disclosed-in-the-code stubs (plumbing works, no physics/block changes
  yet) — see its own section above and `HARNESS-AUTHORING-GUIDE.md` §2
  for the camera-director framing it needs.
- **my-biotech** — real, LIVE-VERIFIED chemistry-research CHTPM game
  with a genuine local-LLM (`gemma3:270m`) loop, real FDA-verdict
  mechanic, DONE through P3. Not taskbar-wired; standalone `button.sh`.
- **my-lawyer** — real legal case-building/argument CHTPM game, same
  async LLM-worker pattern as my-biotech, real implemented judge
  classifier — less exhaustively harness-tested than my-biotech.
