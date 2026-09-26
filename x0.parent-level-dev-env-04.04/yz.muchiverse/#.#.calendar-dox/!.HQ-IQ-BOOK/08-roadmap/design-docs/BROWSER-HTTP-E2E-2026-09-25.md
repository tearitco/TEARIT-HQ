# BROWSER HTTP E2E — real http:// fetch breadth (Rung6) on fix

**Date:** 2026-09-25
**Branch:** `opencode-fix: aebd4ebc1` `fix-small` `working` `0 0` `main: aebd4ebc1` for pull
**Verify:** `python3 -m http.server 8126` `inline a="a" + external b.js b="b" + c="c"` `document.title seq=a,b,c` `localStorage http-ok` `fetch("api.json")` `st=200` `{"hello":"rung4-http"}` via `resolve_doc_url()` `double-port` fix `nb_host.h:552` `manager resolve_url` `worker nb_fetch_sync 3785` `FETCH/FETCHED 7e55fc8b` `cookie jar` `NB_COOKIES_FILE` `manager handle_worker_fetch 1621/1691`, `collect_scripts NB_MAX 64/48` `04-network`, `wft` `httpStatus 200` `559B` `wcn 11/11` `wck 3/3` `wst 2/2` `wcs 12/12` `GREEN`, `khtpm_core_render` untouched `CENTROID_GOLD_STD` `strict ok` before merge to `main`.
