#!/usr/bin/python3
"""pet_tts_local.py - drop-in for the edge-tts command the pet voice code used: same arguments, but offline (Kokoro-82M via kokoro-onnx, no network, no rate limit).
  pet_tts_local.py --voice <edge voice name> [--rate=+10%] [--pitch=+30Hz] --text <text> --write-media <out>
Why: from 2026-10-12 Microsoft's read-aloud service answers the edge-tts websocket with HTTP 403, and many pets speaking could also hit a rate limit.
How: the edge voice name from voices.pdl is mapped to a Kokoro voice by the KMAP rows in voices.pdl (`KMAP | edge voice | kokoro voice | language`); --rate becomes Kokoro speed (+10% -> 1.10); --pitch is ignored (Kokoro has no pitch control: say so in voices.pdl, not silently).
Output: a real WAV written to the --write-media path whatever its extension (the callers pass it to ffmpeg, which detects the format from the content).
Pronunciation map (slice S2): words listed as ACTIVE rows in &.widgits/concept-bank/data/audio/pron.pdl are spoken with the given phonemes (BOOK 19.audio/PRONUNCIATION-MAP.md); `--pron <file>` uses another map and also its `candidate` rows (a trial, nothing promoted); `--phonemes-only --text "..." [--lang en-us]` prints the phonemes the engine would use (with overrides) and exits (no audio, no --voice needed); `--no-pron` ignores the map; `--active-only` with `--pron` uses only the active rows. Text with no mapped word takes the exact old path.
Character (2026-10-14, BOOK 19.audio/VOICE-FX.md): the `FX` row in voices.pdl whose voice name and pitch column match this call (the C apps pass --voice and --pitch, not the species) gives pitch, formant, bright, warm, vibrato, breath and an accent; the effects are applied by the in-house C op _shared-lib/ops/audio_fx after synthesis, the accent (ACCENT rows) rewrites phonemes before synthesis. No FX row, or `--no-fx`, = the old sound.
Manners: 2 threads, one synthesis at a time (a lock), niceness lowered, a phrase longer than 240 characters is cut. Exit 0 = wrote the file; exit 1 = no sound (unknown voice, language Kokoro lacks such as Korean, missing model, any error): callers treat that as quiet failure.
Model files: ~/.local/share/kokoro/kokoro-v1.0.int8.onnx and voices-v1.0.bin (see #.#.calendar-dox/1-1.HARNECIENT.SMOL/convert_night_to_audio_local.py)."""
import os, sys, fcntl
here = os.path.dirname(os.path.abspath(__file__))
def arg(name, default=""):
    for i, a in enumerate(sys.argv):
        if a == name and i + 1 < len(sys.argv): return sys.argv[i + 1]
        if a.startswith(name + "="): return a.split("=", 1)[1]
    return default
pitch_arg = arg("--pitch", ""); voice, rate, out, text = arg("--voice"), arg("--rate", "+0%"), arg("--write-media"), arg("--text")
phon_only = "--phonemes-only" in sys.argv
if not text or (not phon_only and not (voice and out)): sys.exit(1)
kv = None
try:
    for ln in open(os.path.join(here, "..", "voices.pdl")):
        f = [x.strip() for x in ln.split("|")]
        if len(f) >= 4 and f[0] == "KMAP" and f[1] == voice: kv = (f[2], f[3])
except OSError: pass
if phon_only: kv = ("af_heart", arg("--lang", "en-us"))
if not kv or kv[0] in ("", "-"): sys.exit(1)
fx = {}; accents = {}
try:
    for ln in open(os.path.join(here, "..", "voices.pdl")):
        f = [x.strip() for x in ln.split("|")]
        if len(f) >= 4 and f[0] == "FX" and f[1] == voice and f[2] == pitch_arg:
            for kvp in f[3].split():
                if "=" in kvp: a, b = kvp.split("=", 1); fx[a] = b
        if len(f) >= 4 and f[0] == "ACCENT": accents.setdefault(f[1], []).append((f[2], f[3]))
except OSError: pass
if "--no-fx" in sys.argv: fx = {}
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
    import re
    pron_file = arg("--pron") or os.path.join(here, "..", "..", "..", "&.widgits", "concept-bank", "data", "audio", "pron.pdl"); trial = bool(arg("--pron")) and "--active-only" not in sys.argv; pron = {}
    if "--no-pron" not in sys.argv:
        try:
            for ln in open(pron_file):
                f = [x.strip() for x in ln.split("|")]
                if len(f) == 5 and f[0] == "PRON" and (f[4] == "active" or (trial and f[4] == "candidate")): pron[f[1]] = f[2]      # a later row wins; validity is pron_check's job
        except OSError: pass
    text = text[:240]; toks = re.findall(r"[A-Za-z0-9'\-]+|\s+|.", text); hit = [t for t in toks if t.lower() in pron]
    acc = accents.get(fx.get("accent", ""), []) if kv[1].startswith("en") else []
    def accent_word(p):
        for a, b in acc:
            if a.endswith("$"): p = p[:-len(a) + 1] + b if p.endswith(a[:-1]) and a[:-1] else p
            elif a.startswith("^"): p = b + p[len(a) - 1:] if p.startswith(a[1:]) else p
            else: p = p.replace(a, b)
        return p
    if hit or acc:
        ph = "".join(pron[t.lower()] if t.lower() in pron else (accent_word(k.tokenizer.phonemize(t, kv[1])) if re.match(r"[A-Za-z0-9]", t) else t) for t in toks); ph = re.sub(r"\s+", " ", ph).strip()
    else: ph = None
    if phon_only: print(ph if ph is not None else k.tokenizer.phonemize(text, kv[1])); sys.exit(0)
    samples, sr = k.create(ph if ph is not None else text, voice=kv[0], speed=speed, lang=kv[1], is_phonemes=ph is not None)
    with open(out, "wb") as fh: sf.write(fh, samples, sr, format="WAV")
    keys = [k2 for k2 in ("speed", "formant", "pitch", "bright", "warm", "vib_hz", "vib_cents", "breath") if k2 in fx]
    fxbin = os.path.join(here, "..", "..", "..", "&.widgits", "_shared-lib", "ops", "+x", "audio_fx.+x")
    if keys and os.path.exists(fxbin):
        import subprocess
        tmp2 = out + ".fx.wav"
        if subprocess.run([fxbin, out, tmp2] + ["%s=%s" % (k2, fx[k2]) for k2 in keys], capture_output=True).returncode == 0: os.replace(tmp2, out)
        else:
            try: os.unlink(tmp2)
            except OSError: pass
except Exception as e:
    sys.stderr.write("pet_tts_local: %s\n" % e); sys.exit(1)
