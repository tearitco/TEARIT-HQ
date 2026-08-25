# 🦁️ cpulimit-faq.md — how to stop one process from cooking your CPU

**For:** humans, plain language, emoji-heavy on purpose.
**Written:** 2026-08-04, after a real, live use of this — Chrome had a runaway tab (a renderer process pegged at ~100% CPU) plus `opencode` running hot, dragging the whole machine's load average up to 16+. `cpulimit` fixed it live, verified: idle CPU went from ~1% to ~55% within seconds.

---

## 🤔 What even is this?

`cpulimit` is a real, standard Linux tool. It doesn't kill a process — it just **throttles** it, by repeatedly pausing (`SIGSTOP`) and resuming (`SIGCONT`) it so its *average* CPU usage never goes above a percentage you pick. The process keeps running, keeps its state, keeps its windows open — it just gets less CPU time per second.

Think of it like a leash, not a bullet. 🐕‍🦺

## 🧰 Is it installed?

Yes — this machine already has it (`cpulimit` version 2.7-2, confirmed 2026-08-04). If it's ever missing:

```bash
sudo apt install -y cpulimit
```

There's also a whole folder of **house-built** cpulimit-style tools right here in `#.ref/🦁️.cpu-limit]ON]PUR/` (`0.cpulimit]grok]name]b0.c`, `1.cpulimit]grok]b1+top.c`, etc.) if you ever want a custom variant — but the real, standard `cpulimit` binary is simpler and does the job.

## 🎯 How to actually use it

### Limit by PID (best when you already know which process is misbehaving)

```bash
cpulimit -p <PID> -l <PERCENT>
```

Example (this is literally what got run live to fix the slowdown just now):

```bash
cpulimit -p 2101678 -l 50   # cap that Chrome tab's renderer process at 50% CPU
```

### Limit by process name (easier when you don't have the PID handy)

```bash
cpulimit -e firefox -l 77
cpulimit -e chrome -l 50
```

⚠️ **Gotcha**: `-e NAME` matches by process name (matching `/proc/[pid]/comm`, which Linux truncates to 15 characters!). If your target's name is long, `-e` might silently miss it — use `-p <PID>` instead when in doubt. (This exact truncation bug once bit `EMERGENCY_KILL.sh` too — see that file's own header comment for the full story.)

### Run it in the background so it keeps watching

`cpulimit` normally runs in the foreground and keeps enforcing the limit for as long as it's alive. To leave it running after you close your terminal:

```bash
setsid nohup cpulimit -p <PID> -l <PERCENT> >/tmp/cpulimit_<name>.log 2>&1 < /dev/null &
disown
```

(Same `setsid`/`disown`/redirect-everything pattern this house already uses for every other long-lived background process — see `TILE_PICKER_DESIGN.md` for why plain `&` alone isn't reliable.)

## 🔍 How do I find the runaway PID in the first place?

```bash
top -bn1 | head -12
```

Look at the `%CPU` column — anything sustained near or above 100% (on a single core) is worth investigating. Chrome specifically runs as *many* processes (one per tab roughly), so:

```bash
ps aux | grep chrome | grep -v grep | sort -k3 -rn | head -5
```

sorts them by CPU usage, worst offender first.

## ✅ How do I know it actually worked?

Just re-run `top -bn1 | head -12` a few seconds after starting `cpulimit` and watch:
- The `%CPU` column for that specific PID should drop toward your limit.
- The overall `%Cpu(s)` idle number (far right of that line) should climb back up.
- `load average` (top of the `top` output) will take a little longer to come back down — it's a rolling average, not instant.

## 🛑 How do I stop limiting a process?

Just kill the `cpulimit` process itself (not the thing it's limiting!):

```bash
pkill -f "cpulimit -p <PID>"
```

The target process goes back to running at full speed immediately.

## 🙅 What this is NOT for

- **Don't** use this on your own house processes (tile-picker windows, chtpm daemons, etc.) as a substitute for fixing a real bug — if something in *this* house's own code is spinning hot, find and fix the actual cause first (see `TILE_PICKER_DESIGN.md`'s own real CPU-bug writeup for an example of doing that the right way). `cpulimit` is for **other people's software** (browsers, third-party tools) that you can't/won't patch yourself.
- **Don't** cap something so low it stops being useful — a browser tab throttled too hard can become unresponsive/janky, not just "slower." Start around 50% and adjust.
