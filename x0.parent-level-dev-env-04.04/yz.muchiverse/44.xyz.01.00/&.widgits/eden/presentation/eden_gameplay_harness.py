#!/usr/bin/env python3
"""eden_gameplay_harness.py - per-feature presentation script for Eden gameplay, built on the house's
presentation_harness.py (cursword harnesses) and make_presentation_video.py. The shared module is imported from its
seed location, found relative to the house root (no absolute paths).

Honest scope: each scene is only what runs TODAY. Add a scene when a plan step lands
(ENTITY-DRIVEN-GAMEPLAY-VIA-CONTEXT-MENUS-PLAN-2026-10-08.md section 4). It drives windows through the relay
(`KEY_PRESSED:` lines - the module's own send_ascii writes bare codes, which this script does not use) and captures with the
house's dump_frame_png_op directly. It never clicks, and it never writes game state.

  python3 eden_gameplay_harness.py <house_root> [--no-tts] [--scenes viewer,pchq]
Window ids come from the window's size (the windows have no X name and no _NET_WM_PID): pass --viewer-size WxH / --pchq-size WxH if the UI scale differs.
"""
import argparse, subprocess, sys, time
from pathlib import Path

def load_module(house):
    seed = house / "xyzfs/_seed/livedesk/pals/cursword/harnesses"
    sys.path.insert(0, str(seed))
    import presentation_harness as ph
    return ph

def find_window(size):
    out = subprocess.run(["xwininfo", "-root", "-tree"], capture_output=True, text=True).stdout
    hits = [l.split()[0] for l in out.splitlines() if f" {size}+" in l and "mutter guard" not in l]
    return hits[0] if hits else None

def key(relay, code):          # relay line format of khtpm_core_render: KEY_PRESSED: <decimal>
    with open(relay, "a") as f: f.write(f"KEY_PRESSED: {code}\n")

def note(relay, text):
    with open(relay, "a") as f: f.write(f"# {text}\n")

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("house_root"); ap.add_argument("--no-tts", action="store_true")
    ap.add_argument("--scenes", default="viewer,pchq"); ap.add_argument("--viewer-size", default="1125x650"); ap.add_argument("--pchq-size", default="1747x968")
    a = ap.parse_args(); house = Path(a.house_root).resolve(); ph = load_module(house)
    eden_dir = house / "&.widgits/eden/presentation"
    h = ph.Harness(house, "eden-gameplay", presentations_root=eden_dir / "presentations")
    dump = house / "&.widgits/_shared-lib/ops/+x/dump_frame_png_op.+x"
    tmp = Path("/tmp") / "eden_gameplay_frames"; tmp.mkdir(exist_ok=True)
    def shot(name, wid):
        p = tmp / f"{name}.png"; subprocess.run([str(dump), wid, str(p)], check=True, capture_output=True); return p
    scenes = a.scenes.split(",")
    if "viewer" in scenes:
        wid = find_window(a.viewer_size)
        if not wid: sys.exit("eden-viewer window not found: launch &.hq-apps/eden-viewer-hq/open_eden_viewer_hq.sh first")
        h.add_frame(shot("viewer", wid), 8, "Eden World. The current user's Eden game, read only: the day, the farm tiles, the people with their needs, and the last history rows.")
    if "pchq" in scenes:
        wid = find_window(a.pchq_size)
        if not wid: sys.exit("pc-hq board window not found: launch @.apps/piececraft-hq/open_pchq_board.sh first")
        h.add_frame(shot("pchq", wid), 8, "pc-hq. The same kind of entity, as a piece on a board. The hero and the chicken are the pattern for moving entities.")
    mp4 = h.build_video(tts=not a.no_tts)
    print("wrote", mp4)

if __name__ == "__main__":
    main()
