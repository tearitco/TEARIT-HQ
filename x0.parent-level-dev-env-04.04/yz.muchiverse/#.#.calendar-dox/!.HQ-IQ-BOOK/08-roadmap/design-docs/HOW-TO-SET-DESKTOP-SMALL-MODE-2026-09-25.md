# HOW TO SET DESKTOP BACK TO SMALL MODE (both tb bars legible on 1360)

**Why:** `opencode: 5e9fbade9` `fix-small e748` small via `ui_scale 0.54` `DEFAULT 700->378` physical, `opencode: 90029c720` `main` with `claude` `557ab pdl` small via `hq_ui.pdl:360`. After `claude` merge, `khtpm_core_render.c:18628` uses `kh_default_win_w()` `pdl` override, not hardcoded `500`/`1120`.

**Steps (no recompile if pdl logic present, else rebuild):**
1. `P="x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/#.desktop/hq_ui.pdl"`
2. Ensure `P` has (create if missing):
```
default_win_w=360
default_win_h=280
```
`360` reference `-> ~194` physical `1360*0.54` `*1.25 font` `->` tb `960->~518` fits `7` items, `fix-small` legible. For larger `1920` host, `360->~194` still small; bump to `500` if you want bigger there (per-machine, no recompile).
3. If `khtpm_core_render.c` lacks `kh_default_win_w()` `557ab` (check `grep -n kh_default`):
   `bash x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/*.monads/*.livedesk-taskbar/ops/build_core_render.sh` `OK +x/khtpm_core_render.+x` `370K`
4. `bash x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/$.crypts/button.sh run` `CLOSE livedesk` `tool-bar -> run_khtpm_strip.sh boot` `ps 148849 khtpm_core_render`
5. Verify `ps aux | grep khtpm_core` new `370K` pid, `both tb bars small`.

**If on `fix-small e748` (no pdl logic):** `e748` `DEFAULT 700x520` `ui_scale 0.54` already small `378` physical — no `P` needed, just `build_core_render.sh` `fix` file `e748` and `button.sh run`. `opencode: a9edaef30` `90029c720` docs already `fix-small` `5e9fbade9`.
