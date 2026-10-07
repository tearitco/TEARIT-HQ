# DSR voting, socializing and network maps (with ways to see them): design

Status: DESIGN ONLY, written 2026-10-07 by claude. Nothing here is built. Target: `dsr-test`.
Owner (2026-10-07): "we will do voting and socializing and network maps of course. If you have an idea for visualizing those events I love it."
Context: `DSR-SIMULATION-DESIGN.md`, `GAME-SESSIONS-SEATS-RATINGS-LOBBIES-PATTERN.md`.

## 1. What exists
- **A social-graph primitive** (`DAY_23_MEMORY_AND_RELATIONSHIPS.txt`, used by `chat-hai`): `state/relations.pdl`, **one row per pair, one integer score**, raised by a fixed amount whenever two personas' messages land within N ledger lines of each other; no model, grep/awk only. This is the seed of "socializing".
- **`shareholder_registry`** (WSR op) and loan/trade/payroll ledgers: the edges for ownership, debt, trade and employment already exist as ledgers.
- **Government pieces** (`gov_*`, castles) and a tax loop; `LONG-RANGE-SHAPE.md` calls territory/government the largest unbuilt domain.
- **Nothing exists for voting** that I found (word matches are generic), and no graph drawing capability is documented in the house renderer (checked only by search; verify before promising one).

## 2. Voting (events with real effect)
A vote is three append-only rows plus a derived tally: `VOTE_OPEN | id | scope | question | options | closes_day`, `BALLOT | id | voter | option | weight | ts`, `VOTE_CLOSE | id | result | ts`. **Who votes:** players cast real ballots; citizens in a population pool vote in aggregate from policy preferences (cheap, deterministic with the game seed); **shareholders vote weighted by shares** (reuse `shareholder_registry`) on store decisions. **What a vote can change:** a government's tunables (tax rate, rent cap, minimum wage), whether a war is declared (so war needs a vote or a ruler), who owns the castle (election), a company's merger. Outcomes are **events** that write real values, never a flag. Campaigning costs money (a marketing-style budget line), so influence has a cost. If you also meant **lobbying** (interest groups paying to bend a vote), it is the same ledger plus `PLEDGE | group | vote | option | amount` rows; say so and I will add it.

## 3. Socializing
Extend the pair score into **typed ties**: `TIE | a | b | kind=friend|coworker|family|rival | score`, raised by fixed amounts from events (a shared meal, working at the same store, sharing a hotel, chatting, trading) and decayed slowly. Effects stay simple and visible: hiring preference, price discount between friends, vote alignment, later crime tip-offs. Pure data, deterministic, no model.

## 4. Network maps (the data)
One derived **graph snapshot** is replayed from the ledgers by a small op into a plain file (`graph.txt`: `NODE | id | kind | civ | size`, `EDGE | a | b | kind | weight`): nodes = citizens, businesses, civs, hotels; edge kinds = employment, ownership (shares), loan, trade volume, tie (friend/rival), vote alignment, same hotel. Because it is derived by replay, it works at any game speed and for any past day (scrub the ledger, redraw).

## 5. Ways to see it (ideas, cheapest first)
1. **Adjacency heatmap** of the relationship/trade matrix: rows and columns are people or businesses, color = weight. Exact, scales to large populations where a node-link graph turns to spaghetti, and it can be drawn with the **existing generic swatch-grid path** of the renderer (the same one the palette pickers use), so it needs **no new renderer code**.
2. **Parliament view for votes:** a semicircle of dots, one per voter, colored by option and filling in as ballots arrive; a tally bar underneath; for shareholder votes the dots are sized by shares. Sprite rows, no graph drawing.
3. **Node-link network map:** nodes sized by wealth or stock value, colored by civ, edges thick by weight and filtered by kind, with recent events pulsing along edges; click a node to open its entity menu. **This needs a drawing capability with lines**: check whether the board viewer (`bv_render_2d`) can draw lines/sprites for it before designing further (do not add per-app code to `khtpm_core_render.c`).
4. **Money flow ribbons** (Sankey-style) between civs and through the FX rates, per day or per week.
5. **Timeline strip** under any view: icons for votes, loans, wars, IPOs, scandals along a scrubber; drag it to replay from the ledger (this is also the fast-forward control).
6. **Map overlay on the board:** borders tinted by owner, approval/unrest as color, trade routes as lines between buildings, and the Monopoly-token players on top.
7. **News ticker and phone messages** for the headline events (the toy already has a news ticker; WSR's news op ranks the biggest movers).
All views are **read-only projections of the ledgers**, each a toggle (the "show" switches already designed), so none of them can change the game.

## 6. Build order (each step: pal harness first; rehearse data in beta; develop in alpha)
1. Vote ledger + tally op + harness (open/ballot/close, weighted by shares, one ballot per voter, tally equals replay); one vote that changes a tax tunable.
2. `TIE` rows and the pair-score rules; harness (fixed increments, decay, symmetric).
3. `graph` snapshot op (nodes/edges from ledgers) + harness (replay equals snapshot).
4. Heatmap view via the swatch-grid path; parliament view as a sprite-row window.
5. Node-link map after confirming a line-drawing viewer; timeline scrubber.

## 7. Open questions for the owner
1. Did "lobbyies" also mean in-game lobbying (add pledges)?
2. Who may call a vote: the ruler, any player, a threshold of citizens or shareholders?
3. Is war declaration put to a vote, or only the ruler's decision?
4. Which view first: the heatmap (cheap, exact) or the node-link map (needs a viewer check)?
