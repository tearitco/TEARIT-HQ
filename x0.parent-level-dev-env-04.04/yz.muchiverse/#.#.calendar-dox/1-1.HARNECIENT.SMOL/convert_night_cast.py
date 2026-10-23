#!/usr/bin/python3
"""convert_night_cast.py - narrate a NIGHT class with the CAST voice profiles (night_voices.pdl): Kokoro on debil makes one wav per line, then this machine applies each character's studio effects (pitch / formant / bright / warm, the in-house C op _shared-lib/ops/audio_fx) and joins the lines.
  convert_night_cast.py start  <NIGHT_xx.txt>   parse here, upload the job, start rendering on debil (nice, 6 threads, resumable; same remote half as convert_night_to_audio_remote.py)
  convert_night_cast.py status <NIGHT_xx.txt>   last progress line from debil
  convert_night_cast.py fetch  <NIGHT_xx.txt>   when done: copy the per-line wavs back, apply each character's effects, join with pauses, write audio-book/<name>_cast.mp3 (ffmpeg, 96k). An existing output is never overwritten.
Why a second converter: the first one (convert_night_to_audio_remote.py) has fixed stand-in voices and no per-character effects. This one reads night_voices.pdl, the same numbers as the voice-fx studio's VOICE2 rows, so a voice is changed in ONE place. Python is allowed here (one-off converter, like the others); nothing in the games uses it.
Needs: ssh to debil (key login), ffmpeg locally, the audio_fx binary (house root /&.widgits/_shared-lib/ops/+x/audio_fx.+x). No edge-tts, no pydub, no numpy locally.
Honest limit: the profiles are MEASURED (median F0 of a sample), not heard by the builder; the owner's ear decides."""
import json, os, re, subprocess, sys, wave
HERE = os.path.dirname(os.path.abspath(__file__))
HOST = os.environ.get("DEBIL", "debil@10.0.0.16")
OUT_DIR = os.path.join(HERE, "audio-book")
PAUSE_MS = 450
def house_fx():
    d = HERE
    for _ in range(8):
        cand = os.path.join(d, "44.xyz.01.00", "&.widgits", "_shared-lib", "ops", "+x", "audio_fx.+x")
        if os.path.exists(cand): return cand
        d = os.path.dirname(d)
    raise SystemExit("audio_fx.+x not found above %s" % HERE)
def nm(path): return os.path.splitext(os.path.basename(path))[0] + "_cast"
def ssh(cmd, timeout=60): return subprocess.run(["ssh", "-n", "-o", "BatchMode=yes", "-o", "ConnectTimeout=8", HOST, cmd], capture_output=True, text=True, timeout=timeout)
def read_profiles():
    prof = {}
    for ln in open(os.path.join(HERE, "night_voices.pdl"), encoding="utf-8"):
        if not ln.startswith("NIGHTVOICE"): continue
        f = [c.strip() for c in ln.split("|")]
        prof[f[1]] = {"voice": f[2], "lang": f[3], "pitch": f[4], "formant": f[5], "bright": f[6], "warm": f[7]}
    return prof
def chunk(text, size=400):
    out, cur = [], ""
    for part in re.split(r"(?<=[.!?])\s+", text):
        if not part: continue
        if len(cur) + len(part) + 1 > size and cur: out.append(cur); cur = part
        else: cur = ("%s %s" % (cur, part)).strip()
    if cur: out.append(cur)
    return out
def parse(path, prof):
    raw = open(path, encoding="utf-8").read(); spoken = []
    for line in raw.split("\n"):
        s = line.strip()
        if not s or s[0] in "#=" or s == "---": continue
        m = re.match(r"^\*\*([A-Z ]+?):\*\*\s*(.*)$", s)
        if not m: continue
        who, text = m.group(1).strip(), m.group(2).strip()
        if who not in prof: raise SystemExit("no voice profile for %r in night_voices.pdl" % who)
        text = re.sub(r"\s*[-—]{2,}\s*", ". ", text).replace("**", "").replace("`", ""); text = re.sub(r"\s+", " ", text).strip()
        if text: spoken.append((who, text))
    return spoken
def start(path):
    prof = read_profiles(); lines = [{"voice": prof[w]["voice"], "lang": prof[w]["lang"], "pieces": chunk(t)} for w, t in parse(path, prof)]
    job = "/tmp/%s.json" % nm(path); json.dump({"pause_ms": PAUSE_MS, "lines": lines}, open(job, "w")); print("%d lines" % len(lines))
    subprocess.run(["scp", "-o", "BatchMode=yes", "-q", job, "%s:house-tts/%s.json" % (HOST, nm(path))], check=True)
    subprocess.run(["scp", "-o", "BatchMode=yes", "-q", os.path.join(HERE, "remote_tts", "render_lines.py"), "%s:house-tts/render_lines.py" % HOST], check=True)
    r = ssh("cd ~/house-tts && setsid -f nohup nice -n 10 env PYTHONPATH=$HOME/house-tts/lib python3 render_lines.py %s.json %s.wav --threads 6 > %s.log 2>&1 < /dev/null; echo started" % (nm(path), nm(path), nm(path))); print(r.stdout.strip() or r.stderr.strip())
def status(path):
    r = ssh("tail -2 ~/house-tts/%s.log 2>&1; ps -eo args | grep -c '[r]ender_lines.py %s'" % (nm(path), nm(path))); L = r.stdout.strip().split("\n"); print("\n".join(L[:-1])); print("RUNNING" if L[-1].strip() not in ("", "0") else "not running")
def wav_read(p):
    w = wave.open(p, "rb"); par = w.getparams(); data = w.readframes(w.getnframes()); w.close(); return par, data
def fetch(path):
    out = os.path.join(OUT_DIR, nm(path) + ".mp3")
    if os.path.exists(out): print("skip (exists): %s" % out); return
    prof = read_profiles(); spoken = parse(path, prof); tmp = "/tmp/%s" % nm(path)
    subprocess.run(["scp", "-o", "BatchMode=yes", "-q", "-r", "%s:house-tts/parts/%s" % (HOST, nm(path)), "/tmp/"], check=True)      # lands in /tmp/<name>/NNNN.wav
    fx = house_fx(); chunks = []; par0 = None
    for i, (who, _t) in enumerate(spoken, 1):
        src = os.path.join(tmp, "%04d.wav" % i); dst = os.path.join(tmp, "%04d_fx.wav" % i); p = prof[who]
        args = [fx, src, dst, "pitch=%s" % p["pitch"], "formant=%s" % p["formant"]]
        if float(p["bright"]): args.append("bright=%s" % p["bright"])
        if float(p["warm"]): args.append("warm=%s" % p["warm"])
        r = subprocess.run(["nice", "-n", "10"] + args, capture_output=True, text=True)
        if r.returncode != 0 or not os.path.exists(dst): raise SystemExit("audio_fx failed on line %d (%s): %s" % (i, who, r.stderr.strip()[:200]))
        par, data = wav_read(dst)
        if par0 is None: par0 = par
        if (par.nchannels, par.sampwidth, par.framerate) != (par0.nchannels, par0.sampwidth, par0.framerate): raise SystemExit("line %d has a different wav format" % i)
        chunks.append(data); chunks.append(b"\0" * (int(par0.framerate * PAUSE_MS / 1000.0) * par0.nchannels * par0.sampwidth))
        if i % 10 == 0: print("  fx %d/%d" % (i, len(spoken)), flush=True)
    joined = "/tmp/%s_joined.wav" % nm(path); w = wave.open(joined, "wb"); w.setparams(par0); [w.writeframes(c) for c in chunks]; w.close()
    os.makedirs(OUT_DIR, exist_ok=True); subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-i", joined, "-b:a", "96k", out], check=True); os.unlink(joined)
    print("wrote %s (%d KB)" % (out, os.path.getsize(out) // 1024))
if __name__ == "__main__":
    if len(sys.argv) < 3 or sys.argv[1] not in ("start", "status", "fetch"): raise SystemExit(__doc__)
    {"start": start, "status": status, "fetch": fetch}[sys.argv[1]](sys.argv[2])
