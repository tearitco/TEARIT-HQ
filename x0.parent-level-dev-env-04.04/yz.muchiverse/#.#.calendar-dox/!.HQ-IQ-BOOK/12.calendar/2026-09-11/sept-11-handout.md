# 🗓️ Sept 11 Handout — forward-looking, 5 subjects + synergy plan

**Read this cold-open in a few weeks/months and it should still make
sense** — dates and quota windows below are absolute, not "today"/
"tomorrow." Percentages are honest estimates from a real (but
time-boxed) codebase check, not a rigorous audit — flagged where
uncertain.

---

## 1. 🎮 Game dev in pc-hq

**Current real status, checked in-codebase:**

- **Events + DB wiring: ~30-40%.** `db-hq-pal`'s own Common Events tab
  already reuses the real `events-hq` editor (`4a744382`) — the DATA
  layer (events-hq ↔ db-hq) is real and shared. What's **not**
  confirmed wired: pc-hq's actual game board (piececraft-hq, running
  the *separate* `chtpm_parser_pal`/`prisc+x` engine, NOT the khtpm
  family events-hq/db-hq live in) triggering a Common Event on an
  in-game action, or reading Database items as real game state. The
  only real bridge found between pc-hq and the khtpm family is the
  **Interact Mode relay** (`kh_scan_interact_relay()` — forwards raw
  keys from a khtpm window into the game's own input file) — that's
  input routing, not event/database integration. 🚧 **Open, real next
  step**: wire a Common Event to fire from an in-board trigger (an NPC
  touch, an item pickup) — the data layer exists, the trigger layer
  doesn't yet.
- **Drag and drop (into pc-hq, out, onto desk): ~0%, not found.**
  Grepped the whole `piececraft-hq`/`board-viewer` tree for drag/drop
  handling — nothing real. The desktop's own tile-drag system
  (`livedesk` entity tiles, `TILE-SYSTEM-DESIGN.md`) is a *separate*
  mechanism (desktop icons, not board content) — dragging a piece
  *into* the board or an item *out* onto the desk isn't built.
- **Save/load: unclear, likely partial.** One passing reference in
  `bv_render_2d.c`, nothing that reads like a full save-game/load-game
  flow in the time I had to check. 🚧 Worth a dedicated, real audit
  before promising this to anyone — don't assume it works from this
  handout alone.

**Bottom line**: the *editor* layer (events-hq/db-hq, shared, real,
already proven) is well ahead of the *runtime* layer (the actual game
board consuming that data, drag/drop, save/load). Closing that gap is
the real Sept work, not building more editor.

## 2. 🤖 open-hai capability roadmap

**Current real status**: open-hai's manager exposes exactly **3
tools** — `list_dir`, `read_file`, `write_file`. No shell/execute tool
at all. That's the literal ceiling on "basic coding tasks" today: it
can read and write files, it cannot run anything (no test, no build,
no lint) — real "very basic" per your own framing, confirmed in code,
not guessed.

**Harnecient hacks to close the gap toward "50-100% as capable as
Haiku":**
- A real **execute/shell relay** (Day 12's own "CODE_TOOLS_HARNESS" —
  the pattern already exists as a *lesson*, just not wired into
  open-hai's own tool list yet). This is the single highest-leverage
  gap: read+write without run is an editor, not a coding agent.
  🎯 Biggest lever, smallest lift.
- A real **diff/patch tool** (edit-in-place instead of whole-file
  `write_file` round-trips) — cheaper context, safer edits, matches
  how every other serious coding agent works.
- A **search/grep relay** — right now it can only `list_dir` one level
  at a time; no way to find "where is X defined" without brute-force
  reading many files.
- For **this codebase specifically**: none of the above get you
  "understands khtpm/.chtpm/pal semantics" for free — that's a real
  prompt/context problem (the house's own `CENTROID_GOLD_STD.md`/
  pitfalls docs ARE the fix, but open-hai needs a `read_file`-driven
  habit of actually consulting them, which is a prompting/harness
  discipline problem, not a missing tool).
- For **chapter books / franchise books**: `read_file`+`write_file`
  is already structurally enough for a "read chapter, propose edits,
  write back" loop — the real gap there is **chunking** (a novel-length
  file doesn't fit one context window) — needs a real chunked-read/
  chunked-diff harness, not a new tool class.
- For **events (not C, not pal)**: events-hq's own `.ir.pdl` format is
  plain-text, line-oriented — genuinely one of the *easiest* targets
  for a harnecient-style read/write loop, probably before general C
  editing is reliable. 🎯 Good near-term target.
- For **driving house navs**: the relay mechanism (`entity_menu_
  history/<pid>.txt`) IS already a real, text-file-based interface —
  open-hai *could* drive it today with just `write_file` pointed at
  the right relay path + `read_file` on the resulting state file. No
  new capability needed, just a prompting/harness pattern that teaches
  it the convention. 🎯 Also a good near-term target, arguably even
  easier than #events.
- For **"network p2p" apps on the Windows computer**: this is the
  biggest genuine unknown of the five. Nothing in this codebase talks
  to a separate Windows machine today. Real options, roughly cheapest
  to most involved: (a) a **Windows-side shim** (a small script/service
  on the Windows box that watches a shared file/network location the
  same way the relay files work here, translating into whatever the
  p2p app's own control surface is — its CLI, its config files, or its
  own network RPC if it has one); (b) an **install script** that sets
  up that shim + whatever cross-machine transport (shared folder over
  the LAN, or a simple socket) so open-hai's `read_file`/`write_file`
  reach across machines the same way they reach across the house
  today. Neither is built. This needs its own real design pass, not a
  guess baked into a handout — flag it as a dedicated future session's
  topic.

## 3. 🕹️ Chat/forum apps → multiplayer event-driven games

IRC-chat-hq and chain-hq (forum-style apps) already publish real,
structured, per-message state through their own managers — that's
*functionally* an event stream already (a new message = an event with
a sender, a room/thread, a payload). The real reuse path: instead of
(or alongside) rendering that stream as chat UI, feed the *same*
message-event shape into events-hq's own Common Event trigger format —
"a chat message matching pattern X in room Y fires Common Event Z."
That gets you a real event-driven multiplayer substrate almost for
free: players "play" by chatting/posting, the house's own already-built
event engine resolves what happens. 🎯 This is probably the **cheapest
real path to a working multiplayer game loop** of everything in this
handout — no new engine, just a new *consumer* of data these apps
already produce. Not started; a real design doc (mapping chat-hq's
message schema → events-hq's trigger schema) is the concrete next
step, not code.

## 4. 📦 Install / onboarding / wallet

Not independently re-audited for this handout (out of the time budget
for this pass) — flag as a real gap in THIS document rather than
guess. The HARNECIENT.SMOL "Day" series (26 lessons, Day 1 Welcome
through Day 26 Local Fallback) is the real onboarding backbone that
exists today; cursword's own tutorial-onboarding status, and anything
wallet/mining/trading-specific, needs its own dedicated check before
the next handout claims a number here. 🚧 **Action item**: a real,
focused audit pass on install/cursword-onboarding/wallet state,
written up on its own, before this section gets real percentages.

## 5. 🖼️ Image distillation / text-to-image for tilesets, levels

Not found anywhere in this codebase as of this check — no local
model-inference pipeline, no external API wiring for image generation.
**Can harnecient hack get there?** Only for the *plumbing*
(read/write relay files, a manager that shells out to whatever
generation backend exists) — the actual generation capability itself
has to come from somewhere real: either (a) a local model (needs real
GPU/weights the "weak box" this house runs on may not support well —
see the CPU-safety notes in `03-pitfalls/00-INDEX.md`), or (b) an
external API (needs network access + an account/key + a cost model,
none of which exist here yet). **What's actually needed before this is
buildable**: a decision on local-vs-API, a real budget/quota answer if
API, and — since tilesets/sprites in this house already follow a real,
established atlas/`sprite.csv` convention (the `emoji_gen_atlas`/
`tp_asset_to_sprite` pipeline referenced in the strip's own clock-face
build) — a conversion step from raw generated images into THAT format,
not a new asset pipeline from scratch. 🎯 Realistically the
**furthest-out** of the five subjects — don't schedule real work here
until 1-4 have real runtime, not just design docs.

---

## 🧩 Synergizing this effort — which tool for which task

- **Kilo** (agentic, very small context, hard-stops if it overflows):
  best for **narrow, single-file, well-scoped** tasks with a clear
  finish line — a single shell-relay tool wired into open-hai (§2), a
  single Common Event trigger wired to one pc-hq action (§1). Bad fit
  for anything touching many files or requiring you to hold this
  whole handout's context (chat-hq→events-hq schema mapping, the
  Windows p2p shim design) — those will blow its context budget before
  finishing.
- **Browser prompting** (Day 25's own documented pattern): best for
  **research-shaped** tasks — the Windows p2p shim design question
  (§2), the image-gen local-vs-API decision (§5), anything where the
  answer is "go read docs/compare options and report back," not "edit
  this file." Weak fit for anything needing this house's own file-
  relay conventions, which a browser session won't have loaded.
- **By hand / better not delegated at all**: the install/cursword/
  wallet audit (§4) — it needs real judgment about what's worth
  reporting as "done" vs "flagged," exactly the kind of thing that
  gets rubber-stamped wrong by an agent trying to look productive.
  Do that one yourself, or watch it closely.
- **Wait until Sept 13 quota refill**: anything genuinely large and
  multi-session — the pc-hq events/db runtime wiring (§1, real
  trigger-layer work, not a quick patch), and the chat→events-hq
  schema design (§3) if you want it done carefully rather than
  rushed. Both are big enough that starting them on a tight quota
  risks a half-finished state that's worse than not starting.
- **Short-term vs long-term re-use of this list**: if any of §1/§2/§3
  take longer than expected (very possible — none of these are small),
  re-open THIS handout rather than re-deriving the plan from scratch;
  the tool-assignment reasoning above doesn't expire, only the
  underlying code-state percentages do.

---

## 🌌 The games (for `1-1.HARNECIENT.SMOL`, add as new "Night" entries)

Per direct instruction: these belong in the same
`1-1.HARNECIENT.SMOL` series as the existing Day-numbered lessons, but
as a parallel **"NIGHT"** track (new class of entries, not replacing
the Days) — not yet written there, listed here first so the shape is
agreed before it's committed to that series.

1. 🧱 **Minecraft clone** → evolves into 🏛️ **a 2D/3D Civilization
   clone** → evolves into 📈 a **Capitalism-Lab-style stock market
   sim**, trending toward 🌠 **"GTA in space."** The whole arc is
   meant to be **emergent, not scripted**: agents build it, the game's
   own "manager" (an in-fiction research system) discovers physics/tech
   as players research it, and the player picks their own abstraction
   level as they climb the tech tree via LLM-mining. The deeper/richer
   the simulation a player unlocks, the more their in-game
   blockchain-NFT assets (gold, chickens, whatever a given world
   mints) are worth on the cross-chain p2p exchange — where **a
   "chain" = one lobby, solo or multiplayer**, each its own economy,
   trading against each other at a real exchange rate.
2. ♟️ **A simplified wager-chess**: pre-ordained army pieces battling,
   scored by a real Elo rating, each match wagering a set token
   amount — winner takes the pot. Simple rules on purpose — this one's
   scope is depth-of-wagering-loop, not depth-of-chess.
3. 🔫 **FPS shooters, several types** — released as **"NIGHTS"**
   (paired with the existing "Day" lesson numbering, a new night-time
   class alongside it) instead of more Days.
