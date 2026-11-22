# Draw Battle: walk, hit, death (2026-11-21)

RAN on window pid 3432793. Four frames, each opened. The bar text in the picture is the proof. The `.events.txt` beside each png is the desk file copied in the same second. Play is stopped after the last frame.

| Frame | What the bar says | What is on the grass |
|---|---|---|
| `draw-battle-fight-seq/1-start.png` | soldier hp 40 up, cell 2,7. roman hp 60 up, cell 10,7. Player: stopped | one sprite on the left edge of the grass, one on the right |
| `draw-battle-fight-seq/2-walk.png` | soldier hp 40 up, cell 3,7. roman hp 60 up, cell 9,7. Player: playing | both have stepped inward. No hp lost yet |
| `draw-battle-fight-seq/3-contact.png` | soldier hp 29 up, cell 6,7. roman hp 60 up, cell 7,7 | the two sprites are on neighbouring cells. The soldier has taken damage |
| `draw-battle-fight-seq/4-death.png` | soldier hp 0 dead, cell 6,7. roman hp 36 up, cell 7,7 | one sprite left, the roman |

The events copies for walk and contact are one step ahead of the bar in that frame (the file is rewritten, then the picture is taken on the next wake). They still show the same direction: 2,7 toward 10,7, then one row left, `7 7` and the red roman only.
