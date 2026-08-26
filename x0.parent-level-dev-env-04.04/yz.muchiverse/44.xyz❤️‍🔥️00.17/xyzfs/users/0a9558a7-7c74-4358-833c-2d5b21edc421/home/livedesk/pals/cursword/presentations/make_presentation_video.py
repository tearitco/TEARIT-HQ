#!/usr/bin/env python3
"""make_presentation_video.py - turn a folder of dated PNG snapshots + a
plain-text manifest into a single paced MP4, so a human can watch proof
that a feature actually works instead of reading a wall of terminal text.

REAL, direct instruction (2026-08-25): "be very careful deliberate and
make examples and test them and prove harnesses as we go... presentations
being made when we are done of proof each major feature is working...
make sure the video is paced so a human can watch, not at light speed."

Directory shape (one directory per feature/proof, all under
cursword/presentations/):

    presentations/<feature-name>/
        snapshots/
            01_something.png
            02_something_else.png
            ...
        manifest.txt        <- this script's real input, see FORMAT below
        REPRODUCE.md         <- plain-English steps to redo the test yourself
        presentation.mp4      <- OUTPUT, written here by this script

MANIFEST FORMAT (manifest.txt, one row per snapshot, in playback order):
    <snapshot_filename> | <seconds_to_hold> | <caption text>

    - snapshot_filename is relative to the snapshots/ dir next to this
      manifest.
    - seconds_to_hold: MINIMUM time a human sees this frame - with TTS
      enabled (the default) the real hold time is
      max(seconds_to_hold, narration_audio_length + 0.6s), so a long
      caption is never cut off mid-sentence. Pick a number you could
      comfortably read the caption in yourself (5-8s for one sentence)
      as the floor.
    - caption text: one line, plain text (no pipe characters). Wrapped
      and burned onto the bottom of the frame automatically, AND (unless
      --no-tts) spoken aloud via TTS while that frame is on screen.
    - Lines starting with '#' or blank lines are ignored.

TTS (2026-08-25, direct instruction: "get tts output synch with the
video that actually explains what the injection/harness is doing as it
plays"): each caption is synthesized with edge_tts (same engine as
1-1.HARNECIENT.SMOL/convert_one_session.py elsewhere in this house),
one clip per frame, silence-padded to exactly that frame's real hold
duration so audio and video timing always match exactly. All
intermediate audio files are deleted after muxing into
presentation.mp4 - only the final video (with its own audio track)
is kept, per "delete audio after its in video."

Usage:
    python3 make_presentation_video.py <feature-dir>
    python3 make_presentation_video.py <feature-dir> --fps 30 --width 1280
    python3 make_presentation_video.py <feature-dir> --no-tts
    python3 make_presentation_video.py <feature-dir> --voice en-US-AnaNeural

Requires: ffmpeg on PATH, Pillow (PIL) importable. Both confirmed present
in this house's environment as of 2026-08-25. TTS additionally needs
edge_tts + pydub importable (also confirmed present) - if either is
missing, TTS is silently skipped with a warning, video still renders.
"""
import argparse
import asyncio
import shutil
import subprocess
import sys
import textwrap
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

try:
    import edge_tts
    from pydub import AudioSegment
    TTS_AVAILABLE = True
except ImportError:
    TTS_AVAILABLE = False

DEFAULT_VOICE = "en-US-AnaNeural"

FONT_REGULAR = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
FONT_BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"

CAPTION_BG = (16, 16, 16, 235)   # near-opaque dark strip, readable over any frame
CAPTION_FG = (240, 240, 240, 255)
CAPTION_PAD = 18
CAPTION_MIN_H = 90


def parse_manifest(manifest_path: Path):
    rows = []
    for lineno, raw in enumerate(manifest_path.read_text().splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        parts = [p.strip() for p in line.split("|", 2)]
        if len(parts) != 3:
            raise ValueError(
                f"{manifest_path}:{lineno}: expected 'file | seconds | caption', got: {raw!r}"
            )
        fname, secs, caption = parts
        try:
            secs = float(secs)
        except ValueError:
            raise ValueError(f"{manifest_path}:{lineno}: bad seconds value {secs!r}") from None
        if secs <= 0:
            raise ValueError(f"{manifest_path}:{lineno}: seconds must be > 0 (got {secs})")
        rows.append((fname, secs, caption))
    if not rows:
        raise ValueError(f"{manifest_path}: no rows found - nothing to render")
    return rows


def wrap_caption(caption: str, font: ImageFont.FreeTypeFont, max_width: int, draw: ImageDraw.ImageDraw):
    # naive char-count wrap first, then verified/re-wrapped against real
    # pixel width so long/short characters don't blow the frame
    approx_chars = max(20, max_width // max(1, font.getlength("m")))
    lines = textwrap.wrap(caption, width=int(approx_chars)) or [caption]
    fixed = []
    for line in lines:
        while draw.textlength(line, font=font) > max_width and len(line) > 1:
            # binary-shrink long unbroken tokens (urls/paths) rather than overflow
            line = line[: len(line) - max(1, len(line) // 8)]
        fixed.append(line)
    return fixed


def render_captioned_frame(src_path: Path, caption: str, target_w: int, out_path: Path, frame_no: int, total: int):
    img = Image.open(src_path).convert("RGB")
    if img.width != target_w:
        new_h = int(img.height * (target_w / img.width))
        img = img.resize((target_w, new_h), Image.LANCZOS)

    font = ImageFont.truetype(FONT_REGULAR, size=max(14, target_w // 60))
    label_font = ImageFont.truetype(FONT_BOLD, size=max(12, target_w // 80))

    draw_probe = ImageDraw.Draw(img)
    max_text_w = img.width - 2 * CAPTION_PAD
    lines = wrap_caption(caption, font, max_text_w, draw_probe)
    line_h = font.getbbox("Ag")[3] + 6
    caption_h = max(CAPTION_MIN_H, CAPTION_PAD * 2 + line_h * len(lines) + 22)

    total_h = img.height + caption_h
    # libx264 requires even width/height - round up and pad rather than
    # crop, so nothing real ever gets clipped off the bottom.
    if total_h % 2 == 1:
        total_h += 1
    canvas_w = img.width if img.width % 2 == 0 else img.width + 1
    canvas = Image.new("RGB", (canvas_w, total_h), (16, 16, 16))
    canvas.paste(img, (0, 0))
    draw = ImageDraw.Draw(canvas)
    draw.rectangle([0, img.height, img.width, img.height + caption_h], fill=CAPTION_BG[:3])

    label = f"{frame_no}/{total}"
    draw.text((CAPTION_PAD, img.height + 10), label, font=label_font, fill=(150, 150, 150))

    y = img.height + 10 + label_font.getbbox("Ag")[3] + 8
    for line in lines:
        draw.text((CAPTION_PAD, y), line, font=font, fill=CAPTION_FG[:3])
        y += line_h

    canvas.save(out_path)


async def _synthesize_one(text: str, voice: str, out_path: Path):
    communicate = edge_tts.Communicate(text, voice)
    await communicate.save(str(out_path))


def synthesize_caption(caption: str, voice: str, out_path: Path) -> float:
    """Render caption to out_path via edge_tts, return its duration in
    seconds (0.0 if synthesis produced nothing - e.g. empty caption)."""
    asyncio.run(_synthesize_one(caption, voice, out_path))
    if not out_path.exists() or out_path.stat().st_size == 0:
        return 0.0
    return len(AudioSegment.from_mp3(str(out_path))) / 1000.0


def title_from_feature_name(feature_dir: Path) -> str:
    """kebab-case dir name -> a real, YouTube-ready title. Direct
    instruction 2026-08-25: "the presentation mp4 shouldn't have generic
    names in the future but be youtube ready named" - "presentation.mp4"
    for every feature was exactly that generic name."""
    words = feature_dir.name.replace("_", "-").split("-")
    return " ".join(w.capitalize() if not w.isupper() else w for w in words if w)


def slugify(title: str) -> str:
    safe = "".join(c if c.isalnum() or c in " -" else "" for c in title)
    return "-".join(safe.split())


def write_yt_summary(feature_dir: Path, title: str, rows, effective_secs, video_filename: str):
    """A plain-text, YouTube-description-shaped human summary: title,
    what this proves, a chapter/timestamp list matching the real
    captions, and a pointer to REPRODUCE.md. Direct instruction
    2026-08-25: "create a human readable summary file explaining what
    happened for <name>yt-summary.txt"."""
    lines = [title, "=" * len(title), ""]
    reproduce = feature_dir / "REPRODUCE.md"
    if reproduce.exists():
        lines.append("Full reproduction steps: REPRODUCE.md (same directory as this file).")
        lines.append("")
    lines.append(f"Video file: {video_filename}")
    lines.append("")
    lines.append("Chapters:")
    t = 0.0
    for (fname, _secs, caption), eff in zip(rows, effective_secs):
        mm, ss = divmod(int(t), 60)
        lines.append(f"  {mm:02d}:{ss:02d}  {caption}")
        t += eff
    total_mm, total_ss = divmod(int(t), 60)
    lines.append("")
    lines.append(f"Total runtime: {total_mm:02d}:{total_ss:02d}")
    (feature_dir / f"{slugify(title)}-yt-summary.txt").write_text("\n".join(lines) + "\n")


def build_video(feature_dir: Path, fps: int, width: int, use_tts: bool, voice: str, title: str | None = None):
    manifest_path = feature_dir / "manifest.txt"
    snapshots_dir = feature_dir / "snapshots"
    if not manifest_path.exists():
        sys.exit(f"error: {manifest_path} not found")
    if not snapshots_dir.is_dir():
        sys.exit(f"error: {snapshots_dir} not found")

    rows = parse_manifest(manifest_path)
    total = len(rows)

    do_tts = use_tts and TTS_AVAILABLE
    if use_tts and not TTS_AVAILABLE:
        print("warning: --tts requested but edge_tts/pydub not importable - "
              "rendering a silent video instead", file=sys.stderr)

    rendered_dir = feature_dir / ".rendered_frames"
    if rendered_dir.exists():
        shutil.rmtree(rendered_dir)
    rendered_dir.mkdir()

    concat_lines = []
    audio_clips = []  # AudioSegment per frame, only populated if do_tts
    effective_secs = []
    for i, (fname, secs, caption) in enumerate(rows, 1):
        src = snapshots_dir / fname
        if not src.exists():
            sys.exit(f"error: manifest references missing snapshot: {src}")
        out_frame = rendered_dir / f"frame_{i:03d}.png"
        render_captioned_frame(src, caption, width, out_frame, i, total)

        frame_secs = secs
        if do_tts:
            audio_path = rendered_dir / f"frame_{i:03d}.mp3"
            try:
                dur = synthesize_caption(caption, voice, audio_path)
            except Exception as e:
                print(f"warning: TTS failed for frame {i} ({e!r}) - frame will be silent", file=sys.stderr)
                dur = 0.0
            frame_secs = max(secs, dur + 0.6 if dur > 0 else secs)
            clip = AudioSegment.silent(duration=0)
            if dur > 0 and audio_path.exists():
                clip = AudioSegment.from_mp3(str(audio_path))
            pad_ms = int(frame_secs * 1000) - len(clip)
            if pad_ms > 0:
                clip = clip + AudioSegment.silent(duration=pad_ms)
            audio_clips.append(clip)

        effective_secs.append(frame_secs)
        concat_lines.append(f"file '{out_frame.resolve()}'")
        concat_lines.append(f"duration {frame_secs}")
    # ffmpeg concat demuxer quirk: the LAST file's duration line is ignored
    # unless the file is also repeated once more after it - repeat final frame
    concat_lines.append(f"file '{(rendered_dir / f'frame_{total:03d}.png').resolve()}'")

    concat_file = rendered_dir / "concat.txt"
    concat_file.write_text("\n".join(concat_lines) + "\n")

    real_title = title or title_from_feature_name(feature_dir)
    video_filename = f"{slugify(real_title)}.mp4"
    out_path = feature_dir / video_filename

    # REAL BUG FOUND 2026-08-25: an earlier two-pass version of this
    # (render silent video, then mux it with narration audio as a SECOND
    # ffmpeg invocation using "-c:v copy") silently mangled the video -
    # remuxing the concat demuxer's vfr output into a fresh container via
    # stream-copy dropped all but a handful of frames with NO ffmpeg
    # error (confirmed: a real 90s/4-frame presentation came out a
    # 5-frame, ~35s-real file). Re-encoding as a second pass "fixed" the
    # frame count but then produced a video stream LONGER than the
    # source (112s from a 90s input) - still wrong. The reliable fix:
    # ONE ffmpeg invocation, images (concat demuxer) and narration audio
    # both as inputs, "-shortest" trims to whichever is shorter (should
    # be equal by construction, since every frame is silence-padded to
    # its own real hold duration before concatenation). Verified via a
    # real frame extraction at t=89s of a 90.3s video, landed correctly
    # on the final caption, not cut short or misaligned.
    if do_tts and audio_clips:
        combined_audio = AudioSegment.empty()
        for clip in audio_clips:
            combined_audio += clip
        combined_audio_path = rendered_dir / "narration.mp3"
        combined_audio.export(str(combined_audio_path), format="mp3")

        cmd = [
            "ffmpeg", "-y",
            "-f", "concat", "-safe", "0", "-i", str(concat_file),
            "-i", str(combined_audio_path),
            "-vsync", "vfr",
            "-r", str(fps),
            "-pix_fmt", "yuv420p",
            "-c:a", "aac",
            "-shortest",
            str(out_path),
        ]
    else:
        cmd = [
            "ffmpeg", "-y",
            "-f", "concat", "-safe", "0", "-i", str(concat_file),
            "-vsync", "vfr",
            "-r", str(fps),
            "-pix_fmt", "yuv420p",
            str(out_path),
        ]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(result.stderr, file=sys.stderr)
        sys.exit("ffmpeg failed - see stderr above")

    # per direct instruction: delete audio once it's baked into the video -
    # only the final .mp4 (with its own audio track) survives
    shutil.rmtree(rendered_dir)
    write_yt_summary(feature_dir, real_title, rows, effective_secs, video_filename)
    total_secs = sum(effective_secs)
    tts_note = "with TTS narration" if do_tts else "silent (no TTS)"
    print(f"wrote {out_path} ({total} frames, ~{total_secs:.0f}s total runtime, {tts_note})")
    print(f"wrote {feature_dir / f'{slugify(real_title)}-yt-summary.txt'}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("feature_dir", type=Path, help="e.g. presentations/h6-h7-h8-events-batch/")
    ap.add_argument("--fps", type=int, default=30)
    ap.add_argument("--width", type=int, default=1280, help="frames are scaled to this width, aspect preserved")
    ap.add_argument("--no-tts", dest="tts", action="store_false", default=True,
                     help="render a silent video (default: narrate captions via TTS)")
    ap.add_argument("--voice", default=DEFAULT_VOICE,
                     help=f"edge_tts voice name (default: {DEFAULT_VOICE})")
    ap.add_argument("--title", default=None,
                     help="YouTube-ready title (default: derived from the feature dir name)")
    args = ap.parse_args()
    build_video(args.feature_dir.resolve(), args.fps, args.width, args.tts, args.voice, args.title)


if __name__ == "__main__":
    main()
