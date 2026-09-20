# 2026-09-20 — what happened / what's next

## What landed (2026-09-19 → 09-20, all on `claude` + `main`, pushed at `2c1301ab`)

- Merged Grok's branch (File Explorer grid/drop/Place, DSR app, `khtpm_entity` split start) and opencode's QuickJS/kevlar browser work.
- DSR + db-hq-pal toys did nothing when clicked (toys menu runs `button.sh run`; their scripts read `run` as the house root) — fixed.
- File Explorer: PAL entries show the entity's real sprite (transparent pixels blend with the body colour, not the header); Search field above Back; right-click Cut/Copy/Paste/Delete/Place actually reach the explorer; global cross-window clipboard; Place can target an open Inventory (green highlight); entity's own METHODs in an Inventory right-click; Inventory row on every entity, several Inventory windows at once.
- Cli-io `mv <nav#> <nav#>` handler (only verb so far); `nav.sh` retargeted to live relay files.
- UI scales to the monitor (`ui_scale`/`ui_ref_*` in `hq_ui.pdl`); **not yet checked on the second computer.**
- Unfactor: `tp_main` + pal code moved out of `khtpm_core_render.c` (19.1k → 12.5k lines) into `khtpm_entity.c` (6.3k) + shared `khtpm_ui_common.c`.
- Space = open the context menu (HQ windows) / = Enter (dock).
- Place overlay closes on Esc; csv-hq grid arms/keeps state; shared `khtpm_grid_jump.c` helper.
- **The big one:** csv-hq (and every window) had a dead keyboard because the Cursword pal held a display-wide `XGrabKeyboard` it never released (`kh_ungrab_kbd()` tested a NULL global). Fixed `2c1301ab`; restarting Cursword once dropped the held grab; user confirmed. Written up as `03-pitfalls/HOUSE_CODE_PITFALLS.md` #24 + `X11-AND-SESSION-PITFALLS.md`.

## Next — ordered

### Verify first (cheap, might close bugs)
1. Restart the other pals (started 02:21 with older binaries) so nothing else holds a stale grab.
2. Re-test **text-edit-hq** typing — the 2026-09-14 "keyboard never arrives" entry (`bug_bounty.md`) is very likely the same Cursword-grab cause; close it if it types.
3. Real-hardware checks still owed: Space menu, Search field, Place Esc + Inventory drop, sprite/nav-chip overlap on 2-digit numbers in list view, UI scaling on the second computer.
4. Audit `khtpm_entity.c` for other helpers that use the always-NULL `dpy` global (pitfall #24 rule 1).

### Inventory / File Explorer (user's current thread)
5. **Place-grid labels**: A–Z columns + 1–N rows on the Place overlay, type a ref (`c7`/`7c`) + Enter to highlight, second Enter places — use `_shared-lib/khtpm_grid_jump.c` (see `GRID-ELEMENT-DESIGN.md` "Reuse by overlay pickers").
6. **Cli-io on all entity context menus** (experimental; more verbs than `mv`; re-add the Cli-io row to File Explorer's `meta.pdl` once a human-typable field exists).
7. Place from the **desk** into an Inventory; drag between explorer windows; right-click methods for a pal with no `sprite.csv`.
8. Robot/puzzle-piece entities carrying events, dropped into inventories, methods run from the Inventory right-click; slow migrate of `inventory.txt`/`qolq`; File METHOD stub + 📁 icon mode.

### Engine / architecture
9. UI scaling: `desktop_pos.txt` is absolute pixels and ~10 tools write it — convert to reference-space; look at an entity window on a small screen.
10. `XHTPM-RE.md` plan: `.xhtpm` → `.xhtm` rename; keep shrinking `khtpm_core_render.c` (still 12.5k) and cut `khtpm_entity.c`'s dependence on shared includes.
11. Unfactor leftovers not re-verified: Cursword 3D/phymoji camera keys, z-layer changes, XDND drops from other apps.
12. `livedesk_override_redirect.pdl=true` ("@" always-on-top) is still a separate documented cause of dead keys for override_redirect windows; text-field windows are now forced managed, but consider the wider policy.

### Paused tracks (resume decision is the user's)
13. **WSR-CIV + DSR** (kilo, `13.agent-coms/KILO/claude-2-kilo-9.17.md`): the pause trigger ("Inventory window shows `cursword/inventory/` as a grid and a human clicked it") is effectively met. Start at Step A with the fixed `nav.sh`; still open: piececraft-hq ".main tab only" board bug, DSR menu class (New/Load/Save/Save As), Human/Harness flag, `ai_*` event primitives.
14. Cursword IRL/watch layer (LLMUD-HACK / DUSTOPIA-HACK): first slice = relay watcher + Synonym Bank fed by kilo's own event authoring.

### Housekeeping
15. The main tree still has ~775 uncommitted deletions (old docs, NIGHT_12–16 mp3s, network fixtures) and ~200 untracked files (asset dirs, notes, the 638KB `kilo-post-mortem-s17.md`) — nobody committed them on purpose; decide keep/delete.
16. Older open bugs: piececraft-hq board tab, network-browser address bar (recurring), pc-hq board focus vs taskbar, toys-launch PID tracking, `ktb_pid_alive()` zombie false-positive, `nav.sh` `row`/`type` not exercised on live rows.
17. `db-hq-pal` toy fix was only syntax-checked, not launched.
