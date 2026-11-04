#!/usr/bin/env python3
"""remote_tts.py - drop-in for pet_tts_local.py (--voice V --text T --write-media OUT) that narrates on debil (house policy: heavy work goes to debil/Mac, not the desktop).
The caption is sent base64 over ssh to ~/house-tts/one_line.py (copy of presentation/remote_one_line.py; Kokoro-82M int8, 4 threads, niced) and the wav comes back on stdout.
If debil cannot be reached the local, throttled pet_tts_local.py is used instead (one thread, nice 19). Env: PRESENT_TTS_HOST (default debil@10.0.0.16)."""
import sys, os, base64, subprocess
def arg(n, d=""):
    return sys.argv[sys.argv.index(n) + 1] if n in sys.argv and sys.argv.index(n) + 1 < len(sys.argv) else d
text, out = arg("--text"), arg("--write-media")
if not text or not out: sys.exit(1)
host = os.environ.get("PRESENT_TTS_HOST", "debil@10.0.0.16")
try:
    r = subprocess.run(["ssh", "-n", "-o", "BatchMode=yes", "-o", "ConnectTimeout=6", host, "cd ~/house-tts && PYTHONPATH=lib nice -n 10 python3 one_line.py %s af_heart 4" % base64.b64encode(text.encode()).decode()], capture_output=True, timeout=180)
    if r.returncode == 0 and r.stdout[:4] == b"RIFF": open(out, "wb").write(r.stdout); sys.exit(0)
except Exception: pass
here = os.path.dirname(os.path.abspath(__file__)); local = os.path.join(here, "..", "..", "..", "@.apps", "pet-trainer", "ops", "pet_tts_local.py")
os.environ["PET_TTS_THREADS"] = "1"; os.nice(19)
os.execv(sys.executable, [sys.executable, local] + sys.argv[1:])
