# 08 — Roadmap

- `design-docs/TODO-2026-09-15/MAJOR-PRIORITIES-2026-09-15.md` — **the
  current real major-priorities brainstorm**: 3 tracks settled via
  live Q&A - (1) mineclonia as a real, loadable, re-referenceable
  file:desk (Grok, direct in-house), (2) AI-ARC/hai-studio
  (discovery-then-iteration, starts by synthesizing the HARNECIENT.SMOL
  NIGHT_07-14 conversations, not re-deriving), (3) minigames
  (DSR/TSOTS/TPMOJIO/Dwarf-Fortress) + shared AI direction. 5
  cross-cutting decisions settled (NPC move = Common-Event scripted;
  triggers on play=on in a loaded file:desk; rendering reuses
  board-viewer's existing per-frame entity read; updates flow through
  the real `master_ledger`/`ledger_append()` standard; a new shared,
  reusable "AI-framework Common Event" vocabulary connects Track 2 to
  1/3). Also flags a hard blocker: no per-entity trigger scoping yet
  (only one `player-touch` Common Event exists house-wide today) - must
  land before Track 1 can actually work.
- `FORWARD-ROADMAP-2026-09-02.md` — **the current real plan going
  forward**: hardening a live, human-supervised Sonnet/Grok chat
  channel, then Grok's real task sequence (media-studio + network-app
  khtpm ports, settings/polish, db-hq RPG-Maker parity, image-editor+AI
  roadmap). Start here for "what's next."
- `design-docs/GROK-HANDOFF-2026-09-02.md` — **the current Grok
  onboarding doc** — replaces the old, now-archived render/input
  handoff (stale filenames, predates `CENTROID_GOLD_STD.md` and
  everything since). Read this before tasking Grok with anything.
- `OPEN-ITEMS.md` — the current, real open-item summary (start here).
- `design-docs/SQL-HQ-DESIGN.md` — **2026-09-08 BUILT (steps 1–6, 9)**:
  sql-hq = SQL over `.csv` / `.pdl` (vendored sqlite3 amalgamation);
  x11-hq window + macro sidebar + `sql_hq repl` CLI, opens from
  `[ ]db → sql-hq`. Remaining: staged Commit/Rollback, Export/History,
  grid polish (§6 table).
- `design-docs/TASKBAR-MENUS-DATA-DRIVEN.md` — **in progress**: strip
  submenu rows moved to `livedesk_taskbar.pdl` `<cell>_menu_N_label/_cmd`
  read at open time. player/ai/db converted; user/pals/toys/clock
  (directory-scanning) still hardcoded.
- `design-docs/MY-PALETTES-TILED-OHR-TILE-EDITOR-DESIGN.md` — Tiled +
  OHR pickers + import-only My Palettes + tile-editor v1 **landed
  2026-09-08** (MVP). Design still the contract for TMX/TSX and saver.
- `design-docs/PIECECRAFT-HQ-GAME-EDITOR-AND-PLAY.md` — **2026-09-08
  studio vision + implementer briefing**: desktop is an RM map too;
  play does not hide chrome; Transfer/Shop/Battle/db guidance; CDDA /
  MC / Civ / Pokemon as event skins (§§6–9). Range overlay §6.7
  (priority; builtin compositor+BFS, plugin = registry+db).
- `TILESETS-EVENTS-AND-GAME-CLONES.md` — **2026-09-08 find-it-later map**:
  palettes categories (RMMV / Mineclonia / CDDA / emoji / tiled / ohr),
  outside-zip asset PDLs, event-guide sheets, registry + `mr_world`,
  sample piececraft maps, and how those attach to MC / CDDA / Civ /
  GTA / RPG Maker clones. Start here before hunting tileset or event
  paths.
- `au-31/` — 2026-08-31's live in-progress work directory (`00-todo.md`
  real todo list, `01-manager-design.md`/`02-network-browser-...md`
  design docs). Moved verbatim from `1.^V-hq/au-31/`.
- `design-docs/LLMUD-INTEGRATION-DESIGN.md` — **2026-09-17 design**:
  folds the external `XO/LLMUD_CODE` LLMUD/Bank-Systems architecture
  proposals into this house's own AI-system plan; cross-references
  `H-AI-LAB-DESIGN.md` Part 4/5 directly (independent convergence, not
  new work there) and names a genuinely-new Synonym/Relation/Sentence
  Bank layer, smallest-first-step = a Synonym Bank + lookup op wired
  into Part 5's `ai_fsm_transition`.
- `design-docs/AI-FUNCTION-CRAFTING-DB-HQ-DESIGN.md` — **2026-09-17
  design**: a new db-hq tab that crafts events-hq AI command rows
  (`H-AI-LAB-DESIGN.md` Part 5) the way `CANVAS-CRAFT-DESIGN.md`
  crafts chemistry — bench/recipe/inventory UI reused, bounded
  Gemma-suggests-never-auto-writes discipline from Part 4. Gated on
  Part 5's own smallest-first-step landing first.
- `design-docs/LLMUD-HACK.md` — **2026-09-18 design, technical-depth
  pass for the ai-research/ML-PhD side**: the Harnecient Hack (see
  `HARNECIENT-HACK.md`, kept simple on purpose) mutated to watch real
  action sequences instead of generating text - 4 shapes (Original/
  Watch/Behavior-Bank-Slotting/Meta-LLMUD), verifies + resolves every
  real gap in `4.qwen-harnextend++.txt`'s proposal (DESCRIBE-not-
  CLASSIFY ambiguity in Watch Mode, missing fallback, missing
  artifact-visibility, undefined weight-update rule - now a real
  Laplace-smoothed formula), names the real, already-existing watch
  surfaces (`entity_menu_history/<pid>.txt`, Claude Code's own tool
  transcript - no new capture infra needed), and draws a real, new
  bridge to `IRL-BOOTSTRAP-RECURSION-SPEC.md`'s famous-llm work
  (Behavior Banks as a second Layer-0 curriculum source). §7 lists 5
  real open theory questions for the ai team, deliberately unresolved.
- `design-docs/DUSTOPIA-HACK.md` — **2026-09-18 design**: sibling to
  `LLMUD-HACK.md`, verifying `6-qwen-dustopia-hack.md`'s "self-building
  world" proposal (DESCRIBE→SCORE→STORE→SCALE applied to fractal
  world-CONTENT generation, not action-replay). §1 gives the mechanical
  (not just cited) explanation of why DESCRIBE beats CLASSIFY, the gap
  flagged as missing from `NIGHT_16`/`LLMUD-HACK.md`. §3 honestly
  scores each of the 4 named bank layers against real house status
  (Synonym Bank real, Behavior Bank reuses LLMUD-HACK's own schema
  unchanged, Relation/Sentence Bank real ideas with zero house-side
  design work). §4 flags the source doc's "λ spectral flow parameter"
  as imported-but-unverified rather than inventing an explanation. §5
  sets a real, stricter auto-execute policy than LLMUD-HACK's
  (propose-and-confirm by default), since this hack can spawn new game
  content, not just replay known-good actions. §6/§7 name PALCRAFT/
  mineclonia as the real near-term testbed and argue for building one
  shared watch/observe layer (not two) ahead of either hack's own
  bank-scoring work. Companion audio: `1-1.HARNECIENT.SMOL/NIGHT_17_
  NEW_GAME_PLUS.txt`/`.mp3` — "new game plus" framing, all three hacks
  as one recursive describe-then-score system run three times on a
  bigger object each loop. Follow-up: `1-1.HARNECIENT.SMOL/NIGHT_18_
  THE_PRIMITIVE_AND_THE_PAGE.txt`/`.mp3` — dramatizes the live
  `13.agent-coms/KILO/claude-2-kilo-9.17.md` handoff Q&A: the new
  house-wide "never write C for what an event can do" rule with its one
  exception (new `ai_*` event-command primitives), the DSR/WSR-CIV
  parallel-track multi-copy event-reuse experiment, and the confirmed
  zero-recompile context-menu-as-event scaffolding. Follow-up:
  `1-1.HARNECIENT.SMOL/NIGHT_19_THE_UNFINISHED_LEDGER.txt`/`.mp3` —
  an explicitly OPEN episode (no closed loop, unlike 16-18): reviews
  the external JEV docs honestly (useful diagram, unverified external
  product claims, banks table overstated vs `DUSTOPIA-HACK.md` §3's
  real status) and records the user's KPI/token-saving/human-parity-
  docs conversation from `AI-TRACK-BRAINSTORM-QUESTIONS.md` Questions
  5-6 as still unresolved, on purpose. Direct follow-up, same day:
  `1-1.HARNECIENT.SMOL/NIGHT_20_THE_SCHOOL.txt`/`.mp3` — a BREAKTHROUGH
  episode: finds the missing Corpus/Training Layer (belongs to Famous
  LLM/tomom, not the Bank Layer or frozen Gemma), confirms all-four-
  Banks/every-app scope, and names the "school" model (per-entity
  learner instances, curriculum classes, teacher now the user/Gemma
  later, pass/fail gating, growable primitives) from
  `AI-TRACK-BRAINSTORM-QUESTIONS.md` Question 7 — design shape found,
  nothing built yet. Direct follow-up, same day:
  `1-1.HARNECIENT.SMOL/NIGHT_21_THE_RETURN_PATH.txt`/`.mp3` — a
  video-prep agent reviewing NIGHT_20 catches the real hole: no
  return path from tomom's learning back into gameplay. Names and
  orders four injection points (`AI-TRACK-BRAINSTORM-QUESTIONS.md`
  Question 8) — `.pdl` parameter overrides (most concrete), FSM path
  candidates promoted through the existing Bank reward-weight gate,
  an unresolved DESCRIBE-vs-direct-emission decision for Event
  generation, and an unshaped shadow-scored primitive approximation —
  design shape found, nothing built yet. Direct follow-up, same day:
  `1-1.HARNECIENT.SMOL/NIGHT_22_THE_CONCEPT_BANK.txt`/`.mp3` — opens
  with a real, unplanned discovery (tomom's `chatbot_moe_v1.+x` binary
  was a day stale behind its own already-fixed source; one rebuild,
  verified live across all 10 real subject curricula), then designs
  the Concept Bank (`AI-TRACK-BRAINSTORM-QUESTIONS.md` Question 9):
  named z-nodes, fixed-slot pointer records, hub-and-spoke topology
  (not a maze), weights living only at the spoke with a derived mirror
  at the master, and the FSM/RL/GOAP meta-level extension — design
  shape found through real back-and-forth, nothing built yet. Direct
  follow-up, same day: `1-1.HARNECIENT.SMOL/
  NIGHT_23_TERUMON_OPEN_THEIR_OWN_CLASSROOM.txt`/`.mp3` — a new
  evolvable pet class (terumon, formerly circulated as fuzzpets/
  dustpets/muchipets) checked directly against seven real house
  pieces (entity system, tomom's school model, the `A-TEARIT-IS-ALL-
  YOU-NEED.md` promotion loop, Watch Layer, chemistry tiles, Events
  pipeline, chain-hq/myne-qrypto); designs the owner-set
  `learning_limits.pdl` schema (direction/size/strength/schools/
  environment) as a proposal-side filter sitting in front of the
  existing validator, resolves isolation-chatbot mode as a second
  proposer into the same learner instance rather than a separate one,
  and opens `x0.parent-level-dev-env-04.04/yz.muchiverse/#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/terumon-dev/` with four
  seeded dustball terumon carrying meaningfully different learning
  limits — design shape found and four experiment seeds filed, no
  Watch Layer observation or promotion run yet.
- `design-docs/A-TEARIT-IS-ALL-YOU-NEED.md` — **2026-09-22, real
  technical spec, not a dramatization**: the full propose → validate →
  replay → promote loop that lets Gemma (constant, cheap, DESCRIBE)
  and Claude/tomom (occasional, deep judgment) jointly hand-tune the
  Concept Bank from real feedback instead of GPU-brute-force training
  — real record formats (`OBS`/`FEEDBACK`/`EDIT`), why FSM/GOAP
  self-authoring reuses the existing Events compiler instead of a new
  code-gen path, the bootstrapping/trust-tier order (mirrors the
  grade-level curriculum idea), and where classic FF/BP/QKV-style
  training still has a real, scoped role (magnitude-refinement within
  an already-named topology only, routed through the same promotion
  gate as any other proposer — never topology discovery). Explicit
  OPEN vs. decided sections, not papered over.
- `design-docs/RELAY-WINDOW-TARGETING-DESIGN.md` — **2026-09-18
  design, not started**: formalizes how a relay-driving agent picks
  the right window/PID. Real current mechanism confirmed by direct
  code read: `history_path()` keys purely on the process's own
  `getpid()`, dispatch never checks X11 focus (deliberate, so
  human+agent can share a display), and per-PID files (fixed
  2026-08-29) already prevent cross-window bleed. The real gap: no
  formal target-resolution/liveness-check step, and the taskbar's own
  `focused=0/1` tracking (`livedesk_hq_windows_<pid>.txt`) doesn't gate
  relay consumption at all — meaning a relay-only test can pass a step
  a real human replay would need an extra focus-click for (ties
  directly to the `relay-testing-may-mask-real-focus-bugs` house
  rule). Proposes making that focus-raise step explicit/logged rather
  than silently skipped, and extending the existing per-window registry
  with a `purpose=`/`title=` field for a real purpose->PID lookup
  instead of `ps aux` grepping. §5 lists 3 open questions for the user.
- `design-docs/` — 63 design/plan/handoff/investigation docs moved in
  bulk (`git mv`, history preserved) from `1.^V-hq/`. **Not
  individually hand-condensed** in this pass (see note below) — still
  real, readable source material, just not yet trimmed to the book's
  target prose tightness. Treat as a holding pen: the next pass
  through this chapter should fold each into `OPEN-ITEMS.md` (if still
  open) or a short dated `04-bugs`/status note (if resolved), then
  delete the original.

## ⚠️ 2026-09-02 correction — a stray nested duplicate held newer content

A leftover pre-migration duplicate folder was found nested inside
`44.xyz.01.00/#.#.✅️.cal-user-sum/1.^V-hq/` (missed by the main sweep
since it was outside the "root-only .md files" scope) — 9 real files,
all dated 2026-08-29 through 2026-09-01, now filed into `design-docs/`.
**Two of them SUPERSEDE files already in this chapter** — the older
versions are kept alongside as `*-EARLIER-SUPERSEDED.md` rather than
deleted, in case anything in the earlier draft didn't make it into the
newer one:
- `design-docs/HANDOFF-2026-09-01.md` — the newer one describes chat-hai
  fully migrated onto the shared renderer, 4 house-wide bugs fixed, and
  a working global z-order/always-on-top toggle — all genuinely newer
  than the superseded version's status.
- `design-docs/CURSWORD-DESKTOP-3D-AND-PIECECRAFT-INSCENE-DESKS-DESIGN.md`
  — the newer copy is 867 lines vs. the superseded 225-line draft.

Two more of the 9 are real, still-relevant, standalone reference docs
worth flagging directly (not just buried in the holding pen):
- `design-docs/sep-1-events-SOS.md` — a ranked, copy-pasteable "what's
  actually left to code" list for db-hq/events-hq commands (Tier 1
  registry-only edits through Tier 4 real-system work), plus a
  step-by-step guide for a low-context agent to add a Common Event.
- `design-docs/sep-1-grok.md` — explains the real events/pal-script
  architecture as the template the other 13 db-hq tabs (Actors,
  Classes, Items, etc.) should be measured against; names the concrete
  next audit step (none of those tabs have been checked yet for how
  close they are to this standard).

## Honest scope note

Given time budget, chapters 01-06 (orientation, architecture,
pitfalls, bugs, faq, testing) got the full "condense, don't just
relocate" treatment this migration's principles call for. Chapter 08
did not get the same per-file treatment — 63 files from `1.^V-hq/`
were moved in bulk into `design-docs/` rather than each read in full
and hand-condensed into prose. This is a deliberate, disclosed
tradeoff (the task instructions explicitly allow lighter/faster
treatment of 07-09), not an oversight. Nothing was deleted without
reading its INDEX.md summary first — the dead `archive/`-prefixed
pointers were the only outright deletions.

## Real, current known-open work (from `INDEX.md`'s own Tier-1 list, 2026-08-31/09-01)

- **Events/db-hq**: real, low-risk next steps identified but not
  started (see `design-docs/!.OPEN-2do-events-db-networking-2026-08-
  28.md`, `design-docs/EVENTS_AND_DB_GUIDE_🎪.md`).
- **Cross-platform (Windows/Mac)**: tracked in
  `design-docs/CROSS-PLATFORM-PENDING-2026-08-29.md`.
- **Generic khtpm dispatch table** (replacing `g_is_<mode>` flags):
  design in `02-architecture/xperiments/khtpm-generic-dispatch-
  design.md`, not yet implemented — see `CENTROID_GOLD_STD.md` §3
  rule 7 for the ordered migration plan.
- **ASCII/headless khtpm renderer** (`ascii_draw_elem()`): the strip
  half is **BUILT + verified** (`cli`, bidirectional, DIAMOND
  marker-driven, 2026-09-06). Extending it to every window (entity
  menus, sub-menus, HQ windows, headless) is planned in
  `design-docs/TERMINAL-MIRROR-PARITY-all-windows.md` (4 shippable
  steps). Older sketch: `02-architecture/xperiments/chtpmx11-
  refactor.md` §8.
- **LayDoc → Elem/CSS taskbar retarget**: not started — see
  `design-docs/LAYDOC-ELEM-PORT-IMPLEMENTATION-PLAN.md`.
- **`dbhq_load_actors()` and sibling inline loaders**: audit pass not
  done — see `04-bugs/BUG-LOG.md` and `TPMOS-COMPLIANCE-DEBT.md` (in
  `design-docs/`).
- **Toys-launch PID tracking gap**: see `04-bugs/BUG-LOG.md`.
- **chat-hai migration to Harnecient/khtpm standard**: per
  `design-docs/HANDOFF-2026-09-01.md`, chat-hai itself is NOT yet
  migrated onto whatever that handoff scoped — read that file directly
  for the real, current scoped plan before assuming it's done.
- **Joystick / controller support**: referenced in
  `0.browser-prompting/bugs-toys-gl/5.remaining-bugs-joystick-toys-
  delegation.md` (see appendix cross-reference) — not started as of
  this pass.
- **Future games planned**: `44.xyz.01.00/2xx.*` and `3xx.*` app
  folders (glut-craft, dwarf-fortress-clone, rpg-maker-clone,
  snes-civ, gb-pokemon, sw-battlefront, ttg-tactics, sp-irl, rpg-xyz,
  rtp-xyz) each carry their own `ARCHITECTURE.md`/`PROMPT.md`/
  `README.md` — left in place (per-app docs, out of this migration's
  scope), but worth a future roadmap pass to summarize status here.
