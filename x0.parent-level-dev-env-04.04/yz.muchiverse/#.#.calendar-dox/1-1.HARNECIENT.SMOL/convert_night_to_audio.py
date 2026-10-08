#!/usr/bin/python3
"""
convert_night_to_audio.py - narrate one or more NIGHT class scripts to mp3 (generalised convert_night32_to_audio.py).

Same house convention: edge-tts, one neural voice per CHARACTER, pydub to join. The voices are read from each script's own
"# Voices: NAME (voice), ..." header line so the script and the audio cannot drift apart.

Usage:   nice -n 15 /usr/bin/python3 convert_night_to_audio.py NIGHT_35_THE_SCHOOL_IS_A_PLACE.txt [more.txt ...]
Output:  audio-book/<same name>.mp3   (an existing output is SKIPPED, never overwritten)
Needs:   edge_tts and pydub in /usr/bin/python3, and ffmpeg. edge-tts sends the spoken text to Microsoft's online voice service
         (the same as every existing audio-book file here), so only narrate text that is fine to send out.
"""
import asyncio
import os
import re
import sys
import tempfile

import edge_tts
from pydub import AudioSegment

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
OUTPUT_DIR = os.path.join(BASE_DIR, "audio-book")
SETTINGS = {"rate": "+0%", "pitch": "+0Hz"}
CHUNK_CHARS = 1800


def read_voices(raw):
    for line in raw.split("\n"):
        if line.startswith("# Voices:"):
            pairs = re.findall(r"([A-Z][A-Z ]*?) \(([a-z]{2}-[A-Z]{2}-\w+)\)", line)
            if pairs:
                return {who.strip(): voice for who, voice in pairs}
    raise SystemExit("no usable '# Voices:' header line")


def parse_script(raw, voices):
    lines, speaker_re = [], re.compile(r"^\*\*([A-Z ]+?):\*\*\s*(.*)$")
    for line in raw.split("\n"):
        s = line.strip()
        if not s or s.startswith("#") or s.startswith("=") or s == "---":
            continue
        m = speaker_re.match(s)
        if not m:
            continue
        who, text = m.group(1).strip(), m.group(2).strip()
        if who not in voices or not text:
            continue
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


async def narrate(path):
    out_file = os.path.join(OUTPUT_DIR, os.path.splitext(os.path.basename(path))[0] + ".mp3")
    if os.path.exists(out_file):
        print(f"skip (exists): {out_file}")
        return
    raw = open(path, "r", encoding="utf-8").read()
    voices = read_voices(raw)
    spoken = parse_script(raw, voices)
    if not spoken:
        raise SystemExit(f"no spoken lines parsed in {path}")
    jobs = [(voices[who], c) for who, text in spoken for c in chunk(text)]
    print(f"{os.path.basename(path)}: {len(spoken)} lines, {sum(len(t) for _, t in spoken)} chars, {len(jobs)} chunks", flush=True)
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    combined = None
    with tempfile.TemporaryDirectory() as tmp:
        for i, (voice, text) in enumerate(jobs, 1):
            part = os.path.join(tmp, f"p{i:03d}.mp3")
            for attempt in range(3):
                try:
                    await edge_tts.Communicate(text, voice, **SETTINGS).save(part)
                    break
                except Exception as exc:  # noqa: BLE001
                    print(f"  chunk {i} attempt {attempt + 1} failed: {exc}", flush=True)
                    if attempt == 2:
                        raise
            seg = AudioSegment.from_file(part, format="mp3")
            combined = seg if combined is None else combined + seg
            if i % 10 == 0 or i == len(jobs):
                print(f"  chunk {i}/{len(jobs)} ok", flush=True)
    tmp_out = out_file + ".part"
    combined.export(tmp_out, format="mp3")
    os.replace(tmp_out, out_file)  # only a complete file ever has the final name
    print(f"wrote {out_file} ({len(combined) / 1000:.0f} s)", flush=True)


async def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    for p in sys.argv[1:]:
        await narrate(p if os.path.isabs(p) else os.path.join(BASE_DIR, p))


if __name__ == "__main__":
    asyncio.run(main())
