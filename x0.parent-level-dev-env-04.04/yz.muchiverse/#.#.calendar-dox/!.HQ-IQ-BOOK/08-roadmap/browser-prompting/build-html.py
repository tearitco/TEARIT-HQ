#!/usr/bin/env python3
"""
build-html.py - regenerate the readable HTML "Browser Prompting Guide" from
this directory's markdown source.

  source : #.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/browser-prompting/*.md
  output : #.#.calendar-dox/#.browser-prompting.html/
             0.browser-p-index=#.html
             #.browser-prompting/1..8.<slug>.html

The HTML tree is a SNAPSHOT for user reading, not a live mirror - re-run this
script after editing the markdown so the two stay in sync. Stdlib only.

    python3 build-html.py
"""
import html, os, re, sys, datetime

HERE = os.path.dirname(os.path.abspath(__file__))
# .../#.#.calendar-dox/!.HQ-IQ-BOOK/08-roadmap/browser-prompting  -> up 3 = #.#.calendar-dox
CALDOX = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
OUT_ROOT = os.path.join(CALDOX, "#.browser-prompting.html")
OUT_SUB = os.path.join(OUT_ROOT, "#.browser-prompting")

# page slug, nav label, status dot, list of source md files (relative to HERE)
PAGES = [
    ("1.platform-primer", "Platform Primer", "done",
     ["1.platform-primer-ALWAYS-ATTACH.md"]),
    ("2.house-shape-status", "House Shape Status", "done",
     ["2.house-shape-status-report.md"]),
    ("3.ai-agents", "AI Agents", "wip",
     ["ai-agents/3.chat-hai-openhai-harnecient-delegation.md",
      "ai-agents/4.rmmv-event-scripting-ai-teaching-delegation.md",
      "ai-agents/5.local-gemma-harness-fallback-delegation.md"]),
    ("4.open-hai-free-ai", "open-hai + Free AI", "done",
     ["open-hai-free-ai/1.live-test-findings-2026-08-18.md",
      "open-hai-free-ai/#.wassup-hai.txt"]),
    ("5.bugs-toys-gl", "Bugs / Toys / GL", "wip",
     ["bugs-toys-gl/5.remaining-bugs-joystick-toys-delegation.md",
      "bugs-toys-gl/6.gl-to-x11-toolbar-piececraft-delegation.md"]),
    ("6.architecture-explainers", "Architecture Explainers", "wip",
     ["architecture-explainers/7.collaborative-entity-games-delegation.md",
      "architecture-explainers/8.data-storage-ledger-delegation.md",
      "architecture-explainers/9.networking-13network-delegation.md",
      "architecture-explainers/10.board-viewer-drag-drop-entities-delegation.md",
      "architecture-explainers/11.palettes-toys-pals-delegation.md"]),
    ("7.platform-passes", "Grok's Lanes", "wip",
     ["platform-passes/12.grok-windows-mac-compat-delegation.md",
      "platform-passes/13.grok-media-studio-continuation-delegation.md"]),
    ("8.project-starters", "Project Starters", "wip",
     ["project-starters/14.business-sim-game-starter-delegation.md",
      "project-starters/15.xyz-installer-dev-exploration-delegation.md",
      "project-starters/16.calendar-notebook-hq-delegation.md"]),
]
CARD_DESC = {
    "1.platform-primer": "PIECE→MODULE→OS, the two parser families, input/relay conventions, CENTROID_GOLD_STD. Your default context upload.",
    "2.house-shape-status": "Honest answer to “is the house in shape?” — what's verified solid, what's genuine drift, where docs actually live.",
    "3.ai-agents": "chat-hai UI, Harnecient technique, RMMV event scripting, and pushing weak local models (gemma3:270m/1b) via deterministic dispatch.",
    "4.open-hai-free-ai": "Live test results — OpenRouter key works, which free models return real tool-calls, quota mechanics, the read/write approval boundary.",
    "5.bugs-toys-gl": "Leftover bugs, joystick port, more taskbar toys, piececraft-xyz GL→X11 migration.",
    "6.architecture-explainers": "Collaborative entity games, the append-only ledger / one-writer model, networking cell wiring, board-viewer drag-drop, palettes-as-pals.",
    "7.platform-passes": "Grok's lanes: Windows/Mac compat catch-up, and the 103.media-studio → x11-HQ toy conversion (decision made, phased handoff).",
    "8.project-starters": "Business-sim game plan, xyz-installer-dev exploration, calendar/notebook HQ app.",
}

STYLE = """<style>
  :root {
    --bg: #f4f3ef; --surface: #ffffff; --surface-2: #ececE6; --border: #dcdad2;
    --text: #1c1c1a; --text-dim: #6b6a63; --accent: #1a8a4a; --accent-warm: #b5720a;
    --status-wip: #2563a8; --code-bg: #ececE6; --code-text: #2b2b28;
    --mono: ui-monospace, "SF Mono", "Cascadia Code", Consolas, "Liberation Mono", monospace;
    --sans: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
  }
  :root:not([data-theme="light"]) { @media (prefers-color-scheme: dark) {
    --bg: #14161a; --surface: #1b1e24; --surface-2: #21252c; --border: #2b2f37;
    --text: #e9e7e2; --text-dim: #9599a1; --accent: #22c55e; --accent-warm: #f0a63f;
    --status-wip: #5b9dd9; --code-bg: #16181c; --code-text: #dcdad4;
  } }
  :root[data-theme="dark"] {
    --bg: #14161a; --surface: #1b1e24; --surface-2: #21252c; --border: #2b2f37;
    --text: #e9e7e2; --text-dim: #9599a1; --accent: #22c55e; --accent-warm: #f0a63f;
    --status-wip: #5b9dd9; --code-bg: #16181c; --code-text: #dcdad4;
  }
  * { box-sizing: border-box; }
  body { margin: 0; background: var(--bg); color: var(--text); font-family: var(--sans); line-height: 1.65; -webkit-font-smoothing: antialiased; }
  a { color: var(--accent); }
  a:focus-visible, .navlink:focus-visible { outline: 2px solid var(--accent); outline-offset: 2px; }
  .shell { display: grid; grid-template-columns: 260px minmax(0, 1fr); min-height: 100vh; }
  .sidebar { border-right: 1px solid var(--border); background: var(--surface); padding: 28px 20px 40px; position: sticky; top: 0; height: 100vh; overflow-y: auto; }
  .brand { font-family: var(--mono); font-size: 0.78rem; text-transform: uppercase; letter-spacing: 0.08em; color: var(--text-dim); margin-bottom: 4px; }
  .brand strong { display: block; font-family: var(--sans); font-size: 1.05rem; font-weight: 800; letter-spacing: -0.01em; color: var(--text); text-transform: none; margin-top: 2px; }
  nav { margin-top: 24px; display: flex; flex-direction: column; gap: 20px; }
  .navgroup-label { font-family: var(--mono); font-size: 0.7rem; text-transform: uppercase; letter-spacing: 0.1em; color: var(--text-dim); margin-bottom: 8px; }
  .navlinks { display: flex; flex-direction: column; gap: 2px; }
  .navlink { display: flex; align-items: center; gap: 8px; padding: 6px 8px; border-radius: 6px; color: var(--text); text-decoration: none; font-size: 0.88rem; }
  .navlink:hover { background: var(--surface-2); }
  .navlink.current { background: var(--surface-2); font-weight: 700; }
  .navlink .dot { width: 6px; height: 6px; border-radius: 50%; flex-shrink: 0; background: var(--border); }
  .navlink .dot.done { background: var(--accent); }
  .navlink .dot.wip { background: var(--status-wip); }
  main { padding: 56px 40px 120px; min-width: 0; }
  .col { max-width: 820px; margin: 0 auto; }
  .hero-eyebrow { font-family: var(--mono); font-size: 0.78rem; text-transform: uppercase; letter-spacing: 0.1em; color: var(--accent); margin-bottom: 10px; }
  h1.pagetitle { font-size: 2.1rem; font-weight: 800; letter-spacing: -0.02em; text-wrap: balance; margin: 0 0 14px; }
  .lede { font-size: 1.08rem; color: var(--text-dim); max-width: 64ch; margin: 0 0 40px; }
  section.doc { padding-top: 40px; margin-top: 8px; border-top: 1px solid var(--border); scroll-margin-top: 24px; }
  section.doc:first-of-type { border-top: none; padding-top: 0; }
  .section-path { font-family: var(--mono); font-size: 0.78rem; color: var(--text-dim); margin: 0 0 4px; }
  h1 { font-size: 1.7rem; font-weight: 800; letter-spacing: -0.02em; text-wrap: balance; margin: 0 0 14px; }
  h2 { font-size: 1.35rem; font-weight: 800; letter-spacing: -0.01em; text-wrap: balance; margin: 30px 0 8px; }
  h3 { font-size: 1.08rem; font-weight: 700; margin: 26px 0 10px; }
  h4 { font-size: 0.95rem; font-weight: 700; margin: 20px 0 8px; color: var(--text-dim); }
  p { margin: 0 0 16px; max-width: 74ch; }
  ul, ol { margin: 0 0 16px; padding-left: 22px; }
  li { margin-bottom: 6px; max-width: 72ch; }
  li > ul, li > ol { margin: 6px 0 4px; }
  code { font-family: var(--mono); font-size: 0.86em; background: var(--code-bg); color: var(--code-text); padding: 0.12em 0.4em; border-radius: 4px; }
  pre { background: var(--code-bg); color: var(--code-text); border: 1px solid var(--border); border-radius: 8px; padding: 16px 18px; overflow-x: auto; margin: 0 0 20px; }
  pre code { background: none; padding: 0; font-size: 0.84rem; line-height: 1.55; }
  blockquote { margin: 0 0 18px; padding: 4px 16px; border-left: 3px solid var(--accent); color: var(--text-dim); }
  blockquote p { margin: 8px 0; }
  hr { border: none; border-top: 1px solid var(--border); margin: 28px 0; }
  .table-wrap { overflow-x: auto; margin: 0 0 20px; }
  table { border-collapse: collapse; width: 100%; font-size: 0.9rem; }
  th, td { text-align: left; padding: 8px 12px; border-bottom: 1px solid var(--border); vertical-align: top; }
  th { font-family: var(--mono); font-size: 0.74rem; text-transform: uppercase; letter-spacing: 0.06em; color: var(--text-dim); }
  .cardgrid { display: grid; grid-template-columns: repeat(auto-fill, minmax(280px, 1fr)); gap: 14px; margin: 0 0 32px; }
  .card { display: block; border: 1px solid var(--border); background: var(--surface); border-radius: 10px; padding: 18px 20px; text-decoration: none; color: var(--text); }
  .card:hover { border-color: var(--accent); }
  .card .card-title { font-weight: 700; font-size: 1.0rem; margin-bottom: 6px; }
  .card .card-desc { color: var(--text-dim); font-size: 0.86rem; margin: 0; }
  footer.pagefoot { max-width: 820px; margin: 64px auto 0; padding-top: 24px; border-top: 1px solid var(--border); color: var(--text-dim); font-size: 0.85rem; }
  @media (max-width: 880px) { .shell { grid-template-columns: 1fr; } .sidebar { position: static; height: auto; border-right: none; border-bottom: 1px solid var(--border); } main { padding: 40px 20px 100px; } }
  html { scroll-behavior: smooth; }
</style>"""

# ----------------------------------------------------------------------------
# tiny markdown -> HTML (block parser + conservative inline pass)
# ----------------------------------------------------------------------------
def esc(s):
    return html.escape(s, quote=False)

_INLINE_CODE = re.compile(r"`([^`]+)`")
_LINK = re.compile(r"\[([^\]]+)\]\(([^)\s]+)\)")
_BOLD = re.compile(r"\*\*([^*]+)\*\*")

def inline(s):
    # protect code spans first
    spans = []
    def stash(m):
        spans.append(m.group(1))
        return "\x00%d\x00" % (len(spans) - 1)
    s = _INLINE_CODE.sub(stash, s)
    s = esc(s)
    s = _LINK.sub(lambda m: '<a href="%s">%s</a>' % (html.escape(m.group(2), quote=True), m.group(1)), s)
    s = _BOLD.sub(r"<strong>\1</strong>", s)
    s = re.sub(r"\x00(\d+)\x00", lambda m: "<code>%s</code>" % esc(spans[int(m.group(1))]), s)
    return s

def _split_row(line):
    line = line.strip()
    if line.startswith("|"):
        line = line[1:]
    if line.endswith("|"):
        line = line[:-1]
    return [c.strip() for c in line.split("|")]

def render_markdown(text):
    lines = text.replace("\r\n", "\n").split("\n")
    out = []
    i, n = 0, len(lines)
    while i < n:
        line = lines[i]
        # fenced code
        if line.lstrip().startswith("```"):
            i += 1
            buf = []
            while i < n and not lines[i].lstrip().startswith("```"):
                buf.append(lines[i]); i += 1
            i += 1  # closing fence
            out.append("<pre><code>%s</code></pre>" % esc("\n".join(buf)))
            continue
        # blank
        if not line.strip():
            i += 1; continue
        # heading
        m = re.match(r"^(#{1,6})\s+(.*)$", line)
        if m:
            lvl = len(m.group(1))
            out.append("<h%d>%s</h%d>" % (lvl, inline(m.group(2).strip()), lvl))
            i += 1; continue
        # hr
        if re.match(r"^\s*---+\s*$", line):
            out.append("<hr>"); i += 1; continue
        # table (header line followed by a |---| separator)
        if "|" in line and i + 1 < n and re.match(r"^\s*\|?[\s:|-]+\|[\s:|-]+$", lines[i + 1]):
            header = _split_row(line)
            i += 2
            rows = []
            while i < n and "|" in lines[i] and lines[i].strip():
                rows.append(_split_row(lines[i])); i += 1
            th = "".join("<th>%s</th>" % inline(c) for c in header)
            trs = []
            for r in rows:
                tds = "".join("<td>%s</td>" % inline(c) for c in r)
                trs.append("<tr>%s</tr>" % tds)
            out.append('<div class="table-wrap"><table><thead><tr>%s</tr></thead><tbody>%s</tbody></table></div>'
                       % (th, "".join(trs)))
            continue
        # blockquote
        if line.lstrip().startswith(">"):
            buf = []
            while i < n and lines[i].lstrip().startswith(">"):
                buf.append(re.sub(r"^\s*>\s?", "", lines[i])); i += 1
            out.append("<blockquote>%s</blockquote>" % render_markdown("\n".join(buf)))
            continue
        # lists (one level of nesting for '- ')
        m = re.match(r"^(\s*)([-*]|\d+[.)])\s+(.*)$", line)
        if m:
            ordered = bool(re.match(r"\d", m.group(2)))
            tag = "ol" if ordered else "ul"
            items, cur_indent = [], len(m.group(1))
            while i < n:
                mm = re.match(r"^(\s*)([-*]|\d+[.)])\s+(.*)$", lines[i])
                if not mm:
                    # continuation line indented under the item
                    if lines[i].strip() and lines[i].startswith(" " * (cur_indent + 2)) and items:
                        items[-1][1] += " " + lines[i].strip(); i += 1; continue
                    break
                ind = len(mm.group(1))
                if ind > cur_indent + 1:
                    items[-1][2].append(mm.group(0)); i += 1; continue
                items.append([mm.group(2), mm.group(3), []]); i += 1
            lis = []
            for _mark, body, sub in items:
                inner = inline(body)
                if sub:
                    inner += render_markdown("\n".join(s.strip() for s in sub))
                lis.append("<li>%s</li>" % inner)
            out.append("<%s>%s</%s>" % (tag, "".join(lis), tag))
            continue
        # paragraph (gather until blank / block start)
        buf = [line]; i += 1
        while i < n and lines[i].strip() and not re.match(
                r"^(#{1,6}\s|\s*```|\s*[-*]\s|\s*\d+[.)]\s|\s*>|\s*---+\s*$)", lines[i]):
            buf.append(lines[i]); i += 1
        para = " ".join(x.strip() for x in buf)
        out.append("<p>%s</p>" % inline(para))
    return "\n".join(out)

# ----------------------------------------------------------------------------
# page assembly
# ----------------------------------------------------------------------------
BUILT = datetime.date.today().isoformat()

def sidebar(depth, current_slug):
    up = "../" * depth
    rows = ['<a class="navlink%s" href="%s0.browser-p-index=%s.html">Overview / index</a>'
            % ("" if current_slug else " current", up, "%23")]
    for slug, label, dot, _ in PAGES:
        href = ("%s#.browser-prompting/%s.html" % (up, slug)) if depth == 0 else ("%s.html" % slug)
        cur = " current" if slug == current_slug else ""
        rows.append('<a class="navlink%s" href="%s"><span class="dot %s"></span>%s</a>'
                    % (cur, href.replace("#", "%23"), dot, label))
    house = "%s#.house-user-guide.html/0.user-guide-index=%s.html" % (up, "%23")
    return """<aside class="sidebar">
  <div class="brand">TEARIT-HQ<strong>Browser Prompting Guide</strong></div>
  <nav>
    <div>
      <div class="navgroup-label">Guide</div>
      <div class="navlinks">
%s
      </div>
    </div>
    <div>
      <div class="navgroup-label">Elsewhere</div>
      <div class="navlinks">
        <a class="navlink" href="%s">← Muchiverse House — User Guide</a>
      </div>
    </div>
  </nav>
</aside>""" % ("\n".join("        " + r for r in rows), house)

def page_shell(title, depth, current_slug, body):
    return """<title>%s</title>

%s

<div class="shell">
%s
  <main><div class="col">
%s
    <footer class="pagefoot">
      Snapshot generated %s from <code>!.HQ-IQ-BOOK/08-roadmap/browser-prompting/*.md</code>
      by <code>build-html.py</code>. Not a live mirror — re-run the script after editing the
      markdown source.
    </footer>
  </div></main>
</div>
""" % (esc(title), STYLE, sidebar(depth, current_slug), body, BUILT)

def build_index():
    md = open(os.path.join(HERE, "0.START-HERE.md"), encoding="utf-8").read()
    cards = ['<a class="card" href="#.browser-prompting/%s.html">'
             '<div class="card-title">%s</div><p class="card-desc">%s</p></a>'
             % (slug.replace("#", "%23"), esc(label), esc(CARD_DESC[slug]))
             for slug, label, _dot, _ in PAGES]
    body = ('<div class="hero-eyebrow">Delegation map — not a research report</div>\n'
            '<h1 class="pagetitle">Browser Prompting Guide</h1>\n'
            '<p class="lede">A managerial delegation map for handing scoped work to a '
            'token-limited browser-window AI (or Grok, or a small local model): what to attach, '
            'what to say, and what has already been verified live — so house context is never '
            're-derived from scratch per task.</p>\n'
            '<div class="cardgrid">\n%s\n</div>\n'
            '<section class="doc">\n%s\n</section>'
            % ("\n".join(cards), render_markdown(md)))
    path = os.path.join(OUT_ROOT, "0.browser-p-index=#.html")
    open(path, "w", encoding="utf-8").write(page_shell("Browser Prompting Guide", 0, None, body))
    return path

def build_topic(slug, label, dot, srcs):
    secs = []
    for rel in srcs:
        raw = open(os.path.join(HERE, rel), encoding="utf-8").read()
        if rel.endswith(".txt"):
            inner = "<pre><code>%s</code></pre>" % esc(raw.strip("\n"))
        else:
            inner = render_markdown(raw)
        secs.append('<section class="doc">\n<div class="section-path">%s</div>\n%s\n</section>'
                    % (esc("browser-prompting/" + rel), inner))
    body = ('<div class="hero-eyebrow">Browser Prompting Guide</div>\n'
            '<h1 class="pagetitle">%s</h1>\n%s' % (esc(label), "\n".join(secs)))
    path = os.path.join(OUT_SUB, "%s.html" % slug)
    open(path, "w", encoding="utf-8").write(page_shell("%s — Browser Prompting" % label, 1, slug, body))
    return path

def main():
    os.makedirs(OUT_SUB, exist_ok=True)
    made = [build_index()]
    for slug, label, dot, srcs in PAGES:
        made.append(build_topic(slug, label, dot, srcs))
    for p in made:
        print("wrote", os.path.relpath(p, CALDOX))

if __name__ == "__main__":
    sys.exit(main())
