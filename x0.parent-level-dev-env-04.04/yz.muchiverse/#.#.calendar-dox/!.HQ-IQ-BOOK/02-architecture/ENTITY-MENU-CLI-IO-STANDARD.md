# Entity menu `Cli-io` — the standard for giving an entity a real typed command line

Written 2026-10-07 after the Eden button shipped with a `Cli-io` row that did nothing. Every fact below was read from the
code named next to it; "not verified" marks what was not run on a real screen.

## 1. What a real Cli-io is (and what a fake one looks like)

A **real** Cli-io is ONE element inside the entity's `menu.chtpm`:

```
<cli_io id="cmd" target_id="cmd" label="Cli-io: " action="${PKG}/cli.sh"/>
```

The shared renderer (`_.monads/_.livedesk-taskbar/ops/khtpm_core_render.c`, tag `cli_io`) turns it into an armed text field:
click it (or its nav number) to arm, type, **Enter** runs `action`. Zero per-app renderer code (house rule, see
`khtpm-house-standards`). Typing is live-synced to `<pal>/cli_io_state.txt` (`target_id=typed text` lines, rewritten whole
on every keystroke; `default_cli_io_save`).

A **fake** one is `<item label="Cli-io" action="CLI_IO"/>`. It is what `meta_to_menu_chtpm.py` appends by default when
nothing better exists. The renderer has no `CLI_IO` action on the menu.chtpm path (open gap logged in the converter's own
docstring, 2026-09-22): the row is clickable and does nothing. **If the menu shows `Cli-io` as a plain item, it is the fake.**

## 2. The contract of `action`

- `${HOUSE}` and `${PKG}` are expanded at parse time (`kh_get_var`): `${HOUSE}` = house root, `${PKG}` = the entity's own folder.
- On Enter the renderer runs the action with three argv: `<package_dir> <house_root> <typed text>`. The typed text is passed
  directly (not re-read from `cli_io_state.txt`, which races the buffer clear). Quote `$3` yourself.
- It does **not** close the window (a command line is a persistent field, unlike a one-shot menu item).
- Run it in the background in your own code if it can block; the renderer does not wait for a verdict.
- The shared default is `${HOUSE}/&.widgits/entity-cli/ops/entity_cli_commit.sh`: it appends the line to `<pal>/cli_commands.txt`
  and understands two hard-wired `two-facts` words (`range`, `advance`). Use it only for entities that want exactly that.
  **A game/conductor entity should bring its own handler** (below) instead of growing the shared one.

## 3. How a NEW entity gets a real one (the standard recipe)

1. Write the handler in the pal folder, e.g. `cli.sh` (or a compiled op; a shell shim that `exec`s an op is fine). Contract:
   `argv1 = pal dir, argv2 = house root, argv3 = typed line`. Append every typed line to `cli_commands.txt` (append-only),
   append answers to `cli_reply.txt`. Unknown words get a one-line help, exit 0 (never crash the field).
2. Declare it in `meta.pdl`: `META | cli_io_action | ${PKG}/cli.sh`
   (the converter reads this row; no other place needs the path).
3. Generate the menu: `python3 -I _.monads/_.livedesk-taskbar/ops/meta_to_menu_chtpm.py <pal_dir> [--force]`.
   Result: a real `<cli_io .../>` line is the last child of `<page name="main">`, and the placeholder item is suppressed.
4. Never hand-edit the line away: an existing `<cli_io` line in `menu.chtpm` is always carried forward on regenerate
   (`extract_real_cli_io_lines`), and wins over `cli_io_action`. To change the action, edit `meta.pdl` AND delete the old
   line (or edit the line itself), then `--force`.
5. Installer-made entities do all of this in code: `&.widgits/eden/install_eden.c` writes `cli.sh`, the META row, and runs
   the converter (best effort, reports a failure on stdout instead of hiding it).

Standard rows every desk pal's menu should also carry (compare `asa`, `eden_robot`): `STATE | kind | deskpal`,
`STATE | glyph | <glyph>`, then `Events (hq)`, `Dir`, `Inventory`, `Close` (`CLOSE`), `Cancel` (`void`). Do not copy
`grab_pointer` / `grab_keyboard` STATE rows from cursword; other pals do not carry them.
A pal also needs a `sprite.csv` (otherwise the window is a plain coloured square): generate with
`emoji_gen_atlas.+x <emoji> atlas.png` then `emoji_xtract.+x atlas.png 0 64 sprite.csv` (`_.monads/_.livedesk-taskbar/ops/+x/`).

## 4. The legacy path (no `menu.chtpm`)

An entity without `menu.chtpm` uses the native popup in `khtpm_entity.c` (`load_methods()` auto-appends a `CLI_IO` method,
which DOES work there, with its own armed/typed/committed handling). That is a second, separately maintained implementation of
the same idea (the house's own recorded fork). New entities should use the menu.chtpm path above; do not rely on the legacy one.

## 5. How to test one (house rules apply)

- **Handler alone (a harness, not a new .sh test):** pal harness case: `RUN | /bin/sh | -c | exec sh "$0/cli.sh" "$0" "$1" "status" | <pal> | <house>`,
  then `EXPECT_HAS` on `cli_commands.txt` / `cli_reply.txt`, plus one unknown-word case. See `_shared-lib/harness/cases/eden_install.pdl`
  (the cli.sh cases) — 114/0 on 2026-10-07.
- **Menu file:** `EXPECT_HAS menu.chtpm '<cli_io id="cmd"'` and `EXPECT_LACKS menu.chtpm 'action="CLI_IO"'` (a regression guard
  against the fake).
- **Real window (not yet done for any generated entity):** open the entity's context menu, find the `cli_io` nav number from a
  fresh frame dump (nav numbers are global across windows, never assume), then drive it through the relay
  `#.desktop/entity_menu_history/<pid>.txt` (`KEY_PRESSED: <ascii>` per character, `13` = Enter), and read `cli_commands.txt`
  back. Remember the repo's own warning: relay passes can mask real focus/grab bugs; confirm once on real hardware.

## 6. Known gaps (as of 2026-10-07)

- The converter can source only ONE generic field (`id="cmd"`). More fields (chat boxes, `text_area`) are still hand-written.
- Not verified on screen: that Enter in the Eden button's field reaches `cli.sh` (the handler itself and the generated menu are
  harness-verified; the renderer path is the shared, long-used one).
- Existing entities created before this standard keep whatever their menu.chtpm already has.

## 7. Where the pieces live

| Piece | Path |
|---|---|
| Renderer element + state file | `_.monads/_.livedesk-taskbar/ops/khtpm_core_render.c` (`cli_io`, `default_cli_io_save`, `kh_get_var`) |
| Converter (+ `META | cli_io_action`) | `_.monads/_.livedesk-taskbar/ops/meta_to_menu_chtpm.py` |
| Shared default handler | `&.widgits/entity-cli/ops/entity_cli_commit.sh` |
| Reference implementation | `&.widgits/eden/install_eden.c` (writes cli.sh + META row + menu) |
| Reference tests | `_shared-lib/harness/cases/eden_install.pdl` |
