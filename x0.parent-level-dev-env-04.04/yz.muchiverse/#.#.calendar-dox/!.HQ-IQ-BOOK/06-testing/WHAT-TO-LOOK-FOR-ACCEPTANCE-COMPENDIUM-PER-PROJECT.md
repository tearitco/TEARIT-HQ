# What to look for: the acceptance compendium (one entry per project)

Owner, 2026-11-08: *"how come u didn't catch the issue of no visible players or changing sidebar when u tested? dumped png? we need to explain in onboarding or test/drive harness docs exactly what to look for, or have a compendium for each project, for what it should be testing for."*
A passing case is a claim about what the case checks, nothing more. This chapter lists, per project, **what a human would see if it worked**, and which check proves each item. Add an entry when you add a project; add a row when a bug slipped past.

## 1. Why the Teropoly bugs got past me (2026-11-08, so the next agent does not repeat it)

| What was missed | What my tests checked instead | Why that could not see it |
|---|---|---|
| **No 3D players** (the engine silently dropped every player row) | `events.txt` had rows | A row that exists is not a figure that is drawn. The loader reads 9 or 15 fields and dropped my 10-field rows (digit glyph). Found only when the owner asked "where are the 3D players?". |
| **No side menus, no players bar** (Player tab said `playing`, no driver running after a session reset) | ledger rows, feed text, element ids, **in a run where the driver was up** | The failing live runs happened; I called them "flaky" and added retries instead of reading the state (`ps`, `game_ui.txt`). The label `playing` came from a persisted flag, not from a live writer. |
| **Sky-coloured seams on the floor** | nothing | No check ever looked at the picture. Screenshots were saved to `shots/` and **never opened by me nor asserted on**. |
| **A fixed CPU number** | one 20 s window | Measured while In: ON had paused the clock; the real running cost was 8% mean and 30% bursts. |
| **Blind toggles** | each script step "passed" | `press tb-player` CLOSES an open menu; `key esc`/`tb-in` assume which way In: is set; the run's result depended on the state the last run left. |

**The rules that follow (each is enforced by a step or a case now, or is a stated review step):**
1. **Data is not display.** For every thing a user must SEE, assert it from the window's own text dump (`relay_drive` step `see <id|label:text> [absent]`, polls up to 5 s) AND, for graphics, **open the PNG and look** (use the Read tool on the file) and write what you saw in the commit. A saved screenshot nobody opens is not evidence.
2. **A failing live run is never "flaky" until the cause is shown.** Read `ps`, the feed, the flag files, the objects dump. If you add a retry, say which fact you found that justifies it.
3. **A label or flag shown to the user is a claim; find its live writer** (CPU-OPTIMIZATION-PITFALLS 20 and 27). `Player: playing` needs a running driver and a feed with `gm_on=1`.
4. **State-aware steps only.** Use `in off|on`, `ensure <target> <opener>` instead of blind presses. A case must pass from ANY starting state (menu open, In on, game running, driver down).
5. **Every "nothing happened" check has a vacuity check** (the thing could have happened) and every new guard is run once against the old binary or a wrong expectation.
6. **A CPU number carries the producer's state next to it** (`running=1`?) and is measured both paused and running, in several windows (CPU-OPTIMIZATION-PITFALLS 28).
7. **After any session reset, relaunch or rebuild, check the whole screen once by eye** (one PNG of each state below) before saying "done".

## 2. Entry: Teropoly (`@.apps/deskopoly`, shown to the user as Teropoly, played inside rpg-edit-hq)

Screens and states, with **what you must see** and **how it is checked**. `see X` = relay_drive step, `png` = open the saved picture, `ledger`/`feed` = file check.

| State | You must see | Check |
|---|---|---|
| Editor opened on the book, Play pressed, Setup screen | popup titled `TEROPOLY classic`; text `Choose the players, then START`; buttons players -/+, you play, seed, speed, START GAME; palettes bar HIDDEN, right chat titled `TEROPOLY table talk` (not h-ai) | `see label:TEROPOLY`, `see label:Choose the players`, feed `gm_on=1`, `side_pal=` empty |
| Game started (pets only) | **left players bar with one row per player** (`pt0..ptN`, cash, `>` marks whose turn), **footer with net worth** (`gme0`), table talk filling with rows, **3D: one extruded RPG Maker figure per player standing on the board** (figure count = players), moving along the tiles as rolls land; **2D (key `0`): the board with the same figures** | `see pt0..ptN`, `see gme0`, ledger rows growing, `events.txt` has N rows of 9 or 15 fields ending in a LETTER (case `dsk_driver`), **png of 3D and 2D opened and the figures counted** |
| Human seat, my turn | popup `your turn, A: use the buttons` with `Roll` (then `Buy <tile> (<price>)`, `Skip`, `End turn`); the game does NOT advance on its own (0 engine calls while a human thinks) | `see label:Roll`, ledger frozen, case `live_dsk_seat` |
| Stopped (Player > Stop) | `Player: stopped`; ledger frozen; walk finishes; sidebars STILL present | switch `running=0`, `see pt0` |
| Driver missing (session reset, crash) | within ~10 s the core starts it again: feed back, bars back | case `game_driver_watchdog`; live: `ps` shows `deskopoly_driver`, `see pt0` |
| Setup screen BEFORE START | the authored board desk: no players and an empty 2D view is EXPECTED (the world switches to the generated `play` desk at START) | known behaviour, not a bug |
| Human seat, `Trade...` pressed | four panes across the top: `p1 A $<cash> holds` with p1's tradable tiles (or `(nothing to trade)`), `you GIVE` (cash rows, then moved tiles `<name> (press: back)`), `you GET`, `p2 B $<cash> holds`; under them the popup with `Send offer`, `Partner: pN`, `Back to game` ALL on the canvas; after Send the popup says `offer sent to pN`, the pet answers, the left bar cash changes by exactly the deal (found 2026-11-08: the Partner and Back buttons first ran off the canvas) | `see label:you GIVE`, `see label:Orange3 (press: back)`, `tests/dsk-seat-trade.pdl`, ledger `offer` then `accept` rows, **png 11, 12, 13 opened** |
| Human seat, an offer is open (own or addressed to me; stopped game so no pet answers) | **own offer**: popup line `YOUR OFFER #<id> to pN: you give <..>, want <..>` with `Amend offer...` and `Cancel offer` (and no Accept/Counter). `Amend offer...` opens the SAME four panes prefilled with MY terms, popup text `AMEND your offer #<id> to pN`, button `Send amendment`, the partner is plain text (no Partner button). **offer to me**: `pN OFFERS: gives <..>, wants <..>` with `Accept offer`, `Reject offer`, `Counter...` (no `Trade...` when it is not my turn); `Counter...` opens the panes with the SWAPPED terms (what they ask in `you GIVE`, what they give in `you GET`), text `COUNTER to pN's offer #<id>: your terms replace theirs`, button `Send counter`; it stays open on the pet's turn. After Send the original offer is gone and the counter is MY open offer (new id, round 2) | `see label:YOUR OFFER`, `see label:AMEND your offer`, `see label:COUNTER to p2`, `tests/dsk-stop.pdl` + `dsk-seat-amend.pdl` + `dsk-seat-pass-turn.pdl` + `dsk-seat-counter.pdl`, ledger `amend` / `cancel` / `counter` rows, case `dsk_tradeui` (183), **png 20-24, 30-33 opened** |
| Human seat, an auction is open (after Skip on a tile for sale) | popup text `AUCTION <tile> <standing bid> by pN`, buttons `Bid <min>`, `Bid <min+50>` (only when the cash allows), `Pass`; NO Roll/Buy/End turn; pets bid on their own; at the end table talk shows `auction_end sold-to=pN-amount`, the winner's cash in the bar drops by exactly the amount, `Bid`/`Pass` buttons are gone and `End turn` is back (found 2026-11-08: a three-line text made the first button cover its end, so the line is one short line) | `see label:AUCTION Power 20 by p3`, `see label:Bid 30`, `see label:Bid 60 absent`, `tests/dsk-seat-auction.pdl`, **png 07, 08, 09 opened** |
| Low camera in 3D | wooden floor is solid: NO one-pixel sky-coloured (135,180,220) lines between the floor rows (fixed 2026-11-08: per-cell depth removed from the page, shader entry-face fallback). Look at the PNG AND count: `ffmpeg -i shot.png -vf crop=W:H:X:Y -f rawvideo -pix_fmt rgb24 f.rgb` then `img_sky_seams.+x f.rgb W H` on a FLOOR-ONLY box (a box with HUD text or the horizon counts those too) | `png`, `img_sky_seams` 0, offline case `bv_gpu_seam` |

Cost line to quote with any change (measured with the producer running): clock daemon mean 2%, max 9% per 10 s window over 180 s since the 12 s global spacing (2026-11-08; before it 119% in a guard window), 0% paused; driver 4 wakeups/s stopped, 14 live; see the handoff section 4.
Harnesses: `dsk_auction` (57), `live_dsk_seat` (23), `dsk_driver` (70), `dsk_page` (42), `game_driver_watchdog` (10), `lc_clock_burn` (7); nav map `@.apps/deskopoly/nav.pdl`; scripts `tests/dsk-open-play.pdl`, `dsk-seat-setup.pdl`, `dsk-seat-turns.pdl`, `dsk-in-on.pdl`.

## 3. Entry template (copy for the next project)

```
## Entry: <project> (<folder>, shown to the user as <name>)
| State | You must see | Check |
(one row per user-visible state: what a human sees, and the step/case/file that proves it)
Known invisible failure modes: <what failed silently before, with date and the guard>
Cost line: <idle / active numbers with the producer state next to each>
Harnesses and scripts: <names and counts>
```

## 4. Entries still to write (from what the memory and skills already record; fill them in when you next touch the project)

- **rpg-space board** (skill `rpg-space-board-driving`): prove control before any claim (session folder exists, `bv_session` non-empty, one real key changes `trainer.txt`, a window dump differs from the previous: a stale engine session gives identical dumps for minutes). Known invisible failures: engine daemon dead with the window still showing its last frame; hidden window (`IsUnMapped`) makes every key fail.
- **petslike boards** (rpg-dust, rpg-pet, ...): the engine reloads a desk when `cells.marker` SIZE changes; a changed atlas or extrusion file shows nothing until it does (2026-11-08 I forced no reload and drew three false conclusions from three identical frames; compare against a positive control that MUST change the picture before trusting a null result).
- **editor Harness tab / taskbar harness cell**: see D51/D52 in the decision log.
