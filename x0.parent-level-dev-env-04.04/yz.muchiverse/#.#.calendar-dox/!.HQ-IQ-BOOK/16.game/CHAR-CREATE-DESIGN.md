# Character creation (design, 2026-10-21)

Status: **not built.** The owner asked for a character engine later, and for img-hai to leave the toys menu. The menu move is done. The engine is not.

## SAID

A script, later, that can make as many of these people as we want. Three ways in:

- Arguments: a name and the fields a `party.pdl` row already has.
- A sprite already on disk: pick a free cell on an RPG Maker sheet.
- Diffusion: draw a new picture, the way img-hai already drafts one.

The window is an x11-hq window in the same shape as img-hai. It belongs under h-ai, not under toys.

## Where it sits

img-hai is `&.hq-apps/img-hai/`. It opens with `sh &.hq-apps/img-hai/button.sh run` (shared `khtpm_core_render.+x`, `img-hai.xhtpm`, a small manager). The h-ai cell reads `ai_menu_N_*` in `#.desktop/livedesk_taskbar.pdl`. Toys only lists a folder that still has a `toy.pdl`. img-hai's `toy.pdl` is removed. h-ai row 11 is img-hai. Cancel is row 12.

The character window, when it exists, is a sibling: `&.hq-apps/char-hai/`, its own `button.sh`, its own xhtpm, and one more `ai_menu` row. It does not get a `toy.pdl`.

## What one run writes

One person is one `PET` row plus a picture the games already know how to draw.

```
PET | Vale | People4 | 0
```

The name is the id once lowercased. The sheet and the index are the sprite. A game's own `party.pdl` is the only roster (`BET-RPG-DESIGN.md`). This tool appends a row to the file you name. It does not edit `DsrRpgParty` or `actors.pdl`.

The three sources:

1. **Args.** `char-hai --name Vale --sheet People4 --index 0 --out party.pdl`. No picture is drawn. The cell must already exist on that sheet.
2. **Sprite on disk.** The same sheets under `&.widgits/palettes/sprites/rmmv/dir_characters/`. The tool lists cells not already named in the destination `party.pdl` and writes the next free one. Random means a free cell, not a new file.
3. **Diffusion.** A prompt goes through the img-hai job (`ops/img_hai_job.c`, the same Draft path). The result is reduced to one walk cell and saved beside the app. The `PET` row points at that new sheet. Until that reducer exists, this source is a note, not a button.

A batch is the same row repeated: `--count 6` writes six names. Names come from an argument list, or from a short word list in the app when the argument is `random`. The tool refuses a name that is already in the destination file.

## Not in this note

No new window, no menu row for char-hai, no call to a model. Shooter, studio, and the island keep the casts they have. Real-money, accounts, and a shared cast across games stay out.
