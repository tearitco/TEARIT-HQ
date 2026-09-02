#!/usr/bin/env python3
"""Convert ALL DAY_*.md textbook chapters to audio with rate limit protection."""

import asyncio
import edge_tts
import os
import re
import sys
import shutil
from edge_tts.exceptions import NoAudioReceived

BASE_DIR = "."
OUTPUT_DIR = os.path.join(BASE_DIR, "audio-book")

SPEAKER_VOICE_MAP = {
    "Narrator": "en-US-GuyNeural",
    "The Narrator": "en-US-GuyNeural",
    "Tomo": "ja-JP-NanamiNeural",
    "Rahweh": "zh-CN-YunxiaNeural",
    "Maxine": "en-GB-MaisieNeural",
    "Iqa": "en-GB-SoniaNeural",
    "IQABELLA": "en-US-AnaNeural",
    "Boss": "en-US-ChristopherNeural",
    "Tom": "en-US-GuyNeural",
    "MAN": "en-US-AndrewNeural",
    "WOMAN": "en-US-AriaNeural",
    "DR. PATEL": "en-IN-NeerjaNeural",
}

DEFAULT_VOICE = "en-US-GuyNeural"

VOICE_SETTINGS = {
    "en-US-GuyNeural": {"rate": "+0%", "pitch": "+0Hz"},
    "ja-JP-NanamiNeural": {"rate": "+0%", "pitch": "+0Hz"},
    "zh-CN-YunxiaNeural": {"rate": "+0%", "pitch": "+0Hz"},
    "en-GB-MaisieNeural": {"rate": "+0%", "pitch": "+0Hz"},
    "en-GB-SoniaNeural": {"rate": "+0%", "pitch": "+0Hz"},
    "en-US-ChristopherNeural": {"rate": "+0%", "pitch": "-50Hz"},
    "en-US-AndrewNeural": {"rate": "+0%", "pitch": "+0Hz"},
    "en-IN-NeerjaNeural": {"rate": "+0%", "pitch": "+0Hz"},
    "en-US-AnaNeural": {"rate": "+0%", "pitch": "+0Hz"},
}


def parse_md_chapter(text):
    segments = []
    lines = text.split('\n')
    current_speaker = "Narrator"
    current_text = []

    for line in lines:
        stripped = line.strip()

        if not stripped:
            if current_text:
                segments.append((current_speaker, ' '.join(current_text).strip()))
                current_text = []
            current_speaker = "Narrator"
            continue

        if stripped.startswith('#'):
            if current_text:
                segments.append((current_speaker, ' '.join(current_text).strip()))
                current_text = []
            current_speaker = "Narrator"
            header_text = re.sub(r'^#+\s*', '', stripped)
            header_text = re.sub(r'\*\*|\*', '', header_text)
            current_text.append(header_text)
            continue

        if stripped.startswith('---'):
            if current_text:
                segments.append((current_speaker, ' '.join(current_text).strip()))
                current_text = []
            current_speaker = "Narrator"
            continue

        if stripped.startswith('```'):
            if current_text:
                segments.append((current_speaker, ' '.join(current_text).strip()))
                current_text = []
            current_speaker = "Narrator"
            continue

        if stripped.startswith('|'):
            if current_text:
                segments.append((current_speaker, ' '.join(current_text).strip()))
                current_text = []
            current_speaker = "Narrator"
            continue

        if stripped.startswith('> ') and not re.match(r'^>\s*\*\*', stripped):
            if current_text:
                segments.append((current_speaker, ' '.join(current_text).strip()))
                current_text = []
            current_speaker = "Narrator"
            text_content = re.sub(r'^>\s*', '', stripped)
            text_content = re.sub(r'\*\*|\*', '', text_content)
            if text_content:
                current_text.append(text_content)
            continue

        speaker_match = re.match(r'^>\s*\*\*(.*?)\*\*\s*(.*)', stripped)
        if speaker_match:
            if current_text:
                segments.append((current_speaker, ' '.join(current_text).strip()))
                current_text = []
            current_speaker = speaker_match.group(1).strip().rstrip(':')
            dialogue = speaker_match.group(2).strip()
            if dialogue:
                current_text.append(dialogue)
            continue

        if stripped.startswith('> '):
            text_content = re.sub(r'^>\s*', '', stripped)
            text_content = re.sub(r'\*\*|\*', '', text_content)
            if text_content:
                current_text.append(text_content)
            continue

        clean = re.sub(r'\*\*|\*|`|<[^>]+>', '', stripped)
        if clean:
            if current_speaker != "Narrator":
                if current_text:
                    segments.append((current_speaker, ' '.join(current_text).strip()))
                    current_text = []
                current_speaker = "Narrator"
            current_text.append(clean)

    if current_text:
        segments.append((current_speaker, ' '.join(current_text).strip()))

    return segments


async def create_silent_segment(output_path, duration=1.0):
    try:
        cmd = f'ffmpeg -y -f lavfi -i anullsrc=r=24000:cl=mono -t {duration} -c:a libmp3lame -b:a 32k "{output_path}" 2>/dev/null'
        os.system(cmd)
        return os.path.exists(output_path) and os.path.getsize(output_path) > 100
    except:
        return False


async def generate_speech(text, voice, output_path, max_retries=2):
    settings = VOICE_SETTINGS.get(voice, {"rate": "+0%", "pitch": "+0Hz"})
    for attempt in range(max_retries + 1):
        try:
            communicate = edge_tts.Communicate(
                text, voice, rate=settings["rate"], pitch=settings["pitch"]
            )
            await communicate.save(output_path)

            if os.path.exists(output_path) and os.path.getsize(output_path) > 300:
                return True
            else:
                raise NoAudioReceived("File too small")

        except Exception as e:
            if attempt < max_retries:
                wait = 1.5 + (attempt * 1.2)
                print(f" ⚠️ retry {attempt+1}", end="", flush=True)
                await asyncio.sleep(wait)
                continue
            print(" ❌", end="")
            return False
    return False


async def convert_chapter(chapter_name):
    md_file = os.path.join(BASE_DIR, f"{chapter_name}.md")
    if not os.path.exists(md_file):
        print(f"  ⚠️ File not found: {md_file}")
        return False

    output_mp3 = os.path.join(OUTPUT_DIR, f"{chapter_name}.mp3")
    if os.path.exists(output_mp3) and os.path.getsize(output_mp3) > 1000:
        print(f"  ⏭️ Already exists, skipping")
        return True

    with open(md_file, 'r', encoding='utf-8') as f:
        text = f.read()

    segments = parse_md_chapter(text)
    if not segments:
        print(f"  ⚠️ No segments found")
        return False

    print(f"  📝 {len(segments)} segments")

    chapter_tmp = os.path.join(OUTPUT_DIR, f"_tmp_{chapter_name}")
    os.makedirs(chapter_tmp, exist_ok=True)

    seg_files = []
    failed_count = 0

    for i, (speaker, seg_text) in enumerate(segments):
        if not seg_text.strip():
            continue
        voice = SPEAKER_VOICE_MAP.get(speaker, SPEAKER_VOICE_MAP.get(speaker.upper(), DEFAULT_VOICE))
        if voice == DEFAULT_VOICE and speaker in SPEAKER_VOICE_MAP:
            voice = SPEAKER_VOICE_MAP[speaker]
        seg_path = os.path.join(chapter_tmp, f"seg_{i:03d}.mp3")

        print(f"    [{i+1:2d}/{len(segments)}] {speaker:<20}", end="", flush=True)

        success = await generate_speech(seg_text, voice, seg_path)

        if not success and voice != DEFAULT_VOICE:
            print(f" 🔄 trying Narrator", end="", flush=True)
            success = await generate_speech(seg_text, DEFAULT_VOICE, seg_path, max_retries=2)

        if success and os.path.exists(seg_path) and os.path.getsize(seg_path) > 300:
            seg_files.append(seg_path)
            print(" ✅")
        else:
            failed_count += 1
            silent_ok = await create_silent_segment(seg_path, duration=1.2)
            if silent_ok:
                seg_files.append(seg_path)
                print(" 🔇 (silent fallback)")
            else:
                print(" ❌ (fatal failure)")

        await asyncio.sleep(0.65 if success else 1.8)

    if not seg_files:
        print(f"  ❌ No segments at all")
        shutil.rmtree(chapter_tmp, ignore_errors=True)
        return False

    list_file = os.path.join(chapter_tmp, "filelist.txt")
    with open(list_file, 'w', encoding='utf-8') as f:
        for sp in seg_files:
            rel_path = os.path.basename(sp)
            f.write(f"file '{rel_path}'\n")

    print(f"  🔗 Concatenating {len(seg_files)}/{len(segments)} segments...")

    ffmpeg_log = os.path.join(chapter_tmp, "ffmpeg.log")
    cmd = f'ffmpeg -y -f concat -safe 0 -i "{list_file}" -c copy "{output_mp3}" 2>"{ffmpeg_log}"'
    os.system(cmd)

    if os.path.exists(output_mp3) and os.path.getsize(output_mp3) > 2000:
        size_mb = os.path.getsize(output_mp3) / (1024 * 1024)
        print(f"  ✅ {chapter_name}.mp3 ({size_mb:.1f} MB) — {failed_count} silent/fallback")
        success = True
    else:
        print(f"  ❌ Final concat failed or file too small")
        if os.path.exists(ffmpeg_log):
            print("     → Check error log:", ffmpeg_log)
            with open(ffmpeg_log, 'r') as f:
                print(f.read()[-500:])
        success = False

    shutil.rmtree(chapter_tmp, ignore_errors=True)
    return success


async def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    all_chapters = sorted([f.replace('.md', '')
                        for f in os.listdir(BASE_DIR)
                        if f.startswith('DAY_') and f.endswith('.md')])

    if len(sys.argv) > 1:
        chapters = [s for s in sys.argv[1:] if s]
    else:
        chapters = all_chapters

    print(f"Converting {len(chapters)} chapters to audio...\n")
    done = 0
    for s in chapters:
        print(f"\n{'='*65}")
        print(f"🎬 {s}")
        print(f"{'='*65}")
        if await convert_chapter(s):
            done += 1

    print(f"\n✅ Finished: {done}/{len(chapters)} chapters processed successfully")


if __name__ == "__main__":
    asyncio.run(main())
