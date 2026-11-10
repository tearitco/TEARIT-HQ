# teario (the SHARED core folder: no toy.pdl, no game.pdl, so it is not a book and adds no menu entry)

Plan: BOOK `16.game/TEARIO-MAKE-PLAY-LEVEL-EDITOR-AND-PLATFORMER-PLAN-2026-11-09.md` section 5.1. Two shells share ONE level format, kept here:

* `levels/first-steps.pdl`  a 20 x 12 fixture with a START and a GOAL (the format: the header of the file, `META START GOAL ROW` rows).
* `tiles.pdl`               the tile legend (`.` empty, `#` solid).
* `@.apps/teario-make/`     an editor-kind game: the level is a DOCUMENT of rows `name start goal r00..r11` built by EDIT rows (add / set / del) in a ledger; `export` writes `level.pdl`, the same bytes as a level file; `clear` records a verified clear; `publish` is REFUSED without one.
* `@.apps/teario-play/`     a realtime-kind game (MODE walk): reads a level file named in its `board.pdl`, hashes it into the first ledger row, an inputs-only run ledger, `verify_clear` prints the proof `clear` records. NO physics yet: the hero walks along the START row, a solid tile stops it, the GOAL column wins.

Both run on the shared engine `&.widgits/_shared-lib/ops/+x/game_core.+x` (zero per-game C).
