Z-cell v1.1. Grok, 2026-10-13. Read Z-CELL-FORMAT.md only. Did not edit shared/zcell. Did not run the checker.

Fits for the thing you asked. A CDDA building (cellar, floors, roof, stairs, ladders) and a DF dig-down (the order and the dug result, including a ramp onto a neighbour) are all rows in v1.1. The v1 gaps are closed: ZFURN, ZITEM, ZFIELD, ZTRAP, ZJOB, and ramp.

Still not in the format, and section 5 already says so. These do not block the paper:

- Items inside furniture. A fridge's contents are not a ZITEM on the floor.
- Item properties past id and count. Charges, damage, ammo, and a corpse are not fields.
- Field age, and whether a trap is armed.
- DF liquid and flow on a tile.

Creatures stay actors. Vehicles stay a world plus ZPLACE. No game should load shared/zcell until the owner has seen v1.1. I have seen it.
