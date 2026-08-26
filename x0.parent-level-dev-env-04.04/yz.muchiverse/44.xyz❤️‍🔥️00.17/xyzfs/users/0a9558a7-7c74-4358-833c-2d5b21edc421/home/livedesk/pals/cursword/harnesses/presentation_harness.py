#!/usr/bin/env python3
"""presentation_harness.py - shared, reusable primitives for building a
feature-proof presentation end to end: drive a real khtpm/-hq window via
its relay/history file (never a real mouse click - house standing
convention, see 44.xyz.../_.0.aigent-testing-k9.txt), dump real frames,
assemble a manifest, and hand off to make_presentation_video.py.

REAL, direct instruction (2026-08-25): "make a harness instead of doing
it urself, then monitor/test the harness output, so the harnesses can be
reused." This module is the REUSABLE part. A per-feature harness script
(e.g. bookmark_badge_contrast_fix_harness.py, next to this file) imports
it and supplies the feature-specific sequence - which relay codes to
send, what disk state to check, what to caption - since that part is
inherently different for every feature and can't be auto-derived.

Import this from a harness script that already knows its own house_root:

    from presentation_harness import Harness

    h = Harness(house_root, feature_name="my-feature",
                pals_dir_hint="xyzfs/users/<uuid>/home/livedesk/pals")
    h.assert_zero_stray_processes("khtpm_entity_menu_render")
    ... launch the window yourself (feature-specific) ...
    h.send_ascii(relay_path, 50)          # digit '2'
    h.send_ascii(relay_path, 13)          # Enter
    h.wait(1.5)
    snap = h.dump_frame(relay_path, "/tmp/some-frame.png")
    h.add_frame(snap, min_seconds=6, caption="...")
    ...
    h.build_video()   # writes presentations/<feature-name>/presentation.mp4

See bookmark_badge_contrast_fix_harness.py in this same directory for a
complete, working, real example - it reproduces
presentations/bookmark-badge-contrast-fix/ end to end with zero manual
relay-file wrangling.
"""
from __future__ import annotations

import shutil
import subprocess
import sys
import time
from pathlib import Path

PRESENTATIONS_MAKER = None  # resolved lazily, see _find_maker()


def _find_maker(house_root: Path) -> Path:
    """Locate make_presentation_video.py - it lives in cursword's own
    presentations/ dir, a sibling of this harnesses/ dir. Resolved by
    walking up from THIS file rather than hardcoding a full path, so a
    copy of this harness elsewhere in the house still finds the real
    script next to wherever ITS OWN cursword-style pal directory is -
    but the real, single copy is expected to stay in cursword's own
    presentations/ dir (don't fork it per-pal)."""
    here = Path(__file__).resolve().parent
    candidate = here.parent / "presentations" / "make_presentation_video.py"
    if candidate.exists():
        return candidate
    raise FileNotFoundError(
        f"presentation_harness: could not find make_presentation_video.py "
        f"next to {here} (expected at {candidate})"
    )


class Harness:
    def __init__(self, house_root: str | Path, feature_name: str,
                 presentations_root: str | Path | None = None):
        self.house_root = Path(house_root).resolve()
        self.feature_name = feature_name
        # default: cursword's own presentations/ dir, matching the house
        # standing convention documented in _.0.aigent-testing-k9.txt
        self.presentations_root = (
            Path(presentations_root).resolve() if presentations_root
            else self._default_presentations_root()
        )
        self.feature_dir = self.presentations_root / feature_name
        self.snapshots_dir = self.feature_dir / "snapshots"
        self.snapshots_dir.mkdir(parents=True, exist_ok=True)
        self._manifest_rows: list[tuple[str, float, str]] = []
        self._frame_n = 0
        self._maker_script = _find_maker(self.house_root)

    def _default_presentations_root(self) -> Path:
        here = Path(__file__).resolve().parent
        return here.parent / "presentations"

    # ---------------- process hygiene ----------------

    def stray_pids(self, name_substring: str) -> list[int]:
        """pgrep -f equivalent - returns PIDs whose full cmdline contains
        name_substring. Real, direct subprocess call, not a regex guess
        at bash quoting (matches this house's own same_entity_pids()
        convention in button.sh scripts)."""
        try:
            out = subprocess.run(["pgrep", "-f", name_substring],
                                  capture_output=True, text=True).stdout
        except FileNotFoundError:
            out = ""
        return [int(p) for p in out.split() if p.strip()]

    def assert_zero_stray_processes(self, name_substring: str):
        pids = self.stray_pids(name_substring)
        if pids:
            raise RuntimeError(
                f"presentation_harness: {len(pids)} stray process(es) matching "
                f"{name_substring!r} already running (PIDs {pids}) - kill them "
                f"first (house standing rule, see _.0.aigent-testing-k9.txt "
                f"SCOPE ADDENDUM 2026-08-13)."
            )

    def kill(self, pids: list[int], grace: float = 1.0):
        import os
        import signal
        for pid in pids:
            try:
                os.kill(pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
        time.sleep(grace)

    # ---------------- relay/history file driving ----------------

    def wait(self, seconds: float):
        time.sleep(seconds)

    def send_ascii(self, relay_path: str | Path, code: int):
        """Append one decimal ASCII code line - the house's universal
        relay/history-file protocol. Reminder baked in here on purpose:
        code is the CHARACTER's ASCII value, not the literal digit - to
        send keypress '6' you pass 54, not 6 (see the guide's own
        "biggest gotcha" section, EVENTS_AND_DB_GUIDE_🎪.md)."""
        with open(relay_path, "a") as f:
            f.write(f"{code}\n")

    def send_digit(self, relay_path: str | Path, digit: int):
        """Convenience: send the keypress for a single digit 0-9 (does
        the ASCII conversion for you, so a caller can't make the exact
        mistake this guide warns about)."""
        if not (0 <= digit <= 9):
            raise ValueError(f"send_digit: {digit} is not 0-9")
        self.send_ascii(relay_path, ord(str(digit)))

    def send_enter(self, relay_path: str | Path):
        self.send_ascii(relay_path, 13)

    def send_escape(self, relay_path: str | Path):
        self.send_ascii(relay_path, 27)

    def send_text(self, relay_path: str | Path, text: str, per_char_delay: float = 0.0):
        """Send each printable character of text as its own ASCII code
        line, in order - for typing into an armed text-entry field
        (e.g. the trigger-edit or bookmark New+ inputs)."""
        for ch in text:
            code = ord(ch)
            if not (32 <= code <= 126):
                raise ValueError(f"send_text: {ch!r} (code {code}) outside printable ASCII")
            self.send_ascii(relay_path, code)
            if per_char_delay:
                time.sleep(per_char_delay)

    def dump_frame(self, relay_path: str | Path, expect_png: str | Path,
                    wait_after: float = 1.5) -> Path:
        """Sends the universal 'p' debug key (ASCII 112, dump_frame_png()
        in every khtpm/-hq window) and waits for the write. expect_png is
        the REAL path that mode writes to (varies by window - db-hq/
        bookmarks: /tmp/db-hq-frame.png, events-hq: /tmp/events-hq-
        frame.png, etc; caller must know which, this harness doesn't
        guess). Returns that path for chaining into add_frame()."""
        self.send_ascii(relay_path, 112)
        time.sleep(wait_after)
        p = Path(expect_png)
        if not p.exists():
            raise RuntimeError(f"presentation_harness: expected dump at {p} but it doesn't exist")
        return p

    # ---------------- manifest / snapshot assembly ----------------

    def add_frame(self, src_image: str | Path, min_seconds: float, caption: str):
        """Copy src_image into this feature's snapshots/ dir (sequentially
        numbered) and append a manifest row. Call in playback order."""
        if "|" in caption:
            raise ValueError("add_frame: caption may not contain '|' (manifest delimiter)")
        self._frame_n += 1
        dest_name = f"{self._frame_n:02d}_{Path(src_image).stem}.png"
        dest = self.snapshots_dir / dest_name
        shutil.copyfile(src_image, dest)
        self._manifest_rows.append((dest_name, min_seconds, caption))
        return dest

    def write_manifest(self, header_comment: str | None = None):
        lines = []
        if header_comment:
            for hl in header_comment.splitlines():
                lines.append(f"# {hl}")
        for fname, secs, caption in self._manifest_rows:
            lines.append(f"{fname} | {secs} | {caption}")
        (self.feature_dir / "manifest.txt").write_text("\n".join(lines) + "\n")

    def write_reproduce(self, markdown_text: str):
        (self.feature_dir / "REPRODUCE.md").write_text(markdown_text)

    # ---------------- final video build ----------------

    def build_video(self, tts: bool = True, voice: str | None = None, fps: int = 30, width: int = 1280):
        if not self._manifest_rows:
            raise RuntimeError("presentation_harness: no frames added - call add_frame() first")
        self.write_manifest()
        cmd = [sys.executable, str(self._maker_script), str(self.feature_dir),
               "--fps", str(fps), "--width", str(width)]
        if not tts:
            cmd.append("--no-tts")
        if voice:
            cmd += ["--voice", voice]
        result = subprocess.run(cmd, capture_output=True, text=True)
        print(result.stdout)
        if result.returncode != 0:
            print(result.stderr, file=sys.stderr)
            raise RuntimeError("presentation_harness: make_presentation_video.py failed - see stderr above")
        return self.feature_dir / "presentation.mp4"
