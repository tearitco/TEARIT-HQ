# 2026-09-07

## Done

- **pc-hq board: "clicking File traps you in Interact Mode" — FIXED.**
  User report: "arrow/nav focus keeps jumping back to `[]3.file`."
  Diagnosis: not a nav bug — `pchq_board_action.sh`'s `file`/`desk`
  verbs auto-engaged Interact Mode (`interact_on || append_key 13`) and
  never released it, so `g_interact_relay_on` stayed armed and the
  renderer forwarded every key (arrows included) to the game instead of
  moving local nav — highlight froze on the File item.
  Fix: `file`/`desk` now engage only if it was off and restore it
  afterward (live-re-checked toggle, never double-toggles). Full
  writeup: `09-appendix/pc-hq-bugs.md` Bug 5. Verified with a
  fake-engine harness (both the not-engaged and already-engaged cases).

## Housekeeping: disposable-file policy (founder ask)

"rgba / ascii frames / log files should be trimmed/deleted, never over
250 KB."

- **`44.xyz.01.00/#.desktop/tidy-runtime.sh`** (new) — deletes
  framebuffers (`rgb_frame*.raw`, `*_3d_overlay.raw`, `canvas.raw`,
  `*.rgba32`) when no session engine is running, empties
  `#.desktop/ascii_frames/`, and tails `*.log` / `*frame_history.txt` /
  `gl_cli_out.txt` to their last 250 KB. Skips anything git-tracked.
  `-n` for a dry run.
- **`rezip-house.sh`** (repo root, new) — timestamped `.7z` that now
  excludes `.git`, `*.raw`, `*.rgba32`, `ascii_frames`, `*.log`,
  `*frame_history.txt`, `gl_cli_out.txt`, `node_modules`; deletes the
  prior archive.
- Ran the cleanup once: tree 617 MB → 580 MB (~37 MB of dead
  framebuffers + oversized frame-history mirrors). Touched **zero**
  tracked files (all deletions were gitignored runtime state).
- New `.7z`: **70 MiB** (was 79 MiB). The rest of the size is nested
  component `.7z` archives + `.exe`/`.dll` twins + `duktape.c` +
  ledgers + `.mp3` — real committed content, not touched.
- Added `.gitignore` rule for `x0.parent-level-dev-env-04.04_*.7z`.
- Still git-tracked and oversized (left alone, need a decision):
  `Mar$.$treetRace…/gl_cli_out.txt` (8.9 MB), the 9
  `blockchain.txt.pre-harness-run-*` July backups (~10 MB), a handful
  of tracked `rgb_frame_3d_overlay.raw`.

## CPU-throttling check (founder reported lag)

- **Not thermal** (40 / 53 °C), **not the house.** Load avg ~14 on 8
  cores = contention. Top consumers: **Chrome ~225% (22 procs)**,
  Firefox ~40%, Claude Code ~15%, opencode + its 400%+ `git` snapshot
  bursts on the 561 MB tree. **The whole house = ~6%.**
- The "7 idle khtpm procs" first flagged as strays are **not strays** —
  they're the livedesk pals (asa/ava/book-stack/cursword/m1_ninjadragon/
  m8_redhorned/self) + one placed tile, one renderer each, **0.7% CPU
  total, parked/idle.** No duplicate or orphaned house processes.
  Nothing to kill or prevent there.
- **Real cruft found + cleaned:** `#.desktop/entity_menu_frame_<pid>.txt`
  (760) + `entity_menu_history/<pid>.txt` (807) — per-PID context-menu
  scratch + input-relay files that never self-clean. Reaped the
  1560 whose PID is dead (kept 4 live). `#.desktop/` 19 MB → 6 MB.
- `tidy-runtime.sh` extended with that dead-PID reap so it self-
  maintains going forward.

## 14.biz/OUTLETS/CONSULTING wired in

- Founder added 2 consulting docs (X.com authority plan + the
  file-lineage-blockchain "Executive Blueprint" with tier pricing) —
  committed `73b824a2`.
- Added `CONSULTING/00-INDEX.md` (nav) + **`HOUSE-TECH-INTEGRATION.md`**
  — a grounded assessment (read the real `041.pal-chain⛓️` code:
  ~2,400 LOC, working plaintext SHA-256 PoW chain + `palnet_peer` P2P
  gossip + `chain_inbox_watcher` file-watcher daemon). Conclusion:
  ~70% of the blueprint already exists; gap to a deployable v1
  (`ANCHOR` tx type + `file_anchor_watcher` + `verify_file --receipt`
  + `validators.txt` permissioning + ed25519 signing + install script)
  is **~2–3 weeks** — independently matches the blueprint's own
  "2–4 weeks / zero disruption" claim. Honest caveats included (static
  peer set, no formal crypto audit, wallet IDs not yet real
  signatures).
- `build-biz-book.sh` now includes every `.md` in an outlet dir (not
  just `00-INDEX.md`); `BIZ-BOOK.html` regenerated → 27 sections.
- Line added to `14.biz/00-INDEX.md` outlet list.

## HQ "X.quit" logged the user out — FIXED

Live report: clicking **HQ → X.quit** ended the whole graphical
session (back to the login screen); user only wanted "close the house
tabs & entities."

- **Cause:** `khtpm_taskbar_manager_main.c`'s `hq_quit_requested`
  handler did `ktb_quit_and_save(s)` (correct: close entities + unlink
  pidfile) **and then** `kill(getppid(), SIGTERM); exit(0)`. This house
  launches the taskbar manager via `setsid`/`nohup`, so — verified
  live — its parent is **`systemd --user`** (pid 155057). SIGTERMing
  that ends the login session. The `ppid > 1` guard only spares init.
  Not in the legacy spec (`#.livedesk/livedesk-editor-design.md` line
  149 = "CLOSE relays + pid unlink" only).
- **Fix:** both `hq_quit_requested` blocks now do
  `ktb_quit_and_save(s); g_running = 0;` — byte-for-byte the same as
  the strip's own `[X]` button (`KSC_CLOSE_QUIT`). Closes everything,
  stops the strip, stays logged in. Logout stays the USER menu's own
  labelled action.
- **Proof:** source no longer contains `getppid`/`kill(ppid`; diff
  shows both blocks converted; live `PPid` of the running manager =
  `systemd --user` (what the old code would have killed). Binary
  rebuilt (`build_khtpm_strip.sh`).
- **Pitfall:** `03-pitfalls/HOUSE_CODE_PITFALLS.md` #15 — never
  `kill(getppid())` to tidy up; session lifecycle belongs to its own
  named action.
- **Note:** a running strip won't pick up the fix until relaunched
  (`$.restart`, or `run_khtpm_strip.sh new`). Until then X.quit still
  logs out.

## Grok handoff updated: finish 103.media-studio → x11-HQ

`08-roadmap/browser-prompting/platform-passes/13.grok-media-studio-
continuation-delegation.md` rewritten from "weighing the decision" to a
completion handoff.

- Confirmed state: **none of the 4 sub-apps converted** (img-editor,
  3d=blender-clone, daw, vid-edit — all still `button.sh` + `*_main.c`
  pile). The prior Grok attempt (`grok-frozen-s3.md`) froze mid-edit;
  nothing landed. tts = skip.
- Decision locked: **Option B, house-spec x11-HQ toy** per app (thin
  manager + `.xhtpm`/`.css` + shared `khtpm_core_render.+x` + `toy.pdl`,
  generic nav, file-backed state). Structural template = `&.hq-apps/
  co-lab-hai/`. Migration, not redesign — keep every `HOW2_*.md`
  feature.
- Order (owner's call): **img + 3d=blender-clone MERGED into one 2D/3D
  app with piececraft-hq camera controls** (reuse Interact-Mode relay +
  `bv_menu_input` camera dispatch, `PLAN-pchq-interact-camera-pov.md`)
  → then daw → then vid-edit.
- Toys menu = `toy.pdl` opt-in, scanned one level deep under
  `house_root/` `@.apps/` `&.widgits/`; recommend converted apps move
  to `@.apps/media-<name>/`.
- Gates spelled out: CENTROID_GOLD_STD, pitfalls #11/#14/#15, relay
  testing, no `g_is_*` / no bespoke layout branch, `kh_plat.h` for the
  manager. Framed as Windows-handoff prep (house-spec rides the shared
  renderer seam).

## Pre-Windows work still queued (context)

`08-roadmap/design-docs/CROSS-PLATFORM-SEAM-AND-SHARED-INFRA.md`:
Part 1 `kh_plat` shipped (elements manager migrated); Part 2 renderer
platform seam = **guidance-only for the porter, not pre-built**; Part 3
generic manager runtime = idea. Windows/Mac delegation itself:
`browser-prompting/platform-passes/12.grok-windows-mac-compat-delegation.md`.

## Open / next

- Everything still open in `2026-09-05/` and `2026-09-06/` 2do.
- Back to gameplay work.

## (2026-09-08) palettes: tilesets blank in picker — FIXED + grok note

- **Cause:** `sprite=` (and `src=`) attribute values were left
  XML-escaped. `kh_substitute_vars()` escapes every `${var}` spliced
  into a quoted attr (`&`→`&amp;`, correct — pitfall #13 fix), but
  `apply_attr()` only `decode_entities()` a hand-maintained list
  (label/onclick/action/…) — `sprite`/`src` weren't on it. House dirs
  are `&.widgits/`, so `${t.sprite}` → `e->sprite` = `.../&amp;.widgits/…`
  → path not found → every blit silently blank.
- **Fix `d71f73e9`:** decode_entities() sprite= and src=. Verified:
  rmmv palette blits all 32 Dungeon_a2 tiles; user confirms "they are
  back". Strip relaunched (`run_khtpm_strip.sh new`).
- **Pitfall #16** added (escape ↔ decode must stay in lockstep; better
  long-term = decode once generically for all attrs).
- **Grok note** appended to `08-roadmap/design-docs/palettes-handoff-
  2026-08-24.md`: the non-tileset RMMV categories (characters, faces,
  sv_actors, sv_enemies, …) still render with one uniform grid, which
  is only right for B/C/D/E tiles. Task: research real per-category
  sprite geometry, fix the crop/view per category — LOW priority,
  skip if it's more than ~a day or needs RMMV-runtime knowledge.
