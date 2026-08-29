#!/usr/bin/env python3
"""meta_to_menu_chtpm.py - real, permanent converter: <package_dir>/meta.pdl
-> <package_dir>/menu.chtpm, for the shared Elem/CSS entity-menu renderer
(khtpm_entity_menu_render.c, class="entity-menu").

Started: 2026-08-28, per ENTITY-MENU-LEGACY-DEPRECATION-PLAN.md's own
Phase 1 (design decision A - a real, durable generator, not the lost
one-off "scratchpad meta_to_chtpm.py" that produced the first 7
converted entities back on 2026-08-16/18 and then wasn't kept around).
This is that mechanism, made real and permanent so it doesn't get lost
again - a real file under version control, not a scratchpad.

Real conversion rule, matching book-stack/ava/asa's own already-proven
hand conversions exactly (mechanical, 1:1, no reinterpretation):
  - Every real `METHOD | <label> | <action>` row in meta.pdl becomes one
    <item label="<label>" action="<action>"/> in menu.chtpm's real
    <page name="main"> - same order, same action string verbatim.
  - action="" values get real XML entity-encoding for the 2 characters
    khtpm_entity_menu_render.c's own apply_attr() decodes for the
    action attribute specifically (see that file's own header comment):
    " -> &quot; and & -> &amp;. No other escaping - this parser only
    supports those 2 entities, and inventing more would just be dead
    weight nothing decodes.
  - `SECTION`/`META`/`STATE`/comment/blank lines are real, legitimate
    meta.pdl content this converter does NOT need to touch - STATE
    rows (menu_stay_open/grab_pointer/grab_keyboard) are read directly
    from meta.pdl at popup-open time by the SAME real mechanism
    tp_desktop_window_rgb.c's own read_menu_config() already uses
    (ported logic, not duplicated - see that function for the real
    STATE-row contract). This converter's only real job is the METHOD
    rows -> <item> rows.

Usage:
    meta_to_menu_chtpm.py <package_dir>          # one real entity instance
    meta_to_menu_chtpm.py --all <house_root>     # real, house-wide backfill
                                                   # (every live instance
                                                   # under xyzfs/users/*/
                                                   # home/livedesk/{pals,
                                                   # sessions/*/entities}/*
                                                   # that has a meta.pdl
                                                   # but no menu.chtpm yet)

Real, honest scope: this does NOT regenerate an existing menu.chtpm -
if one is already there, it's left alone (an existing hand-authored or
previously-generated file is not silently overwritten). Delete it first
if you want a fresh regenerate from the current meta.pdl.
"""
import sys
import os
import glob


def escape_action(value: str) -> str:
    """Real, minimal entity-encoding - the 2 entities khtpm_entity_menu_
    render.c's own apply_attr() actually decodes for the action
    attribute, matching book-stack/ava/asa's own real converted files
    byte-for-byte in convention (& first, so a real literal &quot;/&amp;
    already in the source text - unlikely but real edge case - doesn't
    get double-encoded)."""
    return value.replace("&", "&amp;").replace('"', "&quot;")


def parse_meta_pdl(path: str):
    """Real SECTION|KEY|VALUE parse, same shape read_footprint_tiles()/
    read_menu_config() in tp_desktop_window_rgb.c already use. Returns
    a list of (label, action) tuples, in real file order - order matters,
    it's the real menu order the human sees."""
    methods = []
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n\r")
            if not line.strip():
                continue
            parts = [p.strip() for p in line.split("|")]
            if len(parts) < 3:
                continue
            if parts[0] != "METHOD":
                continue
            label = parts[1]
            # Real value may itself contain real "|" pipe characters
            # (e.g. a shell command with `find ... | head -1`, the
            # exact real case that broke the RENDERER'S OWN separate
            # frame-file format tonight, khtpm_entity_menu_render.c's
            # dbhq_paint_frame_line() - unrelated bug, same real lesson:
            # don't assume "|" only ever appears as a real delimiter).
            # Rejoin everything after the label field as the real,
            # whole action value.
            action = "|".join(parts[2:]).strip()
            if label and action:
                methods.append((label, action))
    return methods


def write_menu_chtpm(package_dir: str, methods, source_meta: str):
    out_path = os.path.join(package_dir, "menu.chtpm")
    lines = []
    lines.append(
        "<!-- menu.chtpm - real, generated entity context menu "
        "(meta_to_menu_chtpm.py, 2026-08-28 permanent converter). "
        f"Converted 1:1 from {source_meta}'s own real METHOD rows - "
        "same real mechanical rule proven on ava/asa/book-stack's own "
        "hand conversions. Regenerate by deleting this file and "
        "re-running the converter; a real hand-edit here will be lost "
        "on the next regenerate, same tradeoff every generated file in "
        "this house already has. -->"
    )
    lines.append('<window class="entity-menu">')
    lines.append('  <page name="main">')
    for label, action in methods:
        lines.append(f'    <item label="{label}" action="{escape_action(action)}"/>')
    lines.append("  </page>")
    lines.append("</window>")
    with open(out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    return out_path


def convert_one(package_dir: str, force: bool = False) -> bool:
    meta_path = os.path.join(package_dir, "meta.pdl")
    menu_path = os.path.join(package_dir, "menu.chtpm")
    if not os.path.isfile(meta_path):
        print(f"skip (no meta.pdl): {package_dir}")
        return False
    if os.path.isfile(menu_path) and not force:
        print(f"skip (menu.chtpm already exists): {package_dir}")
        return False
    methods = parse_meta_pdl(meta_path)
    if not methods:
        print(f"skip (no real METHOD rows found): {package_dir}")
        return False
    out = write_menu_chtpm(package_dir, methods, meta_path)
    print(f"wrote {out} ({len(methods)} items)")
    return True


def find_all_candidates(house_root: str):
    """Real house-wide scan: every live entity instance under
    xyzfs/users/*/home/livedesk/{pals,sessions/*/entities}/* that has a
    real meta.pdl. Deliberately does NOT touch the entity TYPE templates
    under */entities/<name>/meta.pdl (those are source templates, not
    live instances - a live instance's own package_dir is what
    tp_desktop_window_rgb.c actually launches the popup against)."""
    patterns = [
        os.path.join(house_root, "xyzfs", "users", "*", "home", "livedesk", "pals", "*"),
        os.path.join(house_root, "xyzfs", "users", "*", "home", "livedesk", "sessions", "*", "entities", "*"),
    ]
    seen = set()
    for pat in patterns:
        for d in glob.glob(pat):
            if os.path.isfile(os.path.join(d, "meta.pdl")) and d not in seen:
                seen.add(d)
                yield d


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 1
    if argv[1] == "--all":
        if len(argv) < 3:
            print("usage: meta_to_menu_chtpm.py --all <house_root>")
            return 1
        house_root = argv[2]
        converted = 0
        for d in find_all_candidates(house_root):
            if convert_one(d):
                converted += 1
        print(f"--- done: {converted} real menu.chtpm written ---")
        return 0
    else:
        package_dir = argv[1]
        force = "--force" in argv
        convert_one(package_dir, force=force)
        return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
