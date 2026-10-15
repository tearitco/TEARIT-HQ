#!/usr/bin/env python3
"""doom_verb_fix.py [--apply] <doom maps dir>
Make every doom event page run the verb its tile has in events.pdl `cmds=` (row n of events.pdl == ev/n == bar.txt row n,
checked: same cell in all 2,548 cases). Rewrites verb.txt, pages/page_1/cmd_1.sh and event.pal in the exact shape the
already-correct pages have (kill_event line only for change_ammo / change_hp / change_items). Dry run by default."""
import os, re, sys
apply = '--apply' in sys.argv
root = [a for a in sys.argv[1:] if not a.startswith('--')][0]
HEAD = '#!/bin/sh\nD="$(cd "$(dirname "$0")" && pwd)"\nwhile [ "$D" != / ] && [ ! -d "$D/xyzfs" ]; do D="$(dirname "$D")"; done\n'
RUN = 'sh "$D/@.apps/piececraft-hq/ops/doom_event.sh" %s\n'
KILL = ('change_ammo', 'change_hp', 'change_items')
fixed = same = skipped = 0
for desk in sorted(os.listdir(root)):
    ep = os.path.join(root, desk, 'events.pdl'); evd = os.path.join(root, desk, 'ev')
    if not (os.path.exists(ep) and os.path.isdir(evd)): continue
    rows = []
    for l in open(ep, encoding='utf-8', errors='replace'):
        if l.lstrip().startswith('EVENT'):
            m = re.search(r'x=(\d+)\s+y=(\d+)\s+glyph=(\S+)', l); c = re.search(r'cmds=(\S+)', l)
            rows.append((m.groups(), c.group(1)) if m and c else None)
    for e in sorted(os.listdir(evd), key=int):
        n = int(e); d = os.path.join(evd, e); pg = os.path.join(d, 'event_pkg', 'pages', 'page_1')
        vp = os.path.join(d, 'verb.txt')
        if n > len(rows) or rows[n-1] is None or not os.path.exists(vp) or not os.path.isdir(pg): skipped += 1; continue
        (x, y, g), cmd = rows[n-1]
        if (open(vp, encoding='utf-8').read().split() or [''])[0] == cmd: same += 1; continue
        sh = HEAD + RUN % cmd + (RUN % ('kill_event %s %d' % (desk, n)) if cmd in KILL else '')
        pal = '# %s at %s,%s glyph %s\nexec cmd_1.sh\n' % (cmd, x, y, g)
        fixed += 1
        if apply:
            for p, t in ((vp, cmd + '\n'), (os.path.join(pg, 'cmd_1.sh'), sh), (os.path.join(pg, 'event.pal'), pal)):
                with open(p, 'w', encoding='utf-8', newline='') as f: f.write(t)
print('%s: to fix %d, already right %d, skipped %d' % ('APPLIED' if apply else 'dry run', fixed, same, skipped))
