#!/bin/sh
# build-biz-book.sh — render the whole 14.biz/ section into one
# self-contained, human-readable HTML page (BIZ-BOOK.html).
#
# No external deps (no pandoc / no python-markdown). A small, deliberate
# markdown SUBSET is supported: # .. ###### headings, - lists, 1. lists,
# ``` fenced code, > blockquote, | tables, **bold**, `code`, [t](url),
# --- hr, and paragraphs. Good enough for these docs; not a full parser.
#
# Re-run after editing any .md in 14.biz/.  Output is committed so it can
# be opened straight from a checkout.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
BIZ="$(cd "$HERE/.." && pwd)"
OUT="$BIZ/BIZ-BOOK.html"

python3 - "$BIZ" "$OUT" <<'PY'
import html, os, re, sys, datetime

BIZ, OUT = sys.argv[1], sys.argv[2]

# ---- file order -----------------------------------------------------
order = [
    "00-INDEX.md",
    "ELEVATOR-PITCH.md",
    "HOLDINGS-STRUCTURE.md",
    "FUTURE-GOALS.md",
    "STRATEGY/00-INDEX.md",
    "STRATEGY/strategy.md", "STRATEGY/sales.md", "STRATEGY/marketing.md",
    "STRATEGY/pricing.md", "STRATEGY/networking.md", "STRATEGY/legal.md",
    "STRATEGY/ethics.md",
    "OUTLETS/_ALL-cross-cutting.md",
]
for d in sorted(os.listdir(os.path.join(BIZ, "OUTLETS"))):
    p = os.path.join("OUTLETS", d, "00-INDEX.md")
    if os.path.isfile(os.path.join(BIZ, p)):
        order.append(p)

# ---- tiny markdown subset -> html ---------------------------------
def inline(s):
    s = html.escape(s)
    s = re.sub(r'`([^`]+)`', r'<code>\1</code>', s)
    s = re.sub(r'\*\*([^*]+)\*\*', r'<strong>\1</strong>', s)
    s = re.sub(r'\[([^\]]+)\]\(([^)]+)\)', r'<a href="\2">\1</a>', s)
    return s

def render(md, slug):
    out, i, lines = [], 0, md.split("\n")
    def close_list(stack):
        while stack:
            out.append("</%s>" % stack.pop())
    liststack = []
    while i < len(lines):
        ln = lines[i]
        if ln.strip().startswith("```"):
            close_list(liststack)
            i += 1; buf = []
            while i < len(lines) and not lines[i].strip().startswith("```"):
                buf.append(html.escape(lines[i])); i += 1
            i += 1
            out.append("<pre><code>%s</code></pre>" % "\n".join(buf)); continue
        m = re.match(r'^(#{1,6})\s+(.*)$', ln)
        if m:
            close_list(liststack)
            lvl = len(m.group(1)); txt = inline(m.group(2))
            aid = ""
            if lvl <= 2:
                aid = ' id="%s"' % slug if lvl == 1 else ''
            out.append("<h%d%s>%s</h%d>" % (lvl, aid, txt, lvl)); i += 1; continue
        if re.match(r'^\s*---\s*$', ln):
            close_list(liststack); out.append("<hr>"); i += 1; continue
        if ln.startswith(">"):
            close_list(liststack); buf = []
            while i < len(lines) and lines[i].startswith(">"):
                buf.append(inline(lines[i].lstrip(">").strip())); i += 1
            out.append("<blockquote>%s</blockquote>" % "<br>".join(buf)); continue
        if "|" in ln and i + 1 < len(lines) and re.match(r'^\s*\|?[\s:\-|]+\|?\s*$', lines[i+1]):
            close_list(liststack)
            def row(r): return [c.strip() for c in r.strip().strip("|").split("|")]
            head = row(ln); i += 2; body = []
            while i < len(lines) and "|" in lines[i] and lines[i].strip():
                body.append(row(lines[i])); i += 1
            t = ["<table><thead><tr>"] + ["<th>%s</th>" % inline(c) for c in head] + ["</tr></thead><tbody>"]
            for b in body:
                t.append("<tr>" + "".join("<td>%s</td>" % inline(c) for c in b) + "</tr>")
            t.append("</tbody></table>")
            out.append("".join(t)); continue
        m = re.match(r'^(\s*)([-*]|\d+\.)\s+(.*)$', ln)
        if m:
            indent = len(m.group(1)); ordered = m.group(2).endswith(".")
            tag = "ol" if ordered else "ul"
            if not liststack:
                liststack.append(tag); out.append("<%s>" % tag)
            out.append("<li>%s</li>" % inline(m.group(3))); i += 1; continue
        if ln.strip() == "":
            close_list(liststack); i += 1; continue
        close_list(liststack)
        # paragraph: join wrapped lines so multi-line **bold** / links resolve
        buf = [ln]; i += 1
        while i < len(lines):
            nx = lines[i]
            if (nx.strip() == "" or nx.strip().startswith("```")
                    or re.match(r'^#{1,6}\s', nx) or nx.startswith(">")
                    or re.match(r'^\s*([-*]|\d+\.)\s+', nx)
                    or re.match(r'^\s*---\s*$', nx)):
                break
            buf.append(nx); i += 1
        out.append("<p>%s</p>" % inline(" ".join(x.strip() for x in buf)))
    close_list(liststack)
    return "\n".join(out)

# ---- assemble -----------------------------------------------------
sections, toc = [], []
for rel in order:
    fp = os.path.join(BIZ, rel)
    if not os.path.isfile(fp):
        continue
    md = open(fp, encoding="utf-8").read()
    slug = re.sub(r'[^a-z0-9]+', '-', rel.lower()).strip('-')
    m = re.search(r'^#\s+(.*)$', md, re.M)
    title = m.group(1) if m else rel
    depth = rel.count("/")
    toc.append('<li class="d%d"><a href="#%s">%s</a> <span class="src">%s</span></li>'
               % (depth, slug, html.escape(title), html.escape(rel)))
    sections.append('<section><a class="anchor" id="%s"></a>\n%s\n<p class="src">— %s</p></section>'
                    % (slug, render(md, slug), html.escape(rel)))

# founder intake, verbatim, at the end
raw_fp = os.path.join(BIZ, "user-biz-request.txt")
if os.path.isfile(raw_fp):
    raw = html.escape(open(raw_fp, encoding="utf-8").read())
    toc.append('<li class="d0"><a href="#founder-intake">Founder intake (raw)</a> <span class="src">user-biz-request.txt</span></li>')
    sections.append('<section><a class="anchor" id="founder-intake"></a><h1>Founder intake (raw, verbatim)</h1>'
                    '<pre class="raw">%s</pre></section>' % raw)

now = datetime.date.today().isoformat()
TEMPLATE = """<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>14.biz — HQ business book</title>
<style>
:root{--bg:#faf9f7;--fg:#1c1a17;--mut:#6b6459;--line:#e2ddd4;--card:#fff;--acc:#7a5c2e;--code:#f0ece4}
@media (prefers-color-scheme:dark){:root{--bg:#141312;--fg:#e7e3db;--mut:#9a9184;--line:#2c2a27;--card:#1b1a18;--acc:#d7b070;--code:#221f1b}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--fg);
font:15px/1.62 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Ubuntu,sans-serif}
.wrap{max-width:860px;margin:0 auto;padding:34px 22px 90px}
h1{font-size:1.5rem;margin:2.2em 0 .5em;padding-top:.3em;border-top:2px solid var(--line)}
section:first-of-type h1{border-top:0;margin-top:0}
h2{font-size:1.16rem;margin:1.5em 0 .4em}h3{font-size:1rem;margin:1.3em 0 .3em;color:var(--mut)}
h4{font-size:.92rem;margin:1.1em 0 .2em;color:var(--mut);text-transform:uppercase;letter-spacing:.04em}
a{color:var(--acc)}code{background:var(--code);padding:.08em .35em;border-radius:3px;font-size:.86em}
pre{background:var(--code);padding:14px 16px;border-radius:7px;overflow:auto;font-size:.83rem;line-height:1.5}
pre.raw{white-space:pre-wrap}
blockquote{margin:.6em 0;padding:.3em 0 .3em 14px;border-left:3px solid var(--acc);color:var(--mut)}
table{border-collapse:collapse;width:100%;margin:.7em 0;font-size:.92rem}
th,td{border:1px solid var(--line);padding:6px 10px;text-align:left;vertical-align:top}
th{background:var(--code)}
hr{border:0;border-top:1px solid var(--line);margin:1.4em 0}
.src{color:var(--mut);font-size:.8rem}
.masthead{padding:8px 0 4px}.masthead .k{color:var(--mut);font-size:.82rem}
nav.toc{background:var(--card);border:1px solid var(--line);border-radius:9px;padding:14px 18px;margin:18px 0 6px}
nav.toc ol{margin:.2em 0;padding-left:1.4em}nav.toc li{margin:.15em 0}
nav.toc li.d1{list-style:circle}nav.toc li.d2{list-style:square}
nav.toc .src{margin-left:.5em}
section{scroll-margin-top:14px}
.note{background:var(--card);border:1px solid var(--line);border-left:3px solid var(--acc);border-radius:7px;padding:10px 14px;margin:14px 0;font-size:.9rem;color:var(--mut)}
</style></head><body><div class="wrap">
<div class="masthead"><h1 style="border:0;margin:0">14.biz — HQ business book</h1>
<div class="k">Generated __NOW__ from the .md files in <code>14.biz/</code> · one-page view · re-run <code>tools/build-biz-book.sh</code> after edits</div></div>
<div class="note">Founder-owned, internal. Every section below is a <code>DRAFT</code> stub capturing stated intent — no business claims have been invented. For a compact tech+biz brief to hand another agent, use <code>../HQ-BRIEF.md</code>.</div>
<nav class="toc"><strong>Contents</strong><ol>__TOC__</ol></nav>
__BODY__
</div></body></html>
"""
doc = (TEMPLATE.replace("__NOW__", now)
               .replace("__TOC__", "\n".join(toc))
               .replace("__BODY__", "\n".join(sections)))

open(OUT, "w", encoding="utf-8").write(doc)
print("wrote", OUT, "(%d sections)" % len(sections))
PY
echo "OK $OUT"
