#!/usr/bin/env python3
"""map_trigger_convert.py - one-off converter (BOOK BLUEPRINTZ-DESIGN.md 12.14, owner 2026-10-15: "all should use play event").
Map events (doom, chess, ...) keep their trigger in the desk's bar.txt (TSV: id name x y trigger) and have a
condition.pdl with only `COND | switches | none`, so play_event.sh finds no page for ANY trigger and they can only run
through the direct verb scripts. This adds the missing `COND | trigger | <bar.txt column 5>` row to each such page_1 condition.pdl.
Additive and idempotent: a file that already has a trigger row is never touched; an event with no bar.txt row is left alone and counted.
Usage:  map_trigger_convert.py <maps_dir> [--apply]      (dry run by default; prints counts per desk)
        maps_dir = .../piececraft-hq/pieces/system/maps/<book>   (one book, e.g. doom)
Changes nothing else, and does not change which script the game calls today (the direct verb path still exists)."""
import os, sys
def has_trigger(text):
    for l in text.split('\n'):
        f = l.split('|')
        if l.startswith('COND') and len(f) > 1 and 'trigger' in f[1]: return True
    return False
def main():
    if len(sys.argv) < 2: print(__doc__); return 2
    root = sys.argv[1]; apply = '--apply' in sys.argv
    tot = dict(converted=0, had_trigger=0, no_bar_row=0, no_condition_file=0, conflicting=0)
    for desk in sorted(os.listdir(root)):
        d = os.path.join(root, desk); evd = os.path.join(d, 'ev'); bar = os.path.join(d, 'bar.txt')
        if not os.path.isdir(evd): continue
        rows = {}
        if os.path.exists(bar):
            for l in open(bar, encoding='utf-8', errors='replace'):
                f = l.rstrip('\n').split('\t')
                if len(f) >= 5 and f[0]: rows.setdefault(f[0], []).append(f[4].strip())
        c = dict(converted=0, had_trigger=0, no_bar_row=0, no_condition_file=0, conflicting=0)
        for ev in sorted(os.listdir(evd)):
            cp = os.path.join(evd, ev, 'event_pkg', 'pages', 'page_1', 'condition.pdl')
            if not os.path.exists(cp): c['no_condition_file'] += 1; continue
            text = open(cp, encoding='utf-8').read()
            if has_trigger(text):
                c['had_trigger'] += 1; continue
            vals = set(rows.get(ev, []))
            if not vals: c['no_bar_row'] += 1; continue
            if len(vals) > 1: c['conflicting'] += 1; continue
            if apply:
                if not text.endswith('\n'): text += '\n'
                open(cp, 'w', encoding='utf-8').write(text + 'COND | trigger | %s\n' % vals.pop())
            c['converted'] += 1
        print('%-14s %s' % (desk, ' '.join('%s=%d' % kv for kv in c.items())))
        for k in tot: tot[k] += c[k]
    print('TOTAL          ' + ' '.join('%s=%d' % kv for kv in tot.items()) + ('  (APPLIED)' if apply else '  (dry run)'))
    return 0
if __name__ == '__main__': sys.exit(main())
