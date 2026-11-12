#!/bin/bash
# nb_raw_text_test.sh - raw-text/RCDATA elements keep their source text.
#
# The parser used to skip the BODY of script/style/title/noscript entirely
# (it advanced the cursor past the closing tag and stored nothing). The
# element survived, so `getElementById('bundle')` still worked, but its text
# did not: `document.getElementById('__NEXT_DATA__').textContent` was empty,
# `<style>` text was unreachable from a page, and `<title>` text existed only
# in the manager's separate title extraction. innerHTML/DOMParser inherited
# the same hole.
#
# This gate pins the browser shape on BOTH parse paths:
#   - folded (nb_parse_html, the manager's fetch.dom Input): the content is
#     stored on the element's own `text` field, exactly like folded prose;
#   - tree (nb_parse_html_nodes, innerHTML/insertAdjacentHTML/DOMParser): the
#     content is a real #text child (nodeType 3) and nothing inside is ever
#     parsed as markup;
#   - title/textarea are RCDATA, so character references decode; script/
#     style/noscript are raw text, so `&amp;` stays literal;
#   - the wire serializer carries the text through fetch.dom;
#   - RENDER never turns script/style/noscript data into visible text rows.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
PKG="$(cd "$HERE/.." && pwd)"
WORKER="$PKG/ops/+x/nb_js_worker.+x"
FAIL=0

command -v python3 >/dev/null || { echo "FAIL: python3 missing"; exit 1; }
command -v cc >/dev/null || { echo "FAIL: cc missing"; exit 1; }
[ -x "$WORKER" ] || { echo "FAIL: worker binary missing at $WORKER"; exit 1; }
[ -f "$HERE/nb_probe.py" ] || { echo "FAIL: harness missing at $HERE/nb_probe.py"; exit 1; }

WORK="$(mktemp -d /tmp/nb_raw_text.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT INT TERM

# --- 1. folded path: nb_parse_html stores the content on the element -------
cat > "$WORK/folded_test.c" <<'C'
#include "nb_dom.h"
#include <stdio.h>
#include <string.h>

static int pass, total;
static void ck(const char *name, const char *got, const char *want) {
    total++;
    if (got && want && strcmp(got, want) == 0) { pass++; return; }
    printf("FAIL %s got=[%s] want=[%s]\n", name, got ? got : "(null)", want ? want : "(null)");
}
static void cki(const char *name, long got, long want) {
    total++;
    if (got == want) { pass++; return; }
    printf("FAIL %s got=%ld want=%ld\n", name, got, want);
}
static NbNode *find(NbNode *n, const char *tag) {
    for (; n; n = n->next_sibling) {
        if (n->tag && strcmp(n->tag, tag) == 0) return n;
        NbNode *r = find(n->first_child, tag);
        if (r) return r;
    }
    return NULL;
}
int main(void) {
    const char *html =
        "<html><head><title>A &amp; B</title>"
        "<style>.x > .y { color: red }</style></head>"
        "<body><script>if (a < b && c > d) { go(); }</script>"
        "<noscript><p>fallback</p></noscript>"
        "<textarea>t &amp; <b>x</b></textarea></body></html>";
    NbNode *root = nb_parse_html(html, strlen(html));
    if (!root) { printf("FAIL folded parse returned NULL\n"); return 1; }

    NbNode *sc = find(root, "script");
    ck("folded-script-text", sc ? sc->text : NULL, "if (a < b && c > d) { go(); }");
    cki("folded-script-children", sc && sc->first_child ? 1 : 0, 0);

    NbNode *st = find(root, "style");
    ck("folded-style-text", st ? st->text : NULL, ".x > .y { color: red }");

    NbNode *ti = find(root, "title");
    ck("folded-title-entities", ti ? ti->text : NULL, "A & B");

    NbNode *no = find(root, "noscript");
    ck("folded-noscript-raw", no ? no->text : NULL, "<p>fallback</p>");
    cki("folded-noscript-no-markup", find(no ? no->first_child : NULL, "p") ? 1 : 0, 0);

    NbNode *ta = find(root, "textarea");
    ck("folded-textarea-entities", ta ? ta->text : NULL, "t & <b>x</b>");
    cki("folded-textarea-no-markup", find(ta ? ta->first_child : NULL, "b") ? 1 : 0, 0);

    /* fetch.dom must carry the data island text the page will read back. */
    char *buf = NULL; size_t blen = 0;
    FILE *mem = open_memstream(&buf, &blen);
    if (!mem) { printf("FAIL open_memstream\n"); return 1; }
    nb_serialize(mem, root);
    fclose(mem);
    FILE *in = fmemopen(buf, blen, "r");
    NbNode *back = in ? nb_dom_load(in) : NULL;
    if (in) fclose(in);
    NbNode *sc2 = back ? find(back, "script") : NULL;
    ck("wire-script-text", sc2 ? sc2->text : NULL, "if (a < b && c > d) { go(); }");

    /* --- tree path: nb_parse_html_nodes authors a real #text child ------ */
    NbNode *troot = nb_parse_html_nodes(html, strlen(html));
    if (!troot) { printf("FAIL tree parse returned NULL\n"); return 1; }
    NbNode *tsc = find(troot, "script");
    NbNode *tch = tsc ? tsc->first_child : NULL;
    cki("tree-script-children", tch ? 1 : 0, 1);
    ck("tree-script-text", tch ? tch->text : NULL, "if (a < b && c > d) { go(); }");
    cki("tree-script-text-is-textnode", tch && (!tch->tag || !tch->tag[0]) ? 1 : 0, 1);
    cki("tree-script-single-child", tch && !tch->next_sibling ? 1 : 0, 1);

    NbNode *tno = find(troot, "noscript");
    ck("tree-noscript-raw", tno && tno->first_child ? tno->first_child->text : NULL, "<p>fallback</p>");
    cki("tree-noscript-no-markup", find(tno ? tno->first_child : NULL, "p") ? 1 : 0, 0);

    printf("RAWCTEST %d/%d\n", pass, total);
    nb_node_free(root); nb_node_free(back); nb_node_free(troot);
    return pass == total ? 0 : 1;
}
C
if ! cc -O0 -g -I"$PKG" "$WORK/folded_test.c" "$PKG/nb_dom.c" -o "$WORK/folded_test" 2> "$WORK/cc.log"; then
    echo "FAIL: folded parser harness did not compile"
    sed -n '1,20p' "$WORK/cc.log"
    exit 1
fi
if ! "$WORK/folded_test" > "$WORK/folded.out" 2>&1; then
    echo "FAIL: folded parser path does not keep raw text"
    sed -n '1,30p' "$WORK/folded.out"
    FAIL=1
fi
FOLDED=$(grep -o 'RAWCTEST [0-9]*/[0-9]*' "$WORK/folded.out" | tail -1)
echo "folded: ${FOLDED:-none}"

# --- 2. tree/JS path over the real worker --------------------------------
printf '/*nbjs-script-boundary*/\nconsole.log("LOADOK");\n' > "$WORK/page.js"
python3 - "$WORK/dom.txt" <<'PY'
import sys
p = sys.argv[1]
script_text = "if (a < b && c > d) { go(); }"
def rec(tag, id_, cls, text, attrs):
    return "N|%s|%s|%s|%d|%s|%d|%s\n" % (tag, id_, cls, len(text), text, len(attrs), attrs)
dom  = rec('#document','','','', '') + 'D\n'
dom += rec('html','','','', 'html') + 'D\n'
dom += rec('head','','','', 'head') + 'D\n'
dom += rec('title','','','Title & Data','title') + 'U\n'
dom += rec('body','','','', 'body') + 'D\n'
dom += rec('p','t','','tick 0','p id="t"')
dom += rec('script','sc','',script_text,'script id="sc"')
dom += 'U\nU\nU\n'
open(p,'w').write(dom)
PY

cat > "$WORK/probe.js" <<'PROBE'
(function(){
  var pass=0,total=0;
  function check(n,g,w){ total++; if(String(g)===String(w)){pass++; console.log('TC_TESTOK '+n);} else {console.log('TC_TESTFAIL '+n+' got='+g+' want='+w);} }
  var RAW='if (a < b && c > d) { go(); }';

  /* --- LOAD: the page-facing DOM carries the data island --------------- */
  var sc=document.getElementById('sc');
  check('load-script-text', sc?sc.textContent:'missing', RAW);
  check('load-script-kids', sc?sc.childNodes.length:'missing', 0);
  var bodyText=document.body.textContent;
  check('load-body-includes-script', bodyText.indexOf(RAW)>=0, true);

  /* --- innerHTML: content is one real #text child, no markup ---------- */
  var d=document.createElement('div');
  d.innerHTML='<script>if (a < b && c > d) { q(); }</script>';
  var s1=d.firstChild;
  check('ih-script-text', s1?s1.textContent:'none', 'if (a < b && c > d) { q(); }');
  check('ih-script-kids', s1?s1.childNodes.length:'none', 1);
  check('ih-script-text-node', s1&&s1.firstChild?s1.firstChild.nodeType:'none', 3);
  check('ih-script-serialized', d.innerHTML, '<script>if (a < b && c > d) { q(); }</script>');

  d.innerHTML='<style>.x > .y { color: red }</style>';
  check('ih-style-text', d.firstChild?d.firstChild.textContent:'none', '.x > .y { color: red }');

  d.innerHTML='<title>a &amp; b</title>';
  check('ih-title-entities', d.firstChild?d.firstChild.textContent:'none', 'a & b');

  d.innerHTML='<textarea>a &amp; <b>x</b></textarea>';
  check('ih-textarea-entities', d.firstChild?d.firstChild.textContent:'none', 'a & <b>x</b>');
  check('ih-textarea-kids', d.firstChild?d.firstChild.childNodes.length:'none', 1);
  check('ih-textarea-no-markup', d.getElementsByTagName('b').length, 0);

  d.innerHTML='<noscript><p>x</p></noscript>';
  check('ih-noscript-raw', d.firstChild?d.firstChild.textContent:'none', '<p>x</p>');
  check('ih-noscript-no-markup', d.getElementsByTagName('p').length, 0);

  /* --- DOMParser uses the same tree path ------------------------------- */
  var pd=new DOMParser().parseFromString('<html><body><script id="s2">a < b && c</script><style>.p > .q {}</style></body></html>','text/html');
  var s2=pd.getElementById('s2');
  check('parsed-script-text', s2?s2.textContent:'missing', 'a < b && c');
  check('parsed-script-kids', s2?s2.childNodes.length:'missing', 1);
  check('parsed-style-text', pd.querySelector('style').textContent, '.p > .q {}');

  console.log('TC_TESTRESULT '+pass+'/'+total);
  return 'TC_TESTDONE';
})();
PROBE

python3 "$HERE/nb_probe.py" \
    --worker "$WORKER" --page "$WORK/page.js" --dom "$WORK/dom.txt" \
    --href https://example.invalid/ --title raw:fixture \
    --probe "$WORK/probe.js" --console "$WORK/console.txt" \
    --load-timeout 30 --eval-timeout 60 --drain-ms 2000 > "$WORK/out.json" 2> "$WORK/err.log" \
    || { echo "FAIL: harness could not complete the probe"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

grep -q '^=>TC_TESTDONE' "$WORK/console.txt" \
    || { echo "FAIL: the probe never completed"; sed -n '1,30p' "$WORK/err.log"; exit 1; }

TOTAL=$(grep -o 'TC_TESTRESULT [0-9]*/[0-9]*' "$WORK/console.txt" | tail -1 | sed 's|TC_TESTRESULT ||')
grep -q '^LOG|TC_TESTRESULT 17/17' "$WORK/console.txt" \
    || { echo "FAIL: not every probe assertion held (saw ${TOTAL:-none})"; FAIL=1; }

if grep -q '^LOG|TC_TESTFAIL' "$WORK/console.txt"; then
    echo "FAIL: raw-text elements do not keep their content in the tree model"
    grep '^LOG|TC_TESTFAIL' "$WORK/console.txt"
    FAIL=1
fi

# --- 3. RENDER never shows script/style/noscript data --------------------
python3 - "$WORK/out.json" "$WORK/console.txt" <<'PY' || FAIL=1
import json, sys
d = json.load(open(sys.argv[1]))
if d.get("errors"):
    print("FAIL: harness errors:", d["errors"]); raise SystemExit(1)
renders = [f for f in d.get("frames", []) if f.startswith("RENDER")]
if not renders:
    print("FAIL: no RENDER frame"); raise SystemExit(1)
rows = [l for l in renders[-1].split("\n")[1:] if l]
bad = []
for needle, why in [("go();", "script body"), ("color: red", "style body"),
                    ("<p>fallback</p>", "noscript body")]:
    if any(needle in r for r in rows): bad.append("RENDER leaked the %s" % why)
if "TEXT|tick 0" not in rows: bad.append("visible TEXT|tick 0 missing")
if not any(r.startswith("TITLE|") for r in rows): bad.append("TITLE row missing")
if bad:
    print("FAIL: post-JS rows are wrong")
    for b in bad: print("  " + b)
    print("  rows: " + " ; ".join(rows))
    raise SystemExit(1)
print("rows: visible prose rendered, raw-text data never leaked")
PY

if [ "$FAIL" -eq 0 ]; then
    echo "nb_raw_text_test.sh PASS (${FOLDED:-?} folded + ${TOTAL:-?} probe + rows)"
else
    echo "nb_raw_text_test.sh FAIL"
fi
exit $FAIL
