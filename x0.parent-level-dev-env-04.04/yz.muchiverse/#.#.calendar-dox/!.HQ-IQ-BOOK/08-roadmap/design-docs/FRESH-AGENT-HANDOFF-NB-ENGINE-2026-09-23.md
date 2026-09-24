# FRESH-AGENT HANDOFF — NB-JS network-browser engine cell
**2026-09-23 — distinct handoff for a fresh agent. House branch: `opencode` (this is the ONLY house with a git remote; never push to main/claude/grok).**

## If you are picking this up cold
This is the resident-JS network-browser worker cell (`&.hq-apps/network`). It is already a real,
shippable engine — rungs 3 (event loop + timers + lifecycle) and 4 (XHR/fetch via manager RPC)
are **SHIPPED and in-tree**, not future work. The plan doc's own §8.1/§8.2 prose lagged the tree;
the authoritative state is the SHIPPED-LOG in the plan (`§8.6`) plus the anchors below.

## Verified state (read before touching anything)
- `origin/opencode` parity with `HEAD`: **0 0** (fully landed; this handoff is the next single commit).
- Gate: `make nbjs` in the network ops Makefile must exit **0 = GREEN** before ANY push.
- Worker: `x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/&.hq-apps/network/ops/nb_js_worker.c`
- Plan: `x0.parent-level-dev-env-04.04/yz.muchiverse/#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/design-docs/NB-JS-ENGINE-WORKER-PLAN.md`

## The land law (hard; do not bend)
1. Bounded, one at a time. Verify (read-only) before you touch.
2. GREEN gate BEFORE any push; parity `0 0` before and `0 0` after; one non-force push
   (`git push origin opencode:opencode`); re-verify after.
3. No `--force`, no force-with-lease, no chasing a "moving" remote — if parity is non-zero,
   STOP and report; do not chase.
4. Never commit the runtime/never-source house files (`porcelain` will show many; they ride in
   the worktree by design, never into commits) — only commit the one thing the task names.
5. If unsure how to proceed: stop and ask. "Land a rung" means: read §8.6 SHIPPED-LOG first;
   if the rung you were told to land is already there, report it SHIPPED and re-point — do not
   re-land.

## Documented house pitfalls (nested in the house book; skim OPERATIONAL-LANDMINES.md)
git rebase --autostash refused on the never-source runtime files; parity misread as remote
movement — always verify remote-tip timestamp vs your own first artifact. Fully-documented in
the house landmine file (03-pitfalls).

## What this cell needs next (BOUNDED — pick ONE)
The rung order that is NOT yet in-tree (the actual frontier, to be verified by grep first):
1. **Real BOM remainder per plan §8.3** — history/URL real pushState, file-backed cookie jar,
   real location.assign (manager-level). 
2. **Client-side routing / back-forward** (rung 5 real).
3. Deferred (per plan, not a goal): layout/paint measure (getBoundingClientRect).

Commits land GREEN, each independently shippable, one per bounded pass.

---

## Addendum 2026-09-23 — stale plan retired, Rung 5 scoped

**Stale plan retired:** `NB-JS-ENGINE-WORKER-PLAN.md` Status flipped `PLAN` → `RETIRED` (see §8.6 SHIPPED-LOG). §9 non-goals marked `RETIRED 2026-09-23` — rungs 3/4 listed there as non-goals are now SHIPPED. Do not use §9 as scope.

**Rung 5 — render feedback loop — SCOPED: SHIPPED, not next.**
- Plan defines Rung 5 as `RENDER\n<rows>` glue (ROADMAP Rung 5 DONE 2026-09-05, ea864cea).
- In-tree: `nb_js_worker.c:447 RENDER_MAX 60000`, emit at `:4921`, re-emit `:4994`, manager `merge_render_rows()` overlays onto `page.state.txt`. Post-JS DOM is authoritative; no new work.
- Added to plan §8.6 as entry 3 (2026-09-23). Next is **not** Rung 5.

**Actual next frontier (per plan §8.5 — updated 2026-09-23):**
- **§8.5 C/D Rung 4 XHR/fetch async RPC — SHIPPED 7e55fc8b** (see `RUNG-4-C-SCOPE` — blocking curl + manager RPC)
- **§8.5 B EVENT RPC — SHIPPED 43c72099**
- **Rung 6 remainder (§8.3 / §8.5 E-F) — largely SHIPPED/verify-only:** `history.pushState`/`location` NAV (`g_pending_nav@1032`), `document.cookie` (`1849` file-backed), `MutationObserver` stub (`2776`)
- **Next green frontier: Phase 3 Rung 7 — CSS/layout awareness (getBoundingClientRect etc.) — see briefing below**

**Scope instruction for fresh agent:** Before claiming any Phase-2 rung as "next," grep `nb_js_worker.c` for its anchors. The plan prose lagged the tree twice already (rungs 3+4, now 5). Verify in-tree vs prose, then update §8.6 — one green commit at a time.

---

**Update 2026-09-23 — Rung 6 + Rung 4 C/D scoped and landed:** see `RUNG-6-REMAINDER-SCOPE-2026-09-23.md` and `RUNG-4-C-SCOPE-2026-09-23.md` — Rung 5 SHIPPED, Rung 6 remainder largely SHIPPED; **§8.5 B EVENT RPC SHIPPED 43c72099**, **§8.5 C/D async FETCH RPC SHIPPED 7e55fc8b** (manager-owned network, blocking curl fallback). **Update 2026-09-23 — Phase 3 slice 2 SHIPPED 514b8ab9:** simple block layout for `getBoundingClientRect` (`layout_xy` parent y + siblings) — `wcs` 9/9 PASS, `make nbjs` GREEN. Next is Phase 3 flex/grid or Rung 7 full layout if needed.
Next is **§8.5 E/F history/location/cookies verify-only or Phase 3 Rung 7** — see Phase 3 briefing below.

---

## Future Todo — Presentation proof (parked for future agents)

**Owner rule** `PRESENTATION-VIDEO-PIPELINE.md:8` — *"prove harnesses as we go... presentations being made when we are done of proof each major feature is working"* — `dump_frame_png_op` → `snapshots/` + `manifest.txt` (`<png> | <sec> | <caption>` ) → `make_presentation_video.py` → `presentation.mp4` + `REPRODUCE.md`.

**Parked until images land:** Per `PIPELINE:115` network browser is `TEXT-only` today — no `<img>`/`<video>` decode. Image support is next (see below), then full `normal browser` presentation:
- Dir: `presentations/network-browser-normal-YYYYMMDD/` with 4–6 live snapshots via `dump_frame_png_op --root` on `opencode` (go:example.com → fetch JSON → click via EVENT RPC `43c72099` → pushState → getBoundingClientRect carousel), `manifest.txt` 5–8s holds, `REPRODUCE.md` with `page.state.txt`/`RENDER`/`NAV` receipts, `make_presentation_video.py --width 1280` → `presentation.mp4`.

**Next green frontier before presentation: <img> Steps 1-2 SHIPPED e4428e13/cdb51504, Step 3 wire SHIPPED d8bc3378 — next is Step 3 draw (khtpm_draw_core.c clip at getBoundingClientRect)**
- Decoder in worker (`stb_image.h` — house already ships `stb_image_write.h`), `img` element + fetch in `nb_dom.c`, binary `IMG` wire frame, draw-image op + clip in renderer. Video (`ffmpeg` demux/decode) after `<img>`. Scope doc to be added as `RUNG-7-IMG-SCOPE-*.md` before code.

**Instruction for fresh agent:** Do **not** start presentation capture until `<img>` Steps 1-3 land (Step 1 SHIPPED e4428e13) and `make nbjs` + `wcs` are GREEN for it. Then follow `PIPELINE:35` directory shape verbatim — never template `scrot`.

