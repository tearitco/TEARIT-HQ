#!/usr/bin/env python3
"""pet_stt.py <wav 16k mono> - offline speech to text (vosk small English model under state/stt). Prints the text, nothing else."""
import sys, os, json, wave
here = os.path.dirname(os.path.abspath(__file__)); base = os.environ.get("PET_STT_DIR", os.path.join(here, "..", "state", "stt"))
sys.path.insert(0, os.path.join(base, "lib"))
from vosk import Model, KaldiRecognizer, SetLogLevel
SetLogLevel(-1)
w = wave.open(sys.argv[1], "rb"); r = KaldiRecognizer(Model(os.path.join(base, "model")), w.getframerate()); out = []
while True:
    d = w.readframes(4000)
    if not d: break
    if r.AcceptWaveform(d): out.append(json.loads(r.Result()).get("text", ""))
out.append(json.loads(r.FinalResult()).get("text", ""))
print(" ".join(x for x in out if x))
