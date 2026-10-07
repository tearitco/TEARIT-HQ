# music-daemon v0 (scratch / staging only, nothing starts it)

Seeded background-music generator for the game: a pattern generator, a small software synth, and a loop daemon. Headless-tested; **nobody has listened to it yet** - the harness cannot judge how it sounds.

| File | What |
|---|---|
| `ops/music_seq.c` | seed + mood + time_of_day + weather -> pattern text (integer-only, deterministic; Heal = built-in safe pattern) |
| `ops/music_synth.c` | pattern -> 16-bit mono WAV (sine/square/saw/triangle/noise, ADSR, 2-pole low-pass, noise drums, tanh limiter) |
| `ops/music_daemon.c` | loop: reads `music_state.txt`, runs seq+synth by fork/exec/waitpid, plays via paplay > aplay > ffplay, appends `music_ledger.txt` |
| `mood.pdl` `scales.pdl` `instruments.pdl` | all the numbers (moods, TOD/WEATHER variant rows, scales, instrument envelopes) |
| `music_state.txt.example` | `on` `mood` `volume` `seed` `time_of_day` `weather` |
| `ops/build_music_daemon.sh` | builds the three ops into `ops/+x/` (picked up by `compile-runner.sh`) |

Pattern format = the Muchi DAW's sequence lines (`NOTE track= pitch= start= len= vel=`, PPQN 480) plus `tempo= bars= beats= trackN name= inst=`.

## Install / try (not done automatically)
```
sh ops/build_music_daemon.sh
cp music_state.txt.example music_state.txt        # edit mood/seed
nice -n 15 ops/+x/music_daemon.+x --dir . &       # plays through paplay/aplay/ffplay; --once 2 plays two phrases and exits
touch music_stop.flag                             # clean stop (also: on=0 in the state file); pid is in music_daemon.pid
ops/+x/music_daemon.+x --dir . --sink file:/tmp/m --once 2   # headless: writes /tmp/m.1.wav, /tmp/m.2.wav
```
Settings are re-read before every phrase (size/content change, never mtime), so a change is heard from the next phrase. Ledger rows: `START|pid|player`, `PHRASE|n|mood|seed|bytes`, `HEAL|n|reason`, `STOP|reason`.

Harness: `_shared-lib/harness/music_daemon.pal` (72 checks). Design: EDEN-PLAYABLE-LOOP-AND-BOOK-PAGE-HARNESS-PLAN.md section 4 and its "Build report: music daemon v0".
