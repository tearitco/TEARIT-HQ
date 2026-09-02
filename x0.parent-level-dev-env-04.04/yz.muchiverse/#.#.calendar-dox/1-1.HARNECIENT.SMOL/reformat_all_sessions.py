#!/usr/bin/env python3
"""
Reformat all DAY_*.md documents to match the example format.
Converts markdown dialogue format to plain text with **CHARACTER:** format.
"""

import os
import re
from pathlib import Path

import os as _os_pathfix  # REAL FIX 2026-09-01 (S1_HOUSE_PATH_MIGRATION.md) - was a hardcoded absolute path
BASE_DIR = _os_pathfix.path.join(_os_pathfix.path.dirname(_os_pathfix.path.abspath(__file__)), "..", "1-1.HARNECIENT.AUBIO")
BASE_DIR = _os_pathfix.path.normpath(BASE_DIR)

# Voice mapping for all speakers
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

def extract_title(filename):
    """Extract title from filename."""
    # DAY_01_WELCOME -> "Welcome" or use full text
    parts = filename.replace("DAY_", "").replace(".md", "").split("_", 1)
    if len(parts) > 1:
        title = parts[1].replace("_", " ")
    else:
        title = parts[0]
    return f"HARNECIENT TRAINING — {title}"

def extract_metadata_from_md(text):
    """Extract metadata like title, topic from markdown."""
    lines = text.split('\n')
    title = ""
    topic = ""

    # Get title from first h1
    for line in lines:
        if line.startswith('# ') and not line.startswith('## '):
            title = line.replace('# ', '').strip()
            title = re.sub(r'[🎓📚🌅🌙🗝️🔗📂🎬💠🚀🔬🌱❓]*', '', title).strip()
            break

    return title, topic

def parse_markdown_dialogue(text):
    """Parse markdown format and extract dialogue segments."""
    segments = []
    lines = text.split('\n')
    current_speaker = None
    current_text = []

    # Pattern for **Speaker:** text format (handles both > quotes and direct text)
    speaker_pattern = re.compile(r'^\s*>?\s*\*\*(.+?):\*\*\s*(.*)')

    for line in lines:
        # Skip headers and metadata
        if line.startswith('#') or line.startswith('---') or line.startswith('###'):
            if current_speaker and current_text:
                full_text = ' '.join(current_text).strip()
                if full_text:
                    segments.append((current_speaker, full_text))
                current_speaker = None
                current_text = []
            continue

        # Skip emoji-heavy metadata lines
        if '🎓' in line or '📅' in line or '🧭' in line:
            continue

        match = speaker_pattern.match(line)
        if match:
            # Save previous speaker's dialogue
            if current_speaker and current_text:
                full_text = ' '.join(current_text).strip()
                if full_text:
                    segments.append((current_speaker, full_text))

            # Start new speaker
            current_speaker = match.group(1).strip().upper()
            text_part = match.group(2).strip()
            current_text = [text_part] if text_part else []
        elif line.strip() == '':
            # Empty line = end of speech
            if current_speaker and current_text:
                full_text = ' '.join(current_text).strip()
                if full_text:
                    segments.append((current_speaker, full_text))
            current_speaker = None
            current_text = []
        elif current_speaker and line.strip():
            # Continuation of current speaker
            cleaned = line.strip()
            # Remove markdown formatting
            cleaned = re.sub(r'>\s*', '', cleaned)
            cleaned = re.sub(r'\*\*(.+?)\*\*', r'\1', cleaned)
            cleaned = re.sub(r'[`*_]', '', cleaned)
            cleaned = re.sub(r'\[(.+?)\]\(.+?\)', r'\1', cleaned)  # [text](link) -> text
            if cleaned:
                current_text.append(cleaned)

    # Don't forget last speaker
    if current_speaker and current_text:
        full_text = ' '.join(current_text).strip()
        if full_text:
            segments.append((current_speaker, full_text))

    return segments

def format_output(title, segments):
    """Format segments into the target format."""
    if not segments:
        return ""

    # Get unique speakers and determine voices
    speakers = list(dict.fromkeys([s[0] for s in segments]))
    voice_lines = []
    for speaker in speakers:
        voice = VOICE_MAP.get(speaker, "en-US-AnaNeural")
        voice_lines.append(f"{speaker} ({voice.split('-')[2] if '-' in voice else voice})")

    # Build header
    output = []
    output.append(f"# {title}")
    output.append("# " + "="*len(title))
    output.append(f"# Title: \"{title}\"")
    output.append(f"# Characters: {', '.join(speakers)}")

    voices_list = []
    for s in speakers:
        voice = VOICE_MAP.get(s, "en-US-AnaNeural")
        voices_list.append(f"{s} ({voice})")
    output.append(f"# Voices: {', '.join(voices_list)}")
    output.append("# Topic: HARNECIENT Training Session")
    output.append("")

    # Add dialogue
    for speaker, text in segments:
        output.append(f"**{speaker}:** {text}")
        output.append("")

    return '\n'.join(output)

def main():
    base_path = Path(BASE_DIR)
    md_files = sorted(base_path.glob('DAY_*.md'))

    print(f"Found {len(md_files)} documents to reformat\n")

    for md_file in md_files:
        print(f"Processing: {md_file.name}")

        # Read markdown
        with open(md_file, 'r', encoding='utf-8') as f:
            content = f.read()

        # Extract metadata
        title, _ = extract_metadata_from_md(content)
        if not title:
            title = extract_title(md_file.name)

        # Parse dialogue
        segments = parse_markdown_dialogue(content)

        if segments:
            # Format output
            formatted = format_output(title, segments)

            # Save as .txt
            txt_file = md_file.with_suffix('.txt')
            with open(txt_file, 'w', encoding='utf-8') as f:
                f.write(formatted)

            print(f"  ✓ Converted {len(segments)} segments → {txt_file.name}\n")
        else:
            print(f"  ✗ No segments found\n")

if __name__ == '__main__':
    main()
