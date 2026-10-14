#!/usr/bin/python3
"""pet_tts_local.py - drop-in for the edge-tts command the pet voice code used: same arguments, but offline (Kokoro-82M via kokoro-onnx, no network, no rate limit).
  pet_tts_local.py --voice <edge voice name> [--rate=+10%] [--pitch=+30Hz] --text <text> --write-media <out>
Why: from 2026-10-12 Microsoft's read-aloud service answers the edge-tts websocket with HTTP 403, and many pets speaking could also hit a rate limit.
How: the edge voice name from voices.pdl is mapped to a Kokoro voice by the KMAP rows in voices.pdl (`KMAP | edge voice | kokoro voice | language`); --rate becomes Kokoro speed (+10% -> 1.10); --pitch is ignored (Kokoro has no pitch control: say so in voices.pdl, not silently).
Output: a real WAV written to the --write-media path whatever its extension (the callers pass it to ffmpeg, which detects the format from the content).
Manners: 2 threads, one synthesis at a time (a lock), niceness lowered, a phrase longer than 240 characters is cut. Exit 0 = wrote the file; exit 1 = no sound (unknown voice, language Kokoro lacks such as Korean, missing model, any error): callers treat that as quiet failure.
Model files: ~/.local/share/kokoro/kokoro-v1.0.int8.onnx and voices-v1.0.bin (see #.#.calendar-dox/1-1.HARNECIENT.SMOL/convert_night_to_audio_local.py)."""
import os, sys, fcntl
here = os.path.dirname(os.path.abspath(__file__))
def arg(name, default=""):
    for i, a in enumerate(sys.argv):
        if a == name and i + 1 < len(sys.argv): return sys.argv[i + 1]
        if a.startswith(name + "="): return a.split("=", 1)[1]
    return default
voice, rate, out, text = arg("--voice"), arg("--rate", "+0%"), arg("--write-media"), arg("--text")
if not (voice and out and text): sys.exit(1)
kv = None
try:
    for ln in open(os.path.join(here, "..", "voices.pdl")):
        f = [x.strip() for x in ln.split("|")]
        if len(f) >= 4 and f[0] == "KMAP" and f[1] == voice: kv = (f[2], f[3])
except OSError: pass
if not kv or kv[0] in ("", "-"): sys.exit(1)
try: speed = max(0.5, min(1.6, 1.0 + float(rate.rstrip("%")) / 100.0))
except ValueError: speed = 1.0
try:
    os.nice(10); os.makedirs(os.path.expanduser("~/.cache/pet_tts"), exist_ok=True)
    lock = open(os.path.expanduser("~/.cache/pet_tts/synth.lock"), "w"); fcntl.flock(lock, fcntl.LOCK_EX)      # one synthesis at a time
    import onnxruntime as ort
    _orig = ort.InferenceSession
    ort.InferenceSession = lambda p, *a, **k: _orig(p, sess_options=_so(), *a, **{x: y for x, y in k.items() if x != "sess_options"})
    def _so():
        s = ort.SessionOptions(); s.intra_op_num_threads = 2; s.inter_op_num_threads = 1; return s
    from kokoro_onnx import Kokoro
    import soundfile as sf
    d = os.path.expanduser("~/.local/share/kokoro")
    k = Kokoro(os.path.join(d, "kokoro-v1.0.int8.onnx"), os.path.join(d, "voices-v1.0.bin"))
    samples, sr = k.create(text[:240], voice=kv[0], speed=speed, lang=kv[1])
    with open(out, "wb") as fh: sf.write(fh, samples, sr, format="WAV")
except Exception as e:
    sys.stderr.write("pet_tts_local: %s\n" % e); sys.exit(1)
