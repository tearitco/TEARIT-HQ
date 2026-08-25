#!/usr/bin/env python3
"""
Convert ONE HARNECIENT training session (any DAY_*.txt, by name) to
multi-voice audio. Same parser/voice map/render pipeline as
convert_all_sessions.py, unchanged - only main()'s file-selection is
different (a single named file instead of "first N sorted"), so a
future day can be rendered on its own without re-running the whole
book. Direct instruction 2026-08-13: "make it able to target any 1
file for future use."

Voices:
  Tomo (Xiaoxiao)
  Rahweh (Yunxia)
  Maxine (Maisie)
  Iqa (Ana)
  The Narrator (Xiaoxiao)
  Boss (Xiaoxiao)

Usage: python3 convert_one_session.py DAY_21_THE_SCRATCH_FILE
       python3 convert_one_session.py DAY_21_THE_SCRATCH_FILE.txt
       python3 convert_one_session.py /full/path/to/DAY_21_THE_SCRATCH_FILE.txt
"""

import asyncio
import edge_tts
import os
import re
import sys
import tempfile
from pathlib import Path
from pydub import AudioSegment

BASE_DIR = "/home/no/Desktop/🤖️🪤️🏠️/🥡️🪜️/🪜️-00.00/NNEST_CLEAN_PARENT/NNEST-11.17/x0.parent-level-dev-env-04.04/yz.muchiverse/#.#.✅️.cal-user-sum/1-1.HARNECIENT.SMOL"
OUTPUT_DIR = os.path.join(BASE_DIR, "audio-book")

VOICE_MAP = {
    "TOMO": "zh-CN-XiaoxiaoNeural",
    "RAHWEH": "zh-CN-YunxiaNeural",
    "MAXINE": "en-GB-MaisieNeural",
    "IQA": "en-US-AnaNeural",
    "IQABELLA": "en-US-AnaNeural",
    "THE NARRATOR": "zh-CN-XiaoxiaoNeural",
    "NARRATOR": "zh-CN-XiaoxiaoNeural",
    "BOSS": "zh-CN-XiaoxiaoNeural",
}

VOICE_SETTINGS = {
    "zh-CN-XiaoxiaoNeural": {"rate": "+0%", "pitch": "+0Hz"},
    "zh-CN-YunxiaNeural": {"rate": "+0%", "pitch": "+0Hz"},
    "en-GB-MaisieNeural": {"rate": "+0%", "pitch": "+0Hz"},
    "en-US-AnaNeural": {"rate": "+0%", "pitch": "+0Hz"},
}


def parse_segments(text):
    """Parse text file and extract dialogue segments."""
    segments = []
    current_speaker = None
    current_text = []
    speaker_pattern = re.compile(r'^\*\*(.+?):\*\*\s*(.*)')

    for line in text.split('\n'):
        if line.startswith('# ') or line.startswith('## ') or line.startswith('---'):
            continue
        match = speaker_pattern.match(line)
        if match:
            if current_speaker and current_text:
                full_text = ' '.join(current_text).strip()
                if full_text:
                    segments.append((current_speaker, full_text))
            current_speaker = match.group(1).strip()
            current_text = [match.group(2).strip()] if match.group(2).strip() else []
        elif line.strip() == '':
            if current_speaker and current_text:
                full_text = ' '.join(current_text).strip()
                if full_text:
                    segments.append((current_speaker, full_text))
                current_speaker = None
                current_text = []
        else:
            cleaned = re.sub(r'\*\*(.+?)\*\*', r'\1', line.strip())
            cleaned = re.sub(r'[`*_]', '', cleaned)
            if cleaned:
                current_text.append(cleaned)

    if current_speaker and current_text:
        full_text = ' '.join(current_text).strip()
        if full_text:
            segments.append((current_speaker, full_text))

    return segments


async def tts_segment(text, voice):
    """Convert text segment to audio using edge_tts."""
    if not text.strip():
        return None
    tmp = tempfile.NamedTemporaryFile(suffix='.mp3', delete=False)
    tmp.close()
    settings = VOICE_SETTINGS.get(voice, {"rate": "+0%", "pitch": "+0Hz"})
    try:
        communicate = edge_tts.Communicate(text, voice, rate=settings["rate"], pitch=settings["pitch"])
        await communicate.save(tmp.name)
        if os.path.exists(tmp.name) and os.path.getsize(tmp.name) > 0:
            return tmp.name
    except Exception as e:
        print(f"            ✗ TTS Error: {e}")
    if os.path.exists(tmp.name):
        os.unlink(tmp.name)
    return None


async def process_session(txt_file):
    """Process a single session file."""
    basename = os.path.basename(txt_file).replace('.txt', '')
    output_file = os.path.join(OUTPUT_DIR, f"{basename}.mp3")

    print(f"\n{'='*70}")
    print(f"  {basename}")
    print(f"{'='*70}")

    with open(txt_file, 'r', encoding='utf-8') as f:
        text = f.read()

    segments = parse_segments(text)
    print(f"  Parsed {len(segments)} segments")

    combined = AudioSegment.empty()
    success = 0
    failed = 0

    for i, (speaker, seg_text) in enumerate(segments):
        voice = VOICE_MAP.get(speaker)
        if not voice:
            print(f"  [{i+1:2}/{len(segments)}] ✗ Unknown speaker '{speaker}'")
            failed += 1
            continue

        display = seg_text[:45] + "..." if len(seg_text) > 45 else seg_text
        voice_short = voice.split('-')[2] if '-' in voice else voice[:5]
        print(f"  [{i+1:2}/{len(segments)}] {speaker:12} ({voice_short:10}): {display}", end=" ")

        try:
            tmp_path = await tts_segment(seg_text, voice)
            if tmp_path:
                audio = AudioSegment.from_mp3(tmp_path)
                combined += audio + AudioSegment.silent(duration=400)
                os.unlink(tmp_path)
                success += 1
                print("✓")
            else:
                failed += 1
                print("✗")
        except Exception as e:
            print(f"✗ Error: {e}")
            failed += 1

    if combined:
        combined.export(output_file, format='mp3', bitrate='192k')
        size = os.path.getsize(output_file) / (1024 * 1024)
        print(f"\n  Result: {size:.2f} MB ({success} ok, {failed} failed)")
        return True
    else:
        print(f"\n  ✗ No audio generated for {basename}")
        return False


def resolve_target(arg):
    """Turn a bare day name, a name with .txt, or a full path into a
    real file - checked against BASE_DIR first (the book's own
    directory) so 'DAY_21_THE_SCRATCH_FILE' alone is enough for the
    common case; falls through to treating the argument as a literal
    path for anything outside BASE_DIR."""
    candidate = arg if arg.endswith('.txt') else f"{arg}.txt"
    in_base = Path(BASE_DIR) / candidate
    if in_base.is_file():
        return in_base
    as_path = Path(candidate)
    if as_path.is_file():
        return as_path
    return None


async def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    if len(sys.argv) < 2:
        print("Usage: python3 convert_one_session.py <DAY_NN_NAME | path-to-.txt>")
        print(f"  looks first in: {BASE_DIR}")
        sys.exit(1)

    target = resolve_target(sys.argv[1])
    if not target:
        print(f"convert_one_session.py: could not find '{sys.argv[1]}' "
              f"in {BASE_DIR} or as a literal path")
        sys.exit(1)

    print(f"\n🎙️  HARNECIENT — Converting 1 session to audio\n")
    ok = await process_session(str(target))

    print(f"\n{'='*70}")
    print(f"{'✓' if ok else '✗'} CONVERSION {'COMPLETE' if ok else 'FAILED'}: {target.name}")
    print(f"  Output directory: {OUTPUT_DIR}")
    print(f"{'='*70}\n")


if __name__ == '__main__':
    asyncio.run(main())
