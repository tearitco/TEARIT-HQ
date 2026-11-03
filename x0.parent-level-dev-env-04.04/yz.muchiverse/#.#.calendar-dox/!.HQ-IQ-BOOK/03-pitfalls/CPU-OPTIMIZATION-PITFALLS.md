# CPU optimization pitfalls (written 2026-11-03 after a night of two guard kills, an orphan clock, a compositor that fed nobody, and a `%` tab frozen at 95 %)

Every item below happened in this house, in one night. Each has a check. Companion docs: `hq-cpu-safety.md` (sections 9-10h: numbers and tools), `RUNTIME-FILE-GROWTH-STANDARD.md`.

## Measuring
1. **A filter that misses your processes reads "calm" for the wrong reason.** The 3D engine and renderer run with RELATIVE command lines (`ops/+x/bv_render_3d.+x --daemon`, `./system/chtpm_rgb_render`); a sampler that matched only the house path saw neither. Check: the sampler's process count equals what `ps` shows for the stack.
2. **Sample during AND after the input.** Idle (about 1 %), driven (58-106 %), and the minute after input stops are three different numbers. A tool that is cheap idle and expensive driven is only fixed when both are measured.
3. **Compare with a sibling before blaming one app.** The same camera test on rpg-dust showed the editor's numbers: the cost was the shared pipeline, not the editor.
4. **Measure what the guard measures.** `cpu_gate` read the `acpitz` zones (38-47 C) while the die sensor `k10temp` (what `cpu_guard` reads) was 70-87 C: a gate that disagrees with the guard approves the load that gets the house killed. Same sources, same units, same thresholds.
5. **`perf` and `strace -p` are blocked here.** Profile as a child (`strace -c ./prog`, `/usr/bin/time`), with an isolated micro-benchmark of the suspected steps, or with the program's own timers (`touch pieces/display/.gpu_debug` for the engine). Do not guess from the source.
6. **An old measurement is evidence for the architecture it was taken on.** September's "3D is not I/O bound" was true with a CPU raymarch; with the GPU path the I/O was a third of what remained. Re-measure when the thing under it changed.
7. **Measure the medium before building a transport.** The in-memory-frame idea was worth about 15-20 % (tmpfs experiment); the unread compositor was worth 34 %.

## Before optimizing a producer
8. **Find out who reads its output.** `chtpm_rgb_render` re-composed a 2 MB file 28 times a second for nobody: the window reads `rgb_frame_3d_overlay.raw` / `rgb_frame_2d.raw`. Check: `grep` the readers, then PROVE it by removing the producer and watching the result (the canvas still changed).
9. **A comment is a claim, not a measurement.** The comment said the khtpm window reads the compositor's file; the code and the experiment said it does not.

## Tests that cost CPU
10. **Gate every round, not once.** Fourteen back-to-back level picks, each starting helpers, next to a browser, got the house killed. `cpu_gate check --wait` before each round of a loop.
11. **A bounded helper waits for its OUTPUT, never for a fixed `timeout`.** `timeout 3 <polling daemon>` burns the full 3 s every time. Run it niced, stop it when the file it exists to write changes, keep a hard cap as the backstop, and do not start it at all when the machine is already busy.
12. **Never rebuild a binary under a running daemon** without restarting it: the daemon keeps running the deleted old code (`rpg_edit_hq.+x (deleted)` in an incident file).
13. **`pkill -f '<pattern>'` in a tool command matches the tool's own shell** and kills it (exit 144). Select by the process's own command start: `ps -eo pid=,args= | awk '$2 ~ /^ops\/.../ {print $1}'`.
14. **A test that changes live state restores it by hash** (palette `_active.txt` files), and deletes what it created (sprite caches, layout files, per-test `_active` files).

## Processes and ownership
15. **Find orphans by closing the window and listing what survives.** An unowned `lc_clock` (registered `keep`, owner 0) ticked game days with every window closed: 6 % of a core, the 118 % of the first incident. Ownership passed by an environment variable works for the resident daemon but not for the short-lived verb processes that also start things (the `In:` button). Derive the owner from something real (the holder of the daemon's `flock`), never default an unknown owner to `keep`.
16. **A healthy stack does not leak.** Memory, descriptors and threads were identical before and after a 3D burst. Test that before hunting leaks; look for files that grow (diff every file's size around a burst) and for dead-writer leftovers instead.

## Governors and caps
17. **Defer, do not drop.** A frame governor leaves the pulse unconsumed so the next pass renders it; dropping it loses the last frame. Sample the machine every ~500 ms (not per frame) with `/proc/stat`, not the 1-minute load average (it stays high for minutes after a spike).
18. **A governor trims the stream; it does not remove the cost.** Calm 3D use went 53 -> 38 % with it; the real wins were removing work nobody uses (106 -> 72 %) and capping files.
19. **Every runtime file needs an enforced cap and a row in the standard.** `frame_history.txt` grew 12 KB/s uncapped; trimmed by `loop_guard` every ~8 s. Also: a directory that once held 58,000 files stays 80 MB forever (ext4); recreate it.

## What the user sees
20. **A number shown to the user is a claim; a dead writer makes it a lie.** The guard ended the taskbar manager as a stray, and `#.desktop/load_state.txt` stayed `load_label=!%95` for 90 minutes: every board's `%` tab read 95 % on a machine at 6 %. Every displayed metric needs a writer-alive check or a timestamp the reader honours; the guard now clears the label when no manager exists. First question when a displayed number looks wrong: is its writer running, and when did it last write.
21. **A guard kill takes the desktop shell too.** After any guard action check the taskbar manager, the tiles and the labels, not only the process that was hot. `sh _.monads/_.livedesk-taskbar/ops/run_khtpm_strip.sh new` (through `cpu_gate`) brings the shell back without touching board windows.
22. **A guard kill is OUR footprint until proven otherwise** (AGENTS.md): read the incident file, name the processes, measure them, fix the cause. "The browser" is a finding only after the house share is shown to be small.
