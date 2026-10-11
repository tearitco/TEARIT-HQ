#!/usr/bin/env python3
"""
convert_night32_to_audio.py - narrate the NIGHT 32 night class to mp3.

Follows the house audio-book convention (see convert_user_guide_to_audio.py
and NIGHT_30/NIGHT_31): edge-tts, one voice per CHARACTER, pydub to join.

Unlike the user-guide converter this one is MULTI-VOICE, because a night
class is a dialogue. Each **CHARACTER:** line is spoken in that character's
assigned neural voice:

    MAXINE      en-GB-MaisieNeural
    TOMO        zh-CN-XiaoxiaoNeural      (the teacher)
    RAHWEH      zh-CN-YunxiaNeural
    THE NARRATOR zh-CN-XiaoxiaoNeural

TOMO and THE NARRATOR share a voice, matching the Voices: header line in
every existing night class.

Creates: audio-book/NIGHT_32_THE_MACHINE_THAT_CAN_WRITE.mp3

Usage: python3 convert_night32_to_audio.py
"""

import asyncio
import os
import re
import tempfile

import edge_tts
from pydub import AudioSegment

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
INPUT_FILE = os.path.join(BASE_DIR, "NIGHT_32_THE_MACHINE_THAT_CAN_WRITE.txt")
OUTPUT_DIR = os.path.join(BASE_DIR, "audio-book")
OUTPUT_FILE = os.path.join(OUTPUT_DIR, "NIGHT_32_THE_MACHINE_THAT_CAN_WRITE.mp3")

# Voices, taken verbatim from the night class's own "Voices:" header so
# the script and the audio cannot drift apart.
VOICES = {
    "MAXINE": "en-GB-MaisieNeural",
    "TOMO": "zh-CN-XiaoxiaoNeural",
    "RAHWEH": "zh-CN-YunxiaNeural",
    "THE NARRATOR": "zh-CN-XiaoxiaoNeural",
}
SETTINGS = {"rate": "+0%", "pitch": "+0Hz"}

CHUNK_CHARS = 1800

def parse_script(path):
    """-> [(voice, text), ...] with header and markup removed.

    Lines beginning "Say ..." are KEPT. They read like stage directions but
    they are spoken dialogue in this format: one character asking the next
    to elaborate, and the answer is the next speaker's line. Dropping them
    - which the first version of this script did - removes roughly a third
    of the class and turns the remaining narration into an unattributed
    monologue."""
    with open(path, "r", encoding="utf-8") as fh:
        raw = fh.read()

    lines = []
    speaker_re = re.compile(r"^\*\*([A-Z ]+?):\*\*\s*(.*)$")
    for line in raw.split("\n"):
        s = line.strip()
        if not s or s.startswith("#") or s.startswith("=") or s == "---":
            continue
        m = speaker_re.match(s)
        if not m:
            continue
        who, text = m.group(1).strip(), m.group(2).strip()
        if who not in VOICES:
            continue
        if not text:
            continue
        # Long dash runs used as stage pause read better as a full stop.
        text = re.sub(r"\s*[-—]{2,}\s*", ". ", text)
        text = text.replace("**", "").replace("`", "")
        text = re.sub(r"\s+", " ", text).strip()
        if text:
            lines.append((who, text))
    return lines


def chunk(text, size=CHUNK_CHARS):
    out, cur = [], ""
    for part in re.split(r"(?<=[.!?])\s+", text):
        if not part:
            continue
        if len(cur) + len(part) + 1 > size and cur:
            out.append(cur)
            cur = part
        else:
            cur = f"{cur} {part}".strip()
    if cur:
        out.append(cur)
    return out


async def synth(text, voice, out_path):
    await edge_tts.Communicate(text, voice, **SETTINGS).save(out_path)


async def main():
    if not os.path.exists(INPUT_FILE):
        raise SystemExit(f"input not found: {INPUT_FILE}")
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    spoken = parse_script(INPUT_FILE)
    if not spoken:
        raise SystemExit("no spoken lines parsed - check the script format")

    pieces = []
    for who, text in spoken:
        pieces.append((VOICES[who], text))

    total_chars = sum(len(t) for _, t in pieces)
    jobs = []
    for voice, text in pieces:
        for c in chunk(text):
            jobs.append((voice, c))

    print(f"narrating {len(spoken)} lines / {total_chars} chars "
          f"in {len(jobs)} chunks across {len(VOICES)} voices")

    combined = None
    with tempfile.TemporaryDirectory() as tmp:
        for i, (voice, text) in enumerate(jobs, 1):
            part = os.path.join(tmp, f"p{i:03d}.mp3")
            for attempt in range(3):
                try:
                    await synth(text, voice, part)
                    break
                except Exception as exc:  # noqa: BLE001
                    print(f"  chunk {i} attempt {attempt + 1} failed: {exc}")
                    if attempt == 2:
                        raise
            seg = AudioSegment.from_file(part, format="mp3")
            combined = seg if combined is None else combined + seg
            print(f"  chunk {i}/{len(jobs)} [{voice}] ok")

    combined.export(OUTPUT_FILE, format="mp3")
    size_mb = os.path.getsize(OUTPUT_FILE) / (1024 * 1024)
    print(f"wrote {OUTPUT_FILE} ({len(combined) / 1000:.0f}s, {size_mb:.1f} MB)")


if __name__ == "__main__":
    asyncio.run(main())