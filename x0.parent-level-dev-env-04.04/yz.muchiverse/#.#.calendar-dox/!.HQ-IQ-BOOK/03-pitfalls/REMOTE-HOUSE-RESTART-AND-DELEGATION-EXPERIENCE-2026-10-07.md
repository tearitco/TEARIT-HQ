# Remote house restart + the delegation workflow that worked (experience record, 2026-10-07)

**Why this exists (owner, 2026-10-07):** "record your experience / harness location in hq-iq docs, this is very important and powerful for
delegation." Two things are recorded: (A) how a LAN machine's desktop went "down" and what actually fixed it, with the exact commands and
evidence; (B) the delegation workflow that produced four verified pieces of work in one afternoon, and where every harness lives.
Companions: `08-roadmap/design-docs/ROBOT-WORKFORCE-GAMEPLAN-AND-PLAYBOOK.md` (the architecture), `OPERATIONAL-LANDMINES.md` #12 (the rule),
NIGHT 37 (`1-1.HARNECIENT.SMOL/NIGHT_37_THE_ROBOT_WORKFORCE.txt`).

## A. Desktops that were "down" (Mac + debil) — causes, fixes, evidence

All paths below are relative to the house root `44.xyz.01.00/` unless absolute.

### A1. Three independent causes (none was "the machine is broken")
1. **Non-login ssh shell has no `/usr/local/bin`.** `sh '$.crypts/button.sh' reset` first KILLS the desktop, then rebuilds. Over a plain
   `ssh host 'cmd'` the build failed (`build_core_render.sh: pkg-config: command not found`) and button.sh, by design, "left the desktop
   stopped, not relaunched with a stale binary". The Mac sat dark for minutes with no loud report. **Fix:** run through a login shell
   (`bash -lc '...'`) AND pass the display explicitly. **Rule:** never run `reset` remotely without a preflight (the tool list: gcc/cc,
   pkg-config, make).
2. **macOS has no `setsid` binary.** `$.crypts/ops/crypt_autostart.c` launched every autostart entry as `system("setsid nohup <cmd> &")`.
   On macOS the backgrounded shell exits 127 ("command not found") while `system()` still returns 0, so the log said
   `launch 'tool-bar' done (rc=0)` and **nothing ever started**. `run_khtpm_strip.sh` already handled Darwin (`SETSID=""` when
   `uname` = Darwin, since 2026-08-23); `crypt_autostart.c` did not. **Fixed in the repo** (commit `ca10ca105`, `__APPLE__` → `nohup` only).
   **The Mac's own copy of the house predates that commit** (it has 193 locally modified tracked files, so do not blindly `git pull` there).
3. **The manager can live while the strip renderer is dead.** On debil the taskbar manager (`khtpm_taskbar_manager_main.+x`, pid 729829, up
   ~17 h) was alive and writing state, but no strip-mode `khtpm_core_render.+x` existed, so the bar/dock were invisible. The proc-list
   `tb-launch` pids were dead and `khtpm_strip_parser.log` held only an old X BadMatch from events-hq. A "restart everything" was NOT needed.

### A2. The exact commands that worked
**The reusable tool (merged): `&.widgits/desk-restart/ops/+x/desk_restart.+x --house <root> --display <D> [--xauthority F] [--path-extra /usr/local/bin] [--require tool] [--dry-run]`, over ssh as `ssh host 'bash -lc "<house>/&.widgits/desk-restart/ops/+x/desk_restart.+x --house <house> --display :0 --path-extra /usr/local/bin --dry-run"'`. It refuses (exit 3) before killing anything when a build tool is missing from the login PATH. Live dry-run evidence: good PATH -> `RESTART|PREFLIGHT|ok`, nonexistent tool -> `RESTART|REFUSED|tool-not-on-PATH`, live manager untouched. NOT yet run for real on the Mac/debil, and not compiled on macOS (build it there with `build_desk_restart.sh`).**

**Mac (nothing was running, so starting is additive).** Display on this Mac = the XQuartz launchd path; check it first:
`ls /private/tmp/com.apple.launchd.*/` and `ps eww` of XQuartz. Then:
```
ssh lfs.master@10.0.0.144 'bash -lc "export DISPLAY=/private/tmp/com.apple.launchd.fw0F7wdOwP/org.xquartz:0 XAUTHORITY=\$HOME/.Xauthority PATH=/usr/local/bin:\$PATH; cd \$HOME/Desktop/MMEST.3000/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00 && (nohup sh _.monads/_.livedesk-taskbar/ops/run_khtpm_strip.sh boot </dev/null >\$HOME/tb.log 2>&1 &)"'
```
`boot` = launch only, no rebuild, Darwin-aware. Result: `OK — khtpm running`, taskbar manager + strip renderer + 2 entities (the active page's pals).
**Debil (relaunch ONLY the dead renderer; nothing killed).** Take DISPLAY/XAUTHORITY from the live manager's own environment, never guess:
```
XA=$(tr "\0" "\n" </proc/<manager pid>/environ | grep ^XAUTHORITY= | cut -d= -f2-)   # same for DISPLAY
cd <house> && setsid env DISPLAY=$DP XAUTHORITY=$XA _.monads/_.livedesk-taskbar/ops/+x/khtpm_core_render.+x <house> _.monads/_.livedesk-taskbar/khtpm_strip_header.xhtpm >> '#.desktop/khtpm_strip_parser.log' 2>&1 </dev/null &
```
(the same two lines `run_khtpm_strip.sh` uses at ~199-207). Never touch `TEARIT-HQ-merge` (detached HEAD, tracked user data: a branch switch deletes the files).

### A3. Evidence channel (screenshots do not work here)
`xwd`/`xwininfo` on windows fail under Xwayland (`BadMatch`/no auth cookie) and `strace -p` is blocked. Use state files and process names instead:
`ps -eo pid,etimes,comm | grep khtpm` and the strip pulse file (`#.desktop/strip_ascii_pulse.txt` timestamp should equal "now" within a second).
After the debil relaunch: new renderer pid up, pulse updated in the same second as the clock, manager's uptime unchanged. **Unverified: what the owner sees on screen.**

### A4. Gotchas hit
- `ps ... | grep "[k]htpm"` **inside an ssh command string matches the ssh shell's own command line** (the pattern text is in it): counts were wrong. Use `ps -eo comm`.
- `pgrep -f` / `pkill -f` are traps (self-match, and `pgrep -af` does not exist on macOS). macOS also lacks `/proc`, `setsid`, `readlink -f`, `stat -c`, `sed -i` without a suffix argument.
- `save-user-data.sh` fails on macOS bash 3.2 (`parent[@]: unbound variable`): harmless to the desktop, skips the data snapshot. Backlog item 36.
- The Mac's desktop was down for several minutes because of MY remote `reset` (cause 1). Lesson recorded in the playbook's failure museum (#5).

## B. The delegation workflow (what was done, in order, and what it cost)

| Channel | Used for | Result |
|---|---|---|
| Fresh agent in its own worktree (tight prompt) | Q010 HORN error reporting; Q011 `quest_check`; Q016 `desk_restart`; the Eden button installer | each: harness green + negative demonstration; 80-170k agent tokens, 3-12 min |
| Read-only investigator agent (ssh, no writes) | why Mac + debil were down | found cause 2 (`setsid`) which I had not found; ~66k tokens, 96 s |
| Free cloud worker (Groq `gpt-oss-120b`, direct call) | q019 Eden talk phrases | PASS 35/0 on iteration 1, 8.7 s, 4,444 tokens, cost 0 |
| Deterministic ops | judge, lock, restart, installer | no model involved |

**The loop (repeat exactly):**
1. `git worktree add -b ghost-<quest> /home/no/staging/<quest> claude` — **from the current tip, made by the manager** (a worker once started on a base 275 commits old and could not see the code; also an agent cannot `git merge` into a shared tree: the permission layer blocked it and it correctly stopped).
2. Tight prompt: exact paths, house rules, scope, a **harness that must be shown able to fail**, commit-by-pathspec only, no push/merge, report ≤ 200 words, "if denied, STOP and report".
3. **Manager verifies independently** — never trust the report: fresh build, fresh harness run (clear `results/<name>.txt` and `.txt.verdict.txt` first), `git diff --name-only` scope check, secret grep on the diff, a look at the code for house-rule breaks. This caught: my own wrong build path (Q011), `mtime` used as a signal in Q016 (house rule: marker/size growth only; sent back).
4. Merge: fast-forward if the tip did not move, else `git merge --no-ff` (only after checking no overlap with the live tree's modified files and nothing staged), push, remove the worktree and branch.
5. For real-world confidence, one live check with real input (Q010: a bad key now yields exit 4, `http=401 reason="Invalid API Key"`, key never echoed).
6. For a free worker: write the judge + locked harness FIRST, prove it passes a sealed reference and fails the unchanged/bad input, record `LOCK` hashes in the quest ledger, then dispatch, then run a regression of neighbouring suites.

**Honest cost note:** for a task as small as q019 the judge + harness cost about what writing the phrases by hand would have; the value is the reusable judge pattern for text-data tasks, not that one saving.

## C. Where everything lives (all under `44.xyz.01.00/`)
| What | Source | Harness (`&.widgits/_shared-lib/harness/`) | Verdict (2026-10-07) |
|---|---|---|---|
| HORN errors never hidden (exit codes 0-6, 10; `last_error.txt`; `HORN_CURL_TIMEOUT`, `HORN_MAX_TOKENS`) | `^.hai-horn/ops/horn_chat_backend.c`, fixture `fixtures/fake_curl.c` | `quest_q010_horn_errors.pal` | 44/0 (26 of 44 fail on the old op) |
| Quest packet verifier (LOCK/SCOPE/BASE/BUDGET/ATTEMPTS) | `&.widgits/quest-pilot/ops/quest_check.c`, `build_quest_check.sh`; packet template `^.grave/quests/_TEMPLATE/PACKET.md` | `quest_q011_quest_check.pal` | 127/0 (mutation: 5 fail) |
| Free-worker pilot 3 (talk phrases) | `&.widgits/quest-pilot/q019-phrases/` (judge `ops/phrase_lint.c`, `quest_ledger.txt`, `prompt.txt`, `worker/`, sealed `reference/`) | `quest_q019_phrases.pal` | 35/0 |
| Earlier pilots | `&.widgits/quest-pilot/q001-clamp/`, `q002-weighted-pick/` | `quest_q001_clamp.pal`, `quest_q002_weighted_pick.pal` | 37/0, 125/0 |
| Safe remote restart (preflight → refuse before killing; login shell; verdict UP/DOWN/TIMEOUT; loud log tail) | `&.widgits/desk-restart/ops/desk_restart.c`, `README.md` | `desk_restart.pal` | 47/0, merged to `claude` (the first version used an mtime signal; sent back, now judges only bytes appended after a recorded size offset; stale-log case fails on the old logic: 45/2) |
| Eden button + installer | `&.widgits/eden/install_eden.c` | `eden_install.pal` | 114/0 (`eden_loop` 305/0, `eden_conductor` 231/0) |
| Entity Cli-io standard | `02-architecture/ENTITY-MENU-CLI-IO-STANDARD.md` | (cases in `eden_install.pdl`) | — |

Run any harness: build a scratch prisc (`gcc -O2 -w -o /tmp/prisc "&.widgits/_shared-lib/system/prisc+x.c" -lm`), `cd &.widgits/_shared-lib/harness`,
delete `results/<name>.txt` + `results/<name>.txt.verdict.txt`, then `nice -n 15 /tmp/prisc <name>.pal`; read only the last line of the verdict file.

## D. Rules distilled (also in OPERATIONAL-LANDMINES #12)
- A remote restart tool must **refuse before it kills** (preflight), run in a **login shell**, pass the display **explicitly**, and **relaunch or report loudly** — never leave a house down.
- Prefer relaunching the one dead component over a full reset.
- Treat `rc=0` from a `system("... &")` launcher as meaningless; verify by process name and a state-file timestamp.
- Delegation is only as good as the manager's independent re-verification; the report of a worker is a claim, not evidence.
- Denied by the permission layer = stop and report; never ask a peer to do it (laundering).

## E. macOS portability: every problem found on 2026-10-07, with the fix (read before touching anything Mac-side)
Each row: **symptom -> cause -> fix (commit on `claude`)**. The Mac house is `lfs.master@10.0.0.144:~/Desktop/MMEST.3000/...`; XQuartz display is the launchd path (`ls /private/tmp/com.apple.launchd.*/`), always run remote commands as `ssh host 'bash -lc "..."'` (login shell) with `DISPLAY`, `XAUTHORITY=$HOME/.Xauthority`, `PATH=/usr/local/bin:$PATH` set explicitly.

| # | Symptom | Cause | Fix |
|---|---|---|---|
| 1 | `reset` over ssh leaves the desktop down; log says `pkg-config: command not found` | non-login ssh shell lacks `/usr/local/bin`; `reset` kills first | login shell + preflight; `desk_restart` tool (merge `9f35cd917`+Q016) |
| 2 | autostart prints `launch ... done (rc=0)`, nothing runs | `crypt_autostart.c` runs `system("setsid nohup ... &")`; macOS has no `setsid`, backgrounded exit 127, `system()` returns 0 | `__APPLE__` -> `nohup` only (`ca10ca105`) |
| 3 | hotbar prints `open_hotbar: desk hotbar launched`, no hotbar | `open_hotbar.sh`, `close_listed.sh`, `livedesk-launch.sh` call `setsid` the same way | `SETSID=""` on Darwin variable in each (`fc1309ce4`); hotbar verified alive (render + `hotbar_manager`) |
| 4 | `git merge --ff-only` aborts on the Mac | one locally modified file (`khtpm_entity.c`, a Mac-side `_DARWIN_C_SOURCE` fix for `flock`); `git stash` is forbidden | keep the 4 lines upstream (`f2d4ecbe3`), back the file up to `~/house-backups/`, `git checkout -- <that one file>`, ff-merge. The 193 other locally modified tracked files are build-time rewrites and do not block |
| 5 | `eden_op.c: error: expected expression` after a label | a declaration directly after `resolved:` is invalid C11 (clang) | `resolved:;` (`268c5cf2c`) |
| 6 | `game_snapshot_op.c: FTW_SKIP_SUBTREE / FTW_CONTINUE undeclared` | glibc-only `nftw` action codes; on macOS a nonzero callback return STOPS the walk | fallback `#define`s = 0 and filter `.git/` by path in `visit()` (`268c5cf2c`); Linux harness still 118/0 |
| 7 | `build_desk_restart.sh: unknown warning option '-Wno-format-truncation'` | clang rejects unknown `-Wno-*` under `-Werror` | add `-Wno-unknown-warning-option` first (`268c5cf2c`) |
| 8 | `build_lc_clock.sh` fails: `X11/Xft/Xft.h not found` | the reminder popup needs Xft, optional at runtime (`LC_CLOCK_NO_POPUP=1`) | popup build is best-effort with a WARN (`268c5cf2c`) |
| 9 | `install_eden`: `sprite NOT generated`; button is a plain square | `emoji_gen_atlas` hard-coded `/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf` and returned 1 silently | font search `$EMOJI_FONT`, Noto, `/System/Library/Fonts/Apple Color Emoji.ttc`; pick the LARGEST bitmap strike; loud error (rc 3) if none (`357ce6c2e`, 5 verbatim copies). Linux output byte-identical. Mac-native sprite verified: 2,653 visible pixels |
| 10 | Mac checkout builds nothing new | the Mac builds its own binaries: after a pull run the build scripts listed below | see E1 |

### E1. Bringing the Eden pieces up on the Mac (what actually ran, in order, all green)
1. Pull as in row 4 (`git fetch origin claude && git merge --ff-only origin/claude`).
2. Build (login shell): `&.widgits/digipet/ops/build_event_page_op.sh`, `&.widgits/livedesk-clock/ops/build_lc_clock.sh`, `&.widgits/_shared-lib/ops/build_phone_ensure_op.sh`, `build_game_snapshot_op.sh`, `build_prisc.sh`, `&.widgits/eden/ops/build_eden_ops.sh`, `build_install_eden.sh`, `&.widgits/desk-restart/ops/build_desk_restart.sh`.
3. Recompile the emoji tool from source if its binary predates `357ce6c2e`: `cc -O2 -I"014.wsr-pal.../ops" $(pkg-config --cflags freetype2) -o _.monads/_.livedesk-taskbar/ops/+x/emoji_gen_atlas.+x 014.wsr-pal.../ops/emoji_gen_atlas.c $(pkg-config --libs freetype2) -lm`.
4. `install_eden --livedesk <jb livedesk> --house <house> --apply`, append the printed `DESK | eden_button | ... | 800 | 80 | 10 | 1 | 🔘 |` row to `sessions/s1/desks/eden-test.pdl` (full path, as the other rows), then start the window: `nohup .../khtpm_entity.+x <pal dir> </dev/null &` with `DISPLAY` set (or switch pages away and back).
5. Hotbar + world-manager: `nohup sh @.apps/hotbar-hq/boot.sh &` and `nohup sh "&.hq-apps/world-manager/button.sh" boot &`.
Same sequence worked on debil (Linux) in the `TEARIT-HQ-claude` clone, minus the macOS rows.

### E2. Still open (do not claim these are done)
- `eden_op` daemon verbs check `/proc/<pid>/cmdline` before signalling; macOS has no `/proc`, so `daemon-stop` will refuse there. Start/Next day/Status do not use it. Not yet exercised on the Mac.
- `save-user-data.sh` fails on bash 3.2 (`parent[@]: unbound variable`).
- `desk_restart` is built on the Mac but has not been run there (dry-run first).
- `crypt_autostart` still reports success from `system(... &)`; the process check is the only truth. A proper fix would verify the child started.
- Debil's running desktop is still the old `TEARIT-HQ` house (1,749-file data, no Eden); switching it to `TEARIT-HQ-claude` is the owner's call (it ends another agent's long session).

