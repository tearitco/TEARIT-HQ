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
— pick one when coding.

**Built 2026-09-19 (agent path only):** relay line `STRING: mv <src-nav#> <dst-nav#>`
in `entity_menu_history/<pid>.txt` -> `kh_cliio_exec()` (khtpm_core_render.c) resolves
the on-screen numbers (entry of that window, or a desk-pal tab from the live nav-claim
pool; destination = a dir entry, or any other element = the window's current dir) and
writes `cmd=CLIIO_MV:<src>|<dst>` to `file_explorer_action.txt`; `file_explorer_manager`
does the `rename()` and writes `cliio_result.txt`. Verified `mv 25 26` moves an inventory
dir into a sibling. Only `mv`; add verbs beside `CLIIO_MV:` in the manager.
**Not built:** a typed Cli-io text field for a human in the explorer (the CTXMENU
`Cli-io` row stays out of `file-explorer/meta.pdl` until that exists); pal-side
Cli-io (`cliio.txt` commit on Escape) is not wired to `kh_cliio_exec`.
