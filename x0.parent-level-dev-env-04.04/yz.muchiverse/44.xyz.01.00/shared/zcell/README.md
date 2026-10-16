z-cell format v1 (roadmap M1). Spec: BOOK `16.game/Z-CELL-FORMAT.md`.
Build: `gcc -O2 -o zcell_check zcell_check.c`. Run: `./zcell_check zcell-fixture.pdl ../brixels.pdl` (expect VALID, exit 0). Each file in `bad/` must exit 1.
