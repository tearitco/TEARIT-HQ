#!/usr/bin/python3
"""synth_set.py - one-off converter/harness (python allowed for one-offs; a pal harness is the house goal, not built).
  synth_set.py make <outdir>              speak the sentences in data/synth_set.pdl with the house voices (pet_tts_local.py, offline Kokoro) -> <outdir>/NN.wav (16 kHz mono)
  synth_set.py eval <outdir> [--prompt T]  run ops/+x/hear.+x on every wav (VAD gate + whisper tiny.en), then ops/+x/wer.+x against the reference; print a table and the total WER
SYNTHETIC speech: this measures the pipeline and the house vocabulary, NOT the owner's voice (BOOK 19.audio/LISTENER-STT-DESIGN.md section 6, L1)."""
import os, subprocess, sys, tempfile
here = os.path.dirname(os.path.abspath(__file__)); L = os.path.join(here, ".."); HOUSE = os.path.abspath(os.path.join(L, "..", ".."))
rows = []
for ln in open(os.path.join(L, "data", "synth_set.pdl")):
    f = [x.strip() for x in ln.split("|")]
    if len(f) == 4 and f[0] == "CLIP": rows.append((f[1], f[2], f[3]))      # CLIP | id | voice | reference text
def make(out):
    os.makedirs(out, exist_ok=True)
    for cid, voice, text in rows:
        raw = os.path.join(out, cid + ".raw.wav"); wav = os.path.join(out, cid + ".wav")
        subprocess.run(["nice", "-n", "15", sys.executable, os.path.join(HOUSE, "@.apps", "pet-trainer", "ops", "pet_tts_local.py"), "--voice", voice, "--no-fx", "--text", text, "--write-media", raw], check=True)
        subprocess.run(["ffmpeg", "-loglevel", "quiet", "-y", "-i", raw, "-ar", "16000", "-ac", "1", "-c:a", "pcm_s16le", wav], check=True); os.unlink(raw); print("made", cid, voice)
def evaluate(out, prompt):
    data = tempfile.mkdtemp(prefix="synth_eval_"); errs = words = 0; print("%-4s %-6s %-48s %s" % ("id", "WER", "reference", "heard"))
    for cid, voice, text in rows:
        cmd = [os.path.join(L, "ops", "+x", "hear.+x"), os.path.join(out, cid + ".wav"), "--data", data, "--consent", "synthetic"] + (["--prompt", prompt] if prompt else [])
        r = subprocess.run(cmd, capture_output=True, text=True); heard = r.stdout.strip() if r.returncode == 0 else "<rc %d>" % r.returncode
        w = subprocess.run([os.path.join(L, "ops", "+x", "wer.+x"), text, heard if r.returncode == 0 else ""], capture_output=True, text=True).stdout.split("|")
        e, n = int(w[2]), int(w[3]); errs += e; words += n; print("%-4s %-6.2f %-48s %s" % (cid, e / n, text[:48], heard[:70]))
    print("TOTAL: %d errors / %d words = WER %.3f%s" % (errs, words, errs / words, "  (prompt: %s)" % prompt if prompt else ""))
if __name__ == "__main__":
    if len(sys.argv) >= 3 and sys.argv[1] == "make": make(sys.argv[2])
    elif len(sys.argv) >= 3 and sys.argv[1] == "eval": evaluate(sys.argv[2], sys.argv[sys.argv.index("--prompt") + 1] if "--prompt" in sys.argv else None)
    else: print(__doc__)
