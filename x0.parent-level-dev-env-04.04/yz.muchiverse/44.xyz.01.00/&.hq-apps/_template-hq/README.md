# _template-hq — copy-paste skeleton for a new X11-HQ window

A **working, runnable** minimum HQ app. Launch it to see a window with
full chrome (`X` / `_`), a bottom-taskbar entry, a live-refreshing
sidebar, two buttons, and a repeat list:

```sh
sh 44.xyz.01.00/&.hq-apps/_template-hq/button.sh \
   /…/yz.muchiverse/44.xyz.01.00
```

It doubles as a **smoke test** for the renderer + `<module>` +
`vars=` + `<repeat>` + `action=` path.

## Make your own app from it

```sh
cp -r &.hq-apps/_template-hq &.hq-apps/myapp-hq
cd &.hq-apps/myapp-hq
git mv template-hq.xhtpm myapp-hq.xhtpm
git mv template-hq.css   myapp-hq.css
# edit button.sh:  APP="myapp-hq"
# edit myapp-hq.xhtpm: <window label="My App" class="myapp-hq database-window" ...>
#   and the <module src="&.hq-apps/myapp-hq/refresh.sh"/> path
```

Then wire the taskbar HQ menu (`#.desktop/livedesk_taskbar.pdl`): add
`hq_menu_<N>_label | myapp` + `hq_menu_<N>_cmd | sh
*.monads/*.livedesk-taskbar/ops/open_myapp.sh`, and drop a 3-line
`open_myapp.sh` next to `open_mon.sh` that resolves `house_root` from
`$0` and `exec`s this `button.sh` — the menu dispatch runs the cmd via
`sh -c`, and a leading `&` in a path is shell job-control, so the app
dir can't be named directly in the `.pdl` row.

For anything real, replace `refresh.sh` + `actions.sh` with a compiled
`ops/myapp_manager.c` `<module>` that owns state and polls a
`myapp_action.txt` — see
`#.#.calendar-dox/!.HQ-IQ-BOOK/02-architecture/X11-HQ-APP-DESIGN-WISDOMS.md`
(esp. §1, §2 "delegating the manager", §12 "the skeleton").

## The five things that are NOT obvious

1. **`<sidebar>` + `<panel>` is mandatory for chrome + the taskbar
   entry.** `khtpm_core_render` only runs `layout_sidebar_panel()` —
   which synthesises the `X` / `_` chrome and writes
   `#.desktop/livedesk_hq_windows_<pid>.txt` (the strip entry) — when
   the page has BOTH. A `<panel>`-only page renders with neither.
2. **`class="… database-window"` makes it persistent.** Without it (or
   `palettes-pal`), the renderer closes the window after any `action=`
   fires.
3. **`<repeat bind="X">` → `${X.text}` → `${X_<n>_text}`.** The bind
   name must equal the key *prefix* your publisher writes. `bind="r"`
   looks for `r_0_text`; if you wrote `row_0_text` the list is blank.
4. **`<module src="a b c"/>` is exec'd directly** (shebang honoured),
   with `<house_root> <package_dir>` appended. It is `SIGTERM`ed on
   window close. Omit the tag for a static window.
5. **`vars="state/ui.txt"` and `${PKG}` are relative to the `.xhtpm`'s
   own directory.** An `action=` string that isn't a built-in verb is
   run as a shell command with `'<package_dir>' '<house_root>'`
   appended.

## Files

| file | keep? | role |
|---|---|---|
| `template-hq.xhtpm` | rename | layout — every line is commented with why |
| `template-hq.css` | rename | colour/border only |
| `button.sh` | edit `APP=` | launcher (single-instance guard, seed, `setsid`) |
| `refresh.sh` | replace | demo `<module>` loop → `state/ui.txt` |
| `actions.sh` | replace | demo `action=` handler |
| `state/` | git-ignored | runtime projection |
