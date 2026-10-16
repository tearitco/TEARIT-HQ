Image word bank, W0 (plan: BOOK 16.game/WORD-BANK-TO-IMAGE.md).
Build: gcc -O2 -o img_prompt ops/img_prompt.c. Run: ./img_prompt data/img/words.pdl data/img/parts.pdl car_door [min_weight]
bad/: words-* are used with parts.pdl, parts-unknown-word.pdl with words.pdl; each must exit 1. An unknown brixel also exits 1.
