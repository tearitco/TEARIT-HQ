#!/usr/bin/python3
"""convert_night_to_audio_remote.py - narrate a NIGHT class script OFFLINE with Kokoro on debil over ssh (heavy work off this weak machine; policy: big jobs go to debil or the Mac).
  OTHER HOST (done 2026-11-26 for the owner's Mac): DEBIL=lfs.master@10.0.0.144 REMOTE_PY=$HOME/house-tts/py/cpython-3.11.18-macos-x86_64-none/bin/python3.11 REMOTE_THREADS=6 python3 convert_night_to_audio_remote.py start|status|fetch <NIGHT>. One-time setup on that host (no sudo): the two Kokoro model files copied to ~/house-tts (sha256 prefixes as below), uv + a private python 3.11 under ~/house-tts, then `uv pip install --python <py> --target ~/house-tts/lib kokoro-onnx soundfile numpy onnxruntime`. macOS has no setsid: the launcher falls back to nohup.
  convert_night_to_audio_remote.py start  <NIGHT_xx.txt>    parse here (same parser/voices as convert_night_to_audio_local.py), upload the job, start rendering on debil in the background (nice, 6 threads)
  convert_night_to_audio_remote.py status <NIGHT_xx.txt>    last progress line from debil
  convert_night_to_audio_remote.py fetch  <NIGHT_xx.txt>    when done: copy the wav back, make audio-book/<name>_debil.mp3 with local ffmpeg (96k); an existing output is never overwritten
One-time setup on debil (done 2026-10-14): ~/house-tts/{kokoro-v1.0.int8.onnx, voices-v1.0.bin (sha256 prefixes 6e742170d309016e, bca610b8308e8d99), render_lines.py, lib/ (pip --target: kokoro-onnx soundfile numpy onnxruntime)}. No ffmpeg or sudo there, hence wav out + mp3 here.
Voices are the same as the local converter (LOCAL_VOICES). Remote state: ~/house-tts/parts/<name>/ (one wav per line, resumable), ~/house-tts/<name>.json/.log/.wav. Host: DEBIL env var (default debil@10.0.0.16)."""
import json, os, subprocess, sys
here = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, here)
import convert_night_to_audio as base          # parser + chunker (imports edge_tts/pydub: needs them installed locally, as the local converter does)
HOST = os.environ.get("DEBIL", "debil@10.0.0.16")
PYBIN = os.environ.get("REMOTE_PY", "python3")          # the remote python: on the Mac a private 3.11 (uv) because the system python is 3.9: REMOTE_PY=$HOME/house-tts/py/cpython-3.11.18-macos-x86_64-none/bin/python3.11
THREADS = os.environ.get("REMOTE_THREADS", "6")
LOCAL_VOICES = {"MAXINE": ("bf_emma", "en-gb"), "TOMO": ("af_heart", "en-us"), "RAHWEH": ("am_michael", "en-us"), "THE NARRATOR": ("bm_george", "en-gb")}
def ssh(cmd, timeout=60): return subprocess.run(["ssh", "-n", "-o", "BatchMode=yes", "-o", "ConnectTimeout=8", HOST, cmd], capture_output=True, text=True, timeout=timeout)      # -n: no stdin, so a backgrounded remote job cannot hold the session open
def nm(path): return os.path.splitext(os.path.basename(path))[0]
def start(path):
    raw = open(path, encoding="utf-8").read(); voices = {w: w for w in base.read_voices(raw)}; miss = [w for w in voices if w not in LOCAL_VOICES]
    if miss: raise SystemExit("no local voice for: %s" % miss)
    lines = [{"voice": LOCAL_VOICES[w][0], "lang": LOCAL_VOICES[w][1], "pieces": base.chunk(t, 400)} for w, t in base.parse_script(raw, voices)]
    job = "/tmp/%s.json" % nm(path); json.dump({"pause_ms": 450, "lines": lines}, open(job, "w")); print("%d lines" % len(lines))
    subprocess.run(["scp", "-o", "BatchMode=yes", "-q", job, os.path.join(HOST + ":house-tts", nm(path) + ".json")], check=True)
    subprocess.run(["scp", "-o", "BatchMode=yes", "-q", os.path.join(here, "remote_tts", "render_lines.py"), HOST + ":house-tts/render_lines.py"], check=True)
    run = "nice -n 10 env PYTHONPATH=$HOME/house-tts/lib %s render_lines.py %s.json %s.wav --threads %s" % (PYBIN, nm(path), nm(path), THREADS)      # PYBIN, THREADS: see the header (a Mac has no setsid and an old system python)
    r = ssh("cd ~/house-tts && if command -v setsid >/dev/null 2>&1; then setsid -f nohup %s > %s.log 2>&1 < /dev/null; else nohup %s > %s.log 2>&1 < /dev/null & fi; echo started" % (run, nm(path), run, nm(path))); print(r.stdout.strip() or r.stderr.strip())
def status(path):
    r = ssh("tail -2 ~/house-tts/%s.log 2>&1; ps -eo args | grep -c '[r]ender_lines.py %s'" % (nm(path), nm(path))); lines = r.stdout.strip().split("\n"); print("\n".join(lines[:-1])); print("RUNNING" if lines and lines[-1].strip() not in ("0", "") else "NOT-RUNNING")
def fetch(path):
    out = os.path.join(base.OUTPUT_DIR, nm(path) + "_debil.mp3")
    if os.path.exists(out): print("skip (exists): %s" % out); return
    wav = "/tmp/%s_debil.wav" % nm(path); subprocess.run(["scp", "-o", "BatchMode=yes", "-q", "%s:house-tts/%s.wav" % (HOST, nm(path)), wav], check=True)
    os.makedirs(base.OUTPUT_DIR, exist_ok=True); subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-i", wav, "-b:a", "96k", out], check=True); os.unlink(wav); print("wrote %s (%d KB)" % (out, os.path.getsize(out) // 1024))
if __name__ == "__main__":
    if len(sys.argv) < 3 or sys.argv[1] not in ("start", "status", "fetch"): raise SystemExit(__doc__)
    {"start": start, "status": status, "fetch": fetch}[sys.argv[1]](sys.argv[2])
