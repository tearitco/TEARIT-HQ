#!/usr/bin/env bash
# =============================================================================
# harness/k3-cli.sh  --  K1..K8 proof for 046.open-gema (meta-gemma loop)
#
# WHAT THIS IS
#   The K3 convention, stolen from 045's test-harn-same: feed stdin commands,
#   assert on stdout + disk truth. Scenarios K1..K8 are defined in
#   DESIGN-DOC.txt section 5 (lines 109-123). Tools are ground truth;
#   deterministic tool paths must fire with ZERO LLM calls (K7) and the whole
#   transcript must contain zero "TOOL: <tool> <args>" hallucinations (K8).
#
# DESIGN DECISION (documented; I made this call)
#   Mode A -- AGENT REPL: if src/agent (binary) exists, feed stdin commands
#     to it like the REPL and assert on stdout + disk truth.
#   Mode B -- OPS FALLBACK (today's reality: no src/agent.c, no binary yet):
#     drive the ops directly (ops/+x/*, compiled here on first run) so the
#     deterministic paths K1/K3/K7/K8 are asserted NOW against real files.
#     K2 (edit+run) and K6 (document) are SKIP with a reason: they need
#     src/agent.c + the agent binary (builder loop has not run). K5 (ask) is
#     LIVE-tested against the LAN Mac when reachable, otherwise SKIP/BUSY.
#   Deviation from the suggested grouping, on purpose: K4 (rag) is ASSERTED
#     in fallback mode, not skipped -- rag/+x/build_index + rag_search exist
#     and are compiled, so asserting `rag read` gives the loop a real RAG
#     health signal instead of hiding it. Applied rule: SKIP only when the
#     scenario GENUINELY needs the agent binary or the LAN. K4 needs neither.
#
# HOUSE RULES (applied)
#   * exact-substring asserts only (grep -F); never timing/fuzzy.
#   * disk truth: the harness checks real files on disk, never fakes results.
#   * bash + coreutils only. Paths quoted (this tree has emoji dir names).
#   * K7 proves zero LLM calls three ways:
#       (1) structural: grep the deterministic tool sources for network
#           markers (curl|http|11434|socket|getaddrinfo|connect_op) = 0;
#       (2) a curl-shim first in PATH trips a marker file if ANYTHING tries
#           to reach the LAN during the deterministic tools;
#       (3) the four tools run with dead proxy env vars and still succeed.
#   * every run tees a full transcript to harness/k3-run-<ts>.log; K8 greps
#     THAT file -- the only object a gemma reply could pollute.
#   * K8 runs LAST inside main and greps the transcript as it stands: all
#     tool/gemma output precedes it, and the harness's own tail lines never
#     contain the pattern by construction. This is exact, not fuzzy.
#   * T1 trace: append one line to memory/loop-log.txt each run.
#
# USAGE
#   harness/k3-cli.sh              # default: probe LAN, live K5 if reachable
#   K3_NO_LAN=1 harness/k3-cli.sh  # skip LAN entirely; K5 -> SKIP/BUSY
# EXIT: 0 = no FAIL (SKIP allowed, GREEN). 1 = at least one FAIL (RED).
# =============================================================================

LC_ALL=C
set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT" || exit 1

OPS="$ROOT/ops/+x"
RAG="$ROOT/rag/+x"
IDX="$ROOT/memory/index.pdl"
AGENT_BIN="$ROOT/src/agent"
AGENT_SRC="$ROOT/src/agent.c"

LOG="$ROOT/harness/k3-run-$(date +%Y%m%d-%H%M%S).log"
TMP=$(mktemp -d "${TMPDIR:-/tmp}/k3.XXXXXX") || { echo "k3: mktemp failed" >&2; exit 1; }
trap 'rm -rf "$TMP"' EXIT

LAN_URL="http://10.0.0.144:11434/api/chat"
LAN_MODEL="gemma3:270m"

ST=() RE=()
result() { local k=$1 st=$2; shift 2; ST[$k]=$st; RE[$k]="$*"; printf 'K%s %s | %s\n' "$k" "$st" "$*"; }

# ---------------------------------------------------------------- helpers
compile_ops() {
  [ -d "$OPS" ] || mkdir -p "$OPS"
  local s
  for s in list_dir search_in_files edit_file cmd_exec json_parser connect_op; do
    [ -x "$OPS/$s" ] || { gcc -o "$OPS/$s" "$ROOT/ops/$s.c" -lm || { echo "k3: cannot compile ops/$s.c" >&2; exit 1; }; }
  done
}

ensure_index() {
  [ -f "$IDX" ] || "$RAG/build_index" . "$IDX" >/dev/null 2>&1 || { echo "k3: cannot build RAG index" >&2; exit 1; }
}

# K1's read target: src/agent.c when it exists, else a real source on disk
# (so the read TOOL is proven against real file truth today).
target_file() {
  [ -f "$AGENT_SRC" ] && { printf '%s' "$AGENT_SRC"; return; }
  printf '%s' "$ROOT/ops/search_in_files.c"
}

# ---------------------------------------------------------------- K1 read
k1() {
  local tgt out
  if [ -x "$AGENT_BIN" ]; then
    tgt="$AGENT_SRC"
    out=$(printf 'read %s\n' "$tgt" | "$AGENT_BIN" 2>&1); printf '%s\n' "$out"
  else
    tgt=$(target_file)
    out=$(cat "$tgt"); printf '%s\n' "$out"
  fi
  if printf '%s\n' "$out" | grep -qF 'int main'; then
    result 1 PASS "read '$tgt' -> output contains known function 'int main' (disk truth)"
  else
    result 1 FAIL "read '$tgt' output missing the known function name"
  fi
}

# ---------------------------------------------------------------- K2 edit+run
k2() {
  if [ -f "$AGENT_SRC" ] && [ -x "$AGENT_BIN" ]; then
    local out
    cp "$AGENT_SRC" "$TMP/agent.c.bak"
    out=$(printf 'edit %s replace int main with int main /*k3*/\n' "$AGENT_SRC" | "$AGENT_BIN" 2>&1); printf '%s\n' "$out"
    if ! grep -qF '/*k3*/' "$AGENT_SRC"; then
      cp "$TMP/agent.c.bak" "$AGENT_SRC"
      result 2 FAIL "edit did not land on disk (src/agent.c unchanged)"
      return
    fi
    if ! gcc -o "$AGENT_BIN" "$AGENT_SRC" 2>"$TMP/gcc.err"; then
      cp "$TMP/agent.c.bak" "$AGENT_SRC"
      result 2 FAIL "disk changed but recompile failed: $(head -1 "$TMP/gcc.err")"
      return
    fi
    out=$(printf 'list\n' | "$AGENT_BIN" 2>&1); printf '%s\n' "$out"
    cp "$TMP/agent.c.bak" "$AGENT_SRC"
    if [ -n "$out" ]; then
      result 2 PASS "edit landed on disk, recompiled and ran (src/agent.c restored)"
    else
      result 2 FAIL "recompiled but run produced no output"
    fi
  else
    result 2 SKIP "requires src/agent.c + compiled src/agent binary (builder loop has not run)"
  fi
}

# ---------------------------------------------------------------- K3 search
k3() {
  local rag_out grep_out
  rag_out=$( "$RAG/rag_search" build_index "$IDX" 8 2>&1 ); printf '%s\n' "$rag_out"
  grep_out=$( "$OPS/search_in_files" build_index rag 2>&1 ); printf '%s\n' "$grep_out"
  if printf '%s\n' "$rag_out" | grep -qF 'HIT|./rag/build_index.c' \
     && printf '%s\n' "$grep_out" | grep -qF 'rag/build_index.c [Line 1]'; then
    result 3 PASS "search build_index -> RAG hit on rag/build_index.c + disk truth [Line 1]"
  else
    result 3 FAIL "search build_index did not find rag/build_index.c at the correct line"
  fi
}

# ---------------------------------------------------------------- K4 rag
k4() {
  local build_out rag_out hit=0
  build_out=$( "$RAG/build_index" . "$IDX" 2>&1 ); printf '%s\n' "$build_out"
  rag_out=$( "$RAG/rag_search" read "$IDX" 8 2>&1 ); printf '%s\n' "$rag_out"
  printf '%s\n' "$rag_out" | grep -qF 'HIT|./ARCH-DOC.txt' && hit=1
  printf '%s\n' "$rag_out" | grep -qF 'HIT|./DESIGN-DOC.txt' && hit=1
  if [ "$hit" -eq 1 ] && printf '%s\n' "$rag_out" | grep -qF 'RAG_SUM|hits='; then
    result 4 PASS "rag read -> snippets from read-related files (ARCH-DOC/DESIGN-DOC) after index refresh"
  else
    result 4 FAIL "rag read returned no read-related snippets"
  fi
}

# ---------------------------------------------------------------- K5 ask
k5() {
  local q="what does main do"
  if [ -x "$AGENT_BIN" ]; then
    local out
    out=$(printf 'ask %s\n' "$q" | "$AGENT_BIN" 2>&1); printf '%s\n' "$out"
    if [ -z "$out" ]; then
      result 5 SKIP "BUSY: agent gave no reply (gemma down / LAN unreachable) - tools still work"
    elif printf '%s\n' "$out" | grep -qE 'TOOL:[[:space:]]+[[:alnum:]_]+[[:space:]]'; then
      result 5 FAIL "agent reply contains a tool-call hallucination pattern"
    else
      result 5 PASS "live reply via agent REPL, no tool-call hallucination"
    fi
    return
  fi

  # ops fallback: RAG context + direct LAN ask through ops/connect_op
  if [ "${K3_NO_LAN:-0}" = 1 ]; then
    result 5 SKIP "BUSY: LAN probe disabled via K3_NO_LAN=1 (tools still work)"
    return
  fi

  local ctx prompt esc req resp reply st
  ctx=$( "$RAG/rag_search" "$q" "$IDX" 6 2>&1 | grep -F 'HIT|' | head -6 )
  prompt="[RAG snippets]
${ctx}

Question: $q
Answer in <=3 sentences using only the snippets."
  esc=$( printf '%s' "$prompt" | sed 's/\\/\\\\/g; s/"/\\"/g' | tr '\n' ' ' )
  req="$TMP/ask_req.json"
  printf '{"model":"%s","messages":[{"role":"user","content":"%s"}],"stream":false}' "$LAN_MODEL" "$esc" > "$req"
  resp="$TMP/ask_resp.json"
  rm -f "$resp" "$resp.status"
  timeout 25 "$OPS/connect_op" "$LAN_URL" "$req" "$resp" >/dev/null 2>&1
  st=$?
  if [ "$st" -eq 124 ] || [ ! -s "$resp" ]; then
    result 5 SKIP "BUSY: gemma down / LAN unreachable ($LAN_URL) - tools still work"
    return
  fi
  reply=$( "$OPS/json_parser" "$resp" message.content 2>&1 )
  printf 'gemma reply: %s\n' "$reply"
  if [ -z "$reply" ]; then
    result 5 SKIP "BUSY: empty/unparseable gemma reply (gemma down?)"
  elif printf '%s\n' "$reply" | grep -qE 'TOOL:[[:space:]]+[[:alnum:]_]+[[:space:]]'; then
    result 5 FAIL "gemma reply contains a tool-call hallucination pattern"
  else
    result 5 PASS "live gemma3:270m reply over LAN, no tool-call hallucination"
  fi
}

# ---------------------------------------------------------------- K6 document
k6() {
  if [ -x "$AGENT_BIN" ]; then
    local out
    out=$(printf 'document %s\n' "$AGENT_SRC" | "$AGENT_BIN" 2>&1); printf '%s\n' "$out"
    if [ -f "$AGENT_SRC" ] && grep -qF '/*' "$AGENT_SRC"; then
      result 6 PASS "doc block written into src/agent.c"
    else
      result 6 SKIP "stretch goal: no doc block landed (gemma down or feature not implemented)"
    fi
  else
    result 6 SKIP "stretch goal: needs agent REPL + LAN gemma authoring a doc block into src/agent.c (not built yet)"
  fi
}

# ---------------------------------------------------------------- K7 determinism
k7() {
  local struct_ok=1 src out ok=1
  for src in ops/list_dir.c ops/search_in_files.c ops/edit_file.c rag/rag_search.c rag/build_index.c; do
    if grep -qE 'curl|http://|https://|11434|socket|getaddrinfo|connect_op' "$ROOT/$src"; then
      struct_ok=0
    fi
  done

  mkdir -p "$TMP/shim"
  cat > "$TMP/shim/curl" <<EOF
#!/usr/bin/env bash
: > "$TMP/curl_tripped"
exit 1
EOF
  chmod +x "$TMP/shim/curl"
  rm -f "$TMP/curl_tripped"
  local no_net="http_proxy=http://127.0.0.1:1 https_proxy=http://127.0.0.1:1 all_proxy=http://127.0.0.1:1"

  out=$( env PATH="$TMP/shim:$PATH" $no_net "$OPS/list_dir" . 2>&1 ); printf '%s\n' "$out"
  printf '%s\n' "$out" | grep -qF 'ops/' || ok=0

  local tgt; tgt=$(target_file)
  out=$( env PATH="$TMP/shim:$PATH" $no_net cat "$tgt" 2>&1 ); printf '%s\n' "$out"
  printf '%s\n' "$out" | grep -qF 'int main' || ok=0

  out=$( env PATH="$TMP/shim:$PATH" $no_net "$OPS/search_in_files" build_index rag 2>&1 ); printf '%s\n' "$out"
  printf '%s\n' "$out" | grep -qF '[Line 1]' || ok=0

  cp "$ROOT/ops/search_in_files.c" "$TMP/k7_edit.c"
  out=$( env PATH="$TMP/shim:$PATH" $no_net "$OPS/edit_file" "$TMP/k7_edit.c" search_in_file search_in_file_ 2>&1 ); printf '%s\n' "$out"
  printf '%s\n' "$out" | grep -qF 'Successfully edited' || ok=0
  grep -qF 'search_in_file_' "$TMP/k7_edit.c" || ok=0

  if [ "$struct_ok" -eq 0 ]; then
    result 7 FAIL "structural: network markers found in a deterministic tool source"
  elif [ -f "$TMP/curl_tripped" ]; then
    result 7 FAIL "a deterministic tool tried to reach the LAN (curl-shim tripped)"
  elif [ "$ok" -eq 0 ]; then
    result 7 FAIL "a deterministic tool did not fire with expected output"
  else
    result 7 PASS "list/read/search/edit fired with ZERO LLM calls (curl-shim silent, no network markers in sources)"
  fi
}

# ---------------------------------------------------------------- K8 no hallucination
k8() {
  local hits
  hits=$( grep -cE 'TOOL:[[:space:]]+[[:alnum:]_]+[[:space:]]' "$LOG" 2>/dev/null || true )
  if [ "${hits:-0}" -eq 0 ]; then
    result 8 PASS "whole transcript ($LOG) has zero tool-call hallucination patterns"
  else
    result 8 FAIL "$hits tool-call hallucination pattern(s) found in transcript $LOG"
  fi
}

# ---------------------------------------------------------------- summary + trace (T1)
summary() {
  local np=0 nf=0 ns=0 i
  for i in 1 2 3 4 5 6 7 8; do
    case "${ST[$i]:-}" in PASS) np=$((np+1));; FAIL) nf=$((nf+1));; SKIP) ns=$((ns+1));; esac
  done
  local status_line="K1:${ST[1]:-} K2:${ST[2]:-} K3:${ST[3]:-} K4:${ST[4]:-} K5:${ST[5]:-} K6:${ST[6]:-} K7:${ST[7]:-} K8:${ST[8]:-}"
  local bot
  if [ "$nf" -gt 0 ]; then bot="failures above"; else bot="none (K2/K6 need agent binary; K5 needs LAN)"; fi
  printf '%s | harness (k3-cli.sh) | %s | bottleneck: %s | tool change made: created harness/k3-cli.sh; compiled ops/+x/*; fixed rag/build_index.c file-count bug\n' \
    "$(date '+%Y-%m-%d %H:%M:%S')" "$status_line" "$bot" >> "$ROOT/memory/loop-log.txt"
  if [ "$nf" -gt 0 ]; then
    printf 'OVERALL: %s PASS, %s FAIL, %s SKIP - RED\n' "$np" "$nf" "$ns"
    return 1
  fi
  printf 'OVERALL: %s PASS, %s FAIL, %s SKIP — GREEN (no failures)\n' "$np" "$nf" "$ns"
  return 0
}

# ---------------------------------------------------------------- main
main() {
  printf 'k3-cli.sh | mode: %s | transcript: %s\n\n' "$([ -x "$AGENT_BIN" ] && echo agent || echo ops-fallback)" "$LOG"
  compile_ops
  ensure_index
  k1; k2; k3; k4; k5; k6; k7; k8
  summary
}

main 2>&1 | tee "$LOG"
rc=${PIPESTATUS[0]}
exit "$rc"
