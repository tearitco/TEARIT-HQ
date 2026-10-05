# HANDOFF — 2026-10-05: network `<select>`, capture harness, HQ toolbar restore

Emergency-resume snapshot for whoever takes over `opencode-fix`. Written at
session close, so the reasoning survives even if the transcript does not.

**Branch:** `opencode-fix`
**Remote head at handoff:** `534719ff74` — a merge commit, parents
`989808c82d` (this side) + `bb99e4c925` (`origin/opencode`).
**Rollback point:** tag `handoff-verified-989808c` (= `989808c82d`, the
pre-merge verified state).
**Verified at handoff:** `make check` EXIT=0, **64 PASS, 0 FAIL** on the
merged tree — identical to the pre-merge baseline.

---

## 1. READ THIS FIRST — do not `git pull` in the checkout that ran the live app

The machine this session ran on has `opencode-fix` checked out at
`989808c82d`, **not** the remote head, and its working tree carries **446
deleted runtime files** (generated `*_ui.txt`, `*_action.txt`, `*.pid`,
`livedesk_taskbar.pdl`, `hq_ui.pdl` …) that the live taskbar and apps read
at runtime.

Fast-forwarding that checkout would overwrite live state. Clone fresh if you
need the merge locally. This is intentional, not a forgotten push.

Runtime state under app dirs is generated and intentionally untracked — a
dirty `git status` full of those is normal here and must not be swept into a
code commit (`AGENTS.md`).

---

## 2. Open items — none are done, all three are deliberate

### 2.1 `origin/opencode` is NOT fully merged (4 commits outstanding)

An earlier claim in this session that the other side could merge back with a
fast-forward was **wrong** and was corrected. After the merge:

    theirs-only: 4    ours-only: 19

So merging `opencode-fix` back into `opencode` is a real merge, not a no-op.
**Baseline to merge against: `534719ff74`.** Those 4 commits almost certainly
landed after this session's `fetch`, so re-fetch first and re-check the
left/right count rather than trusting the numbers above.

### 2.2 The presentation mp4 is STALE

`snapshots/` are current (7 PNGs at 784x504, including the new scene 07).
`presentation.mp4` still encodes the **older six-scene 960x640** run.
`make_presentation_video.py` was **not** re-run. Re-render before shipping any
video. Also recorded in that presentation's own `REPRODUCE.md` under Status.

### 2.3 Timeout interaction: 20s vs 23s

Their `cmd_load` adds `NB_LOAD_WALL_MAX_S 20`. This side's worker waits
`FETCH_RPC_WAIT_MS 15000` for a FETCHED reply, then falls back to a direct
curl capped at 8s — **23s worst case**. Their cap can therefore fire before
the fallback completes, truncating it. Harmless when the manager replies
promptly; a real edge on a slow one. Not fixed. Whoever reconciles the two
branches' timeouts owns this.

---

## 3. Landed work

### 3.1 FETCH/FETCHED protocol — `20027d7759`

Request headers + body, response headers, curl config files instead of
page-controlled shell interpolation, worker-side cookie jar ingesting
`Set-Cookie`, bounded `FETCH_RPC_WAIT_MS`.

### 3.2 `<select>` / `<option>` semantics — `be3a82fa2e`

`.value`, `.options`, `.length`, `.selectedIndex`, `option.value/.index/
.selected`, text fallback for options with no `value`, script-driven selection
firing a bubbling `change`. Added `nb_attr_has()` because `nb_attr_get()`
cannot see bare attributes (`<option selected>` reads as `""`) — left
`nb_attr_get`'s `""` contract alone on purpose.

Regression: `wsel` (`tests/worker_select_test.{c,js}`), 35 assertions, in
`Makefile:79`'s `check:` target.

### 3.3 Scene 07 + capture-harness fix — `989808c82d`

`capture_scenes.sh` found its window by grepping `960x640`, the size in
`hq_ui.pdl` at the time. Commit `b1ede54f55` changed that default to
**784x504**, so `win()` began returning empty, `dump_frame_png_op` got
window `0x0`, and every scene died on `BadWindow (invalid Window parameter)`
— aborting the run under `set -e` and **leaving the previous run's PNGs in
place looking current.** A stale capture set is byte-identical from the
outside to a fresh one, so the failure was invisible.

`win()` now reads `default_win_w/h` out of `hq_ui.pdl` and matches the
**unnamed** viewable override-redirect window against it. "Unnamed" is
load-bearing: a largest-window heuristic picks the compositor's 1360x768
**"mutter guard window"**, which cannot be dumped. The browser's own window
is `(has no name)`. `shot()` now fails loudly on empty `win()` instead of
passing `0x0` mid-scene.

### 3.4 Merge of `origin/opencode` — `534719ff74`

Only 2 files conflicted, both resolved on evidence:

- **`khtpm_core_render.c`** — took **theirs**. Looked dangerous (theirs 25
  commits vs my 2, 10 hunks) but was not a disagreement: every UI-scale
  symbol added here already exists in their version with identical counts
  (`kh_ui_apply_scale` 12/12, `g_ui_user_pct` 6/6, `kh_auto_pct_now` 2/2).
  168 of 172 added lines survive in their file; the 4 that don't are all
  `dock_pager_px` **comments** — the config key itself reads
  `g_dock_pager_base` in both. Their file is 977 lines longer. **Zero
  behavioural loss.**

- **`network-browser-hq.xhtpm`** — genuine **union**, 1 hunk. Theirs added
  `class="nb-text"`/`class="nb-link"` styling; mine added
  `action="${c.click_action}"` on `ct`/`cx` for clickable rows. Independent
  attributes, so both kept. Neither side's attribute dropped.

Verified byte-identical across the merge: `ops/nb_js_worker.c`, `nb_dom.c`,
`nb_dom.h`, `Makefile`, `.gitignore`, `tests/worker_select_test.{c,js}`.

---

## 4. Two silent-failure traps worth not regressing

1. **`STATUS ok` does not mean the page script succeeded.** `cmd_load` sends
   it unconditionally; a thrown script surfaces only as a stderr `WERR|`.
   Every harness relying on a JS `throw` for failure shares this hole.
   `wsel` alone checks the console capture for an explicit end-marker and
   absence of failure markers, and is negative-controlled both ways.

2. **The toolbar can be alive with no bar, and look "running".** Symptom
   here: `khtpm_taskbar_manager_main` alive ~3.8 days (pid 916033) with its
   bar window never mapped — the X display held only the browser, the
   compositor guard window, and orphaned 40x40 per-app entity cells. Nothing
   had killed it; a stale-PID theory was wrong. The self-restart the taskbar
   menu already uses is the correct fix (rebuild is hash-gated, then
   TERM→KILL→relaunch under a restart lock):

       sh _.monads/_.livedesk-taskbar/ops/run_khtpm_strip.sh new

   Result: pid **2653544**, a 960x23 window flush to the bottom edge at
   y=745 plus a 958x23 header strip, ~1000 unique colours and luma stdev ~90
   in the dumps — i.e. rendering content, not a blank rectangle. Note the
   script can run >5 min without output while working correctly; a tool
   timeout on it is cosmetic, not a failure.

---

## 5. Fixture location

Presentation fixture HTML lives in `/tmp/nb_site`, **not in git**:
`index.html`, `page2.html`, `api.json`, `pic0..5.png`, `scene2_fetch.html`,
`scene3_image.html`, `scene6_rect.html`, `scene7_click.html`,
`scene8_select.html` (scene 07). Regenerated by hand; `REPRODUCE.md` in that
presentation says so.