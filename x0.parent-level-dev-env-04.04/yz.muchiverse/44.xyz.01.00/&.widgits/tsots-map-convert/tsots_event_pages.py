#!/usr/bin/env python3
"""Turn each TSOTS map event into a house event package.

The sprite line in events.txt stays the picture. This writes the pages
the bottom bar, the context menu, and play-mode touch actually open.

Per event, under the desk:

    ev/<id>/event_pkg/pages/page_<n>/condition.pdl
    ev/<id>/event_pkg/pages/page_<n>/event.pal
    ev/<id>/event_pkg/pages/page_<n>/cmd_<k>.sh
    ev/<id>/name.txt

And one desk index the footer reads:

    bar.txt    id, name, x, y, trigger of the active page

Show Text (MV 101/401) becomes mr_show_text. Every other command is
kept as a comment row so the events menu still lists it. The database
name tables land in the book's db/ folder (id and name only).

Play vs edit is not decided here. Play mode fires the package. Edit
mode opens the context menu.
"""
import json
import os
import re

BOOK = os.path.normpath(os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "../../@.apps/piececraft-hq/pieces/system/maps/tsots"))

FLAGS = ("actorValid", "itemValid", "selfSwitchValid",
         "switch1Valid", "switch2Valid", "variableValid")

# MV page trigger -> the name play_event.sh matches.
TRIG = {0: "on-click", 1: "on-touch", 2: "event-touch",
        3: "Autorun", 4: "parallel"}

# Codes that already have a house op we can call with the text we have.
# Anything else is stored as a comment row, not dropped.
SHOW_TEXT = 101
TEXT_LINE = 401


def find_data():
    base = "/home/no/Desktop"
    for root, dirs, files in os.walk(base):
        dirs[:] = [d for d in dirs if d not in (
            "img", "audio", "movies", "js", "node_modules", ".git")]
        if "Map004.json" in files and "Actors.json" in files and "RMMV_TSOTS" in root:
            return root
    raise SystemExit("TSOTS data not found")


def sh_quote(s):
    return "'" + str(s).replace("'", "'\\''") + "'"


def safe_name(s, fallback):
    s = (s or "").replace("\n", " ").replace("\t", " ").strip()
    s = re.sub(r"[^\w .,'!?-]", "", s)
    return (s[:40] or fallback)


def active_trigger(event):
    """Last page with every condition flag off, same rule as the sprite."""
    trig = "on-click"
    for p in event.get("pages") or []:
        cond = p.get("conditions") or {}
        if not any(cond.get(k) for k in FLAGS):
            trig = TRIG.get(int(p.get("trigger") or 0), "on-click")
    return trig


def write_cmd(page_dir, n, body):
    path = os.path.join(page_dir, "cmd_%d.sh" % n)
    with open(path, "w", encoding="utf-8") as f:
        f.write(body)
    os.chmod(path, 0o755)
    return "exec cmd_%d.sh\n" % n


def show_text_body(text):
    # mr_show_text wants the entity dir and the words. Speaker is omitted.
    return (
        "#!/bin/sh\n"
        "cd \"$(dirname \"$0\")/../../..\" || exit 1\n"
        "ENT=\"$PWD\"\n"
        "D=\"$ENT\"\n"
        "while [ \"$D\" != / ] && [ ! -d \"$D/xyzfs\" ]; do D=\"$(dirname \"$D\")\"; done\n"
        "exec \"$D/&.widgits/events-hq/ops/+x/mr_show_text.+x\" \"$ENT\" %s\n" % sh_quote(text)
    )


def comment_body(text):
    return "#!/bin/sh\n# %s\nexit 0\n" % text.replace("\n", " ")[:200]


def write_page(ev_dir, page, number):
    page_dir = os.path.join(ev_dir, "event_pkg", "pages", "page_%d" % number)
    os.makedirs(page_dir, exist_ok=True)
    trig = TRIG.get(int(page.get("trigger") or 0), "on-click")
    with open(os.path.join(page_dir, "condition.pdl"), "w", encoding="utf-8") as f:
        f.write("COND | trigger | %s\n" % trig)
    lines = []
    cmds = page.get("list") or []
    n = 0
    i = 0
    while i < len(cmds):
        code = int(cmds[i].get("code") or 0)
        if code == 0:
            i += 1
            continue
        if code == SHOW_TEXT:
            bits = []
            j = i + 1
            while j < len(cmds) and int(cmds[j].get("code") or 0) == TEXT_LINE:
                params = cmds[j].get("parameters") or [""]
                bits.append(str(params[0] if params else ""))
                j += 1
            text = " ".join(b for b in bits if b).strip()
            n += 1
            if text:
                lines.append(write_cmd(page_dir, n, show_text_body(text)))
            else:
                lines.append(write_cmd(page_dir, n, comment_body("Show Text (empty)")))
            i = j
            continue
        params = cmds[i].get("parameters") or []
        short = " ".join(str(p)[:40] for p in params[:4])
        n += 1
        lines.append(write_cmd(page_dir, n, comment_body("mv %d %s" % (code, short))))
        i += 1
    if not lines:
        lines.append(write_cmd(page_dir, 1, comment_body("empty page")))
    with open(os.path.join(page_dir, "event.pal"), "w", encoding="utf-8") as f:
        f.write("# event.pal - from the TSOTS map JSON\n")
        f.writelines(lines)


def write_db(data, book):
    db = os.path.join(book, "db")
    os.makedirs(db, exist_ok=True)
    tables = ("Actors", "Classes", "Skills", "Items", "Weapons", "Armors",
              "Enemies", "Troops", "States", "CommonEvents")
    for name in tables:
        path = os.path.join(data, name + ".json")
        if not os.path.isfile(path):
            continue
        rows = json.load(open(path, encoding="utf-8-sig"))
        out = []
        for i, row in enumerate(rows):
            if not row:
                continue
            label = row.get("name") or row.get("name") or ""
            if not label and name == "CommonEvents":
                label = ""
            label = safe_name(label, "")
            if not label:
                continue
            out.append("%d\t%s\n" % (i, label))
        with open(os.path.join(db, name.lower() + ".txt"), "w", encoding="utf-8") as f:
            f.writelines(out)
        print("db", name, len(out))


def main():
    data = find_data()
    write_db(data, BOOK)
    events_n = 0
    for fn in sorted(os.listdir(data)):
        if not (fn.startswith("Map") and fn.endswith(".json") and fn != "MapInfos.json"):
            continue
        mid = int(fn[3:-5])
        desk = os.path.join(BOOK, "map%03d" % mid)
        if not os.path.isdir(desk):
            continue
        doc = json.load(open(os.path.join(data, fn), encoding="utf-8-sig"))
        bar = []
        ev_root = os.path.join(desk, "ev")
        os.makedirs(ev_root, exist_ok=True)
        for ev in doc.get("events") or []:
            if not ev:
                continue
            pages = ev.get("pages") or []
            if not pages:
                continue
            eid = int(ev.get("id") or 0)
            ev_dir = os.path.join(ev_root, str(eid))
            # A re-run replaces the pages. Leave the directory.
            os.makedirs(ev_dir, exist_ok=True)
            name = safe_name(ev.get("name"), "event %d" % eid)
            with open(os.path.join(ev_dir, "name.txt"), "w", encoding="utf-8") as f:
                f.write(name + "\n")
            for i, page in enumerate(pages, start=1):
                write_page(ev_dir, page, i)
            bar.append("%d\t%s\t%d\t%d\t%s\n" % (
                eid, name, int(ev.get("x") or 0), int(ev.get("y") or 0),
                active_trigger(ev)))
            events_n += 1
        with open(os.path.join(desk, "bar.txt"), "w", encoding="utf-8") as f:
            f.writelines(bar)
        if bar:
            print("map%03d" % mid, len(bar))
    print("events", events_n)


if __name__ == "__main__":
    main()
