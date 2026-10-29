# 16 — Game

The front door for the game: desks, pages, actors, events, playtest,
and the two representations of one fact (a turn on a page, and a
numeric step). The AI that may later append a command is chapter
`17.ai`. This chapter does not own that. It owns the machine the
command runs on.

- `GAME.md` — start here. Includes the open parity gaps: the
  battle screen, loading or saving an RPG Maker project, and range
  on the placing grid (not built; tactics proof waits on Ember's HP).
- `GAME-MECHANICS-SPRINT.md` — short loops for df-hq, cdda-hq, and
  mcl-hq. Shared event verbs.
- `S1-EVENT-LEDGER.md` — the event line that is built.
- `RULES-df-hq.md`, `RULES-cdda-hq.md`, `RULES-mcl-hq.md` — verb, who, cost, result, file.
- `GAME-DEV-HQ-DESIGN.md` — Game Dev Story as a studio week: staff, one game, ship. Not built.
- `RPG-PVP-HQ-DESIGN.md` — GoldenEye-style city shooter for the pets (AI and human write the same INTENT rows): Doom vs cdda art split, what to take from `007-goldeye`, steps P0-P9 incl. later Fortnite-style building. Draft, not built.
- `EVENT-VERB-PAGES-DESIGN.md` — **closing the events / blueprintz gap** (2026-10-20): game verbs get event pages that the UI really plays (`fire`/`run`, fallback to the raw verb), generated from one `verbs.pdl` manifest; phases V0-V7. **Built and proven for rpg-pvp-hq (V1-V5, 2026-10-20, section 8)**: verbs.pdl, verbgen check/gen, 13 verb pages, `fire` in the engine, window actions routed through the pages; game-dev-hq (V6) built the same day (section 9: 17 verb pages, genre bug fixed, the clock daemon now stops with the window); V7 built (section 10): `bp_edit_op` plus the `EVENTS`/`BP` playthrough rows program a verb page by blueprint, headless; phase tables for both games PASS; the events-hq manager's 48-command registry cap (late commands compiled to pages that did nothing) is fixed.
- `RPG-PVP-HQ-EVENTS.md` — which games use event pages vs ledger rows, how rpg-pvp-hq fires events (RAN), the registry/bp_scan rule so blueprintz can read and later edit its scripting. grok's game-dev-hq has no event pages.
- `../02-architecture/PLAYTEST-DESK-AND-PCHQ.md` — taskbar cell 9 and
  the PC-HQ Player menu. The playtest button.
- `../18.pc-hq/00-INDEX.md` — the board itself: the diamond, the
  daemon, Book/Page, and Synch. Playtest stays in this chapter.
- `../08-roadmap/design-docs/DUSTOPIA-HACK.md` — chemistry, astronomy,
  mechanics, and the weight that picks which representation advances.
  The 2026-09-23 addendum is the current ladder.
- `../08-roadmap/design-docs/TILESETS-EVENTS-AND-GAME-CLONES.md` — why
  clones are event pages, not a second engine.
- `../12.calendar/2026-09-23/notes.md` — the day the receipt page and
  the play flag were proven.
- `../12.calendar/2026-09-23/grok-to-claude.txt` — the stop-point
  for the next agent. Same text as the external handoff. Merge
  branch `grok`. Do not commit onto it.
- `HANDOFF-2026-10-21-0512-CLAUDE.md` — 05:12 PDT handoff, added 05:19. Edit board still needs RPG Maker buildings, trees, doors, and the eight walking sprites. Later the same window edits RPG events the way palettes and the events editor do.
- `HANDOFF-2026-10-21-CASINO-AND-RPG-EDIT.md` — where Casino Island actually is (`bet-rpg`, not the TON board, not `rpg-bet`), and the first-pass plan for `rpg-edit-hq` on book TON page centroid-u. Read before building that app.
- `PET-HQ-MUST-HAVES.md` — one template for the pet boards: one footer of pets, acts on the left, doors, clock, and INT `0` / `1`–`4` that actually move the board camera. Survey of which daemons only store `flag_pov`.
- `HANDOFF-2026-10-28-PIECES-DB-CHAIN.md` - **start here for pieces / db / chain (2026-10-28):** where each piece stands with commits, owner decisions, the next slices with proof requirements, rules and traps, open questions.
- `PIECE-IDENTITY-AND-MASTER-PAL-LIST-DESIGN-2026-10-28.md` - **design (2026-10-28, nothing built):** a piece (rare, valuable, one uid) vs an actor (db template) vs an appearance (a book); the user's Master Pal List ("Bill's PC"); minting as an op + event (`piece_mint`); copy / fork / share / buy; the checker `piece_check`; migration of today's 30 game-local ids.
- `CHAIN-MASTER-BANK-FED-AND-CONTRACTS-DESIGN-2026-10-28.md` - **design (2026-10-28, nothing built):** the chain as master bank / "Fed": tributes and mining for pieces, master tax and master tx fee on every purchase, bonds, owner contracts (tax, dividend, royalty, annuity), `policy.pdl` knobs, what "uncheatable" really needs.
- `PER-BOOK-DB-2026-10-28.md` - the db as a layer over a shared trunk, the db window following the open book, the cast and identity consequences.
- `GAMES-DECISIONS-2026-10-13.md` — owner decisions for cdda-hq, df-hq,
  gta-hq: z-levels, vehicles as part-cell grids, scale, physics in the
  concept bank, learning with review. Read before touching those games.
- `GAMES-ROADMAP.md` — milestones with proof/KPI, how to use/modify
  (filled as each lands), and the tangent documents to write. All
  rows start NOT BUILT.
- `IMG-HAI-DESIGN.md`, `WORD-BANK-TO-IMAGE.md` - the text-to-image studio window. It is an h-ai menu row now, not a toy.
- `CHAR-CREATE-DESIGN.md` — a later h-ai window that writes `party.pdl` rows from arguments, a free sprite cell, or an img-hai draft. Not built.
- `BET-RPG-DESIGN.md` — Casino Island. Three hotels, three casinos, People4 cast. The room view is the island. An open hand switches the same window to that game's board (poker, blackjack, roulette, slots, craps), and the footer navs become bet, hit, and the other acts. The walk window is open. The table picture is not built.
  (img-hai) and the word-bank pipeline behind it. **The full handbook (user
  guide, code guide, extension recipes, testing, operations, handoff) lives
  with the app: `44.xyz.01.00/&.hq-apps/img-hai/docs/` (start at README.md);
  a one-page copy is `XO/25.games-plan/10-IMG-HAI-DOCS.html`.**
- `IMG-HAI-BREAKTHROUGH.md` - class-style lesson on the word-to-judged-pixel-art loop: what is proven or not, ten lessons, exercises, next steps.
- `PETSLIKE-NEW-ENTRIES.md` - the "+ new page / book / room" rows of every pets-like board (two families, what each verb does, evidence) and the research + plan for "+ new pet" under a "pals" tab.
- `PETSLIKE-FRAMEWORK-PLAN.md` - are the pets-like boards one framework yet (no): what exists, measured duplication of the eight copies, the best-of table, proposed core + rules + data split, the diff gate.
- `RPG-DUST-DESIGN.md` - **paper design, nothing built** (2026-10-23): a pets-like board that simulates a planet from a seed, solar system down to molecules; the scale ladder as data rows, the world as a function of seed + an append-only overrides ledger, what is done by hand (laws), by weights (choices), by events (actions), by ops; how the current boards and the marathon refactor are reused; nine build steps each with a test that can fail; what is not known. Companion: NIGHT 52 (audio class) and the XO email summary.
