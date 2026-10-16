#!/usr/bin/python3
"""
convert_night_to_audio_local.py - narrate NIGHT class scripts to mp3 OFFLINE with Kokoro-82M (kokoro-onnx), no network.

Why: on 2026-10-12 the Microsoft read-aloud service began answering the edge-tts websocket handshake with HTTP 403 for every voice
(the voice-list call still worked), so convert_night_to_audio.py could not make NIGHT 49. This is the local fallback. It reads the
same script format (same speaker lines, same parser as convert_night_to_audio.py) but picks its own voice per CHARACTER from
LOCAL_VOICES below (the script header's edge voices are ignored). Output has a _local suffix so it is never mistaken for the
Microsoft-voiced files: audio-book/<name>_local.mp3 (an existing output is SKIPPED, never overwritten).

Usage:   nice -n 15 ionice -c3 /usr/bin/python3 convert_night_to_audio_local.py NIGHT_49_HOW_WE_BUILD_FEEDS_WHAT_THE_PETS_LEARN.txt
Needs:   pip --user kokoro-onnx soundfile (and the existing pydub + ffmpeg); model files in ~/.local/share/kokoro/ :
         kokoro-v1.0.int8.onnx and voices-v1.0.bin (github.com/thewh1teagle/kokoro-onnx, release model-files-v1.0).
Speed:   measured 2026-10-12 under nice: about 2.5 s of compute per second of audio on this 8-core machine.
"""
import glob
import io
import os
import sys
import time

import numpy as np
import onnxruntime as ort
import soundfile as sf
from kokoro_onnx import Kokoro
from pydub import AudioSegment

import convert_night_to_audio as base  # parser + chunker shared with the edge-tts converter

MODEL_DIR = os.path.expanduser("~/.local/share/kokoro")
# character -> (kokoro voice, language code). British female/male voices for MAXINE / narration, American for the other two.
LOCAL_VOICES = {
    "MAXINE": ("bf_emma", "en-gb"),
    "TOMO": ("af_heart", "en-us"),
    "RAHWEH": ("am_michael", "en-us"),
    "THE NARRATOR": ("bm_george", "en-gb"),
}
PAUSE_MS = 450
THREADS = int(os.environ.get("KOKORO_THREADS", "2"))   # 2 of 8 cores: the first all-core run was followed by a machine reset (cause unproven)
MAX_TEMP_C = int(os.environ.get("KOKORO_MAX_TEMP_C", "70"))
PARTS_DIR = os.path.expanduser("~/.cache/kokoro_parts")  # one wav per script line so a crash/reset can resume


def hottest_c():
    t = []
    for p in glob.glob("/sys/class/thermal/thermal_zone*/temp"):
        try:
            t.append(int(open(p).read().strip()) / 1000.0)
        except (OSError, ValueError):
            pass
    return max(t) if t else 0.0


def cool_wait():
    while hottest_c() > MAX_TEMP_C:
        print(f"  too hot ({hottest_c():.0f} C > {MAX_TEMP_C}), pausing 20 s", flush=True)
        time.sleep(20)


def narrate(path, kokoro):
    out_file = os.path.join(base.OUTPUT_DIR, os.path.splitext(os.path.basename(path))[0] + "_local.mp3")
    if os.path.exists(out_file):
        print(f"skip (exists): {out_file}")
        return
    raw = open(path, encoding="utf-8").read()
    # parse with the script's own speakers; map every header speaker to a local voice
    voices = {who: who for who in base.read_voices(raw)}
    missing = [w for w in voices if w not in LOCAL_VOICES]
    if missing:
        raise SystemExit(f"no local voice for: {missing}")
    lines = base.parse_script(raw, voices)
    print(f"{len(lines)} lines, {sum(len(t) for _, t in lines)} chars")
    os.makedirs(base.OUTPUT_DIR, exist_ok=True)
    parts = os.path.join(PARTS_DIR, os.path.splitext(os.path.basename(path))[0])
    os.makedirs(parts, exist_ok=True)
    audio, pause, t0 = AudioSegment.empty(), AudioSegment.silent(PAUSE_MS), time.time()
    for i, (who, text) in enumerate(lines, 1):
        part = os.path.join(parts, f"{i:04d}.wav")
        if not os.path.exists(part):                      # resume: only render lines not saved yet
            voice, lang = LOCAL_VOICES[who]
            seg = AudioSegment.empty()
            for piece in base.chunk(text, 400):
                cool_wait()
                samples, sr = kokoro.create(piece, voice=voice, speed=1.0, lang=lang)
                buf = io.BytesIO()
                sf.write(buf, samples, sr, format="WAV")
                buf.seek(0)
                seg += AudioSegment.from_wav(buf)
            seg.export(part + ".tmp", format="wav")
            os.replace(part + ".tmp", part)
        audio += AudioSegment.from_wav(part) + pause
        if i % 5 == 0 or i == len(lines):
            print(f"  {i}/{len(lines)} lines, {len(audio) / 1000:.0f}s audio, {time.time() - t0:.0f}s elapsed, {hottest_c():.0f} C", flush=True)
    audio.export(out_file, format="mp3", bitrate="96k")
    print(f"wrote {out_file} ({len(audio) / 1000:.0f}s, {os.path.getsize(out_file) // 1024} KB)")


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    opts = ort.SessionOptions()
    opts.intra_op_num_threads, opts.inter_op_num_threads = THREADS, 1
    sess = ort.InferenceSession(os.path.join(MODEL_DIR, "kokoro-v1.0.int8.onnx"), sess_options=opts, providers=["CPUExecutionProvider"])
    kokoro = Kokoro.from_session(sess, os.path.join(MODEL_DIR, "voices-v1.0.bin"))
    for p in sys.argv[1:]:
        narrate(p, kokoro)


if __name__ == "__main__":
    main()
