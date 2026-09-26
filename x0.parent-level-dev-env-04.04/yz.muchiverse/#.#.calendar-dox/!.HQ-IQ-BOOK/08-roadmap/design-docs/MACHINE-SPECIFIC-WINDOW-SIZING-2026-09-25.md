# MACHINE-SPECIFIC WINDOW SIZING — so other clones aren't stuck with one hardcoded size

**Date:** 2026-09-25
**Branch:** `opencode: e53bca89f` `fix-small e748` `DEFAULT 700x520` `auto 0.46` `font 1.25`
**Why this doc:** `opencode` had three per-machine hardcodes in a row (`1120x720` `this house` `784 via kh_auto 70%` `600x400` `500x350` `1360`) — each only fit one screen, the next puller on a different screen got `tb cut off at 7` or `too big` with no clue how to fix. This is machine-specific, not a shared constant.

## Current generic (what other clones pull)

`opencode: e53bca89f` reverted to `e748edbb8` exact working `khtpm_core_render.c:3437` `DEFAULT 700x520` `ui_scale auto` `min(2496/1664)` `0.46` on `1360x768` `0.77` on `1920x1080` `font_scale 1.25` `#.desktop/hq_ui.pdl: font_scale=1.25` `ui_scale` `auto` `default_win` none. Generic, no per-machine `360` committed.

## Per-machine override (do not commit)

`khtpm_core_render.c:3814` `kh_default_win_w()` `claude 557ab` (now on `origin/claude: f379`, pulled `23bd51682`) adds real `hq_ui.pdl: default_win_w/h` when set, else `pct 58%/67%` fallback `18628` `g_user_resizable` `4678` `DEFAULT`.

**If your pulled window is wrong size on your screen:**
1. `P="x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/#.desktop/hq_ui.pdl"`
2. Add (no `#`, last wins):
```
default_win_w=360
default_win_h=280
```
`360` reference `-> ~194` physical `1360*0.54*1.25` small like `fix`; `500` `-> ~270` bigger; pick what fits your `tb` without `cut off at 7`, no recompile.
3. `bash .../ops/build_core_render.sh` only if your `khtpm_core_render.c` lacks `kh_default` `grep -n kh_default`, else just `bash .../$.crypts/button.sh run` live-reloads `hq_ui.pdl`.

**Do not commit your `default_win` line.** `hq_ui.pdl` is tracked (`ls-files` shows it), but your `360` is per-machine like `win_top_y=96` `font_scale` — keep it local, or use `hq_ui.local.pdl` `gitignore` `*.local.pdl` if you want an untracked file (not yet wired, easy to add). `HOW-TO-SET-DESKTOP-SMALL-MODE-2026-09-25.md` is the `how`.

## For future hardcode changes

Never hardcode `500`/`600`/`1120` in `khtpm_core_render.c:18628` `4678` for one screen. Use `kh_default` `pdl` or `ui_scale` `auto` so other clones stay `small` and legible. If you must change `DEFAULT`, keep `auto` fallback, not a new constant.

