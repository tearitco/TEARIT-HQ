# Placement from File Explorer / Inventory (not unfactor)

**Status:** SPEC only. Implement after unfactor pieces 2–4 settle.
**Do not mix this into `UNFACTOR-PAL-X.md`.**

## Gaps (user 2026-09-18)

1. **Right-click Place** on an entity in the file browser does **not**
   show the desk tic-tac-toe overlay (`tp_arm_placer_rmmv`). It only
   writes `fe_place_armed.txt`.
2. **Target = empty desk:** overlay click → spawn pal
   (`khtpm_entity.+x`) at grid snap (or `mv` out of inventory onto
   `pals/` then spawn).
3. **Target = another Inventory / drop-target HQ window:** same **green
   fill + dotted slot** as a manual drag, then `mv`. Not a tile stamp.

## Agent / human CLI move

Context menu row **Cli-io** on the pal and/or explorer. Then:

```
mv <entity-nav-#> <window-nav-#>
```

Same `mv` as drag. Nav # is the on-screen `[ ]N` (desk pal, grid cell,
or HQ chrome). Agent: `STRING:` / `cli_io` / `entity_menu_history/<pid>.txt`
— pick one when coding. **Not built.**
