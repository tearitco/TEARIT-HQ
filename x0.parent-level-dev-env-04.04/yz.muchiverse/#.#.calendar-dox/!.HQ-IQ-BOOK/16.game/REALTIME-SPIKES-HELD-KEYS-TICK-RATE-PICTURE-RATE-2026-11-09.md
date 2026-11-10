# Real-time spikes: held keys, tick rate, picture rate (measured 2026-11-09)

The blocker shared by Snake (S9) and Teario-Play (M0): the generated driver stepped at 100 ms at best and nobody had measured what a fast loop costs, what keys the window really delivers, or how fast the picture can follow. Three spikes, all in SCRATCH folders (no live editor, no live window touched). Labels: **OBS** = observed this session with the number written next to it; **READ** = read in code, not run; **NOT MEASURED** = said plainly.

## 1. The verdict

**Can the generated driver run a 30 Hz game on this machine? YES for the loop and the keys, NOT YET PROVEN for the picture.**

| Question | Answer | Evidence |
|---|---|---|
| 30 logical ticks a second, no process per tick | **yes** | OBS: `tick_ms` 33 for 30 s: 902 ticks stepped of 903 expected, 30.27 Hz, 0 dropped, 0 children, 1 thread |
| Cost of that loop on this weak CPU | **0.9 percent of ONE core, 35 wakeups a second** (Ryzen 7 Surface, 8 cores, Chrome and Firefox open, machine 5 to 23 percent busy, cpu_gate niced 15) | OBS, three runs: 0.93, 0.90, 0.40 percent |
| Paused, stopped or over | **0.00 percent, 2 wakeups a second** (was 4 with the old 250 ms nap) | OBS |
| Jitter | lateness p50 0.36 ms, p99 0.9 ms, max 3.2 ms; interval p99 33.6 ms | OBS |
| An arrow key to a `turn` row | median **53 ms** end to end, p90 66, max 70, 40 of 40 keys became rows | OBS (real X key events in a nested X server) |
| A held key | X auto-repeat arrives as repeated PRESS lines; **there is no release line, ever** | OBS |
| Picture at 30 Hz | **not measured.** The feed (`game_ui.txt`) is rewritten at most 5 times a second by design; the 3D figures path was not run | section 5 |

The 3 percent line the lead set is not approached; 20 Hz and 15 Hz are therefore not offered as a need (a 100 ms run is in the table for comparison). The expensive unknown is the picture, not the loop.

## 2. Method

* **Scratch only.** A copy of `@.apps/snake` under the session scratchpad with `tuning.pdl` edited, a scratch editor folder (`state/world/switches.txt`, `state/interact_relay.txt`), a fake owner (`sleep`), the REAL installed `game_core.+x driver`. Nothing under the live editor. Board 32 x 32, `walls wrap`, `ckpt_every` 64 (or 1000 for the latency run), `max_ticks` 200000 so a 30 s run is not cut by the rule (see section 6).
* **Scripted input stream.** A background loop appended one arrow code (1002 1001 1003 1000, a clockwise square) to `interact_relay.txt` every 330 ms: 89 `turn` rows in 30 s, 14 `ckpt` rows.
* **Cost.** New C op `harness/ops/proc_cost_op.c` (`proc_cost_op <pid> <seconds> [sample_ms]`, CPU-OPTIMIZATION-PITFALLS 26): `/proc/<pid>/stat` utime+stime and cutime+cstime, `voluntary_ctxt_switches` (the wakeups), live children seen every sample, machine busy share and load next to it. Each COST line carries the producer's state: the driver was `running=1` for the live rows and `running=0` (game started, paused) for the idle rows (pitfall 28).
* **Ticks and jitter.** The driver's own measuring switch `GAME_TICK_STATS=<file>` (off in play; writes nothing when unset): per live stretch, ticks run, ticks expected (live time / pace), achieved Hz, lateness against the scheduled deadline and interval since the previous tick (percentiles), steps run in a catch-up pass, time dropped, feed writes.
* **Keys.** A real khtpm window (the installed `khtpm_core_render.+x`, the same scratch `.xhtpm` the case `live_esc_first` uses, Interact armed through its vars) inside a nested X server (`Xephyr :77`, closed afterwards) so the owner's display and focus were never involved; real X key events from `xdotool` on that nested display only. Latency by file mtime differences (kernel jiffy, about 4 ms) and `date +%s%N` before the `xdotool` call (so the `xdotool` start, a few ms, is inside the first number).
* All heavy runs went through `cpu_gate ... run` one at a time. The gate itself waited several times on die temperature 87 to 95 C (limit 85) during back-to-back runs: a finding for the next agent: space these runs out.

## 3. S-A: keys (OBS unless marked READ)

1. **Arrows reach `interact_relay.txt` only while Interact is armed.** Armed (`interact_armed=1`, `bv_h1` set): Left 1000, Right 1001, Up 1002, Down 1003, each as `<code> <ms>` (ms = CLOCK_MONOTONIC). Unarmed (`interact_armed=0`): the same keys wrote **0 lines** to the relay and instead went to the window's own history (`#.desktop/entity_menu_history/<pid>.txt`) as 200 (Up), 201 (Down), 203 (Right), the "agent relay band" the core also accepts (READ, `petslike_core.c` header). In the editor "armed" is the In: switch (READ: `kh_scan_interact_relay` arms on `class=interact-active` + `relay=`; I did not drive the editor's own template).
2. **Latency.** n = 40 taps (Up Right Down Left, 400 ms apart, `ckpt_every` 1000 so no other write blurs the ledger mtime), driver at 33 ms:

| leg | min | p50 | p90 | max |
|---|---|---|---|---|
| `xdotool` start to the relay line (window) | 35.1 | 37.6 | n/a | 40.7 ms |
| relay line to the `turn` row (driver) | 0 | 16 | 29 | 33 ms |
| total | 38.2 | 53.2 | 66.4 | 70.3 ms |

   The driver leg is bounded by one tick (the key is read just before the next step and stamped with that tick), as designed. All 40 taps became 40 relay lines and 40 `turn` rows. The per-pid RELAY path (`relay_drive`, `KEY_PRESSED: n` into `entity_menu_history/<pid>.txt`) is slower and quantized by the renderer's poll: 12 samples from 3 to 174 ms (sorted: 3 23 48 55 75 100 106 127 133 148 158 174). **A harness driving by relay sees up to about 175 ms of latency; a person with a real key sees about 40 ms.**
3. **A held key** (`xdotool keydown Right; sleep 1.6; keyup`): 25 lines `1001`: the first at 0, the second at +661 ms, then one every 40.6 ms (n = 23). That is the X server's own auto-repeat (the nested server runs delay 660 ms, rate 25 per second; `xset r rate` did not change Xephyr's). One second after the key-up there were still 25 lines: **no release line**. Unarmed: 15 lines of `201` in a 1.2 s hold, again no release. The owner's real display (`xset q` on :0, a read-only query) has delay 500 ms and rate 33 per second, so there the gaps would be 500 ms then 30 ms: **computed, not observed**. `KeyRelease` reaches the dispatcher (READ, `khtpm_core_render.c` line 15936) but is never forwarded.

## 4. S-B: the tick loop (built; OBS)

### What changed in `ops/game_core.c` (the driver loop only; the engine, ledger rows, ckpt rows and the hash are untouched)

* `tuning.pdl` DATA key `TUNE | tick_ms | N`: milliseconds per step at the normal speed button. Realtime default 100 (the old behaviour), ledger default 550 (the old behaviour). **33 to 2000.** Fast / slow scale it by the kind's own ratios (40:100:250 realtime, 120:550:1400 ledger) and the result is never under 33. A value outside the range, a fraction, a word, an empty value: **refused with the reason** by the new read-only verb `tuning` (exit 2: `tuning.pdl: tick_ms must be a whole number of milliseconds from 33 (30 steps a second) to 2000, got '5'`); the driver does not hide it: the reason is the first line of the feed and the game runs at the default. It is not in the rules hash and never in the ledger.
* One monotonic deadline per step (`next_us += pace`, so jitter does not become drift), no fork, no shell, no process per tick (the driver already ran verbs in-process; what changed is the clock and the sleep). The sleep is a `ppoll` that ends at the next deadline or when the game folder changes (a button press is seen at once, through `inotify` on the game folder only: the world folder is busy). Live: one wake per step, at least every 250 ms. Not live (setup, paused, stopped, over, a human to roll, an editor game): blocks 500 ms in `ppoll` = 2 wakeups a second.
* A late wake runs at most 4 steps (realtime; 1 for a ledger game) and drops the rest of the lateness, counted (`dropped`). A stall slows the game, it never bursts, and since ticks are logical the ledger is the same. A step that errors ("stuck": `max_ticks`, ledger full) stops stepping until a button or an outside write (before: it replayed the ledger at every tick).
* The ledger is what it was: a row per input and per consequence, `ckpt` every `ckpt_every` ticks. Registration per AGENTS.md is unchanged: the editor's `book_driver()` registers the driver in the process registry with lifetime `owner` before it execs; the driver locks itself (flock), starts no process, and exits when its owner is gone (checked in the cases).

### Numbers (cpu_gate niced; Chrome and Firefox open; scripted key stream; raw COST and STATS lines)

| run | live | cpu of one core | wakeups/s | ticks / expected | achieved | lateness p50 / p90 / p99 / max | interval p50 / p99 / min / max | dropped |
|---|---|---|---|---|---|---|---|---|
| **33 ms**, 30 s | 29.8 s | **0.93 %** | 34.8 | 902 / 903 | 30.27 Hz | 364 / 508 / 891 / 3187 us | 32999 / 33568 / 30005 / 35784 us | 0 |
| 33 ms with a busy-loop hog on the same core (taskset, equal nice) | 19.8 s | 0.40 % | 34.8 | 599 / 600 | 30.25 Hz | 232 / 298 / 2924 / 3033 us | 33000 / 35686 / 30184 / 35778 us | 0 |
| 33 ms, driving a small window in the nested X (section 5) | 56.1 s | 0.90 % | 35.2 | 1700 / 1701 | 30.29 Hz | n/a | n/a | 0 |
| 100 ms, 30 s (the old default) | 29.8 s | 0.53 % | 14.9 | 297 / 298 | 9.97 Hz | 687 / 907 / 1181 / 1298 us | 99991 / 100626 / 99255 / 100884 us | 0 |
| OLD driver (HEAD) at 100 ms, 30 s | 30 s | 0.67 % | 24.6 | n/a (no stats) | n/a | n/a | n/a | n/a |
| paused, game started, 10 s (new, in every run) | n/a | **0.00 %** | 2.0 | 0 | n/a | n/a | n/a | n/a |
| paused (OLD, 250 ms nap) | n/a | 0.10 / 0.00 % | 3.9 to 4.0 | 0 | n/a | n/a | n/a | n/a |

Children: `children_max=0`, `children_seen=0`, `child_cpu_pct=0.00`, threads 1 in every row. CPU is utime+stime in 10 ms clock ticks over 20 to 30 s, so the resolution is about 0.03 point. The hog row is lower than the first because the driver was pinned to one core in the shared run queue and did the same work; do not read it as "a hog helps". Longest live stretch measured: 56 s. The first row stepped 902 ticks into a replayable ledger (106 rows: 89 turn, 14 ckpt, `check ok`).

### Determinism (cases, no window)

`snk_tick_live`: the REAL driver at 33 ms plays the seed-7 pet game to its end: 336 ticks stepped (exactly), 97 rows, the replay hash equals the headless `autoplay` hash and the ledger file is byte for byte the headless one; the same game at `tick_ms` 33 and 2000 gives identical ledger bytes; a ledger made at 33 replays under 2000. Negative controls and mutants: section 7.

## 5. S-C: the picture

**Measured (OBS):**
* **Producer:** at 33 ms ticks the feed `state/game_ui.txt` is rewritten **5.06 times a second** (284 writes in 56 s, 34 in 6.2 s): the driver caps it at one write per 200 ms whatever the tick pace (`dirty` is set by every tick because the text shows the tick number). A page showing a value that changes every tick therefore updates at 5 Hz, not 30.
* **Consumer cost:** a small real khtpm window following that feed (nested X, 4 s warm, 10 s): **0.40 % of a core, 18.2 wakeups/s**, 0 children; the driver in the same run 0.90 % and 35.2 wakeups/s.

**NOT measured, and exactly why:**
* **How many distinct pictures per second the window shows.** I sampled screen hashes (`xwd | md5sum`, 50 ms) of the nested X while the driver ran and while it was paused. The paused control FAILED: the screen changed in 38 of 40 samples (36 distinct hashes) with nothing happening, and a hash of the window alone changed in 37 of 40. A null with no quiet baseline measures nothing (pitfall 31), so I report no number. Missing: a way to read the text a window shows (a renderer text dump) or a window region that is quiet when idle.
* **The board picture (the 3D engine redrawing the figures).** Snake has no figures yet (`events.txt` rows) and the generic driver does not write them (plan C5 / S2); the engine path needs a scratch editor board with the GPU raymarch daemon, which I did not start (the 2026-11-08 amdgpu reset, the die temperature above 90 C during these runs, and the single-job rule). Memory of an earlier session says the live board reaches about 17 frames a second on this machine; that figure is not from today. To measure it: generate figure rows from the driver at the tick rate into a scratch board's `events.txt`, run ONE gated engine session, and count frames in the engine's own `.gpu_debug` counters next to `proc_cost_op` on the engine and the renderer.

## 6. What this changes in the plans

**Snake (CHUTES-LADDERS-AND-SNAKE plan, part B):**
* S9 (the CPU budget at the real pace) is **answered for the loop**: 0.5 percent at 10 Hz, 0.9 percent at 30 Hz, idle 0. Open: the picture.
* Steering works with presses alone: median 53 ms from key to `turn` row, never more than 2 ticks late at 30 Hz. Snake needs no release line.
* The plan's default `tick_ms` 150 and floor 50 ms are superseded by data: the floor is 33 ms; Snake ships 100 (the old pace) until the owner picks.
* `max_ticks` 20000 is 11 minutes at 30 Hz and `max_rows` caps the rows; raise `max_ticks` in `rules.pdl` for a longer game (a rule, it changes the rules hash).
* Exclusivity (route 3b) is still open: with In: OFF the arrows go to the window's own history (200 band), with In: ON to the relay, where the core also reads them to walk the selected pet.

**Teario M0 (TEARIO plan section 7):**
* **S1 (held keys): answered.** No release is ever delivered; X auto-repeat arrives as presses (first press, a gap of the server's repeat delay, 500 to 660 ms, then a press every 30 to 40 ms). A person cannot be told apart from a tap during that first gap. Consequences: (a) `hold` mode as drawn in the plan (4.6) cannot be exact; the honest v1 is **latch** (a press sets the direction until the opposite key or a stop) or an **impulse** model (each press moves N ticks; at N = 4 the repeat stream is continuous, but the first 500 ms after a single press stutters); (b) a real hold needs a small renderer change (forward `KeyRelease` as a `KEY_RELEASED: n` line next to the press, in `khtpm_core_render.c`, a shared high-collision file) or the owner shortening the repeat delay: OPEN, owner decision; (c) the ledger stores the sampled MASK per tick (plan), which is fine for either.
* **S2 (picture rate): half.** The driver half is measured (30 Hz for 0.9 percent); the picture half is not.
* S3, S4, S5 were not touched.
* A relay-driven harness cannot hold a key for N ticks (175 ms poll quantum, section 3): keep the plan's scripted mask verb.

## 7. Cases and proof that they can fail

Case `snake`: 117 to **180** checks (63 new, in the cases `snk_tick_ms` (parsing, refusals, determinism), `snk_tick_live`, `snk_tick_pause_stall`, `snk_tick_bad_in_driver`). Case `teario_play`: 57 to **65** (`tp_tick_ms`). `game_new` 225 and `chutes` 175 unchanged. The new checks, each with a negative or a positive control: refusals of 0, 5, 5000, 32, 2001, 33.5, a word, an empty value and an extra argument (boundaries 33 and 2000 accepted); no tuning file = default; paused for 1.5 s adds no row and running for 500 ms adds rows (the positive control); resume adds rows; a 500 ms SIGSTOP stall is dropped and counted but capped; a bad value shows its reason in the feed and the game runs at 100; no child process (and `proc_cost_op` DOES see the children of a script that forks, the positive control); the fake owner's death ends the driver.

**Mutants run against a scratch house** (the same case rows, a mutated `game_core`): (1) ticks while paused: FAILS the two paused checks and the checks that need the stats written at a pause (7 failures, one of them an artifact of the scratch house, which has no level file); (2) no range check: FAILS the refusal checks (0 stays refused, the others do not) and the feed check; (3) no catch-up cap and no drop: FAILS the stall checks (`dropped` 0, 15 catch-up steps); (4) a fork per tick: FAILS `child_cpu_pct=0.00` (it measured 5.47 percent) but **passed `children_seen=0`**: a child that lives under the 100 ms sample is invisible to that field. `child_cpu_pct` is the detector for short forks; keep both.

## 8. Proven, not proven

**Proven (OBS):** the loop numbers above; the key facts of section 3 for a real khtpm window in a nested X; the cases and their mutants; ledger and replay identity at 33 ms; the templates give a generated realtime game `tick_ms 100`, ledger and editor games 550 (checked by generating into a scratch folder).

**Not proven:** the editor's own window and its INT switch (the scratch window uses the same renderer code and relay variables, not rpg-edit's template); the owner's real X server repeat behaviour (computed from `xset q`); the picture rate and the 3D engine; any run longer than 56 s; CPU while a heavy browser tab is busy (Chrome and Firefox were open and idle-ish: 5 to 23 percent machine busy); the lateness numbers were measured niced 15 (cpu_gate), a driver started by the editor is not niced, so real jitter should be no worse; whether a person finds 30 Hz Snake pleasant.

## 9. External references

Repo: `&.widgits/_shared-lib/ops/game_core.c` (driver loop, `tuning` verb, `GAME_TICK_STATS`), `&.widgits/_shared-lib/harness/ops/proc_cost_op.c`, `harness/cases/snake.pdl` (`snk_tick_*`), `harness/cases/teario_play.pdl` (`tp_tick_ms`), `harness/templates/game/realtime/tuning.pdl`, `@.apps/snake/tuning.pdl`, `_.monads/_.livedesk-taskbar/ops/khtpm_core_render.c` (`kh_scan_interact_relay` line 9300, `kh_capture_key` 14633, KeyRelease 15936), `&.widgits/_shared-lib/petslike_core.c` (`poll_keys`, `book_driver`). Book: `16.game/CHUTES-LADDERS-AND-SNAKE-GAME-PLANS-2026-11-09.md` (sections 12 to 15), `16.game/TEARIO-MAKE-PLAY-LEVEL-EDITOR-AND-PLATFORMER-PLAN-2026-11-09.md` (2.4, 4.6, 7), `16.game/GAME-FOUNDATION-GENERATOR-HOW-TO-AGENT-GUIDE-2026-11-09.md`, `03-pitfalls/CPU-OPTIMIZATION-PITFALLS.md` (25, 26, 28, 30, 31), decision D65 in `02-architecture/DESIGN-CHOICE-TEST-AND-DECISION-LOG.md`.
