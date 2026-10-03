#!/usr/bin/env python3
"""
convert_night32_to_audio.py - narrate NIGHT_32 to mp3.

Follows the house audio-book convention (see convert_user_guide_to_audio.py):
edge-tts with the Maxine voice, chunked so a long document does not blow
past the service's per-request limit, then joined with pydub.

Creates: audio-book/NIGHT_32_EXTERNAL_REVIEW_3_DAYS_5_BRANCHES.mp3

Usage: python3 convert_night32_to_audio.py
"""

import asyncio
import os
import re
import tempfile

import edge_tts
from pydub import AudioSegment

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
INPUT_FILE = os.path.join(BASE_DIR, "NIGHT_32_EXTERNAL_REVIEW_3_DAYS_5_BRANCHES.txt")
OUTPUT_DIR = os.path.join(BASE_DIR, "audio-book")
OUTPUT_FILE = os.path.join(
    OUTPUT_DIR, "NIGHT_32_EXTERNAL_REVIEW_3_DAYS_5_BRANCHES.mp3")

MAXINE_VOICE = "en-GB-MaisieNeural"
MAXINE_SETTINGS = {"rate": "+0%", "pitch": "+0Hz"}

# edge-tts truncates long inputs, so the text is split into chunks and
# narrated separately. 1800 chars is well inside what the service accepts
# and keeps each request short enough to succeed on a slow link.
CHUNK_CHARS = 1800


def load_text(path):
    with open(path, "r", encoding="utf-8") as fh:
        raw = fh.read()

    lines = []
    for line in raw.split("\n"):
        s = line.rstrip()
        # Skip the pure-ASCII rules and box drawing: they read as noise.
        if s.startswith("=") or s.startswith("-"):
            continue
        if not s.strip():
            continue
        # Drop the heavy markup but keep the words.
        s = re.sub(r"\*{1,2}", "", s)
        s = s.replace("#", "").replace("`", "")
        s = s.replace(">>", "Note:")
        s = re.sub(r"_+", " ", s)
        s = s.replace("|", ", ")
        s = re.sub(r"[ \t]+", " ", s).strip()
        if s:
            lines.append(s)
    return "\n".join(lines)


def chunk(text, size=CHUNK_CHARS):
    """Split on sentence boundaries where possible so the narration does
    not cut mid-word."""
    out, cur = [], ""
    for para in text.split("\n"):
        # Sentences, keeping the terminator.
        parts = re.split(r"(?<=[.!?])\s+", para)
        for p in parts:
            if not p:
                continue
            if len(cur) + len(p) + 1 > size and cur:
                out.append(cur)
                cur = p
            else:
                cur = f"{cur} {p}".strip()
    if cur:
        out.append(cur)
    return out


async def synth(text, out_path):
    communicate = edge_tts.Communicate(text, MAXINE_VOICE, **MAXINE_SETTINGS)
    await communicate.save(out_path)


async def main():
    if not os.path.exists(INPUT_FILE):
        raise SystemExit(f"input not found: {INPUT_FILE}")
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    text = load_text(INPUT_FILE)
    pieces = chunk(text)
    print(f"narrating {len(text)} chars in {len(pieces)} chunks -> {MAXINE_VOICE}")

    combined = None
    with tempfile.TemporaryDirectory() as tmp:
        for i, piece in enumerate(pieces, 1):
            part = os.path.join(tmp, f"part{i:03d}.mp3")
            for attempt in range(3):
                try:
                    await synth(piece, part)
                    break
                except Exception as exc:  # noqa: BLE001
                    print(f"  chunk {i} attempt {attempt + 1} failed: {exc}")
                    if attempt == 2:
                        raise
            seg = AudioSegment.from_file(part, format="mp3")
            combined = seg if combined is None else combined + seg
            print(f"  chunk {i}/{len(pieces)} ok ({len(seg) / 1000:.1f}s)")

    combined.export(OUTPUT_FILE, format="mp3")
    size_mb = os.path.getsize(OUTPUT_FILE) / (1024 * 1024)
    print(f"wrote {OUTPUT_FILE} ({len(combined) / 1000:.0f}s, {size_mb:.1f} MB)")


if __name__ == "__main__":
    asyncio.run(main())