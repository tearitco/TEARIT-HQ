#!/usr/bin/python3
"""one_line.py (runs on debil): one caption -> a wav on stdout. usage: PYTHONPATH=~/house-tts/lib python3 one_line.py <base64 text> [voice=af_heart] [threads=4]"""
import sys, os, base64, io
import numpy as np, soundfile as sf, onnxruntime as ort
from kokoro_onnx import Kokoro
H = os.path.expanduser("~/house-tts")
text = base64.b64decode(sys.argv[1]).decode("utf-8"); voice = sys.argv[2] if len(sys.argv) > 2 else "af_heart"; th = int(sys.argv[3]) if len(sys.argv) > 3 else 4
o = ort.SessionOptions(); o.intra_op_num_threads, o.inter_op_num_threads = th, 1
k = Kokoro.from_session(ort.InferenceSession(os.path.join(H, "kokoro-v1.0.int8.onnx"), sess_options=o, providers=["CPUExecutionProvider"]), os.path.join(H, "voices-v1.0.bin"))
s, sr = k.create(text, voice=voice, speed=1.0, lang="en-us")
b = io.BytesIO(); sf.write(b, s, sr, format="WAV"); sys.stdout.buffer.write(b.getvalue())
