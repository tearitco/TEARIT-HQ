# 📍 START HERE — au11-hq Document Index

**If you're a new agent picking this up cold, read in this order until you have enough context to act.**

**📋 `au-31/` (2026-08-31) — today's live, in-progress work directory**
(`00-todo.md` is the real todo list, `01-manager-design.md` the real
per-app manager design):
factoring out real per-app managers (irc-chat/forum/chain) BEFORE
building their HQ window shells, per a real compliance finding
(`dbhq_load_actors()` in `khtpm_entity_menu_render.c` loads real data
but from inline loader code in the shared file, not a separate manager
- see `HOUSE_CODE_PITFALLS.md` #11). Nothing on this list has real code
against it yet as of this entry.

**🧭 `^.COMPACTION_HANDOFF_aug29.md` — read this ONE first, before
even Tier 1 below.** Emoji-heavy, human-and-AI-readable, plain-language
summary of the tail end of the Aug 29 session: what shipped, the real
mistakes made (and the pattern behind them — shallow doc searches
leading to redesigning things that already existed), where the real
specs actually live now, genuinely new information from that session's
conversation that isn't written down anywhere else yet, and real
questions worth settling before the next session starts. Smaller and
faster to read than working through Tier 1/2 cold.

**🪤 `44.xyz❤️‍🔥️00.17/HOUSE_CODE_PITFALLS.md` (2026-08-31)** — real,
live-confirmed problems + fixes/diagnostic paths, general-purpose (not
scoped to one feature): stale processes surviving `pkill -f` on this
house's own emoji/star-globbed paths, runtime `#.desktop/*.txt` state
surviving a source `git reset --hard`, external screenshot capture
racing live redraws, timing-race false positives right after an
edit+reset, relay files keyed by package PATH inheriting a stale
command from a killed prior process, static entity lists drifting from
the real active desk, `.chtpm` submenu nesting rules, verifying
subagent root-cause claims before trusting them, and the real
relay-injection testing order of preference. Read this whenever
something "looks broken" right after a change — several of these
produce symptoms indistinguishable from a real code regression.

**🛑 `^.ONE-MAP-ATTEMPT-ABANDONED.md` (2026-08-31) — read this BEFORE
touching camera modes, one-map, or per-desktop z-level stacking.**
The "one map" shared-compositor 3D idea (single X11 window raymarching
every entity together, transparent background like piececraft) was
built, live-tested, and abandoned same day: a real, confirmed
compositor limitation (`ShapeBounding` clips clicks but not actual
painting for a continuously-reshaped override-redirect window) with no
app-side fix found. A related z-level "stacking" experiment was
reverted too, by direct instruction, independent of whether it worked.
Also documents a real, subtle lesson: `#.desktop/*.txt` runtime state
survives a source-code `git reset --hard` and can look exactly like a
fresh regression if left dirty from testing. Don't re-attempt any of
this without reading it first.

**🔧 `NETWORK-CELL-HQ-WINDOWS-DESIGN.md` — Phase 1 done (2026-08-31),
network cell is real and live.** opencode built the real
`cli_io_window.c` console container + launcher scripts but ran out of
tokens before wiring the taskbar menu itself; Sonnet wired it (PDL rows
+ `livedesk_build_network_menu()` + `livedesk:open-network:` dispatch)
and found/fixed a real crash bug in `cli_io_window.c` along the way
(fatal `BadMatch` from a misplaced `XSetInputFocus()`). Live-verified:
all 4 rows (IRC Chat/Forum/Chain/Browser) dispatch correctly. One
separate, NOT-yet-fixed cosmetic bug remains (the cli-io window renders
nothing visible despite being stable) - see that doc's own "REAL PHASE
1 WIRING" section. §10 Phase 2 (real khtpm windows per app, not just a
terminal tab) is still not started.

> **`archive/` DELETED (2026-08-29, direct instruction after the doc-audit
> pass):** this folder existed 2026-08-24 through 2026-08-29 for
> lower-priority docs — closed-bug records, DONE/superseded handoffs,
> single-feature deep-dives, `.ARCHIVE.md` full-history twins. It's gone
> now; every `archive/<file>.md` reference still appearing anywhere below
> in this INDEX (there are ~15) is a **dead pointer** — the content is
> not on disk, only recoverable via `git log --diff-filter=D -- '**/archive/**'`
> in this repo. Treat any such reference below as "this record existed,
> here's roughly what it said, the actual file is gone" — don't go
> looking for it on disk.

---

## Tier 1 — Always read (small context budget, ~2 min)
-4. **CENTROID_GOLD_STD.md — GOLD STANDARD, adopted 2026-08-31 (house root,
   `44.xyz❤️‍🔥️00.17/`)** — the real, final rendering architecture for every
   NEW window/app going forward: one real parsed/laid-out/styled `Elem`
   tree (khtpm's own, real `x/y/w/h` + real `CssStyle`) is the single
   source of truth; every display target (X11/RGB today, a new
   `ascii_draw_elem()` ASCII/headless mirror tomorrow) is a thin, symmetric
   renderer walking that SAME tree — never a second independently-composed
   representation, never business logic inline in the shared renderer file.
   Traces the real 4-stage history that led here (chtpm_parser_pal's
   box-model-less text grid → the blind-rasterize RGB/GL mirror →
   the game-tile double-composer's real, documented race → khtpm's own
   Elem/CSS model getting the look right but losing headless parity and
   picking up the `dbhq_load_actors()` inline-logic drift) and condemns
   each stage's specific, real flaw honestly rather than glossing over it.
   States explicitly: **no free retrofit** for existing chtpm-native apps
   (IRC/forum/chain/mutaclysm/piececraft) — deliberate, opportunistic
   migration only, one app at a time, when a real touch already justifies it.
   Read this before designing ANY new taskbar window, HQ app, or renderer.
   **First real proof case built 2026-08-31**: `&.hq-apps/network/` -
   `network_browser_manager.c` (real manager: fetches a URL via curl,
   does a real simple manual HTML title/text/link extraction, publishes
   `#.desktop/network_browser_page.state.txt` + `..._status.state.txt`,
   consumes `..._action.txt`'s `go:<url>`), `network_browser_render.c`
   (real khtpm Elem-tree X11 window, live-tested/screenshot-verified),
   `network_browser_render_ascii.c` (real interactive CLI mirror of the
   SAME manager state - live-tested, including real link-follow).
   `open_network_browser.sh` now launches this instead of the old
   `cli_io_window.c` stub.
   **CORRECTED 2026-08-31, direct instruction ("we still want to use
   chtpm+layout module, we always should no matter what")**: this
   entry originally described the chrome as legitimately hand-built in
   C (citing `khtpm_choice_picker.c` as precedent) - that framing was
   wrong and is struck; see `CENTROID_GOLD_STD.md` §3 rule 1 and
   `TPMOS-COMPLIANCE-DEBT.md` §5 for the corrected rule and the same
   real gap now flagged in `khtpm_choice_picker.c` itself.
   `network_browser_render.c`/`network_browser_render_ascii.c` need a
   real rewrite to actually parse `network-browser-hq.chtpm`/`.css` -
   not yet done as of this correction. What IS done and real as of
   2026-08-31: the manager, the live taskbar wiring (network cell →
   Browser row → this app, relay-injection-verified via
   `hqcell 13`/`row 4` on `livedesk_agent_relay.txt`/`strip_history.txt`),
   and a real agent-relay (`network_browser_history.txt`) + frame-
   history (`network_browser_frame_history.txt`) pair for the X11
   renderer, matching every other khtpm app's own testing contract.
   **SECOND CORRECTION, same day**: a follow-up attempt to give
   network-browser a real khtpm mode INSIDE `khtpm_entity_menu_
   render.c` (replacing the standalone `network_browser_render.c`)
   was built the same way every existing mode already works - a new
   `g_is_network_browser` global checked at ~15 scattered dispatch
   points - and was caught + fully reverted (`git checkout`, confirmed
   zero diff from `origin/main`) before being committed or compiled,
   direct instruction: "the parser/renderer should have no knowledge
   of new projects and be completely agnostic." See
   `TPMOS-COMPLIANCE-DEBT.md` §6 and `xperiments/khtpm-generic-
   dispatch-design.md` for the real design this produced - network-
   browser-hq is now planned to be the FIRST mode built against a new,
   generic `g_khtpm_modes[]` dispatch table, not the 8th copy of the
   old per-project-hardcoded pattern. Not implemented yet.
-3b. **xperiments/khtpm-generic-dispatch-design.md — READ BEFORE ADDING ANY
   NEW MODE TO `khtpm_core_render.c`** (renamed from
   `khtpm_entity_menu_render.c`), direct instruction ("write to
   standards and index that this should never happen again, this is std
   drift") — REAL PIVOT, 2026-08-31: the `g_khtpm_modes[]` dispatch-
   table idea this entry used to describe was explicitly REJECTED
   (direct instruction: "we dont use .so or linking or anything...
   read/write to from external .txt file or .pdl from manager if you
   need"). The real, adopted, DONE-AND-LIVE-TESTED answer instead:
   - **Generic capability #1 (live `.chtpm` re-parse)** —
     `reparse_chtpm_if_changed()` re-reads a `.chtpm` on mtime change;
     any real manager process can keep regenerating a live `.chtpm`
     projection (same "manager owns projection, renderer just re-
     parses/renders it" philosophy `fo-menu-sys.md` already documents
     for the ASCII/`chtpm_parser.c` family).
   - **Generic capability #2 (`<cli_io>` text input)** — a real,
     project-agnostic armed-text-field element (`target_id=`,
     `input_buffer`, live-synced to `cli_io_state.txt`, Enter fires the
     SAME `action="<cmd>"` dispatch every khtpm element already uses).
     A real, non-obvious follow-up fix included: an armed `cli_io`
     field now takes a real `XGrabKeyboard` (db-hq's own already-
     existing `dbhq_grab_keyboard_retry()`, reused verbatim) for its
     armed lifetime - without it, typing silently failed the instant
     the mouse left the window under this desktop's focus-follows-
     mouse WM policy (confirmed live: real `XGetInputFocus` was `0x0`/
     None with the pointer far from the window). **Any new khtpm app
     needing real text input should use `<cli_io>` for this reason -
     it's the only input mechanism in this house proven immune to that
     class of focus bug.**
   Zero new `g_is_<project>` globals, zero per-project dispatch
   branches for either capability - both are fully generic, used first
   by open-hai's own real conversion (in progress). See
   `CENTROID_GOLD_STD.md` §3 rule 7 and `TPMOS-COMPLIANCE-DEBT.md` §6
   for the real incident this traces back to.
-3. **SKILLS.md (2026-08-29)** — read this FIRST, before anything else in this
   list. Not a task doc — a generalized "how to operate well in this house"
   compaction: the core file-based-state philosophy, the rendering
   architecture in one page, a landmines list of real bugs an agent will
   otherwise re-discover the hard way (build scripts overwriting shared
   source, per-mode files broadcasting to every open window of that mode,
   desktop tiles being real live entities not fixtures, `xdotool click`
   dangers, verification discipline, how to work alongside Grok/opencode
   without duplicating effort), and a read on the user's own working style.
   Everything below this entry is task/architecture detail; this one is
   judgment.
-2. **TPMOS-COMPLIANCE-DEBT.md — REOPENED 2026-08-31 (the original 3
   printf-XML violations stay RESOLVED 2026-08-25; a real, DIFFERENT,
   4th violation was found and condemned same day - `dbhq_load_actors()`
   in `khtpm_entity_menu_render.c` itself reads real PDL data
   (`&.widgits/db-hq/data/actors.pdl` - not hardcoded strings) but does
   so INLINE in the shared "hard boundary" renderer file, instead of a
   real, separate manager publishing a projection - the exact same
   "Manager owns projection" violation this doc exists to name, just
   with real file-backed data instead of fabricated content. Direct
   instruction: "that being hardcoded... should be condemned... it
   should have never happened" - not fixed yet, see the doc's own new
   §4 + `au-31/00-todo.md` for the real remediation plan. **§5, same
   day**: `khtpm_choice_picker.c` hand-builds its Elem tree instead of
   parsing a real `.chtpm` - real, live infrastructure (used by
   `tp_desktop_window_rgb.c`'s desktop-tile context menus and book-
   stack's dialogue-choice flow), flagged for the same fix, not yet
   done - see `CENTROID_GOLD_STD.md` §3 rule 1's own correction)** — real,
   confirmed architecture violations found while migrating stats-hq:
   all 3 launcher scripts (`stats-hq`, `palettes`, `bookmarks`)
   generated `.chtpm` UI markup via raw bash `printf` of XML tags, with
   no real manager process and no compiled, testable Op — violated
   TPMOS §11/§12 directly, and stats-hq's tabs were actually broken
   (labels never matched the renderer's own `TAB_LABELS[]`). All 3 now
   have real managers matching `khtpm_hq_manager.c`'s shape; see that
   doc's own "Status update" section for the rebuild details. Read this
   BEFORE building or extending ANY taskbar-launched window/menu — this
   pattern looks like it works and is the nearest copy-paste example in
   the tree for exactly that reason. Full inventory,
   severity reasoning, the compliant reference pattern (db-hq/events-hq/open-hai's real
   manager binaries), and remediation priority in the doc itself.
-1. **house-compaction.md — STANDING #1 PRIORITY (2026-08-24), NOT YET ACTED ON** —
   documents a confirmed real compliance drift: `khtpm_hq_render.c`'s `dump_frame_png()`
   (the shared renderer behind db-hq/palettes/events-hq/chat-hai/entity-menu) writes a
   PNG only — **no receipt file, no frame-history log** — unlike the house's own
   TPMOS/wraith-alpha standard (see `!.TPMOS_ONBORD_BIBLE_10.md` §13.7 + live
   `wraith-alpha/session/rgb/*.receipt.pdl` files, confirmed real and current) and unlike
   au11-hq's OWN already-written receipt convention (which exists in `ai-cell` but was
   never ported into the merged `khtpm_hq_render` binary). Practical effect: anything
   "verified" via `--dump-and-exit` on this family (incl. `palettes-handoff-2026-08-24.md`'s
   T1/T3 claims) was only pixel-verified, never receipt-verified — treat those claims as
   unconfirmed until the port lands. **Agreed order of operations (direct instruction,
   2026-08-24): document now, do NOT fix yet → finish reading/compacting the docs below
   first → THEN come back and do the receipt-port + any other compliance fixes → THEN
   resume palettes T1–T6.** Full findings, the real wraith-alpha receipt shape, and a
   scoped (not over-built) recommendation for what livedesk's own receipt should contain:
   `house-compaction.md` Part 1 + Part 2. Compaction candidates (Part 3) are the
   immediate next step, not this.
0. **DB-HQ-HOUSE.md — CURRENT db-hq TAB WORK (2026-08-28)** — per-tab khtpm+house-nav
   designs. **Leave Terms and Common Events alone.** Proof tab is **Actors**. Launch
   default must be Actors `[1]`, not Common Events. Palettes T1–T6 is a separate
   backlog (`palettes-handoff-2026-08-24.md`).
1. **This file** — orientation, what exists, read order
2. **HANDOFF.md → "⚡ TL;DR" + "🎯 Project State" sections only** — current status, what's next
3. **$.claude-hai-budget.md — HIGH PRIORITY, direct instruction (2026-08-13)** — before doing scoped/mechanical work yourself, check whether it should be delegated to a Harnecient model (h-ai) instead, to save Claude token budget. Log delegation opportunities there, not just plans.
4. **HARNESS-DELEGATION-PIPELINE.md — HIGH PRIORITY, direct instruction (2026-08-13)** — before writing any new one-off dispatch/test script, check whether it should be a reusable harness instead. Harnesses are sellable products, not just test infra.

Stop here if you just need to know "what's going on."

---

## Tier 2 — Read before touching code (medium context budget, ~10 min)
3. **HANDOFF.md (full)** — architecture (sessions/desks/events), file structure, known gaps, demo game list
4. **HANDOFF.md → "⚠️ STANDING RULE"** (near top) — check local chtpm, then tpmos
   (`1.TPMOS_c_+rmmp.0103.0001/`), before inventing any new UI/state pattern
5. **TESTING_STRATEGY.md** — relay-only testing rule (no direct CLI calls), harness template, frame-history reading. **Also read `_.0.aigent-testing-k9.txt` (THIS directory, `1.^V-hq/` — moved here 2026-08-27 from the house root, where it lived for years; a forwarding stub is left at the old path for the ~25 older comments/scripts still pointing there) alongside it, not instead of it** — that file is the OLDER file's actively-maintained, more detailed twin: per-program-family relay contracts (which files, which decimal codes), the real "text relay → text state dump → PNG/xdotool only as last resort" ordering (added 2026-08-26 after a real agent got this backwards live), and the newest presentation-video archive/pointer convention (added 2026-08-27 - real MP4/PNG evidence moves to `🧩️Piecemark-IT/中.SP_00.00/🗡️.crswrd.media-archive/<Month-DD>/`, a small pointer file stays behind so git pushes don't carry the media weight). **Direct standing note (2026-08-27): agents keep missing this file because it used to only be referenced deep in a routing table below, not up here — if you're reading this Tier 2 list, that IS your signal to go read it now, don't wait for a later section to remind you again.**
5b. **TASKBAR-MENU-ARCHITECTURE.md — READ BEFORE TOUCHING ANY TASKBAR MENU** — cell 14 (h-ai)'s
   submenu is C-hardcoded (`livedesk_build_ai_menu()`), NOT PDL-driven, despite matching PDL rows
   existing and looking live (they're dead — editing them wastes a full debug cycle, confirmed
   2026-08-15). Covers the real two-layer relay/dispatch architecture, the exact recipe to add a
   menu item without repeating a ~15-round-trip debug session, the `nav.sh` double-Enter trap, and
   the standing PDL-externalization refactor debt (why this drifted from config-driven back to
   hardcoded C, and what the real fix looks like).
5b1b. **CREATOR_AGENT.md — READ BEFORE BUILDING ANY NEW MENU, PICKER, OR TB-LAUNCHED WINDOW**
   (2026-08-24, written after a real session confused the two menu systems): the one-page map
   of BOTH menu mechanisms — (A) the tb-native shared dropdown under header cells (builder →
   strip_var_hqitems.txt → anchored popup; self-closing header cell = broken/detached popup;
   `livedesk:*` dispatch forms vs raw shell commands that break on `&` paths; cancel-row rule)
   and (B) hq-style `.chtpm`+`.css` windows via khtpm_hq_render (css filename derived from
   chtpm name — mismatch silently unstyles; apply_css_deep for nested elements; emoji tiles =
   sprite.csv pipeline, never font glyphs), plus the headless frame-dump verification workflow.
   **2026-08-25 addition, §2.5**: the real nav-index assignment protocol (content 1..N first,
   chrome/close control ALWAYS last — direct 2026-08-12 instruction, receipt-verified live,
   not assumed) and the `g_close_elem`-is-a-separate-global gotcha that made it invisible to
   the receipt tooling until fixed the same day.
5b2. **`taskbar-tpmos-parallel-refactor.md` + `taskbar-history-txt-migration-investigation.md` —
   READ BEFORE TOUCHING `khtpm_strip_parser.c`'s REAL X11 INPUT/DISPATCH CODE** (2026-08-18,
   cutover completed 2026-08-19): the taskbar now has a real terminal ASCII mirror (HQ menu's "cli"
   row) AND a single dispatch path — capture-only writers + one read-back dispatcher, matching
   mutaclysm's real `x11_mirror.c`/`game_dispatch.c` split. The old inline-dispatch path and its
   `KHTPM_NEW_DISPATCH_MODE` feature flag are DELETED, not just defaulted off — there is no
   fallback mode anymore. Live-verified with real X11 input (genuine user interaction AND real
   `XTest`-based injection tools - see `&.widgits/tile-picker/ops/
   tp_test_send_key.c`/`tp_test_send_click.c`, the real fix for "no xdotool on this machine"). A
   same-day frame-unification pass (`strip_frame.cells.pdl`) then introduced and fixed a real bug —
   see `taskbar-history-txt-migration-investigation.md`'s "Phase 3" section and
   `au11-hq/TASKBAR-FRAME-UNIFICATION-HANDOFF.md` (under `44.xyz❤️‍🔥️00.17/`) for the frame-sharing
   plan that pass was the first step of.
   `dispatch_key_code()` gained two real relay-forwarding fixes (`KSC_HQ_HEADER_BASE`,
   `KSC_FOCUS_LEFT`/`RIGHT`) this pass - know about them before assuming the relay's own
   capabilities match its older, more limited header comment.
5c. **`!.HOUSE_STDS.md` §J — READ BEFORE ASSUMING A `.chtpm`/CSS FEATURE EXISTS** — there are TWO
   separate, unmerged parser implementations sharing the `.chtpm` name: legacy `chtpm_parser_pal.c`
   (PAL-VM/text-grid, what §A of that doc covers) and the newer `khtpm_*` family (raw Xlib/Xft, no
   PAL VM, its own `khtpm_css_parser.c`) that the taskbar and every tb-launched sub-app (db-hq,
   open-hai/"open-hai", events-hq, chat-hai) actually use. Confirmed real feature drift this session:
   `khtpm_css_parser.c` has no flexbox AND no descendant-selector (`.a .b`) support — both silently
   no-op rather than error, and both caused real, live-caught chat-hai bugs. If you're building on
   any tb sub-app, you're in the `khtpm_*` family — §A of `!.HOUSE_STDS.md` does not apply to you.

---

## Tier 2.5 — Read before major architectural decisions

**HQML-DESIGN+PLANS.md** — Vision for a web-like markup language (HTML/CSS-like syntax) that would enable prettier UIs while keeping .pal scripting. Covers events-hqml (prettier event editor), db-hq (modern database UI), AI applications, network applications (forum/IRC). Design phase, technical requirements, implementation roadmap. Read this if designing new UIs or considering how to modernize existing ones.

**RENDER-FRAME-HISTORY-DRIFT-ASSESSMENT.md** (2026-08-28) — real, confirmed drift: khtpm_entity_menu_render.c (7742 lines, 7 hardcoded "modes": db-hq/events-hq/chat-hai/palettes/bookmarks/stats-hq/taskbar-settings) still carries per-mode layout/redraw/key-handling branches, a leftover from Stage 5's binary-merge (not a deliberate design), instead of being a truly generic engine with all app-specific behavior pushed into manager processes (the way generic onclick dispatch + several managers already prove works). Real end-goal: eliminate mode branches from the renderer entirely, one mode at a time. Design doc — read it once for the "why"; for live status, use the doc below instead.

**RENDER-REFACTOR-2DO-PROGRESS.md** (2026-08-28, LIVE, updated as work happens) — the real status tracker for this same effort: what's actually done vs. not, the next concrete step (no re-deriving needed), and a decisions log. **If resuming this work cold (new session, or picking up after a break), read this file FIRST, before the assessment doc above.**

**GROK-RENDER-INPUT-REFACTOR-HANDOFF.md** (2026-08-28, LIVE, async two-agent collaboration) — shared task/execution-record doc for this same effort, split between Sonnet and Grok working from different terminals. Has its own real hard-boundary/file-claim protocol (the shared render file can't be edited by both sides at once). Read before picking up any task from it.

---

## Tier 3 — Read only if working the specific task (large context budget)
6. **archive/USER_CREATION.md** — full research + plan for wiring account creation into livedesk's USER
   cell (tpmos multi-field `<cli_io>` reference, khtpm gap analysis, step-by-step build plan)
6b. **EVENTS_RUNTIME.md** — how the event-ez runtime actually fires (right-click "Play" /
   `RUN_METHOD:Play` relay, not automatic on-click), real bugs found+fixed (path resolution, stale
   in-memory cache after migration), the completed ops migration (`xyzfs/bin/<game>/ops/+x/`
   pattern), and the ops-vs-events sharing model (shared ops, private-until-published events via the
   `store` cell). Read before touching event/db/common-events work.
6c. **EVENT_AI_VISION.md** — the long-range design intent for events: RPG-Maker-aligned trigger
   types, message/choice events that reuse chtpm's own layout/navigation (self-bootstrapping, not a
   new UI), entity AI (movement/interaction/decision-trees via FSM/BT, referencing agent-45's
   tool-loop pattern and SCM's deterministic-first philosophy), and future network/MMO-participation
   events. Read before designing any new event TYPE (not just wiring existing ones).
6d. **archive/HAIKU_TASKS.md** — ARCHIVED 2026-08-29 (doc-audit pass: framed event commands as
   limited to "the current 3," contradicted by the real registry — 35+ commands now built). Task
   list is stale; historical only.
6e. **archive/DB_CONTEXT.md** — ARCHIVED 2026-08-29 (doc-audit pass: "Still Missing" table listed
   Items/Switches/Actors/Enemies as unbuilt; `DB-HQ-HOUSE.md` now documents 15 live tabs and
   `COMMON-EVENTS-MANAGER-HANDOFF.md` shows Switches/Variables done). `DB-HQ-HOUSE.md` is current.
6f. **EVENTS_AND_DB_GUIDE_🎪.md** — human-readable, emoji-heavy nuance guide for events-hq/db-hq:
   ASCII-vs-literal relay codes, focus-vs-selection confusion, the on-click/on_click trap, highest-
   numbered-page-wins shadowing, single-instance-guard reminders. Read this FIRST if something
   "isn't working" before diving into source — most gotchas are already catalogued here.
6g. **archive/EVENTS_ROADMAP_NEXT_STEPS.md** — ARCHIVED 2026-08-29 (doc-audit pass: its own
   command-count framing, "only 3 of 143 built," is long superseded — registry now has 35+ real
   commands, and Control Switches/Variables/Conditional Branch/Call Common Event all shipped).
   The `.pal`-scripted plugin-system idea it raises before battle/tactics work may still be worth
   reading for that one idea, but don't trust its status claims. `GAME-READINESS-GAP-ANALYSIS-
   2026-08-27.md` is the current gap assessment.
6h2. **COMMON-EVENTS-MANAGER-HANDOFF.md** — LIVE HANDOFF for opencode (2026-08-25): Control
   Switches/Variables, explicit Call Common Event, Conditional Branch (real compiler work using
   `prisc+x`'s already-existing `OP_BEQ`/`OP_J` opcodes), and a real common-events manager copying
   `101.lpns+map+4/system/game_manager.c`'s proven "one dispatcher, one shared ledger" shape.
   Contains task-by-task KPIs, explicit STOP-AND-ALERT checkpoints, a Questions-for-Sonnet section,
   and a Progress Log opencode updates as it works. Check this file's own Progress Log for current
   status before assuming anything below it is stale.
6h1a. **OPENCODE-CATCHUP-2026-08-27.md** — read this if you are opencode/
   ox-alpha resuming `COMMON-EVENTS-MANAGER-HANDOFF.md` from your own last
   log entry (line 2323, a premature "Task 3 DONE" claim). Tells you
   exactly what happened since (the real bug found in that claim, its
   fix, Tasks 4-7 all done, several new docs, two real PAL-authored
   harness proofs, and the real next task queued) and exactly which line
   to resume reading from (2617). A matching inline marker also sits at
   line ~2401 of the handoff doc itself.
6h2a. **PAL-VISUAL-SCRIPTING-PLAN.md** — vision doc for the events editor's
   Scripting/Scratch/Blueprints tabs (Task 5 built the real stub toolbar;
   real block/node rendering still unbuilt). Points at 6h2b for the
   broader harness-authoring direction (moved there to keep this doc
   scoped to visual-editor vision).
6h2b. **HARNESS-AUTHORING-GUIDE.md** — the canonical doc for building/
    updating ANY test/demo harness, single-feature or multi-feature.
    Covers: the current real bash-harness convention (with real examples
    to copy from); a grounded feasibility check for PAL/event-authored
    harnesses against `prisc+x.c`'s actual syscalls (relay injection via
    `SYS_OPEN`+`SYS_WRITE_LINE` already works today; polling frame-history
    needs a small sibling-file fix, no VM change; no `SYS_SLEEP` exists
    yet); a priority list of "harness-friendly" event commands worth
    building next (Loop, Wait, a new Send-Input command); a per-feature
    real launch-mechanism reference table (db-hq/events-hq/palettes/
    entities/Mutaclysm/h-ai/chat-hai/my-lawyer/my-biotech/piececraft-xyz/
    file-desk-switching); and camera/POV "director" guidance for anything
    with real camera control (Mutaclysm, piececraft-xyz's board-viewer
    map-view). Read this BEFORE designing any new harness — also
    cross-referenced from `_.0.aigent-testing-k9.txt` (which stays scoped
    to low-level injection/dump procedure, per its own stated rule) and
    `PAL-VISUAL-SCRIPTING-PLAN.md`.
6h2c. **!.OPEN-2do-events-db-networking-2026-08-28.md** — CURRENT opencode
    task doc (2026-08-28, "renamed handoff" — the active successor to
    COMMON-EVENTS-MANAGER-HANDOFF.md for opencode): Task 1 finish the
    Events list (Message + Character commands, superficial-first per
    direct user instruction tonight), Task 2 db-hq tab stubs, Task 3
    Networking tab + pal-irc/pal-chain/pal-forum GUI mirrors. Carries the
    HARD BOUNDARY (khtpm_entity_menu_render.c = claim/release via
    GROK-RENDER-INPUT-REFACTOR-HANDOFF.md, deferred-edit protocol) plus
    the ⛔ EXECUTION RECORD for Visual Scripting task #2 (Scratch view
    real blocks from compiled Control Switch shapes) and that work's
    deferred renderer glue. Read this FIRST as the current handoff for
    opencode before touching events/db/networking work.
6h3b. **CURSWORD-SOUL-VISION.md** — cursword's identity as the user's "SOUL":
   the account's first entity, free, always-there, unkillable, tied to the
   account — plus a capability roadmap (text chat real today; STT/TTS/image-
   gen/sound-gen/UI-automation all not built yet). §3 scopes a genuinely NEW,
   confirmed-undocumented-elsewhere idea: teaching cursword to navigate the
   real UI via Gemma selecting from a library of FSM/BT actions (not raw
   free-form control), with an RL-flavored context-scoring layer feeding
   that Gemma harness — same "deterministic dispatch, not free-form LLM
   output" doctrine as my-biotech/my-lawyer. §4 RESOLVES gap #0 from
   `GAME-READINESS-GAP-ANALYSIS-2026-08-27.md` (continuous play pauses the
   real clock unless marked non-blocking; turn-based folds the message into
   the current turn with a settable turn-cost variable). §5 CORRECTS that
   same doc's tile/palette gap — real precedent already exists (`rmmv`
   palette category, a compiled RMMV tile extractor,
   `RMMV_EVENT_EDITOR_GUIDE.md`'s UI-chrome-built-logic-not-wired shell) —
   read before scoping tile/map or cursword-automation work.
6h3a. **MARKETABLE-FEATURES.md** — real, cited catalog of EVERY taskbar cell's
   actual working state (HQ/file/desks/pals/palettes/chemistry-palette/edit/
   player/db/plugins/store/network/h-ai/datetime), plus entities/desk-pals,
   Mutaclysm (real, relay-driven 3D space), h-ai (TWO real modes: single
   chat backed by a real local Ollama LLM, and "chat-hai" — a real 4-agent
   +moderator round-robin conversation loop sharing one ledger, NOT just
   saved sessions), current emoji state, and files/desks switching. Written
   2026-08-27 after the marketing/owner-report videos only covered a
   curated subset and the user flagged several real, working things that
   got missed — read this BEFORE scoping any future marketing/onboarding
   material so nothing gets missed again. Ends with a "not yet covered in
   any presentation" punch list.
6h2c. **TILE-SYSTEM-DESIGN.md** — real, from-scratch design (no existing
   tile/map canvas found anywhere in this codebase, verified by grep, not
   assumed) for the tile system named in the "Confirmed next-steps order"
   (autotiling + animated tiles, flagged 2026-08-27 as needing real
   planning before implementation). **Major revision (§0a) after direct
   correction**: a tile IS a real entity, spawned via the SAME
   `tp_desktop_window_rgb.c` mechanism every desk-pal already uses
   (confirmed: that binary already has the real `GRID_CELL_PX=80` grid
   constant) — no separate map-canvas binary needed, that earlier draft
   option was withdrawn. Covers: the tile/tileset/map data model (flat
   PDL), the REAL RPG Maker MV autotile mechanism (§2, corrected
   2026-08-27 against actual engine source — 48 shape slots per kind,
   not 47, and each shape is composited from 4 real quadrant pieces,
   not one whole pre-drawn tile — see 6h2d/6h2e below), the animated-
   tile time function (pure function of wall-clock time), corrected
   RPG Maker MV tile size (48px, not 32px), the tile-scaling decision
   (48px assets scale up to fill the current grid cell), the hybrid
   map-storage model (§4: no map-print file = live entities only;
   map-print file = expanded on load; both = union — the SAME map-print
   format is also what boardview loads for real play/sharing), and
   (§4a) the real, confirmed requirement that tile entities drag-and-
   drop into Mutaclysm/piececraft-xyz as their 3D equivalent —
   extending a REAL, already-existing (but one-directional, pet-
   specific) XDND mechanism documented in `#.DOX/drag-drop-how2.md`,
   with a scoped "hook point" design pass (Phase 1.5) before the full
   voxel/block conversion tables (Phase 2). **§4b (added 2026-08-27,
   real mockup provided by the user)**: the actual tile-picker/
   placement authoring UI, previously left undesigned — the real
   integration point is `&.widgits/palettes/pallets.pdl`'s already-
   reserved-but-currently-static `rmmv` category (confirmed the exact
   real category the user meant by "6. palettes; 4. rpg maker tiles"),
   becoming a real A/B/C/D/E-tabbed tile grid + a real multi-tileset
   chooser (a genuinely new requirement — RPG Maker "tilesets" are
   named bundles of images, not one fixed set), with click-to-select
   arming a "current brush" that a subsequent real desktop click uses
   to spawn/update a tile-entity there. Read before writing any
   tile-placement or autotile code.
6h2d. **RPG-CODE-INDEX-REF.md** — real findings extracted from an actual
   deployed RPG Maker MV game's real, readable engine JS source
   (`rpg_core.js` etc.) — the real `Tilemap` autotile mechanism (tile ID
   encoding constants, the three real shape tables and their real sizes
   48/16/4, the quadrant-compositing draw algorithm, and the honest gap
   that neighbor→shape-index computation is editor-side/closed-source,
   not in this runtime engine). Cited directly by TILE-SYSTEM-DESIGN.md
   §2 for the real corrections made there. Add to this doc (don't
   replace it) as more of the engine source gets read for other real
   reasons later — it deliberately does not try to document the whole
   engine up front.
6h2e. **RMMV-ASSET-SOURCE-LOCATION.pdl** — records the real external
   mount path to the RPG Maker MV game/engine source referenced above,
   specifically so a future session can find it again if the drive/path
   changes (direct instruction: "their location should be stored in a
   related pdl file about tiles in case they move").
6h3. **GAME-READINESS-GAP-ANALYSIS-2026-08-27.md** — real, grounded status check (code actually
   read, not guessed): what's genuinely done vs. missing before a real RPG-Maker-style game can be
   built on top of the events work above. Three real gaps, in build order: (1) palettes/tilesets/
   map-authoring is 0% built for game tiles (today's "palette" is a UI color-theme picker, not a
   tile system), (2) message/choice event commands (Show Text/Choices) + numeric switch/variable
   IDs are not built, (3) no player/map/collision runtime loop exists in the real house code (only
   in disconnected, self-critiqued prototypes). Also names a real design gap-before-the-gap: where
   a message box even renders, and how that interacts with the Parallel-trigger tick loop, needs a
   decision BEFORE Show Text or the tile system get scoped. Read this before picking up any
   "let's build a real game now" work so effort doesn't go into gap #2/#3 before gap #0/#1 are
   decided.
6h. **PLUGINS-ARCHITECTURE-SCOPING.md** — design-only scoping pass (2026-08-25, no code yet) for
   the "10.plugins" taskbar cell: RPG-Maker-style per-project plugin list, `.pal` scripts that
   trigger real ops (not reimplement logic), real house precedent to reuse (sidebar+panel merged
   binary, shell+manager split, PDL registries). Lists open design questions (where plugin code
   vs. per-project opt-in lives, what a plugin can actually hook into, load-order semantics) that
   need a real decision before any Haiku-ready task gets carved out of this.
6i. **`EVENT-COMMAND-REGISTRY-ARCHITECTURE.md`** (this directory — moved here 2026-08-26 as
   part of a documentation-consolidation pass; real docs live here, `#.ref/menu/` is
   basics/reference data only) — REQUIRED reading
   before adding a new "type" to any hardcoded C array/switch anywhere in the house (event
   commands, db tabs, plugin hooks, etc.). The three-tier test for when a C hardcode is genuinely
   necessary (real branch/compiler logic) vs. avoidable bloat (a templatable dispatch, which is
   what most "add a new command type" work actually is) — with a real, live-verified worked
   example (`event_commands.registry.pdl`, a new command type added with zero recompile while both
   binaries were already running). Written after Sonnet's own first-pass answer on this exact
   question was too permissive and had to be corrected by direct user pushback — read it before
   repeating that mistake.
7. **2do-au11.txt** — the working task log: what's done, what's pending, timestamped progress notes.
   This is the most volatile file — always check its bottom (Progress Log) for the latest state.
8. **archive/a11.focus-troubleshooting.md** — historical record of two entity-window bugs already fixed
   (focus-steal, arrow-nav). Reference only; not active work.
9. **maintenance-fixes.md** — running list of small, non-blocking UI polish items (index numbers,
   window sizing, etc.). Check before starting cosmetic work; add to it, don't fix inline elsewhere.
10. **archive/DB-HQ-HANDOFF.md** — Implementation handoff for db cell (cell 9). Status: broken/incomplete
    AS OF BEFORE 2026-08-12 — db-hq itself got real, working this session, see #11 below for the
    current state before trusting this file's own "still-placeholder" framing.
11. **archive/EVENTS-HQ-RGB-HANDOFF.md** — MOVED TO ARCHIVE 2026-08-29 (the file itself already
    self-declared "SUPERSEDED, forward pointer" as of 2026-08-16 — events-hq/db-hq were merged into
    the shared `khtpm_entity_menu_render.c` binary that same day, and the whole render/input stack
    got substantially reworked again in the 2026-08-29 session; nothing here still describes current
    architecture). 2026-08-12 db-hq focus-fix + original events-hq build/wire history — read only
    for historical "how did this start," not current status. `khtpm-merge-how2.md` and
    `EVENTS-HQ-RENDER-UNIFICATION-PLAN.md` are the current real references.
12. **archive/OPENROUTER-INTEGRATION-HANDOFF.md** — 2026-08-16, real router API key work for open-hai.
    Status: **DONE for OpenRouter, TokenRouter marked a real confirmed paywalled non-starter**
    ($0 account credit blocks tool-calling even on free-labeled models — plain chat works fine).
    OpenRouter is fully live-verified through the REAL UI (not just the manager in isolation):
    cycled the model via a real relay-injected UI action, sent a message that ALSO matches the
    local tool-detection harness's own keywords, confirmed it now bypasses that harness for this
    backend ("un-harnessed"), and watched the model's real native `tool_calls` response actually
    EXECUTE (not just get detected) and show a real result in the transcript/GUI. Also caught and
    fixed a real bug mid-session: the shell (`khtpm_open_hai_render.c`) has its OWN separate model
    list from the manager's — adding models to only one silently broke the model-changer UI.
    **Read this before touching open-hai's backend/model logic** — real next steps left: no shared
    source of truth for the model list (real footgun), conversation history, cost tracking,
    model-name/error validation, more tools in the OpenRouter `tools` array.

---

## Document Roles (so you don't duplicate one)

| File | Purpose | Update when |
|---|---|---|
| `INDEX.md` | This file — pure routing, no content | New doc added/removed |
| `#.house-docs.html/1.index-house=solo.html` | **The human-facing house doc — this is what the user actually reads**, not just an agent-routing file. Narrative "how the house actually works" page (Overview/Standards/Taskbar/Legacy Engines/Display/Input/AI Backends/Testing/Extending/Roadmap sections). Keep the "Known Issues & Roadmap" section in sync with real findings the same way `INDEX.md`'s own changelog is kept in sync — when you land a real fix or finding that changes user-facing state, update BOTH this file and INDEX.md, not just one. | Whenever a real, user-relevant finding or fix lands — added the khtpm_hq_render receipt/frame-history gap here 2026-08-24 |
| `CENTROID_GOLD_STD.md` (house root) | **GOLD STANDARD, adopted 2026-08-31, §3 rule 1 CORRECTED same day** — the real, final rendering architecture for every new window/app: one real `Elem` tree, built by ACTUALLY PARSING a real `.chtpm`+CSS through the real layout pipeline (no exception for small/data-driven apps — a same-day correction struck an earlier draft that allowed hand-building one, citing `khtpm_choice_picker.c` as false precedent), N thin symmetric renderers (RGB now, ASCII/headless next) walking that SAME parsed tree, never a second composer or inline business logic. Condemns the real prior stages (text-grid-only chtpm_parser_pal, blind-rasterize RGB/GL mirror, the game-tile double-composer race, khtpm's own isolation-driven `dbhq_load_actors()` drift) by name, with the real flaw each one had. | Design every new taskbar window/HQ app against this doc's §3 rule — parse a real `.chtpm`, never hand-build the tree; build the real `ascii_draw_elem()` renderer described in §2 Stage 4 walking that same parsed tree whenever headless/CLI parity for a khtpm app is next asked for |
| `TPMOS-COMPLIANCE-DEBT.md` | **REOPENED 2026-08-31** — the original 3 printf-XML violations (stats-hq/palettes/bookmarks) stay RESOLVED 2026-08-25, real manager rebuilds; a real, different 4th violation (`dbhq_load_actors()` loading real PDL data inline in the shared renderer instead of via a manager) found and condemned same day, NOT fixed yet — see the doc's own new §4. | Build the real `dbhq_actors_manager.c` (or equivalent) fix before/alongside any new window mode added to `khtpm_entity_menu_render.c`, per `au-31/00-todo.md`; audit Classes/Skills/Items/etc. for the same shape |
| `house-compaction.md` | **STANDING #1 PRIORITY, undone** — the khtpm_hq_render receipt/frame-history compliance-drift finding vs. TPMOS/wraith-alpha standard, plus the doc-compaction candidate list for `1.^V-hq/` (44 files). Agreed order: compact docs first, THEN fix the compliance drift, THEN resume palettes T1-T6. | When the compliance fix lands, or a compaction item from Part 3 is acted on — tick it off, don't just delete the doc |
| `44.xyz❤️‍🔥️00.17/!.HOUSE_STDS.md` (house root) | **THE general house standards doc** ("from zero", §A–§K): CHTPM/PAL mechanics, session isolation/symlink ban, digit-dispatch, marker discipline, runtime-config-over-hardcode, rendering pipeline, CPU/testing discipline, widgets, 3D/raymarch, pitfalls F-18/F-19/#20/#21 (window focus/managed-window standards), §J two-parser-families warning, and **§K UI-authoring standards (2026-08-24): no hardcoded UIs ever (store→generated-artifact rule), context windows OLD vs NEW (`khtpm_entity_menu_render` is THE standard), generic renderer mechanisms (onClick open:/exec:, live reload) with honest port-status caveat, SHOW_PAGE chooser contract, bookmarks spec (superseded 2026-08-25, see its own note), and §K.6 (2026-08-25): no UI element without a mirror keyboard path** | Whenever a standing house standard is set, corrected, or superseded |
| `HANDOFF.md` | Living architecture + status snapshot, "hand this to a fresh agent" doc | Architecture changes, status changes |
| `archive/livedesk-dir-map.md` | ARCHIVED 2026-08-29 (doc-audit pass: §6-7 claimed shared event-command ops live in `xyzfs/bin/muchi-pet/ops/`; that directory no longer exists — ops moved to `&.widgits/events-hq/ops/` on 2026-08-29). Historical directory-map snapshot from 2026-08-17 only. | Historical only - re-derive a fresh directory map if one is needed, don't trust this one's paths |
| `legacy-shared-fix.md` | Separate leg of work from `khtpm-merge-how2.md`: consolidating all 16 legacy-GL projects' `system/`+`ops/` engine binaries. **AS OF 2026-08-17: `chtpm_parser_pal.c`/`prisc+x.c` consolidation is DONE - all 12 real participants (of 16 total; 4 have neither file) now on ONE shared baseline (`&.widgits/_shared-lib/system/`), see §3.10.** `chtpm_rgb_render.c` also consolidated (9 projects, §5c.7 in the other doc). `gl_mirror.c`→`x11_mirror.c` display-shim migration: 3 of 16 projects done (mutaclysm/piececraft-xyz/my-chara-txt), 13 remain - real, open work. Also covers a real mutaclysm interact-mode regression found+fixed post-consolidation (§3.11) and mutaclysm's own separate, deferred camera/3D work (§2.6, handed off to `archive/opencode-mutafix-pie.md`). | Every time the remaining 13-project GL migration or mutaclysm's own deferred camera work advances |
| `HARNECIENT-HACK.md` | **THE COMPANY'S BREAD AND BUTTER** - tool-like use out of NON-tooled models (gemma 270M/1b, stable-code 3b) by never telling the API we want tools: plain /api/chat, persona files forbidding structure, simple plain-text prompts, tolerant parser (`json_parser.+x`), deterministic app-side tool dispatch, real-file folding, fallback-everywhere, DESCRIBE-don't-CLASSIFY. Live reference: `@.apps/my-lawyer` (gemma reads+writes real case docs). Use this pattern for any feature that needs tool-like behavior on non-tooled models. | Before building any agentic/tool feature on a non-tooled LAN model |
| `TESTING_STRATEGY.md` | How to test (relay-only rule, harness patterns, verified recipes) - PLUS (2026-08-18) how to test REAL X11 input specifically (not the relay): real `XTest`-based `tp_test_send_key.+x`/`tp_test_send_click.+x` (confirmed `xdotool` is NOT installed here), and the `WM_CLASS`-vs-title window-matching gotcha | Testing approach changes |
| `TASKBAR-MENU-ARCHITECTURE.md` | Taskbar menu dispatch mechanics: which cells are C-hardcoded vs PDL-driven, the two-layer relay system, the exact recipe to add a menu item, nav.sh gotchas, standing PDL-externalization refactor debt, PLUS the full lifecycle/pitfalls of building a brand-new khtpm_*-family sub-app from scratch (elem-pool exhaustion, apply_css() clobbering, tail-vs-head ledger reads, PDL-driven geometry) | When a new taskbar cell/menu is added, a new sub-app is built, or the PDL-externalization refactor is finally done |
| `CREATOR_AGENT.md` | **THE map of the TWO menu systems** — tb-native shared dropdown vs hq-style `.chtpm` window — with the which-one-do-I-use table, wiring recipes for each, and every trap that cost real debug rounds (self-closing header cell = detached popup; `&`-containing raw commands fail in dispatch; css filename derived from chtpm name; nested elements need deep styling; emoji tiles = sprite.csv pipeline never font glyphs; headless `--dump-and-exit` verification) | When either menu mechanism changes, or a new trap is hit while building menus/pickers/windows |
| `palettes-handoff-2026-08-24.md` | Active session handoff for the palettes work: what landed (dropdown fix, cancel row, Elem.sprite pipeline, apply_css_deep, per-key css), remaining tasks T1–T6 each with how-to + how-to-check (re-verify placement, element sprites, scroll visual pass, stub-category pickers, PDL externalization pointer), env quick-reference incl. headless frame-dump verification recipe and bash-tool detach gotcha | When a palettes task (T1–T6) completes — tick it off; delete/supersede this doc once all are done |
| `taskbar-keyboard-relay-and-terminal-render.md` | **COMPACTED 2026-08-24** to a short stub — ORIGINAL finding (2026-08-18): taskbar's real X11 input+render was bundled into one process (`khtpm_strip_parser.c`), unlike mutaclysm/TPMOS's real split renderer/input binaries. Superseded in practice by the two docs below, which built the actual fix - kept as the original architectural discovery, still cross-linked as prerequisite reading from both. Full original content: `archive/taskbar-keyboard-relay-and-terminal-render.ARCHIVE.md`. | Historical/prerequisite only - read the two docs below for current status |
| `taskbar-tpmos-parallel-refactor.md` | **DONE, live-verified**: taskbar's real terminal ASCII mirror (HQ menu's "cli" row → `khtpm_strip_render_ascii.+x` + `khtpm_strip_keyboard_ascii.+x`, matching TPMOS's real renderer.c/keyboard_input.c split exactly - fixed a real `\r\n`/staircase bug from an earlier combined-binary attempt). Strip + HQ popup + bottom tabs render with the real `[cursor] N. [Label]` format (ported from `chtpm_parser.c`'s own `render_element()`). Real `[>]` cursor + arrow-key nav on the strip, driven by two real relay-forwarding gaps found+fixed in `khtpm_strip_parser.c`'s `dispatch_key_code()`. Still open: bottom-tab-bar activation (renders, doesn't yet activate via relay - bigger shared nav-claims system) and `cli_io` typing. | Before touching the taskbar's ASCII mirror, or `dispatch_key_code()`'s relay-forwarding logic |
| `taskbar-history-txt-migration-investigation.md` | **Phase 1 + Phase 2 DONE + live-verified, Phase 3 (full cutover) DONE 2026-08-19**: real X11 `KeyPress`/`ButtonPress` mirror into `#.desktop/strip_input_history.txt` (real `KEY_PRESSED:`/`MOUSE_EVENT:` format, same as `pieces/keyboard/history.txt`); the old inline-dispatch path and its `KHTPM_NEW_DISPATCH_MODE` flag are DELETED — capture-only writers + one read-back dispatcher (matching mutaclysm's real `x11_mirror.c`/`game_dispatch.c` shape) is the only path now, no fallback. Real XTest-based input-injection tools used for live testing (`&.widgits/tile-picker/ops/tp_test_send_key.c`/`tp_test_send_click.c` - real fix for "no xdotool on this machine"). A same-day frame-unification pass introduced+fixed a real arrow-key-submenu-nav bug (see the doc's own "Phase 3" section). | Before touching real X11 capture/dispatch in `khtpm_strip_parser.c`, or before the next frame-sharing step in `au11-hq/TASKBAR-FRAME-UNIFICATION-HANDOFF.md` |
| `A15.chat-hack.md` | Theory/exploration doc (not yet implemented): maps chat-hai's current bot-loop against the Harnecient Hack's 6 components, finds the real gap (no memory/relationships/document artifacts — only a flat ledger), proposes concrete DESCRIBE-not-CLASSIFY-safe designs for persona memory, relationship scoring, shared documents, and activating the already-stubbed moderator hook | Before implementing chat-hai memory/relationships, or exploring similar "evolving multi-agent conversation" designs elsewhere |
| `khtpm-merge-how2.md` | **COMPACTED 2026-08-24** (was 2839 lines → now current-status + the still-load-bearing HOUSE STANDARD decision rule only). **Stage 5 (literal single-binary merge) is DONE for all 5 window apps** — entity-menu, taskbar-settings, db-hq, events-hq, chat-hai all now live in ONE binary (`khtpm_entity_menu_render.c`), mode-selected via `class=`. Old standalone renderers archived to `_.ARCHIVED-pre-merge-legacy.zip` (db-hq's own kept live — `stats-hq` still uses it). **Legacy GL migration status now tracked in `legacy-shared-fix.md`, not here.** Full historical step-by-step (all 21 dated "real findings/DONE" sub-sections, exact diffs, exact per-app merge logs): `archive/khtpm-merge-how2.ARCHIVE.md`. | Before touching ANY khtpm app's rendering/model/identity logic (read the compacted doc); open the ARCHIVE only for historical HOW, not current WHAT |
| `archive/USER_CREATION.md` | Deep-dive research + plan + test log for one specific feature | That feature's design/status changes |
| `EVENTS_RUNTIME.md` | Event runtime mechanics, real bugs fixed, ops migration, ops-vs-events architecture | Runtime/architecture changes |
| `EVENT_AI_VISION.md` | Long-range intent: trigger types, message/input UI reuse, entity AI, network events | New capability designed/started |
| `archive/a12.opencode-prompt.md` | Self-contained handoff prompt for the parallel palette-picker agent | If that task's scope changes before it's dispatched |
| `archive/opencode-mutafix-pie.md` | Self-contained handoff prompt for a separate opencode agent: port mutaclysm's own 3D camera/render engine to piececraft/board-viewer's own real architecture. Real motivation: 4 separate real camera bugs were found+fixed live this session (camera-struct rewrite, duplicate-process flicker, missing offscreen buffer, a state.txt read-cap bug), each correct in isolation, but a new bug kept surfacing every time - direct instruction: stop patching, port the underlying architecture instead. Written 2026-08-17, full real bug history in `legacy-shared-fix.md` §2.6 | When the porting agent reports its real plan/findings, or scope changes before dispatch |
| `2do-au11.txt` | Task tracker / progress log | Every work session (append to Progress Log) |
| `archive/todo-a12.txt` | Task tracker for the DB Common Events CSS engine, status "not started" — scoped to a minimal CSS subset needed for the Common Events tab's visual structure, not a full `db-0000.html` port | When that task starts/progresses |
| `archive/15.clock-design.md` | Design doc (2026-08-13, not built) for taskbar cell 15's clock menu: realclock + game clocks (`gameclock0000`...), per-session/desk/world, switch/event-tied | Before building cell 15's clock menu |
| `archive/CURSWORD-HQ-SPAWN.md` | DONE, relay-verified (2026-08-24): "cursword" HQ menu row + entity spawn flow, built off `AU24-oc-handon.md` §4.4 | Historical/reference — read before touching the cursword entity template or HQ-menu spawn flow |
| `AU24-oc-handon.md` | Live, still-open task backlog (2026-08-24): events ladder (Show Text/Choices/Input Number/Wait/Play SE), common events in db, CURSword's remaining chat features (minimize/windows-list/voice I/O — §4 spawn mechanics itself is DONE, see `archive/CURSWORD-HQ-SPAWN.md`), test-artifact + video-report generation, hum/idle animation, execution priority order, critical rules, event-runtime quick-reference diagram | Before starting ANY of the events-ladder or CURSword-chat-feature work — this is the real task list, not just a snapshot |
| `archive/a11.focus-troubleshooting.md` | Closed-bug record | Rarely (historical) |
| `maintenance-fixes.md` | Small non-blocking polish items | Whenever one is noticed |
| `HQ-WINDOW-MAP-AND-AGENT-INPUT.md` (THIS directory, 2026-08-28) | Live 2026-08-28: `XMapRaised` on WM-managed chat-hai stole the human browser; override_redirect Settings/entity did not. Map HQ with `XMapWindow`. Do not gate history poll on X focus. Swatch leftover `PICK:` trap. | Opening or driving khtpm_entity_menu_render windows without stealing the human |
| `_.0.aigent-testing-k9.txt` (THIS directory, moved here 2026-08-27) | House-wide testing guide across ALL program families, with a 2026-08-11 khtpm-specific addendum + the presentation-archive convention at the bottom | When a testing mechanism is discovered/corrected for a NEW family |
| `44.xyz❤️‍🔥️00.17/LINUX_ROUNDTRIP.md` | Linux return-leg status: Mach-O quarantine (76 files), house-wide recompile (44/44 PASS), manual rebuilds (treetRace, hm_assert, apply_theme_op), verification, livedesk smoke-test | When returning from macOS to Linux (or any roundtrip); read before relaunching |
| `yz.muchiverse/ROUNDTRIP_FIX.md` | Concise fix log + re-run recipe for future roundtrips (purge Mach-O → compile-runner → manual extras → verify → relaunch) | When re-running the purge+rebuild recipe; includes rollback instructions |
| `$.crypts/compile-runner.ps1` | Windows house-wide compile runner (PowerShell twin of compile-runner.sh). Finds every build.ps1 and runs each from its own directory. **Untested — needs Windows/MSYS2 verification.** | When compiling all projects on Windows natively (no WSL) |
| `archive/EVENTS-HQ-RGB-HANDOFF.md` | ARCHIVED 2026-08-29 (self-declared superseded since 2026-08-16). 2026-08-12 session handoff: db-hq focus fix, events-hq built+wired, RGB Phase 0 result | Historical only - `khtpm-merge-how2.md`/`EVENTS-HQ-RENDER-UNIFICATION-PLAN.md` are current |
| `!.chtpm-render-dedup-guidance.md` (house root) | Deferred: `chtpm_rgb_render.c`/`chtpm_parser_pal.c` duplication across 22/28 dirs, NOT byte-identical (real per-app divergence) - investigation plan for whenever this becomes relevant, not urgent | Only when someone actually starts that dedup pass |
| `HARNECIENT-H-AI-RELAY.md` | **HIGH PRIORITY + LOAD-BEARING** - approved design to wire the Harnecient mode (`HARNECIENT-HACK.md`) into h-ai as a CHOOSABLE model, then demo + bake in a lasting reproducible harness for the full loop: relay injection into the real h-ai window → non-tooled model (270m/1b/3B) → deterministic read/write/run → **real control of the livedesk taskbar state files** (`strip_var_tabs.txt`, `strip_state.txt`). 4 phases (model switcher → `BACKEND_HARNECIENT` backend path → relay demo → `relay-harness/` N/N proof), success criteria + risks + milestones all in the doc. | Before starting any h-ai model-switcher / Harnecient-mode / relay-harness work |
| `chat-hai-design.md` | **HIGH PRIORITY** — design plan for a new side-bar multi-model conversation engine: 4 smol models (gemma270/qwen-ladder) constantly chatting with persistent memory, moderated by slower bigger models (qwen2.5:7b/haiku) that curate, reprompt, and delegate. Proof-of-concept ladder (Phase 0-5), memory/priority/FSM-recall architecture, relationship graphs, moderator loop, and roadmap. | Before starting any multi-agent / ambient-chat / side-bar conversation work |
| `13.AUG.13-HAI-2do.txt` | **DETAILED OPERATIONAL ROADMAP** — phases 1-5 with KPIs, sub-tasks, milestones for Harnecient integration into h-ai. Phases: model switcher (1-2h), Harnecient wiring (4-6h), relay demo (1-2h), reproducible harness (3-4h), autonomous generation (post-phase-4). Includes progression: Claude manages → Claude reviews → h-ai autonomous, with success criteria and risk mitigation. | Every phase completion or when strategy/timelines change |
| `1-1.HARNECIENT.AUBIO/` (this dir) | **HARNECIENT VOL 1 — the living textbook** (2026-08-12). 16 daily lessons in chapter format (README = cover + lesson map), audio-friendly, house cast (tomo/rahweh/maxine/iqa). Covers: the Harnecient Hack + 6 components, DESCRIBE-not-CLASSIFY, the 9/9 proof + war stories (fopen crash, 3B hallucination, 8B lying), the house + relay + nav.sh, h-ai & the relay plan, controlling tb, harness philosophy, and **the prompting masterclass (verbatim session-opener templates + token-saving playbook)**. Part VI = the telescope: create events, range-limited entity movement, **fake time starting 0 A.D. + endturn + time ticker + toolbar options**. Each day file stands alone; new lessons append as work grows. | Before starting a new Harnecient feature or onboarding an agent — read INDEX first, then the relevant day |
| `archive/OPEN-HAI-GUI-DESIGN.md` | REAL AND BUILT (as of 2026-08-12) - cell 14 "h-ai" window: real nav, scroll, disk-persisted deletable history, raw Ollama backend, PNG+receipt verification. Current primary work: Harnecient integration (model switcher → protocol → relay demo → harness). See `13.AUG.13-HAI-2do.txt) for detailed roadmap and phase breakdown. See `ONBOARDING.md` for current status and next steps.` FIRST for the actual current blocker |
| `&.widgits/open-hai/code-tools-harness/api-test-results.md` | 2026-08-12 Ollama tool-use probe. HEADLINE: HARNECIENT (my-lawyer strategy) proven 9/9 on non-tooled `stable-code` 3B + `gemma3:1b` + `gemma3:270m` (deterministic read/edit/run, zero `tools` fields). Contrast: 3B has NO native tools (server rejects); naive text-JSON calls are flaky/hallucinate; 8B native works but read-then-edit FAILS (parallel calls, guessed search). Harness + raw JSONs in same dir. | When tool-use capability/verdicts change |
| `&.widgits/open-hai/code-tools-harness/LEARNINGS.md` | 2026-08-12 "don't waste time again" doc: §0 = THE HARNECIENT HACK (see `HARNECIENT-HACK.md`) - proven 9/9 on 3B/1b/270m; 3B cannot do native tools (don't retest); pure-C-only rule; 8B emoji-path corruption; native args arrive as dict-or-string with `\uXXXX` escapes; 8B won't wait for read before editing; harness C-port parity bugs (missing `n++`, wrong verdict msg); fopen-on-directory `ftell=LONG_MAX` malloc crash. Read BEFORE re-probing Ollama tool use or touching the harness. | Before any new Ollama tool-use probing or harness edits |
| `#.Z.HUMAN_LLM/.MAC-ACCESS.txt` (house root, NOT au11-hq) | LAN Mac SSH+Ollama access (10.0.0.144, real model list, the "Ollama only binds localhost after a restart" fix) - single source of truth, read before any LAN-model work | Before any SSH/Ollama LAN access; if Ollama seems unreachable, check the restart-fix here first |
| `$.claude-hai-budget.md` | **HIGH PRIORITY** - Claude↔h-ai token budget strategy: when to delegate scoped/mechanical work to a Harnecient model instead of doing it in-session, standing offload opportunities, BT/FSM/RL test-runner ops worth building, delegation log | Every time a real delegation opportunity is identified or acted on - log it, don't just plan |
| `EVENTS-PAL-BUILDOUT-PLAN.md` | Delegation plan for building out the RPG-Maker event command vocabulary (~90 commands, `#.ref/menu/`) via `run_plan.sh`/`run_queue.sh` - real architecture confirmed (Change Gold IS `.pal`-driven, not hardcoded), staged rollout (Party/Actor commands first - structurally identical to Change Gold, highest delegation leverage; Flow Control needs real VM opcodes, keep in Claude; Movement/Screen/Audio/Battle blocked on unbuilt infra) | Before starting ANY new event command implementation - check which stage it belongs to first |
| `HARNESS-DELEGATION-PIPELINE.md` | **HIGH PRIORITY, BUSINESS-CRITICAL** - survey of every existing harness (Harnecient Hack, code-tools-harness, detect_tool(), relay harnesses, Phase-4 design) + the real gap (no adaptive multi-step tool chaining exists yet) + a staged design (deterministic FSM, pluggable tool registry, declarative scoring, checkpoint/resume) for a reusable, sellable delegation pipeline. Direct instruction: "always favor making a harness to do work that we can reuse over recoding something by hand." | Before writing ANY new one-off dispatch/test script - check if it should be a reusable harness piece instead; update when a new harness is built or the pipeline design advances |
| `44.xyz❤️‍🔥️00.17/completed-sym-list.md` + `sim-smell-fix.md` (house root) | **SESSION NOTE 2026-08-21 — symlink-migration Step 2 session (opencode agent)**: wired `persist_session_state()` into **19 projects'** `button.sh` (all `0.user-pal👤️`, `@.apps/*`, `&.widgits/*`, TSOTS families) after the earlier house-wide `ln -s`→`cp -r` swap. STRUCTURAL CHANGES to know about if complications arise later: (1) every patched button.sh now copies mutable session state back to the real project root at session exit — data written mid-session only exists under `pieces/sessions/<id>/` (or `/tmp/.<app>-*`) until exit, so anything reading real-root state mid-run must read the SESSION copy instead; (2) persist blocks auto-`mkdir -p` real-root dest dirs on fresh checkouts; (3) volatile files deliberately NOT copied back (`avatar_window_pids.txt`, quit_flag, history, relays, gui_state); (4) identity ops unchanged — they write real roots via `USERPAL_LOGIN_ROOT`/seeded `house_root.txt`; (5) 5 harness scenarios updated (stale real-root assertions → session/house-level paths, post-exit persistence checks added, sessions launched with `setsid` so cleanup sweeps can't kill the scenario); (6) `&.widgits/{context-menu,event-editor,event-ez}` verified STATELESS — no persist wiring, on purpose; (7) known pre-existing quirk left as-is: external TERM on `button.sh run` defers the EXIT trap while keyboard_input is foreground (Ctrl+C in-UI unaffected). Full per-project reports + test recipes in `completed-sym-list.md`; classification/status tables in `sim-smell-fix.md`. Awaiting human per-project signoff before `.pre-symlink-swap` backup deletion. **Also this session: house-wide compile sweep added — all 44 build scripts now PASS** (fixed 5 pre-existing breakages: undefined `$_SS` vars in zoo-INK/muchi-pals/mutaclsym+18.0G build.sh, wsr-pal missing freetype flags, tile-picker missing vendored stb_image.h). **NEW STANDING HOUSE RULE recorded in `!.HOUSE_STDS.md` §A.2 + pitfall 7b: NEVER use symlinks anywhere in this tree (they break on Windows) — copy or compile-from-canonical-source instead; §A.2 rewritten for the copy-in/persist-out model (live binary rebuilds no longer apply to running sessions).** | If any project's save/config data looks stale, missing, or duplicated after a session — read this note + completed-sym-list.md first; if a build fails, re-run its scripts/build.sh and compare against the 44/44 baseline |

**Rule of thumb:** architecture/status → HANDOFF.md. How-to-test → TESTING_STRATEGY.md. Deep
single-feature research → its own `<FEATURE>.md`. Task-by-task progress → 2do-au11.txt. Never duplicate
the same fact across files — link instead (`see HANDOFF.md §X`).

---

## Standing Rules (apply everywhere, not just one doc)

1. **Testing:** All testing goes through relay/inject (`nav.sh`), never direct CLI binary calls.
   See TESTING_STRATEGY.md.
2. **New UI/state patterns:** Check local chtpm usage first, then tpmos
   (`1.TPMOS_c_+rmmp.0103.0001/`) before inventing new shape. See HANDOFF.md's standing-rule section.
3. **Storage:** New work (games, events, common events) goes under `sessions/<user>/<session>/`, not
   `@.apps/`. Games = sessions. Maps = desks.
4. **Ownership:** Store your own harnesses/test output under your own
   `xyzfs/users/<your_account>/harnesses/` so agents don't clobber each other.
5. **Blocked/uncertain?** Document the question + dead end in au11-hq/ rather than guessing silently.
6. **Demo games must use palettes**, not hardcoded content — direct instruction: "when u build the
   game, i want u 2 use pallets, to make sure users can use it and that we have autonomous harnesses
   ready to sell." Palette population UI is on the 2do (Task 4) but not yet built — building it may
   be a prerequisite for the first demo game, not an afterthought.
7. **No hardcoded UIs, ever** (2026-08-24): every window/menu/layout is DATA and a GENERATED
   ARTIFACT of some store (`.ir.pdl`→pal, `bookmarks.pdl`→chtpm, `meta.pdl`→menu rows); extend
   generic renderer mechanisms instead of adding domain branches to renderers. New UI work rides
   the NEW context-window standard (`khtpm_entity_menu_render`), not bespoke windows. Full rule +
   mechanics: `44.xyz❤️‍🔥️00.17/!.HOUSE_STDS.md` §K. **This extends to any fixed vocabulary of
   dispatchable actions, not just windows/menus** — event commands, db tabs, plugin hooks, etc.
   (2026-08-26, direct instruction: "we never hardcode stuff, always keeping things super modular
   and abstract"). Real worked example + the three-tier test for when C is still legitimately
   required (genuine branch/compiler logic) vs. when it's avoidable bloat (a templatable dispatch):
   `EVENT-COMMAND-REGISTRY-ARCHITECTURE.md` (this directory). Read this before adding a
   new "type" to any hardcoded C array/switch anywhere in the house.
8. **No UI element without a mirror keyboard path** (2026-08-25): every real mouse interaction in
   a khtpm-family window needs a keyboard path that genuinely reaches it — arrow-key auto-scroll
   at a visible-region edge, real numbered/nav-dispatched Elems for mouse-only controls (never a
   bespoke click-region check), and "disabled" means dimmed + inert, never unnumbered/vanished
   from the tab order. Full rule + reference implementation (palettes' own scroll arrows):
   `44.xyz❤️‍🔥️00.17/!.HOUSE_STDS.md` §K.6.
9. **Proof-of-feature presentations require permission, always** (2026-08-25): a presentation
   (paced narrated MP4 + REPRODUCE.md + yt-summary.txt, see `_.0.aigent-testing-k9.txt`'s own
   "PRESENTATIONS" section for the full mechanics) is only built for a genuinely meaningful
   deliverable — a real feature, a real bugfix, a real architectural change — never for a minor
   tweak. Even when clearly warranted, **ask the user first before building one; never build one
   unprompted.** DO proactively suggest making one once a lot of real work has landed in a
   session — surfacing the suggestion is expected; deciding for the user is not.

---

## 🌐 Cross-Platform Compatibility (3 legs)

**Doctrine: Linux is canonical. Other OSes = thin shims + resolve-time aliasing — never a second product, never per-OS file renames, never PDL content rewrites.**

| Leg | Status | Doc |
|---|---|---|
| Linux (canonical) | ✅ always current | `44.xyz❤️‍🔥️00.17/!.HOUSE_STDS.md` |
| Windows | ⚠️ last verified pass 2026-08-21 — **stale, see pending doc below** | `yz.muchiverse/8.21.GROK-win.md`, `$.crypts/compile-runner.ps1` |
| macOS (return leg) | ⚠️ last verified pass 2026-08-23 — **stale, see pending doc below** | `44.xyz❤️‍🔥️00.17/LINUX_ROUNDTRIP.md`, `yz.muchiverse/ROUNDTRIP_FIX.md` |

**⚠️ PENDING (2026-08-29): a large body of Linux-only work has landed since the passes above and has NOT been ported/verified on Windows or macOS — most notably `khtpm_entity_menu_render.c` (the canonical merged renderer for 8 window modes) has NO Windows twin at all. Full delta list: `CROSS-PLATFORM-PENDING-2026-08-29.md`. Read this before the next Windows/macOS leg.**

**The round-trip problem (bites EVERY Windows trip, caught 2026-08-22):** NTFS cannot store
names containing `*`, so the house's `*.monads` / `*.START_BUTTON` trees must be renamed to `_.`
to travel; coming back they MUST be restored or every launch says "binary missing" (paths like
`*.monads/*.livedesk-taskbar/ops/+x/...` stop resolving) and the whole tree comes back with
stripped/locked permissions.

**The fix — `$.crypts/win-trip.sh` (house root), run around every physical copy:**
```
$.crypts/win-trip.sh status     # which shape is the tree in right now?
$.crypts/win-trip.sh to-win     # BEFORE copying to Windows: '*.' -> '_.', writes WIN-TRIP-MANIFEST.txt
# ... copy to Windows, work there (Win binaries alias '*.'<->'_.' at RESOLVE time - no content changes) ...
$.crypts/win-trip.sh to-linux   # AFTER copying back: restores '*.' names via manifest + chmod -R 777 whole tree
```
Manifest-driven: legitimately underscore-named entries (e.g. `_.0.aigent-testing-k9.txt`) are never
touched; refuses to restore without a manifest rather than guessing. Verified by full sandbox
round trip (content-identical).

**If you just merged from a Windows checkout and things "randomly" broke:** run
`win-trip.sh status` first. Symptom of an un-restored tree: `crypt_autostart: binary missing for ...`
on every pal, nothing on screen. Also check exec bits if only *some* things fail.

---

## 🎯 Plans AFTER events (recorded 2026-08-24, direct instruction)

The events ladders come first, then these land:

1. **Events ladder** — `44.xyz❤️‍🔥️00.17/#.ref/menu/event.commands.remaining.txt`
   (Wait → Show Choices → Input Number → Play SE, then by category) and
   `house-commands.remaining.txt` (TTS, file ops, exec shell, forum/AI
   commands). **Parity rule:** every event op also lands in db-hq's
   global/common events in the same pass (same shared runtime), and the
   db-side state those commands read (switches/variables kv files,
   Items/Weapons/Armors/System tabs) lands IN STEP — see the coupling
   notes in both files + `db-tabs-remaining.txt`.
2. **Palettes after that** — new categories now registered in
   `44.xyz❤️‍🔥️00.17/#.ref/menu/palletes/pallets-help.txt`:
   chemistry-palette (emoji/formula/color compound tiles + recipe-tree
   view from `elements]new=RECIPEZ+]z2🏆.txt`), tiling-rmmv (A1–E
   autotile mapping), minecraft-blocks, cdda-tiles, df-tiles (ASCII dual
   mode + raised view). Full mechanics:
   `44.xyz❤️‍🔥️00.17/#.ref/menu/tiling-palettes-chemistry.txt`.

---

**2026-08-29** (new long-term design doc, direct request after live
piececraft-xyz verification + board-viewer's gl_mirror→x11_mirror conversion
this session: `CURSWORD-DESKTOP-3D-AND-PIECECRAFT-INSCENE-DESKS-DESIGN.md` -
three connected real pieces, DESIGN ONLY, nothing built yet: (1) replace
piececraft-xyz's blocking pre-setup screen with a real in-scene files/desks
screen; (2) port board-viewer's real, live 2D↔3D + camera-mode + z-level
system (`bv_menu_input.c`, already proven) onto the house desktop itself;
(3) repurpose the real, existing `cursword` desktop pal as the desktop's own
xelector-equivalent interact controller - plain click (not right-click, not
drag) arms a glowing neon-blue halo + a real key-recording session (Escape
ends it), arrows move cursword, POV/camera keys drive the 2D/3D switch.
Grounds the design in real precedent (board-viewer's camera system, the
shared emoji→voxel asset pipeline, this session's own real desk-persistence
work, `tp_desktop_window_rgb.c`'s existing left-click-drag/right-click-menu
dispatch) and records 6 real, explicitly unresolved open questions - most
notably where a desktop-wide shared 3D scene/camera would actually render,
given every desktop entity is today its own independent X11 process/window)
by claude

---

**2026-08-29** (maps/tiles/z-levels documentation cleanup, direct
request after a live, corrected investigation: "clean up the
documentation... rerun by me the actual plan." Real finding chain, in
order: a first design doc wrongly framed tile/map as a from-scratch 2D
RPG-Maker-walk-around problem (retracted, never should have been
written before checking for existing spec); direct user catch ("maybe
its disparate?") led to `PIECECRAFT_XYZ_DESIGN.md`/`xelector-
context.md`/`CURSWORD-SOUL-VISION.md` §5; a further direct catch ("do
u see 101.drag-drop-test=ON🀄️?") found the desktop↔mutaclysm drag-drop
transfer is real, BUILT, and TESTED (2026-07-26, X11 Xdnd + exchange-
dir pet-import round-trip), not new as first assumed; a final direct
catch ("recently edited docs mentioning xdnd?") surfaced the REAL
primary doc this whole thread should have started from -
`TILE-SYSTEM-DESIGN.md` (2026-08-27, same directory, real working
verified code: autotile math pixel-verified against real RMMV assets,
a real tileset registry, `desk_grid.pdl` in `tp_desktop_window_rgb.c`)
- answers the core question (a placed tile is a real `tp_desktop_
window_rgb.c` ENTITY, not a separate canvas) and cites the real 2D→3D
bridge doc, `#.DOX/drag-drop-how2.md`. **Cleaned up, final state**:
`MAPS-TILES-ZLEVELS-CONSOLIDATED-SPEC.md` is now a short real router
doc (not a competing spec) pointing to `TILE-SYSTEM-DESIGN.md` for
single-tile/autotile/palette mechanics and `PIECECRAFT_XYZ_DESIGN.md`
for multi-tile chunked maps/Z-levels, records the 2 genuinely new
integration gaps this session's conversation surfaced (cursword
driving Z-level nav xelector-style; extending the drag-drop transfer
to piececraft-xyz), and `GAME-READINESS-GAP-ANALYSIS-2026-08-27.md`'s
own gap #1 is formally retired pointing here. Read `TILE-SYSTEM-
DESIGN.md` FIRST for anything tile-related.) by claude

**2026-08-29** (post-refactor audit, direct request: real explanation of
"modes" + whether the 4-loop draw collapse held up at 9,950 lines -
`*.monads/*.livedesk-taskbar/ops/parser-walkthru.md`, next to the
renderer itself. Confirmed clean: draw_elem()/render_tree() each still
exactly 1 real definition; per-mode layout/click/key/nav functions
(77 dbhq_/50 chai_/49 evhq_) are separate on purpose, not regrown
duplication - real reasoning why. One honest, acknowledged, not-yet-
fixed piece of debt: the Add-Command picker overlay is still hand-
drawn C, not a real .chtpm-declared generic element.) by claude

**2026-08-29** (Part B done: Common Events shares events-hq's Scripting/
Scratch/Blueprints tabs, 3 live bugs found+fixed along the way, plus the
Part A ghosting regression finally root-caused+fixed - see
`EVENTS-HQ-RENDER-UNIFICATION-PLAN.md`; a real in-picker "Delete" command
action added, nav-driven, no keyboard shortcut; and a real, BREAKING
input-relay fix - `history_path()` is now per-PID like `nav_tab` already
was, not per-mode, after a live incident where test input to the old flat
file also reached the user's own real window - see
`HQ-WINDOW-MAP-AND-AGENT-INPUT.md` §3 and
`GROK-RENDER-INPUT-REFACTOR-HANDOFF.md`'s own new end-of-file entry
before writing any more test input) by claude

**2026-08-25** (real manager rebuild for palettes/bookmarks + real grid scroll +
keyboard-accessibility standard): `khtpm_hq_render.c` DELETED outright (stats-hq/
palettes/bookmarks all migrated off it, confirmed via a full-tree grep for remaining
launch sites first) — see `au11-hq/TPMOS-COMPLIANCE-DEBT.md` for the compliance audit
that drove this (found ZERO other bash-composed-`.chtpm` instances house-wide besides
the three already fixed). Palettes/bookmarks each got real `*_manager.c` binaries
(`palettes_manager.c`, `bookmarks_manager.c`) replacing bash XML composition, per §K.1.
Palettes' chemistry/emoji grid columns and window height are now layout-derived (real
CSS tile width/gap/window-width, not hardcoded 4/10) with a real, working grid scroll
(thumb click/drag, mouse wheel, Page_Up/Down, real numbered up/down arrow buttons) —
a real bug (row containers moving but not their tiles, since the shared layout engine
positions children independently) was found and fixed along the way, verified with
before/after screenshots of a live drag, not just internal-state debug prints. New
**Standing Rule 8 added: no UI element without a mirror keyboard path** (`!.HOUSE_STDS.md`
§K.6) — arrow-key edge-autoscroll, real nav-dispatched Elems for mouse-only controls,
"disabled" ≠ "unnumbered." Also added a real, generic `badge_align_left` field
(`khtpm_render_core.c`/`khtpm_draw_core.c`) for edge-pinned elements whose nav badge
would otherwise run off-screen. `!.HOUSE_STDS.md` §K.2/§K.5 corrected (stale
"stats-hq only" / bash-composition claims) to match. Session trail: `aug-25-bed.txt`
(`yz.muchiverse/`, house root's parent).

**2026-08-24** (created `archive/` subfolder for lower-priority docs, per direct
suggestion: closed-bug records, DONE/superseded handoffs, single-feature deep-dives, and
the two `.ARCHIVE.md` full-history twins moved there — 12 files
(`a11.focus-troubleshooting.md`, `a12.opencode-prompt.md`, `CURSWORD-HQ-SPAWN.md`,
`DB-HQ-HANDOFF.md`, `khtpm-merge-how2.ARCHIVE.md`, `opencode-mutafix-pie.md`,
`OPEN-HAI-GUI-DESIGN.md`, `OPENROUTER-INTEGRATION-HANDOFF.md`,
`taskbar-keyboard-relay-and-terminal-render.ARCHIVE.md`, `todo-a12.txt`,
`15.clock-design.md`, `USER_CREATION.md`); every routing entry above and in Document
Roles updated to the new `archive/`-prefixed path, none left dangling. Main dir went
44→30 files. `AU24-oc-handon.md` deliberately NOT archived — still live backlog, see its
own entry) by Claude (Haiku)

**2026-08-24** (doc-compaction pass, part 2, approved: confirmed `_.hai-LEARNINGS-a12.md`
was a strict subset/earlier draft of the already-indexed `code-tools-harness/LEARNINGS.md`
— diffed line-by-line, deleted; corrected an earlier over-hasty "AU24-oc-handon.md is
superseded" call after actually reading it in full — it's mostly LIVE open backlog
(§1-3, §5-10), only §4's spawn mechanics are done (per `archive/CURSWORD-HQ-SPAWN.md`) — added a
status banner instead of shrinking it, plus its own missing INDEX.md routing row;
compacted `khtpm-merge-how2.md` (2839→~150 active lines, full history preserved in new
`archive/khtpm-merge-how2.ARCHIVE.md`) and `taskbar-keyboard-relay-and-terminal-render.md` (161→~30
lines, full original in new `.ARCHIVE.md`) — both were genuinely superseded, unlike
AU24-oc-handon.md. Also updated the human-facing `#.house-docs.html/1.index-house=solo.html`
Known-Issues section with the khtpm_hq_render receipt/frame-history finding, per direct
instruction that that page is what the user actually reads — added a standing INDEX.md
rule to keep both in sync going forward) by Claude (Haiku)

**2026-08-24** (compaction "safe batch" executed, approved: deleted `session-au15.md`
(raw session-transcript debris, not a doc) and the empty `x/` dir; fixed the stale
`chat-hack.md`→`A15.chat-hack.md` link typo; added missing routing rows for
`archive/todo-a12.txt`, `archive/15.clock-design.md`, `archive/CURSWORD-HQ-SPAWN.md` (all real, just unindexed).
Correction during the pass: `#.house-docs.html` looked like a 0-byte stub but is
actually a DIRECTORY containing a real 47K "Muchiverse House Docs" index page
(`1.index-house=solo.html`, 2026-08-19) — NOT deleted, flagged instead. Remaining
compaction items (AU24-oc-handon.md shrink, LEARNINGS diff, khtpm-merge-how2.md +
taskbar-keyboard-relay-and-terminal-render.md compaction) still pending in
`house-compaction.md` Part 3) by Claude (Haiku)

**2026-08-24** (found + documented, NOT YET FIXED per direct instruction: real compliance
drift — `khtpm_hq_render.c`'s dump path has no receipt and no frame-history log, unlike
au11-hq's own documented receipt convention and unlike the live, confirmed
TPMOS/wraith-alpha `.receipt.pdl` standard; wrote `house-compaction.md` with full findings
+ a doc-compaction candidate list for `1.^V-hq/`; agreed order is compact-docs-first, then
fix the drift, then resume palettes T1-T6) by Claude (Haiku)

**Last updated:** 2026-08-28 (added `ENTITY-MENU-LEGACY-DEPRECATION-
PLAN.md` - real plan to finish the stalled 2026-08-16/18 entity-
context-menu migration: only 7 of 45+ real entities are on the new
shared Elem/CSS renderer today, the rest still use
`tp_desktop_window_rgb.c`'s legacy built-in popup engine. Confirmed a
live bug in the ALREADY-migrated path (book-stack's menu: first item
invisible/jumbled with header) that must be fixed before mass-
rollout. Real design fork flagged for confirmation: generate a static
`menu.chtpm` per entity (current shape) vs. read `meta.pdl` directly
at popup-open time (recommended - no staleness surface). End goal:
archive `tp_desktop_window_rgb.c`'s popup engine once nothing depends
on it) by Sonnet

**Last updated:** 2026-08-28 (added `HOUSE_FAQ.md` 🏠️❓️ - real Q&A doc,
emoji-heavy, append-only, for recurring "why does it work that way"
questions the user asks mid-session. Check this FIRST before
re-deriving an architecture explanation from scratch or re-asking an
agent - if the question's already been answered here, link it instead
of re-answering; if not, answer for real then append it here so it
doesn't get re-asked cold next time) by Sonnet

**Last updated:** 2026-08-28 (added `RENDER-INPUT-REFACTOR-SUMMARY-2026-08-28.md`
— readable summary of the full khtpm_entity_menu_render.c render+input refactor
(frame-file paint, file-boundary input, marker-gated redraw, 4-loop collapse,
popup conversion, focus-steal fix, cross-window Tab-cycle, LayDoc→Elem port);
confirms mutaclysm-neo needed no work (it's the reference implementation);
records 2 known open bugs (toys-launch PID/teardown gap, open-hai+gemma3 not
responding, unrelated to tonight's changes). Full real-time trail in
`GROK-RENDER-INPUT-REFACTOR-HANDOFF.md` + `RENDER-REFACTOR-2DO-PROGRESS.md` +
`LAYDOC-ELEM-PORT-IMPLEMENTATION-PLAN.md`, this same directory) by Sonnet

**2026-08-28 (docs-only correction pass, Grok — append, not rewrite):**
- `HANDOFF.md` — HQ recipe still listed `XMapRaised`; appended WM-managed
  HQ = `XMapWindow`, popups keep Raised. Cite `HQ-WINDOW-MAP-AND-AGENT-INPUT.md`.
- `HQML-DESIGN+PLANS.md` — “new khtpm/-hq window → XMapRaised” was the
  override_redirect/strip rule; appended that WM-managed HQ is MapWindow.
- `_.0.aigent-testing-k9.txt` — entity_menu addendum: poll without X-focus
  gate; map table; strip arrows via `livedesk_agent_relay.txt` 1002/1001.
- `LAYDOC-ELEM-PORT-IMPLEMENTATION-PLAN.md` — Gap 7: do not build an Elem
  synthetic root; strip already has unified_apply. (Gap 2 DEACTIVATE
  correction was already appended by Sonnet.)
Source trail: `GROK-RENDER-INPUT-REFACTOR-HANDOFF.md` Grok docs-only claim.

**2026-08-28 (Q1–Q3 surfaces, Grok — after Sonnet ACK):**
- `#.house-docs.html/1.index-house=solo.html` — Input/Map subsection + roadmap
  bullet; links `HQ-WINDOW-MAP-AND-AGENT-INPUT.md`.
- `!.co-work/c-htpm-agent-onboard-prompt.md` — one-line pointer to the helper
  (no copy-paste of its body).
- `0.browser-prompting/1.platform-primer-ALWAYS-ATTACH.md` — 5-line MapWindow /
  unfocused-history note.

**Last updated:** 2026-08-24 (added 🎯 Plans-after-events section: palettes
categories registered in pallets-help.txt, db-hq↔events-hq event-op parity +
db-coupling rules pinned in the #.ref remaining lists) by opencode (ox-alpha)

**2026-08-24** (pinned the no-hardcoded-UIs standing rule + added Standing Rule 7 +
Document Roles row for `!.HOUSE_STDS.md`; wrote new **§K UI-authoring standards** into
`44.xyz❤️‍🔥️00.17/!.HOUSE_STDS.md`: store→generated-artifact rule, context windows OLD vs NEW
(`khtpm_entity_menu_render` = THE standard, opt-in via `<pkg>/menu.chtpm`), generic hq-renderer
mechanisms with honest caveat they currently live only in the stats-hq legacy binary
(`kptm_hq_render.c`, port to merged binary open), SHOW_PAGE chooser contract, bookmarks spec;
also verified live db-hq launches the MERGED binary via `open_db_hq.sh`) by opencode (ox-alpha)

**2026-08-23** (Linux return leg roundtrip fixed: 76 Mach-O binaries quarantined, house-wide recompile 44/44 PASS, livedesk smoke-test passed; full trail in `44.xyz❤️‍🔥️00.17/LINUX_ROUNDTRIP.md` + `yz.muchiverse/ROUNDTRIP_FIX.md`) by Kilo

**2026-08-22** (macOS leg started + critical path LIVE: taskbar and all entity
windows verified running under XQuartz on the Intel Mac via new `$.crypts/mac-start-livedesk.command`
(mirrors win-start-livedesk.ps1; bypasses /proc-dependent crypt_autostart); taskbar build scripts
patched with guarded Darwin branches, zero C edits; full trail in
`44.xyz❤️‍🔥️00.17/MAC-CONVERSION-STATUS.md`) by opencode (ox-alpha)

**2026-08-22** (added 🌐 Cross-Platform Compatibility section: the Windows
round-trip breakage + `$.crypts/win-trip.sh` fix — star-name aliasing + `chmod -R 777` on return;
and `44.xyz❤️‍🔥️00.17/MAC_COMPAT.md`, the handoff brief for the macOS leg) by opencode (ox-alpha)

**2026-08-21** (added session note for the 2026-08-21 symlink-migration Step 2
session — `persist_session_state()` wired into 19 projects' button.sh across the house, 5 harness
scenarios fixed, full trail in `44.xyz❤️‍🔥️00.17/completed-sym-list.md` + `sim-smell-fix.md`;
see the new Document Roles row for what may bite later) by opencode (ox-alpha)

**Previously:** 2026-08-17 (`legacy-shared-fix.md`: full `chtpm_parser_pal.c`/`prisc+x.c` consolidation done, 12/12 real participants on one shared baseline; `chtpm_rgb_render.c` consolidated for 9 projects; GL→X11 display-shim migration 3/16 done (mutaclysm/piececraft-xyz/my-chara-txt), 13 remain; a real mutaclysm interact-mode regression and a real db-hq/chat-hai `[X]`-close-all-entities bug both found+fixed; mutaclysm's own camera/3D work deferred to `archive/opencode-mutafix-pie.md`. `!.HOUSE_STDS.md` also updated to point its own khtpm-window-reference mentions at the now-merged shared binary instead of the archived standalone files) by claude
😀
