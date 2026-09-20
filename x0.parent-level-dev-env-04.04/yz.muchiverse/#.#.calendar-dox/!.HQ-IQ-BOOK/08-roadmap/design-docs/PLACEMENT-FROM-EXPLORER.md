# Placement from File Explorer / Inventory (not unfactor)

**Status:** Desk Place from explorer **landed 2026-09-18** (overlay +
`khtpm_entity.+x`). Other-inventory hover and Cli-io `mv` still spec.

## Gaps (user 2026-09-18)

1. **Right-click Place** → `tp_arm_placer_rmmv` wireframe (env
   `FE_PLACE_CLICK` skips tile stamp). Click desk → `mv` out of
   `inventory/` onto sibling `pals/<name>/`, write `desktop_pos.txt`,
   `khtpm_entity.+x`. Esc cancels. Reopen explorer if list looks stale.
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
