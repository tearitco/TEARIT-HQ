#!/usr/bin/python3
"""render_lines.py - REMOTE half of convert_night_to_audio_remote.py: runs on debil (no ffmpeg, no pydub). Renders the lines in a job file with Kokoro-82M (kokoro-onnx), writes one wav per line (resumable) and the joined wav.
  PYTHONPATH=~/house-tts/lib python3 render_lines.py job.json out.wav [--threads 6] [--max-temp 80]
job.json: {"pause_ms": 450, "lines": [{"voice": "bf_emma", "lang": "en-gb", "pieces": ["text", ...]}, ...]}   (the local side parsed the script and chunked the text, so this file needs no parser)
Parts: ~/house-tts/parts/<job name>/NNNN.wav; an existing part is not rendered again (a dropped ssh or a restart resumes). Progress lines go to stdout (the caller tails the log). Model files: ~/house-tts/kokoro-v1.0.int8.onnx + voices-v1.0.bin.
Install (one time, no sudo): python3 -m pip install --target ~/house-tts/lib kokoro-onnx soundfile numpy onnxruntime."""
import glob, json, os, sys, time
import numpy as np, soundfile as sf, onnxruntime as ort
from kokoro_onnx import Kokoro
HOME = os.path.expanduser("~/house-tts")
def arg(n, d):
    return sys.argv[sys.argv.index(n) + 1] if n in sys.argv else d
def hottest():
    t = []
    for p in glob.glob("/sys/class/thermal/thermal_zone*/temp"):
        try: t.append(int(open(p).read().strip()) / 1000.0)
        except (OSError, ValueError): pass
    return max(t) if t else 0.0
job_path, out = sys.argv[1], sys.argv[2]; threads = int(arg("--threads", "6")); max_temp = float(arg("--max-temp", "80"))
job = json.load(open(job_path)); name = os.path.splitext(os.path.basename(job_path))[0]; parts = os.path.join(HOME, "parts", name); os.makedirs(parts, exist_ok=True)
opts = ort.SessionOptions(); opts.intra_op_num_threads, opts.inter_op_num_threads = threads, 1
k = Kokoro.from_session(ort.InferenceSession(os.path.join(HOME, "kokoro-v1.0.int8.onnx"), sess_options=opts, providers=["CPUExecutionProvider"]), os.path.join(HOME, "voices-v1.0.bin"))
t0 = time.time(); lines = job["lines"]; sr_out = None; total = 0.0; pause = None; chunks = []
for i, ln in enumerate(lines, 1):
    part = os.path.join(parts, "%04d.wav" % i)
    if not os.path.exists(part):
        segs = []
        for piece in ln["pieces"]:
            while hottest() > max_temp: print("  too hot, pausing 20 s", flush=True); time.sleep(20)
            s, sr = k.create(piece, voice=ln["voice"], speed=1.0, lang=ln["lang"]); segs.append(np.asarray(s, dtype=np.float32)); sr_out = sr
        sf.write(part + ".tmp.wav", np.concatenate(segs), sr_out); os.replace(part + ".tmp.wav", part)
    d, sr = sf.read(part, dtype="float32"); sr_out = sr; chunks.append(d); chunks.append(np.zeros(int(sr * job.get("pause_ms", 450) / 1000.0), dtype=np.float32)); total += (len(d) / sr) + job.get("pause_ms", 450) / 1000.0
    if i % 5 == 0 or i == len(lines): print("  %d/%d lines, %.0fs audio, %.0fs elapsed, %.0f C" % (i, len(lines), total, time.time() - t0, hottest()), flush=True)
sf.write(out + ".tmp.wav", np.concatenate(chunks), sr_out); os.replace(out + ".tmp.wav", out); print("DONE %s %.0fs audio" % (out, total), flush=True)
