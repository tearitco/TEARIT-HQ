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
- `GAMES-DECISIONS-2026-10-13.md` — owner decisions for cdda-hq, df-hq,
  gta-hq: z-levels, vehicles as part-cell grids, scale, physics in the
  concept bank, learning with review. Read before touching those games.
- `GAMES-ROADMAP.md` — milestones with proof/KPI, how to use/modify
  (filled as each lands), and the tangent documents to write. All
  rows start NOT BUILT.
- `IMG-HAI-DESIGN.md`, `WORD-BANK-TO-IMAGE.md` - the text-to-image studio window
  (img-hai) and the word-bank pipeline behind it. **The full handbook (user
  guide, code guide, extension recipes, testing, operations, handoff) lives
  with the app: `44.xyz.01.00/&.hq-apps/img-hai/docs/` (start at README.md);
  a one-page copy is `XO/25.games-plan/10-IMG-HAI-DOCS.html`.**
- `IMG-HAI-BREAKTHROUGH.md` - class-style lesson on the word-to-judged-pixel-art loop: what is proven or not, ten lessons, exercises, next steps.
