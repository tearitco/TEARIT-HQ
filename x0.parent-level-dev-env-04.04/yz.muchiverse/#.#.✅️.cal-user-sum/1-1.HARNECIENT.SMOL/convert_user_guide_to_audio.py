#!/usr/bin/env python3
"""
Convert HARNECIENT_USER_GUIDE.md to audio narration.
Only Maxine speaks (en-GB-MaisieNeural).
Creates: HARNECIENT_USER_GUIDE.mp3

Usage: python3 convert_user_guide_to_audio.py
"""

import asyncio
import edge_tts
import os
import re
import tempfile
from pydub import AudioSegment

BASE_DIR = "/home/no/Desktop/🤖️🪤️🏠️/🥡️🪜️/🪜️-00.00/NNEST_CLEAN_PARENT/NNEST-11.17/x0.parent-level-dev-env-04.04/yz.muchiverse/#.#.✅️.cal-user-sum/1-1.HARNECIENT.AUBIO"
INPUT_FILE = os.path.join(BASE_DIR, "HARNECIENT_USER_GUIDE.md")
OUTPUT_DIR = os.path.join(BASE_DIR, "audio-book")
OUTPUT_FILE = os.path.join(OUTPUT_DIR, "HARNECIENT_USER_GUIDE.mp3")

MAXINE_VOICE = "en-GB-MaisieNeural"
MAXINE_SETTINGS = {"rate": "+0%", "pitch": "+0Hz"}


def parse_guide_content(text):
    """Extract content sections from markdown guide."""
    sections = []
    lines = text.split('\n')
    current_section = ""
    current_text = []

    for line in lines:
        # Skip headers, but use them as section markers
        if line.startswith('# '):
            if current_text:
                full_text = '\n'.join(current_text).strip()
                if full_text and len(full_text) > 50:  # Skip very short sections
                    sections.append(full_text)
                current_text = []
            current_section = line.replace('# ', '').strip()
            # Add section as intro text
            if current_section:
                current_text.append(f"Section: {current_section}")

        # Skip code blocks
        elif line.startswith('```'):
            continue
        # Skip table markers
        elif line.startswith('|') or line.startswith('---'):
            continue
        # Skip empty lines between paragraphs (but keep paragraph structure)
        elif line.strip() == '':
            if current_text and '\n'.join(current_text).strip():
                current_text.append('')
        else:
            # Regular content
            cleaned = line.strip()
            # Remove markdown formatting
            cleaned = re.sub(r'\*\*(.+?)\*\*', r'\1', cleaned)
            cleaned = re.sub(r'`(.+?)`', r'\1', cleaned)
            cleaned = re.sub(r'\[(.+?)\]\(.+?\)', r'\1', cleaned)
            if cleaned:
                current_text.append(cleaned)

    # Don't forget last section
    if current_text:
        full_text = '\n'.join(current_text).strip()
        if full_text and len(full_text) > 50:
            sections.append(full_text)

    # Break into chunks (Maxine can handle ~500 chars per TTS call comfortably)
    segments = []
    for section in sections:
        # Split into sentences for better pacing
        sentences = re.split(r'(?<=[.!?])\s+', section)
        chunk = ""
        for sentence in sentences:
            if len(chunk) + len(sentence) < 500:
                chunk += " " + sentence if chunk else sentence
            else:
                if chunk:
                    segments.append(chunk)
                chunk = sentence
        if chunk:
            segments.append(chunk)

    return segments


async def tts_segment(text, voice):
    """Convert text to speech."""
    if not text.strip():
        return None
    tmp = tempfile.NamedTemporaryFile(suffix='.mp3', delete=False)
    tmp.close()
    try:
        communicate = edge_tts.Communicate(
            text, voice,
            rate=MAXINE_SETTINGS["rate"],
            pitch=MAXINE_SETTINGS["pitch"]
        )
        await communicate.save(tmp.name)
        if os.path.exists(tmp.name) and os.path.getsize(tmp.name) > 0:
            return tmp.name
    except Exception as e:
        print(f"            ✗ TTS Error: {e}")
    if os.path.exists(tmp.name):
        os.unlink(tmp.name)
    return None


async def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    # Read and parse guide
    with open(INPUT_FILE, 'r', encoding='utf-8') as f:
        content = f.read()

    segments = parse_guide_content(content)
    print(f"\n🎙️  HARNECIENT USER GUIDE — Maxine Narration")
    print(f"{'='*70}")
    print(f"  Parsed {len(segments)} segments\n")

    combined = AudioSegment.empty()
    success = 0
    failed = 0

    for i, seg_text in enumerate(segments):
        display = seg_text[:50] + "..." if len(seg_text) > 50 else seg_text
        print(f"  [{i+1:3}/{len(segments)}] {display}", end=" ")

        try:
            tmp_path = await tts_segment(seg_text, MAXINE_VOICE)
            if tmp_path:
                audio = AudioSegment.from_mp3(tmp_path)
                combined += audio + AudioSegment.silent(duration=300)
                os.unlink(tmp_path)
                success += 1
                print("✓")
            else:
                failed += 1
                print("✗")
        except Exception as e:
            print(f"✗ Error: {e}")
            failed += 1

    print()
    if combined:
        combined.export(OUTPUT_FILE, format='mp3', bitrate='192k')
        size = os.path.getsize(OUTPUT_FILE) / (1024 * 1024)
        print(f"{'='*70}")
        print(f"✓ COMPLETE: {size:.2f} MB narration")
        print(f"  Segments: {success}/{len(segments)}")
        print(f"  Output:   {OUTPUT_FILE}")
        print(f"{'='*70}\n")
    else:
        print("\n✗ FAILED: No audio generated\n")


if __name__ == '__main__':
    asyncio.run(main())
