# Chess, events, Elo, peer line (2026-10-09)

The full note is also at `/home/no/Desktop/github/work/XO/18.CHESS/CHESS.md`. Doom corrections are `/home/no/Desktop/github/work/XO/17.DOOM/DOOM-LEARNINGS.md`.

Book: `@.apps/piececraft-hq/pieces/system/maps/chess/`. Pages: `title`, `standard`, `king_pawn`, `endgame`. Rules: `ops/chess_rules.py`. Verbs: `ops/chess_event.sh`. A hotbar click in `pc_entity_ctx.sh` runs the desk row's `cmds=`.

Each cell is an event. `select` writes `move_range_matrix.txt` (`#` legal). `land` moves only onto `#`. The computer plays black with that same move list when `mode=computer`. Two humans leave `mode=player`.

Elo starts at 1200, K=32, expected score `1/(1+10^((opp-me)/400))`. Stored in `elo.pdl`.

No socket. Vs Player and a finished game append `DATA|local|...` to `maps/chess/net/outbox.txt` for `palnet_peer` to carry later.

The end of a game is capturing the king, or the computer having no move. No checkmate, castling, en passant, or promotion. A pawn self-test passed and the standard page was restored to the opening. The board was not started.
