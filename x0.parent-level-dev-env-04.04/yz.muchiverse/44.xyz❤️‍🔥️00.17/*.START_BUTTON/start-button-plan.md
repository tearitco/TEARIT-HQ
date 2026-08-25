# START BUTTON — House Launcher (Design)

**Status:** active design — implementation follows this doc  
**Target:** `*.START_BUTTON/` under house root (44.xyz)  
**TPMOS refs:** `loader.chtpm` / `loader_module.c` / `loader.pdl`, plus `os.chtpm` (pre-menu hrefs)  
**House refs:** avatar `href` screens + `write_chtpm_bridge` / `request_layout` / `${piece_methods}`  
**Sibling folders:** `@.apps/`, `@.app-store/`, `&.widgits/` (widgets; house spelling)

---

## 0. Product shape — pre-screen of categories

START_BUTTON is **not** a flat dump of every `button.sh` on day one.
It opens as a **category pre-screen**, then each category is its own
list screen (TPMOS `os.chtpm` → Project Loader / App Store pattern).

```
+===========================================+
|           H O U S E   L O A D E R         |
|                                           |
|  [>] 1. System                            |
|  [ ] 2. Widgets                           |
|  [ ] 3. Apps                              |
|  [ ] 4. App Store                         |
|  [ ] 5. Quit                              |
+===========================================+
```

| Category   | Catalog root (config)     | Meaning |
|------------|---------------------------|---------|
| **System** | `scan_root` = house `./` (`..` from START_BUTTON) | House **system programs** living as sibling dirs under `./` that expose `button.sh` (editor, login, forum, mutaclsym, …). **Exclude** widget tree, apps tree, app-store tree, and the launcher itself — even if they have `button.sh`. |
| **Widgets**| `widgets_root` = `../&.widgits` | System-installed widgets. Empty / sparse today (`file-explorer` stub). |
| **Apps**   | `apps_root` = `../@.apps` | **Installed** user apps. Empty for now. |
| **App Store** | `store_root` = `../@.app-store` | Catalog of apps **available to install** (not yet run). Empty for now. |

Config PDL owns all four roots so paths can move later without code rewrites.

### Why not one mega-list of every button.sh?

- System / widgets / apps / store are **different install lifecycles**.
- Widgets and store packages must not pollute "system programs".
- Matches TPMOS mental model: OS menu → Project Loader vs App Store.

---

## 1. Prefer chtpm href + dynamic methods (no hard menu tree)

### Pre-screen = static **href** buttons (TPMOS / avatar / forum)

```xml
<!-- home.chtpm -->
<button label="System"    href="pieces/chtpm/layouts/system.chtpm" />
<button label="Widgets"   href="pieces/chtpm/layouts/widgets.chtpm" />
<button label="Apps"      href="pieces/chtpm/layouts/apps.chtpm" />
<button label="App Store" href="pieces/chtpm/layouts/store.chtpm" />
```

Parser already switches `current_layout` on Enter for `href` (same process,
cleanup_module + re-parse). **No custom nav hardcode.**

### Section lists = regenerated **`${piece_methods}`** (TPMOS loader.pdl + house login)

Each section layout:

```xml
<module>system/prisc+x pal/main_loop_chtpm.pal</module>
<interact src="pieces/apps/player_app/interact_relay.txt" />
<text label="${game_map}" />
${piece_methods}
<button label="Back" href="pieces/chtpm/layouts/home.chtpm" />
```

`start_scan` rewrites:

```
projects/start-button/pieces/system/piece.pdl
projects/start-button/pieces/widgets/piece.pdl
projects/start-button/pieces/apps/piece.pdl
projects/start-button/pieces/store/piece.pdl
```

with rows like:

```
METHOD | 102.editor-... | RUN:../102.editor-...
METHOD | 041.pal-forum... | RUN:../041.pal-forum...
```

`load_dynamic_methods` already turns METHOD rows into `KEY:n` buttons.
Menu input only handles `RUN:` / `INSTALL:` / `REFRESH` — not drawing.

### Bridge after href (avatar lesson)

`${piece_methods}` follows `active_target_id` in `state.txt`, **not** the
layout path alone. Pure href does not set that.

**Tight fix:** idle tick in `start_menu_input` (key 0):

1. Read `pieces/display/current_layout.txt`
2. Map `.../system.chtpm` → piece `system` (etc.)
3. If `active_target_id` differs → `write_chtpm_bridge(piece)` + rescan that section
4. Clear stale `interact_relay` on layout change (avatar pattern)

So navigation stays **href-native**; methods stay **PDL-native**.

---

## 2. Same terminal window launch (functionally tight)

Goal: pick an entry → that program owns **this** TTY; when it exits,
return to the loader.

**Not** background `&` (steals nothing but also never "takes" the window).
**Not** nested raw-mode (keyboard_input still holding the TTY).

### Handoff loop (button.sh owns the TTY)

```
button.sh run:
  loop:
    start renderer + chtpm (+ module)
    keyboard_input          # blocks; raw mode
    kill session children
    if handoff_launch.txt exists:
        target=$(cat handoff_launch.txt); rm it
        (cd "$target" && ./button.sh run)   # SAME TTY, foreground wait
        # child exited → loop back into loader
    else:
        break   # real quit
```

### How selection triggers handoff

`RUN:relpath` in menu_input:

1. Resolve `button.sh` under catalog root + relpath  
2. Write absolute path (or dir) to `pieces/system/handoff_launch.txt`  
3. Write `pieces/system/quit_request.txt` so **keyboard_input yields**  
4. keyboard_input exits → shell continues handoff loop  

Requires a **tiny local patch** to START_BUTTON’s `keyboard_input.c`:
poll `quit_request.txt` / `handoff_launch.txt` (select timeout or
non-blocking check) and break — same as quit, without Ctrl+C.

No pipes required. No second TTY. One foreground process at a time.

### Store vs RUN

| Section | METHOD verb | v1 behavior |
|---------|-------------|-------------|
| system / widgets / apps | `RUN:path` | handoff → `button.sh run` |
| app store | `INFO:id` or later `INSTALL:id` | message only ("not installed"); install later |

---

## 3. Discovery rules (per section)

Shared: sort alpha, skip `#.*` / `.*` / `test-harn*`, max_entries from config.

| Section | Root | Include | Exclude |
|---------|------|---------|---------|
| system | `scan_root` (`..`) | dirs with `button.sh` (depth 1–2) | `*.START_BUTTON`, `&.widgits`, `@.apps`, `@.app-store`, notes/docs/net/exchange/metatree/architecture-bible, harness-only |
| widgets | `widgets_root` | child dirs (optional `button.sh` / `project.pdl`) | — |
| apps | `apps_root` | child dirs with launch entry | — |
| store | `store_root` | child dirs / package cards | do not RUN; list only |

`require_project_pdl` remains a config flag (default 0 for system until cards exist).

---

## 4. Architecture

```
*.START_BUTTON/
  button.sh                 # session + same-TTY handoff loop
  config/start_button.pdl   # roots + skip lists
  pal/main_loop_chtpm.pal
  system/                   # prisc+x, renderer, chtpm, keyboard (+ yield patch)
  ops/
    start_scan.c            # regenerate section piece.pdl files
    start_compose_frame.c   # chrome per section into view.txt
    start_menu_input.c      # idle bridge + RUN handoff + REFRESH
  pieces/chtpm/layouts/
    home.chtpm              # category href pre-screen
    system.chtpm            # ${piece_methods} + Back href
    widgets.chtpm
    apps.chtpm
    store.chtpm
  projects/start-button/pieces/{home,system,widgets,apps,store}/piece.pdl
  test-harn-same/
  proof/
```

### Signal flow

```
home href → system.chtpm
  idle: bridge active_target_id=system, start_scan(system)
  ${piece_methods} = RUN rows
  Enter RUN:foo → handoff_launch.txt + quit_request
  keyboard yields → button.sh runs foo in same TTY
  foo exits → loader restarts (loop)
```

---

## 5. Phases

**A (now):** scaffold, config, home hrefs, scan system/widgets/apps/store lists,
compose, idle bridge, RUN writes handoff (harness may not spawn), harness frames.  
**B:** keyboard yield + button.sh handoff loop (same TTY live).  
**C:** project.pdl display names; store INSTALL; refresh.

---

## 6. Thesis

**Pre-screen of four categories via chtpm href; each section’s list is
dynamic `${piece_methods}` from scanned PDL (not hardcoded rows); launch
is a same-TTY handoff loop in button.sh (foreground child, then return) —
not background forks, not in-process LOAD_PROJECT for foreign apps.**
