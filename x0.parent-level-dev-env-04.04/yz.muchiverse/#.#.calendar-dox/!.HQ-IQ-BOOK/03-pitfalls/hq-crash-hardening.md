# Crash hardening of the game daemons (2026-10-17, tag "hard")

Owner: "defend our board code better and avoid crashes". Scope: the C game daemons `@.apps/df-hq/ops/df_hq.c`, `cdda-hq/ops/cdda_hq.c`,
`mcl-hq/ops/mcl_hq.c`, and the five carbon copies that share the same helper code (`rpg-pet`, `civ-rpgce`, `dsr-rpg`, `gta-hq`,
`tcg-rpgce`; they got the same helper fixes and were compiled, but they have no fuzz harness of their own). `_shared-lib/game_events.c`
(the S1 ledger) was not touched; its one open risk is listed in section 2g.

Every defect below was **reproduced on the pre-hardening source first** (snapshots in `XO/hard/orig/`, probe scripts `XO/hard/probe.sh`,
`XO/test_fuzz_state.sh` with `FUZZ_ORIG=1`) and then shown fixed on the new source. Where a suspected defect turned out harmless it says so.

Run it: `XO/test_fuzz_state.sh` (all three games, measured 5 min 31 s wall / 5.2 CPU-minutes on a machine at load average 8; the other agents' work shares the box) or `XO/test_fuzz_state.sh cdda-hq` (one game).
Old-source baseline: `FUZZ_ORIG=1 XO/test_fuzz_state.sh cdda-hq`.

## 1. The shape of the problem

A game daemon is a long-lived process that re-reads dozens of small text files on every tick, plus a family of one-shot verb processes
(`cdda_hq.+x bag`, `... step 0 1`) that read and write the same files at the same moment. Every one of those files can be truncated by a
crash, edited by hand, overwritten by an older version, or written by a different program (the clock, the events-hq pages, the board engine).
A daemon that trusts a file crashes on the first bad byte; a daemon that writes a file in place leaves the next reader a half file. The rule
that fixes both classes is the same: **a file is input from the outside world and every output is replaced whole**.

## 2. Failure classes found, with the real finding for each

### 2a. Array index and arithmetic from file data (reproduced; UBSAN reports it, ASAN-less builds read garbage or fault)
- **`pet_<id>.txt` / `trainer.txt` (`load_ent`)** took `x y dir steps tx ty` with `atoi` and no range check. Findings on the old source:
  `steps=-5` indexed `cyc[steps % 4]` at -1 (`cdda_hq.c:482`, "index -1 out of bounds for type int[4]", also -3 for `steps=-7`);
  `tx=2147483648999` indexed `dist[ty][tx]` in `bfs_first` (index 999 into an `int[40]`, `cdda_hq.c:741`); `dir=99` multiplied into the sprite
  sheet row. Fix: `load_ent` forces x, y into the room's `cols x rows`, `dir` into 0..3, `steps` into 0..1e9 and drops an out-of-room walk target.
- **`help_step.txt`** holding `-3` indexed the hint table `H[-3]` (`atoi(...) % 8` is negative for a negative file). Fix: `((v % n) + n) % n`.
- **`zed.txt`** (cdda): same clamp as the pets. **`party.pdl` / the DB `character` field**: the sheet cell index is clamped to 0..7 (it is multiplied
  into pixel offsets). **screen_state.pdl tint** values are clamped to +-255 (they were multiplied into every pixel).
- Kept as they were because they are already safe: `replay()` validates every `item_idx` / `room_idx`; `nroom<8`, `ndoor<24`, `nscen<24`, `ncat<MAXI`,
  `nrec<MAXREC`, `nparty<NP`, `npl<MAXP`, `nwd<64` bound every table; `over` rooms are range-checked (`cols 4..MAXC`, `tpx 8..48`).
- **Sizes from `rooms.pdl` / `catalog.pdl` as loop bounds (fuzz finding).** A SCENERY row with width `2147483648999` (atoi keeps the low 32 bits: 999) made `blit()` run a 999 x 999 tile
  nested loop in which almost every iteration is clipped away: 3-5 s of pure CPU per repaint, which the harness saw as a 5 s timeout (`buy` after the corruption). Fix: scenery
  w/h clamp to 1..MAXC and cell/offset to 0..255, item w/h to 1..8, price to 0..1e6, door cells to the grid, room tile cell to 0..255, and `blit` / `blit_fit` return at once for a
  destination larger than 4 screens.
- **An empty or garbage `rooms.pdl`** (nroom == 0) left `rooms[0]` zeroed (`tpx == 0`). It did *not* crash the old source (the zero only multiplies), but one
  divide would have; the loader now installs one default 13x8 room so no code ever sees a zero-size room.

### 2b. Non-atomic or unchecked writes (reproduced with a full disk)
- **`write_if_changed`** (the one helper behind ui.txt, scene.raw, every pet file, flags) did `fwrite; fclose; rename` and checked none of them. With the disk full
  (`ulimit -f 100`, SIGXFSZ ignored; ENOSPC behaves the same) the 1,135,200-byte `scene.raw` was replaced by a 102,400-byte truncated file. Fix: the byte count and
  `fclose` are checked, `rename` happens only on success, otherwise the tmp file is removed and the old good file stays. Returns 0 so callers can see it failed.
- **In-place rewrites** that a kill (or a concurrent verb) could expose half-written: `chat_trim` (shared `.tmp` name, no check), `note_set`, `help_step.txt`,
  `copy_file` (Save / Load / New game copy state files **onto** the live files with `fopen(to,"wb")`), and the whole 3D export (`map.txt`, `extrusion.pdl`,
  `cells.rgba`, `cells.txt`, `game.pdl`, `map.png`, `cells.png`, gta's `scene.png`) which a **different process, the board renderer,** reads. All are now
  `<file>.tmp.<pid>` + checked close + `rename`.
- Left as small single appends (atomic by themselves, listed in the table): the relay lines written into the engine session (`126\n`, `KEY_PRESSED: 27\n`),
  the pc-hq `inbox.txt` line, `ctl_trace.txt` (now capped at 1 MB), and the empty-file `fopen(p,"a")` that only creates `interact_relay.txt` / `history.txt`.

### 2c. Torn appends
`append_line` was `fopen("a"); fprintf; fclose`. stdio may split a long line into several `write()` calls, and two processes (daemon and a verb) append to
`events.txt` / `chat.txt` / `house.txt` at once. Now: `open(O_WRONLY|O_APPEND|O_CREAT)` and **one `write()` of the whole line** (the same rule `ge_append_raw` in
`game_events.c` already followed). A `kill -9` therefore cannot leave a torn row; the kill test checks every ledger row after each of 30 kills.

### 2d. `ui.txt` not a complete key=value file
- `out_all` built ui.txt with `o += snprintf(u+o, sizeof u - o, ...)` into a 16 KB buffer. `snprintf` returns the **would-be** length, so once `o` passed the buffer
  `sizeof u - o` wrapped to a huge `size_t` and the next write was an overflow. 200 placed items (MAXP) is already about 14 KB of `placed_N_label` rows, so this was
  one more purchase away from real data. Fix: 64 KB buffer and a `W()` macro that clamps `o` after every call.
- A `note.txt` or chat line holding a newline / CR / control byte injected extra lines (`foo=bar`) or broke the file (reproduced: 2 non-key=value lines, 1 injected key).
  Fix: the note is cleaned at the source and a **final pass keeps only `key=value` lines** (`key` = letters, digits, `_`), turns control bytes in a value into spaces, and drops a
  cut-off last line. The renderer can therefore never read a half-parsed ui file, whatever the state files held.

### 2e. Children that can hang the daemon, and a clock that spins
- `lc_run` (every clock call), `run_quiet(wait=1)` and the `fork` helpers did `waitpid(pid, &st, 0)` with no limit; `run_event_x` and `board_interact` used `system()`.
  A hung `lc_clock`, bash or event page froze the verb or the whole 100 ms loop. Fix: `reap_timed(pid, secs)` polls `waitpid(WNOHANG)` with a deadline, then SIGKILLs the
  process (and its group for `sh_timed`) and reaps it. Deadlines: clock calls 3 s, event pages 20 s, board action 10 s. After one clock timeout `lc_run` refuses for 20 s
  (a circuit breaker) so a stuck clock costs one verb 3 s, not 3 s per call.
- **Real finding:** `time.pdl` with `TURN | advance | 2147483648999m` made `lc_clock advance` spin for 10+ s (11.0 s wall, 9 s of kernel time) on every roguelike turn; the old
  source waited for it forever. Amounts handed to the clock (`TURN` row, `clock advance`, `clock rate`) now pass `lc_amount_ok`: 1-4 digits plus a unit word, or a bare word.
  After the fix the same verb takes 0.16 s.
- `speak()` starts a TTS / `paplay` chain by double fork; the harness puts no-op stubs for `python3`, `paplay`, `espeak` first in PATH and sets `flag_voice=0`, so a fuzz run is
  silent and light (before that, a pet greeting inside a fuzz verb ran the sound chain and slowed everything).

### 2f. Signals and process hygiene
- **SIGPIPE:** default action killed a verb whose stdout closed early (`bag | head -c0` gave rc 141 on the old source). Now `signal(SIGPIPE, SIG_IGN)` in `main`; children get
  `SIG_DFL` back before `exec` so scripts they run still behave normally.
- **SIGCHLD:** fire-and-forget children (`run_quiet(wait=0)`) stayed zombies for the life of the daemon. The loop now does `while (waitpid(-1, NULL, WNOHANG) > 0) {}` every tick.
- SIGTERM / SIGINT / SIGHUP already set `quit_flag` (async-signal-safe `volatile sig_atomic_t`); the loop sleeps in `usleep(100000)` so the flag is seen within 100 ms, and
  `clock_stop()` at the end takes about 2 s (one `usleep(1000000)` plus two timed `lc_run` calls).
- Failed `fork`: every site returns -1 / 0 and carries on. `EMFILE` (`ulimit -n 10`) and `ENOMEM` (`ulimit -v 90000`) are in the harness: status, save, export3d, newgame exit without a signal.
  Not claimed beyond those verbs.
- A leftover from a `kill -9` of the daemon: its `lc_clock` child is orphaned (one per game folder, reused by the next start, known and bounded: `test_clockstop.sh`) and a
  `*.tmp.<pid>` file may remain if the kill landed between `fopen` and `rename`. The kill test counts them (0 in practice); they are never read.

### 2g. Unbounded growth and loops
- Loops audited (all three games): the resident loop is `while (!quit_flag) { usleep(100000); ... }` (no busy loop); every `for` over files / tables is bounded by a fixed table size
  (`bfs_first` queue is `MAXR*MAXC` and each cell enters once; `free_near` ring search is `COLS+ROWS`; `first_free` is rows x cols; `pick_target` tries 40 cells; `help_pages`
  and every `fgets` loop end at EOF; `poll_keys` reads at most 32 codes per call and advances the offset). The only waits are the `reap_timed` polls (5 ms sleeps, deadline),
  the double-fork `waitpid` in `speak` (the first child exits at once), and `clock_stop`'s one `usleep(1000000)`.
- `chat.txt` was already trimmed past 24 KB; `append_line` now drops a non-ledger row once the file is past **4 MB** (events.txt STEP / DOOR / DAY noise rows are about 45 bytes per
  pet step and nothing reads them but a stats script); `house.txt` (the purchase ledger) is never dropped. `ctl_trace.txt` stops at 1 MB. `relations.txt` is covered by the same 4 MB cap.
- **Open, not fixed here (not my file):** `ge_scan()` in `game_events.c` matches every EVENT row against the list of rows that already have an EVRES, a nested loop. It is linear in
  practice (hundreds of rows) and quadratic at the 32 MB read cap. The ledger cannot be trimmed in place because an EVRES row names the **byte offset** of its EVENT row. If the
  ledger ever gets long, the fix is a sorted array + binary search, or an offset-keyed hash, inside `ge_scan`. Also: an EVENT row cut off without its newline glues to the next appended row
  and is REJECTed; harmless, documented.

### 2h. Things that were fine (so nobody "fixes" them)
- NUL bytes in a state file: all readers use C-string functions on a NUL-terminated buffer (`read_file`), so a NUL just ends the data early.
- CRLF: `split_bar` / `trim` strip `\r`; `atoi` stops at it.
- A 200 KB line with no newline: `read_file` caps at the buffer, `fgets` chunks are each validated as a row (a chunk is never trusted), `read_tail` takes only the last bytes.
- Garbage `catalog.pdl` / `recipes.pdl` rows: dropped by the field-count and range checks.

### 2i. The harness itself was wrong once
The first version of `test_fuzz_state.sh` passed everything, including against the old source, because `mut()` took `(file, kind)` where the callers passed `(kind, file)` so **no file was ever
corrupted**. It was found only because the old-source baseline (which must fail) did not. A fuzz harness that has never failed has proven nothing: section 5 keeps two negative controls and the
old-source baseline as permanent checks.

## 3. Rules for every new game daemon

1. **Every file that is replaced is replaced whole:** write `<file>.tmp.<pid>`, check `fwrite` and `fclose`, `rename`; on failure remove the tmp and keep the old file. Use `write_if_changed`
   (it also skips an identical rewrite, which would wake every watcher). Never `fopen(path, "w")` on a file anyone else reads.
2. **Every append is one `write()` of the whole line on an `O_APPEND` fd** (`append_line`, `ge_append_raw`). Rows are `\n`-terminated and carry no `|`, CR or LF inside a field.
3. **Reads are bounded:** a fixed buffer plus `fread`/`fgets` with its size, a NUL terminator, and nothing that assumes the last line ended in a newline. Never `fscanf("%s")`, `strcpy`, `sprintf`.
4. **Never trust a number from a file:** after `atoi` clamp it into the range the code indexes with (room dimensions, table sizes, 0..3 for a facing). Never use a file value as a divisor, a loop bound,
   a shift or an array index unchecked. Never trust a count in the file; count what you parsed.
5. **A bad row is skipped, a bad file is ignored, a missing file is the default.** Never exit, never spin on it.
6. **Capped output:** `snprintf` returns the would-be length: clamp the running offset after every call (the `W()` macro). A generated key=value file ends with a pass that keeps only `key=value` lines.
7. **Children:** `fork`+`exec` and wait with a deadline (`reap_timed`); never `system()`, `popen`+`fgets` to EOF, or `waitpid(pid,&st,0)`. A failed fork is a skipped feature. Values passed to a child are validated first
   (`lc_amount_ok`). Set `SIG_DFL` for SIGPIPE in the child before `exec`.
8. **Signals:** `SIGTERM`/`SIGINT`/`SIGHUP` set a `volatile sig_atomic_t`; `SIGPIPE` is ignored; reap with `waitpid(-1, WNOHANG)` every tick.
9. **Loops:** the resident loop sleeps (`usleep`) every pass; every other loop is bounded by a table size or a deadline; no loop waits on a file or process without both a deadline and a sleep.
10. **Growth:** every append-only file has either a trim (chat), a cap (`append_line` 4 MB, ctl_trace 1 MB), or a documented reason it is the ledger.
11. **One resident daemon per game folder** (`daemon.lock`, `flock`, close-on-exec), so two daemons never write the same files.
12. **Prove it:** add the new files to `XO/test_fuzz_state.sh` (section 5) and look at the ASAN run once.

## 4. Audit table (all three games; "g" means only the named game)

Writers: `wic` = `write_if_changed` (tmp.pid + checked close + rename), `app` = `append_line` (one O_APPEND write, 4 MB cap), `raw` = `ge_append_raw` (one O_APPEND write),
`ext` = written by another program. "Tolerates" says what a truncated / garbage / NUL / CRLF / huge-number file does.

| path (under `state/` unless noted) | writer | atomic? | reader | tolerates truncation / garbage? |
|---|---|---|---|---|
| `house.txt` (purchase ledger, never capped) | `app` (buy / use / place / remove) | yes (single write) | `replay()` fgets 300, `split_bar`, replay only when size changed | yes: unknown rows skipped, item / room looked up, `npl<MAXP`; long line = chunks validated one by one |
| `events.txt` (STEP / DOOR / DAY / DIG / S1 rows) | `app` (noise rows, capped 4 MB); `raw` (EVENT / EVRES / REJECT) | yes | `ge_scan` (slurp <= 32 MB, memchr rows), renderer tabs | yes: bad row -> REJECT row once; over 32 MB -> "ledger unreadable", no crash; unterminated tail ignored |
| `pet_<id>.txt`, `trainer.txt` | `wic` (pet_save) | yes | `load_ent` read 300, strtok, **clamped to the room** | yes (clamp added; found with UBSAN) |
| `active.txt` | `wic` | yes | `pet_load` read 40 | yes (unknown id -> pet 0) |
| `flag_*.txt` | `wic` (set_flag) | yes | `flag_str_raw` read 64, first char | yes |
| `world/switches.txt` | `wic` (sw_set) | yes | `sw_get` read 1024, strtok | yes |
| `world/screen_state.pdl` | `ext` (events-hq tint_screen) | n/a | `read_tone` read 300, sscanf 4 ints, **clamped +-255** | yes |
| `chat.txt` | `app`; `chat_trim` = `wic` of the last 8 KB; `clearchat` = rename | yes | `read_tail` 4000 bytes, last 10 lines, ui pass drops control bytes | yes |
| `relations.txt` (MEET rows) | `app` | yes | `last_meet`, `friend_line`: fgets 200, indexes bounds-checked (`y<NP`) | yes |
| `note.txt` | `wic` (note_set; was in place) | yes | `out_all` read 200, control bytes -> space | yes (found: newline injection into ui.txt) |
| `help_step.txt` | `wic` (was in place) | yes | `atoi` then **positive modulo** | yes (found: negative index) |
| `daylight.txt` | `wic` | yes | `strstr` for night / dusk / dawn | yes |
| `clock_day0.txt`, `clock_lastday.txt`, `clock_ui.txt` | `wic` | yes | read 24-32 bytes, `atoll` | yes |
| `clock_root/#.desktop/clocks/rpg.pdl` | `ext` (lc_clock) | n/a | `clock_kv_ll` read 2048, strtok, atoll; `game_time_epoch_ms` -1 = no clock | yes |
| `zed.txt` (cdda) | `wic` | yes | `zed_load` read 200, **clamped to the room** | yes |
| `wounds.pdl` (cdda) | `wic` | yes | `leg_hurt` read 2400 | yes (it is only a readable copy; the ledger replay is the truth) |
| `jobs.txt` (df) | `app` | yes | job replay, bounds-checked tables | yes |
| `hold.txt` (mcl) | `wic` | yes | read small | yes |
| `blocks_changed.txt`, `ent_changed.txt`, `world_epoch.txt` | `app` markers (truncated by the daemon past 256 KB) | yes | **size only**, never the content | yes |
| `ui.txt`, `scene.raw`, `scene.receipt.txt` | `wic` | yes | the renderer; `ui.txt` is filtered to key=value before the write | yes |
| `uid_<id>.txt` | `wic` | yes | `ent_wallet` read 64, `strlen<32` -> regenerate | yes |
| `keys_seen.txt` | `wic` | yes | debug only | yes |
| `ctl_trace.txt` | one small `fprintf` append, capped 1 MB | yes (one write) | humans | n/a |
| `interact_relay.txt`, `keyboard/history.txt` | `ext` (renderer / engine) | n/a | `poll_keys` offset, fgets 80, <= 32 codes per call | yes: shrink resets the offset; a long line is chunks |
| `slots/<n>/*` | `copy_file` (tmp.pid + checked close + rename; was in place) | yes | `copy_file` back, `clock_ms.txt` atoll | yes |
| `audio/*`, `daemon.lock` | `flock` files | n/a | flock only | n/a |
| `<pchq>/pieces/system/maps/<game>/*` (`map.txt`, `extrusion.pdl`, `cells.rgba`, `cells.txt`, `game.pdl`, `map.png`, `cells.png`) | `fopen_tmp` / `fclose_tmp` / `png_atomic` (**were in place**) | yes | the board renderer (another process) | n/a (the renderer never sees a partial file now) |
| `.../<room>/events.txt`, `ascii_legend.txt`, `view.pdl`, `bar.txt`, `parallax.pdl` | `wic` | yes | board renderer | n/a |
| `.../<room>/parallax.rgba` | tmp + checked close + rename | yes | board renderer | n/a |
| engine session `interact_relay.txt`, `keyboard/history.txt`, pc-hq `widget_cmds/inbox.txt` | one small append (`126\n`, `KEY_PRESSED: 27\n`, `CONFIRM_SET_DESK:<room>`) | yes (< PIPE_BUF, one write) | engine | n/a |
| `<app>/rooms.pdl`, `catalog.pdl`, `party.pdl`, `recipes.pdl` (cdda, mcl), `tuning.pdl`, `time.pdl`, `voices.pdl`, `help.txt` | humans (read-only for the game) | n/a | `load_data` / `load_party` / `cd_reset` / `load_tune` / `turn_str`: fgets 300-400, `split_bar`, field counts, range checks, `nroom<8`, default room if none, `TURN` amount validated | yes (fuzzed) |
| `db-hq/data/system.pdl`, `actors.pdl` (shared, `PET_DB` overrides) | humans | n/a | `load_party` fgets 400 | yes (fuzzed through a copy) |

## 5. The fuzz harness: `XO/test_fuzz_state.sh`

Same shape as the other XO suites: `chk()` lines and a `RESULT: N failures` line; scratch copies are `@.apps/<game>-hard/` with the `@.apps/<game>/` -> `-hard/` path rewrite, so the live
game is never touched; the DB files are copied to `<scratch>/db` and `PET_DB` points there so `system.pdl` / `actors.pdl` can be corrupted too. A stand-in viewer
(`exec -a "khtpm_core_render fake <game>.xhtpm" sleep`) makes the verbs render, so ui.txt is produced. Everything it starts is killed at exit.

The build always has UBSAN (`-fsanitize=undefined -fno-sanitize-recover=undefined`): an out-of-bounds index or signed overflow from file data **aborts** (rc 134) even when it would not
have segfaulted. `FUZZ_ASAN=1` adds the address sanitizer (3-4x slower; use it after changing a parser).

1. **Fuzz (a).** The seeded game's state (about 40 files) and data files are put into groups (`clock`, `events`, `house`, `flags` incl. `world/`, `logs` = chat + relations + relay files,
   `entities` = pets + trainer + active + uids, `misc` = everything else in `state/`, `rooms.pdl`, `data` = catalog + party + recipes, `text` = tuning + time + voices + help, `db`). Each group is corrupted 8
   ways: half-truncated, emptied, 10 KB random, one 200 KB line without newline, vowels and `0` turned into NUL, every number 2147483648999, every number -7, CRLF. After each corruption a fixed verb set runs
   (`event`, and one rotating verb from `step walk buy save load select chat helppanel newgame use strike/mine/dig craft clock control`) then `status`: every run must end with rc < 128 inside
   5 s (`timeout -s KILL`), and `ui.txt` (deleted first) must be rewritten as a complete key=value file with a `title=`.
2. **Daemon on garbage (a2).** All files are corrupted at once with half-truncated / hugenum; the resident daemon runs with INT on and arrow keys fed through `interact_relay.txt`, then INT off; it must stay
   alive and end on SIGTERM with rc < 128. (Default: half-truncated and huge numbers, `FUZZ_DMUTS`.)
3. **Kill test (b).** The daemon (module form, stand-in viewer, INT off so the pets act) runs under churn (events, craft / dig, buy, chat, save from a background loop); it is `kill -9`'d at 30 random moments
   (0.3-0.9 s after the start, so some land in the start-up and some in the steady loop), sometimes with the churn killed mid-verb too. After each kill: `status` runs; each pet file must be exactly the 7 `key=value` lines; no state file may be empty; every `events.txt` row must have a known type;
   every `house.txt` row must be BUY / PLACE / MOVE / REMOVE; `ui.txt` must be complete. It also checks the daemon was really running at the kill (>= 90 %).
4. **Regressions (d).** One check per defect in section 2 (pet file clamp, negative help_step, empty rooms.pdl, ui.txt injection, full disk `ulimit -f 100`, SIGPIPE `bag | head -c0`, EMFILE `ulimit -n 10` over
   4 verbs, ENOMEM `ulimit -v 90000` over 2 verbs).
5. **Negative controls (c).** A scratch copy with the `load_ent` clamp removed must crash (rc 134); a scratch copy whose `write_if_changed` is made non-atomic (in place, slow second half) must be caught by the kill
   test as a torn pet file. Both run every time. Baseline against the old source (`FUZZ_ORIG=1`, cdda): 2 fuzz failures (entities negnum rc 134, time.pdl hugenum timeout) and 5 regression failures
   (pet file, help_step, ui injection, full disk, SIGPIPE); the empty-rooms.pdl check passes on the old source too (it is a guard, not a fix).

Knobs: `FUZZ_KILLS` (30), `FUZZ_MUTS` ("1..8"), `FUZZ_DMUTS` ("1 4 6"), `FUZZ_ONLY=<regex of group names>`, `FUZZ_ASAN=1`, `FUZZ_ORIG=1`, `FUZZ_SKIP_NEG=1`, `FUZZ_KEEP=1` (leave the scratch folder), `FUZZ_TAG`.
Never run two copies at once (same scratch names).

**Add a file:** a state file that the seed already creates is picked up automatically (it is found by `find` in the seeded state); to make it part of a bigger group edit the `case` in the "files to corrupt"
block; a file the seed does not create needs a seed verb (the `SEED=(...)` array per game). A new verb that reads state goes into `T2` (the rotating list) or, if it is the only reader of a file, into `VERBS`. A new defect gets
a line in the regression block with the exact bytes that triggered it, and must first be shown to fail with `FUZZ_ORIG=1`.

## 6. Checklist for a game daemon change

- [ ] Every new `fopen(..., "w"|"wb")` is gone: `write_if_changed` / `fopen_tmp`+`fclose_tmp` / `append_line`.
- [ ] Every new number parsed from a file is clamped before it indexes, divides, loops or allocates.
- [ ] Every new buffer write uses a clamped offset (`W()`), every new key=value output goes through the final `key=value` filter.
- [ ] Every new child process uses `reap_timed` / `sh_timed`; every value passed to it is validated.
- [ ] Every new append-only file has a cap or trim, or is the ledger and says so.
- [ ] The new file is in the harness; `XO/test_fuzz_state.sh <game>` is 0 failures; the old-source baseline still fails where it should.
- [ ] The existing suites still pass (test_chat, test_left, test_lock, test_clockstop, test_tabs_*, test_s1, test_log, test_cdda_c1/c2, test_mcl_blocks, test_doorday).
