# House pattern: game sessions, seats, ratings and lobbies

Status: PATTERN DOC (design), written 2026-10-07 by claude from the owner's DSR answers. The owner: "local, local agents, p2p, standardized Elo rankings (like chess) or free-for-alls, lobbies. This will be common in this house with different games, so let's document that pattern."
Every game in the house (TSC_ELO, DSR, chess/wager, TSOTS, future ones) should use this one shape instead of inventing its own. Nothing new is built by this document except where it says "exists".

## 1. What already exists (read, not assumed)
- **`@.apps/TSC_ELO`** is the reference game: match modes **HvH** (hot-seat), **HvC**, **CvC** (bots duel); "the opponent can be a human or a computer AI"; **the opponent's Elo rating IS the AI's difficulty** (no separate easy/hard); new players start at **1000**; the **`tsc_elo` op** loads and updates ratings in the player's xyzfs **ratings file** (K=32 in the design); match setup goes through the widget command bus (`MATCH:HvC`, `RATING:<n>`, `START`); an append-only **master ledger** records every action. Its design file: `TSC_DESIGN.md` section 6.5 (the Elo algorithm).
- **TSC_ELO P2P PvP** (`TSC_P2P_PVP.md`, `prog-report-pvp.md`): reuses the house `palnet_peer` op verbatim (symmetric peers, no client/server split, flat presence dir `net/presence/<node>.txt`, wire line `MSG|<seq>|<room>|<user>|<ts>|<text>` with room = game id, user = player id, text = action, new peers get the full backlog replayed, one-writer append-only outbox/inbox). The harness proves the wire and ledgers between two real subharnesses (P1-P5, OVERALL PASS). **Not yet proven there:** a duel played to a winner and the post-match Elo write-back.
- **Wager chess** (NIGHT 2): real Elo and an escrowed stake; the stake needs an escrow transaction the chain does not have (NIGHT 6, `AUCTION-SCREEN-DESIGN.md` gate).
- **Same-machine only today:** `palnet_peer` hardcodes 127.0.0.1 (`CROSS-MACHINE-NETWORKING-PLAN.md`, not started).
- **Agents as actors:** robots/ghosts talk through phones and the server (`HAI-ROBOTS-PHONES-SERVER-DESIGN.md`); AI decision modes (preset / weighted / RL / LLM) exist in WSR; models never decide, only pick inside a fixed shape.
- **Per-game setup:** `game.pdl` and its proposed `SETUP` rows (`GAME-SETUP-PDL-DESIGN.md`, `DSR-SIMULATION-DESIGN.md` section 8).

## 2. The pattern
**Session.** One game instance = one **session**: a `game.pdl` (title, `SETUP` rows, seats, mode), one append-only **session ledger** (every action, written by one writer per file), and a **result row** at the end. Reproducible: the setup and a seed are the first ledger rows.

**Seats.** A session has N **seats** (2 to 4 for DSR; any count a game declares). Each seat is one of:
| seat kind | who plays | how it reaches the game |
|---|---|---|
| `human` (local) | a person at this desk (hot-seat) | the normal UI / event commands |
| `agent` (local agent) | an AI entity on this machine | the same event commands a human uses, delivered through its phone/the server; **no special privileges**; its strength is its rating |
| `peer` (p2p) | a person or agent on another node | `palnet_peer` wire (room = session id), actions as `MSG` lines |
| `spectator` | watches only | reads the ledger; fast-forward and view toggles are local |

**Modes.** `ranked` (Elo, 2 seats or pairwise), `ffa` (free-for-all, any seat count, results ranked), `casual` (no rating change), plus game-specific team modes later. Mode is a `SETUP` row, not code.

**Ratings (standard Elo, per game kind).** A player (human or agent) has **one rating per game kind** (`tsc`, `chess`, `dsr`, ...), starting at **1000**. It lives in the player's **ratings file** (private numbers, user data; AGENTS.md data rules) and is **derived from the append-only match-result ledger** (so it can always be recomputed, never hand-edited; the same derived-mirror rule as the Concept Bank). Update at session end by **one shared op** (the generalized `tsc_elo`), K=32 as designed. **FFA/multi-seat rule (proposal):** treat the ranked result as pairwise Elo comparisons between every pair of seats and average the changes. AI agents have ratings too; an agent's **strength is parameterized by its rating** (TSC_ELO's rule), so bots can climb and CvC is meaningful.

**Lobbies.** A lobby is a **list of open sessions** that players browse, create, join and leave: append-only rows `SESSION | id | game | host | mode | seats_total | seats_open | rating_range | ts`, then `JOIN | id | player | seat | ts`, `START | id | ts`, `CLOSE | id | ts`: **the same ledger shape as the auction screen**, so one lobby screen pattern serves every game (a screen in the network cell, X11-HQ, bounded choices). Local lobbies read the local ledger; p2p lobbies are discovered through the presence dir and replayed by `palnet_peer`. **Matchmaking** = a rating-range filter on the list (and "match me against an agent rated ~N" for single player).

**Transport tiers** (pick the lowest that works): **local** (one process/desk), **local agents** (this machine, via phones/server), **p2p** (`palnet_peer`). Cross-machine is the same code once the networking plan is done.

**Trust gate (same as the auction/chain gate).** Ratings are only trustworthy if results and identities are. **Local and local-agent** ratings are fine now. **Remote-peer** results are **advisory** until signed transactions and a provable peer identity exist (otherwise anyone can forge a win). Wagers on top need the chain escrow transaction (not built).

**Headless-capable.** The simulation never waits on drawing: fast-forward, spectating and "show/hide" layers are viewer settings; a session must run with no window at all (needed for CvC, harnesses, and DSR at high speed).

## 3. How each game maps
| game | seats | modes | status |
|---|---|---|---|
| TSC_ELO | 2 | ranked, casual (HvH/HvC/CvC) | built; P2P wire proven; winner + Elo write-back not yet |
| DSR (dsr-test) | 2-4 civs | ffa, casual, ranked (2) | designed (`DSR-SIMULATION-DESIGN.md`); seats are the `civ.x.seat` rows |
| wager chess | 2 | ranked + stake | concept (NIGHT 2); stake needs chain escrow |
| TSOTS / others | per game | per game | to adopt the pattern |

## 4. Build order (each step: pal harness first; rehearse data steps in beta; develop in alpha)
1. **Rating math op** (shared, generalized from `tsc_elo`): game kind parameter, ratings file schema, append-only result ledger, recompute-from-ledger; harness with the standard Elo example vectors, K=32, new-player 1000, symmetric updates, FFA pairwise averaging, "ledger replay equals stored rating".
2. **Session ledger + seat rows** in `game.pdl` (`SETUP | seat.N | human|agent|peer`, `SETUP | mode | ranked|ffa|casual`); parser + harness cases.
3. **Lobby ledger + screen** (copy the auction ledger shape; X11-HQ screen in the network cell).
4. **Agent seats** (rating-parameterized strength; same command interface as humans).
5. **p2p seats** over `palnet_peer`; finish TSC_ELO's missing proofs (duel to a winner, Elo write-back) as the first consumer.
6. Trust gate for remote ratings after signing exists.

## 5. Open questions for the owner
1. FFA rating rule: pairwise Elo averaged (proposed), or placement points?
2. One rating per game kind (proposed), or also per mode (ranked-only)?
3. Do agents share a rating pool with humans or have their own?
4. **"Lobbies"** here means matchmaking rooms. If you also meant **in-game lobbying** (interest groups influencing the castle/government in DSR), that is a separate game mechanic; say so and I will design it.
5. Casual sessions: do they still write a result row (for history) without changing ratings?
