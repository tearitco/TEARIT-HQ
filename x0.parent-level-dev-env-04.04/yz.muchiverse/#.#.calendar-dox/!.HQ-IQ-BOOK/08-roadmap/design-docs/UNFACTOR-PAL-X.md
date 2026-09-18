# Unfactor pal `+x` — one piece at a time (2026-09-18)

Restore the **already-IPC** desktop pal process. Do not fold HQ windows
the other way. See `XHTPM-RE.md` correction.

## Pieces

| # | What | Status |
|---|---|---|
| 1 | Build `khtpm_entity.+x` from the same `.c` with `-DKHTPM_ENTITY_BIN` (`main` → `tp_main` only). HQ binary unchanged, still accepts `argc==2`. | **this burst** |
| 2 | Switch pal spawners (`pets/pieces/*/button.sh`, livedesk open) to `khtpm_entity.+x "$PKG"`. Keep HQ `khtpm_core_render.+x` for `.xhtpm`. | next |
| 3 | HQ `main` stops calling `tp_main` (`argc==2` errors). Prove a pal still launches. | next |
| 4 | Move `tp_main` + tile helpers into `khtpm_entity.c`. Shared draw still `-I _shared-lib`. | later |

Do not skip to 4 in the same sitting as Place-grid.

## Related gaps (docs now, code later)

**Place from File Explorer** (right-click entity in the browser): still
only `fe_place_armed.txt`. Need `tp_arm_placer_rmmv` overlay (desk
tic-tac-toe). Click on **empty desk** → spawn pal (`khtpm_entity.+x`)
at grid snap. Click **over another drop-target window** (second
Inventory) → same green fill + dotted slot as a manual drag, then `mv`.

**Agent / user CLI move:** context **Cli-io** on the entity (and/or
explorer). Input:

```
mv <entity-nav-#> <window-nav-#>
```

Same `mv` as drag. Nav # = on-screen `[ ]N` (pal on desk, or cell in
grid, or HQ window chrome). Agent writes the line into that `cli_io`
(or `entity_menu_history/<pid>.txt` as `STRING: mv …` — pick one in
the implement burst). Spec lives here and in
`INVENTORY-DROP-AND-WINDOW-HIGHLIGHT-2026-09-18.md`. **Not coded.**
