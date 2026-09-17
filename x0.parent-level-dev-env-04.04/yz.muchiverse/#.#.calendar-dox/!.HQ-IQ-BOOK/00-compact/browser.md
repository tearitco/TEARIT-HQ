# HANDOFF — network-browser compact (live browser-state doc)

**Resume-cold first:** this is the compact living handoff for the
`&.hq-apps/network` cell (nb_js_worker engine + tests + docs). If a
session crashed and you are picking this up fresh, read this file, then
the two docs at the bottom. Current session verbatim state (what was
said/done, pending decisions) is in the **Work log** section — update
it at the end of every block.

**Last updated:** 2026-09-17 (row 34b login handshake BUILT — 42 PASS)

**Branch:** `opencode` (this agent's own branch; never commit to
`main`/`claude`/`grok`). Nothing is ever pushed without the user's
explicit "push" verb.
**Build:** `make nbjs` (workspace `44.xyz.01.00/&.hq-apps/network`).
**Full suite:** `make check` — currently **42 PASS / 0 FAIL** (wss added
this set, no regressions).

---

## The browser shape (Chromium parity — the standard, no drift)

- Browsers have **no `LOGIN` op**. Site JS signs `SAPISIDHASH` itself;
  the engine's one job is a single authoritative cookie store.
- **ONE jar** (`NB_COOKIES_FILE`) serves `document.cookie`, wire
  `Set-Cookie` ingress, and `Cookie` egress (`nb_fetch_sync`). The
  worker ignores `NB_CURL_COOKIES_FILE` from 2026-09-16 forward (the
  manager may still use it for its OWN curls). Response `Set-Cookie`
  is the only wire ingress.
- No per-site hardcoding (TPMOS standing rule); generic engine,
  fixtures carry the per-site shape.

## Current state (row 34 of REAL-SPA roadmap)

| Row | Item | Status |
|---|---|---|
| 32 | page-originated fetch/XHR + session attach | **BUILT** — `worker_page_test`/`wpt` */
| 34a | unified jar: wire Set-Cookie -> document.cookie -> reattach | **BUILT** — `worker_login_test`/`wlt` (hermetic loopback) |
| 34b | real login: page JS reads SAPISID, computes SAPISIDHASH, sends `Authorization` on innerTube call | **BUILT** — `worker_sapisid_test`/`wss` (page JS signs via `__nb_sha1`; fixture recomputes sha1 server-side; SIGN=guard-ok, loopback) |

## Work log (most recent first)

### 2026-09-17 row 34b DONE (login half)
- **Engine primitive:** `__nb_sha1(str)` -> `base64(sha1(str))` registered
  in `install_events_timers`. Shared impl in new `ops/nb_sha1.h` (header so
  fixture servers recompute the SAME crypto). Raw digest bytes would be
  CESU-8-mangled inside a Duktape string, so the native pre-encodes ASCII
  base64 (btoa is not used for the digest — it can't survive raw bytes).
- **SHA-1 vectors pinned against openssl** (`abc`,`""`,one long string):
  manual browser-mode run matched `openssl dgst -sha1 -binary | base64`
  exactly before the test suite was written.
- **New hermetic test** `tests/worker_sapisid_test.c|.js` (`wss`, wired
  into Makefile check/clean): loopback `/login` -> `Set-Cookie:
  SAPISID=sapisid_w7k9q2; sid=ssr77`; page reads `document.cookie`,
  signs `SAPISIDHASH = <ts>_<base64(sha1(<ts> " " <SAPISID> " " <origin>))>`
  with stock `Date.now()`, sends `Authorization` + `Origin:` on a
  `/guard` call; the FIXTURE independently recomputes the sha1 from
  received ts + granted SAPISID + received Origin and replies `guard-ok`
  only on exact match; `/reagent` re-attaches the session cookie. All
  three assertions byte-verified. `make check` 42 PASS / 0 FAIL.
- **Fixtures bugs found while making `wss` green:** the fixture's own
  header parsing was wrong twice — `auth+7` misplacements (Auth header is
  14 chars) and a non-NUL-terminated b64 slice (`sent` pointed into the
  request buffer and trailed `" Origin: ..."`). Page + worker never
  changed during debug; the failure was always fixture-side.
- Docs updated: NB-JS-ENGINE-ROADMAP.md login-handshake bullet;
  REAL-SPA roadmap row 34 -> **BUILT**, row 30 cross-ref -> BUILT.
- Committed as `acfbc3bb` (docs compact) then engine+test commit next.

### 2026-09-17 compact handoff relocated
- `browser.md` now lives in `!.HQ-IQ-BOOK/00-compact/` (not
  `09-appendix/`), indexed in `!.HQ-IQ-BOOK-MAP`. Committed `acfbc3bb`.

### 2026-09-17 row 34a DONE + pushed
- **Committed `4855cae4`** then **pushed** `opencode` to
  `origin/opencode` (`918305e7..4855cae4`, 3 commits: wall-5/6 spec
  `55e91bf2`, roadmap flips `ca16a7f0`, unified store). In sync.
- Engine (`ops/nb_js_worker.c`): unified cookie store — `url_host_path`,
  `cookie_header_for_url` (scope-match jar -> `Cookie:` header),
  `cookie_set_from_wire` (parse `Set-Cookie` -> jar), `nb_fetch_sync`
  drops curl `-b/-c`, uses our jar + `dump-header` tmp file for ingress.
- **Two bugs found+fixed while making `wlt` green:**
  1. **CRLF leak:** dump-header preserves `\r\n`; parser stripped only
     `\n`, so `Path=/\r` was stored and every later path-match failed.
     Fixed: strip trailing `\r` per header line.
  2. **`href_parts` port bug (pre-existing):** the digit-skip never
     shrank `hn`, so `http://127.0.0.1:PORT/...` produced host
     `127.0.0.1:PORT`; invisible while set+get shared the same wrong
     host, exposed by the unified store. Fixed: cut `:port` suffix.
- New tests: `tests/worker_login_test.c` + `.js` (hermetic loopback
  fixture: `/auth` -> `Set-Cookie: sid=wlt456`; `/guard` returns
  `guard-ok` only if cookie reattached). `make check` 41 PASS.
- Docs updated: NB-JS-ENGINE-ROADMAP.md scope note (unified store =
  long-term standard), REAL-SPA roadmap row 34 -> PARTIAL (34a built,
  34b login NOT built).
- Roadmap flips `ca16a7f0`: row 32 -> BUILT, row 34 honest-split.

### 2026-09-16 census for row-34b login
- Confirmed **no crypto anywhere** in the worker (no SHA-1/HMAC). Page
  JS needs a SHA-1 primitive to compute `SAPISIDHASH`. `btoa`/`atob`
  exist in the prelude (byte-safe 0-255, so base64 of SHA-1 digests
  works). Prelude has `document.cookie` stub replaced by C natives.

## Next: beyond row 34b (what the real youtube page still needs)

Row 34 is now fully BUILT (unified jar + real-shape login handshake).
Remaining realistic gaps for "render+drive youtube.com" (roadmap rows):

1. **Feed InnerTube from the browser** (roadmap row: "Feed InnerTube API
   requests from the browser") — row 34b proves the SIGNATURE path end-toend
   (page signs SAPISIDHASH; a signed `/youtubei/v1/...` call is accepted).
   A real `yt-visitor_data` + API key flow still has no hermetic receipt.
2. **Execute youtube.com's full JS bundle** (row 31) — comments, lazy
   sections, player config; engine JS + external-slice loading exist
   (ladder tested real third-party pages) but the youtube bundle itself
   has not been exercised hermetically.
3. **Page CSS** (partial) — `.css` files, not full page CSS.

Suggested next receipt when resuming: extend `wss` (or a new `wyg` test)
to a full signed `/youtubei/v1/browse`-shaped call where the fixture
requires BOTH `SAPISIDHASH` Authorization AND a valid `yt-visitor_data`
cookie — proving the visitor-data + signature combination the real page
sends; then tackle the real page's bundle size/performance.

## The row-34b plan (as executed, for the record)

Google algorithm to reproduce hermetically:
```
timestamp = floor(Date.now()/1000)          # page JS has Date.now
hash  = base64( SHA-1( timestamp + " " + SAPISID + " " + origin ) )
SAPISIDHASH = timestamp + "_" + hash
# sent as: Authorization: SAPISIDHASH <timestamp>_<hash>
# Origin for youtube is https://www.youtube.com (scheme+host of page)
```
Steps:
1. Give page JS a generic SHA-1 primitive (small C native in
   `nb_js_worker.c`, e.g. `__nb_sha1(str)->hex` or b64; keep it
   engine-generic, NOT youtube-specific). No crypto exists today.
2. New hermetic test `worker_sapisid_test.c/.js` (or extend `wlt`):
   loopback fixture sets `SAPISID` via Set-Cookie on `/login`; page JS
   reads it from `document.cookie`, computes `SAPISIDHASH`, sends it as
   `Authorization` on `/youtubei/v1/...`-shaped call; fixture recomputes
   SHA-1 server-side and replies `guard-ok` only if signature matches.
   Third assertion: jar bytes + reattach on a follow-up call.
3. Wire `wlg`/`wsh` into `Makefile` check/clean (mirror `wlt`).
4. Full `make check`, then docs: flip row 34b -> BUILT (receipt sha),
   roadmap row 34 -> honest two-cell, design-doc scope note about
   login (page JS owns signing; engine only provides the primitive).
5. Commit scoped, present receipts, await "push".

## Paths (verbatim from `git ls-files`, never typed)

- Engine: `44.xyz.01.00/&.hq-apps/network/ops/nb_js_worker.c`
- SHA-1: `44.xyz.01.00/&.hq-apps/network/ops/nb_sha1.h`
- Prelude: `44.xyz.01.00/&.hq-apps/network/ops/nb_host.h`
- Makefile: `44.xyz.01.00/&.hq-apps/network/Makefile`
- Tests: `44.xyz.01.00/&.hq-apps/network/tests/worker_login_test.c|.js`,
  `worker_sapisid_test.c|.js`
- Manager: `.../network/network_browser_manager.c` (sets
  NB_CURL_COOKIES_FILE at :1418)
- Roadmap: `yz.muchiverse/#.#.calendar-dox/!.HQ-IQ-BOOK/
  02-architecture/REAL-SPA-SITE-GAP-VS-NATIVE-PLAYER-ROADMAP.md`
- Design doc: `.../08-roadmap/design-docs/NB-JS-ENGINE-ROADMAP.md`
- Site-gap checklists (what real youtube login needs): in the roadmap
  file rows 34-37 and "What we did NOT claim" section.

## Recurring house rules (restated, cheap insurance)

- Commit scoped to ONLY changed files at end of every block (`git add
  <path>` per file, never `-A`). Runtime noise (pdl, logs, pids) never
  swept in. Mid-work snapshots may use `wip: <what>`.
- Own branch `opencode`; no merge/cherry-pick/push without verb.
- Never report done without fresh build + fresh run + evidence.
- Load `khtpm-house-standards` skill before touching engine/manager/
  renderer code.