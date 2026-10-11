#!/usr/bin/env python3
# themecheck.py - static half of the light-theme readability pin.
# Usage: themecheck.py <xhtpm> <css>   (exit 0 = covered + contrasting)
#
# The 2026-10-04 trap, stated as a check: on a light user theme
# g_theme_fg is #000000 while the browser window forces a dark chrome,
# so any text-drawing element WITHOUT its own color rule renders
# black-on-black. This asserts the wiring that prevents it:
#   A. every <text>/<item>/<cli_io> element carries a class list with
#      at least one color rule (single-class or compound; tag and #id
#      selectors count too), and container tags carry no label=
#      (they cannot render text, so their classes need nothing).
#      <canvas> is exempt (kh_draw_canvas blits pixels, never text);
#      <bar> is exempt (C defaults, pixel-pinned by nb_theme_test.sh).
#   B. every color: rule contrasts >= 4.5 (WCAG AA) against its own
#      declared background, else against both window surfaces
#      (#141414 window, #1a1a1a content/sidebar).
# Stdlib only.
import re
import sys


def lum(h):
    h = h.lstrip('#')
    c = [int(h[i:i + 2], 16) / 255 for i in (0, 2, 4)]

    def f(v):
        return v / 12.92 if v <= 0.03928 else ((v + 0.055) / 1.055) ** 2.4

    return 0.2126 * f(c[0]) + 0.7152 * f(c[1]) + 0.0722 * f(c[2])


def ratio(fg, bg):
    a, b = sorted([lum(fg), lum(bg)])
    return (b + 0.05) / (a + 0.05)


def main():
    xhtpm = open(sys.argv[1]).read()
    css = re.sub(r'/\*.*?\*/', '', open(sys.argv[2]).read(), flags=re.S)
    fails = []

    # css color rules: (subject_classes_or_None, tag_or_None, id_or_None, fg, bg)
    rules = []
    for m in re.finditer(r'([^{}]+)\{([^}]*)\}', css):
        for sel_raw in m.group(1).split(','):
            sel_raw = sel_raw.strip()
            if not sel_raw:
                continue
            body = m.group(2)
            fg = re.search(r'(?<![a-zA-Z-])color:\s*(#[0-9a-fA-F]{6})', body)
            if not fg:
                continue
            bgm = re.search(r'background-color:\s*(#[0-9a-fA-F]{6})', body)
            subj = sel_raw.split()[-1].split(':')[0]  # last segment, no :hover
            if subj.startswith('#'):
                rules.append(([], None, subj[1:], fg.group(1),
                              bgm.group(1) if bgm else None))
            elif subj.startswith('.'):
                rules.append((subj[1:].split('.'), None, None, fg.group(1),
                              bgm.group(1) if bgm else None))
            else:
                tag = subj.split('.')[0]
                rules.append(([], tag, None, fg.group(1),
                              bgm.group(1) if bgm else None))

    # A. element coverage
    for m in re.finditer(r'<(text|item|cli_io|bar|canvas)\b([^>]*?)>', xhtpm, re.S):
        tag, attrs = m.group(1), m.group(2)
        if tag == 'canvas':
            continue  # blits pixels, never draws text
        if tag == 'bar':
            continue  # C defaults (#ccc on #222), pixel-pinned, not css
        cm = re.search(r'class="([^"]*)"', attrs)
        if not cm or not cm.group(1).strip():
            fails.append('<%s> without class (falls to theme fg): %s'
                         % (tag, attrs.strip()[:60]))
            continue
        have = cm.group(1).split()
        eid = re.search(r'id="([^"]*)"', attrs)
        eid = eid.group(1) if eid else None
        ok = False
        for need, rtag, rid, fg, bg in rules:
            if rid is not None:
                if eid and '${' not in eid and eid == rid:
                    ok = True
                    break
            elif rtag is not None:
                if rtag == tag:
                    ok = True
                    break
            elif all(c in have for c in need):
                ok = True
                break
        if not ok:
            fails.append('<%s> classes=%s: no color rule' % (tag, have))
    # containers must not smuggle text through label=
    for tag in ['page', 'panel', 'row', 'scrolllist', 'repeat', 'box',
                'sidebar', 'title']:
        for m in re.finditer(r'<%s\b([^>]*?)>' % tag, xhtpm, re.S):
            if re.search(r'label="', m.group(1)):
                fails.append('<%s> carries label= (container with text)' % tag)

    # B. arithmetic contrast on every rule
    for need, rtag, rid, fg, bg in rules:
        bgs = [bg] if bg else ['#141414', '#1a1a1a']
        for b in bgs:
            r = ratio(fg, b)
            if r < 4.5:
                name = ('#' + rid) if rid else (rtag or '.' + '.'.join(need))
                fails.append('%s fg=%s on %s = %.1f (< 4.5)' % (name, fg, b, r))

    n_elem = len(re.findall(r'<(text|item|cli_io)\b', xhtpm))
    print('themecheck: %d text/item/cli_io elements, %d color rules, %d failures'
          % (n_elem, len(rules), len(fails)))
    for f in fails:
        print('FAIL: ' + f)
    return 1 if fails else 0


sys.exit(main())
