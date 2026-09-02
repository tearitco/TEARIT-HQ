#!/bin/bash
# events_hq_task2b_party_actor_commands_test_harness.sh — Party/Actor
# commands (Task "2b"/phase (b)): real, text-verifiable end-to-end proof
# for the ~19 Change-Gold-shaped commands.
#
# Proves, for every non-gold Party/Actor command, the FULL real chain:
#   IR node (event.ir.pdl) -> khtpm_events_hq_manager compile_page()
#   -> event.pal bytecode (+ cmd_N.sh wrappers for the exec-shaped
#   string ops) -> prisc+x executes -> REAL state files (items.txt /
#   weapons.txt / armors.txt / party.txt / actor_<id>_stats.txt /
#   actor_params.txt / actor_states.txt / actor_skills.txt /
#   actor_equipment.txt / history.txt) -> text-state-dump verification.
#
# Covered commands (18): change_items, change_weapons, change_armors,
# change_party_member, change_hp, change_mp, change_tp, change_exp,
# change_level, change_parameter, recover_all, change_state,
# change_skill, change_equipment, change_class, change_name,
# change_nickname, change_profile. change_gold itself was already proven
# by the Task 1 harness — this file covers the rest of the ~19.
#
# Two emission shapes intentionally mixed in one page:
#   - the 15 numeric commands compile via the registry's PAL lines into
#     INLINE prisc+x bytecode (ri 15=6/7 GET/SET_KV_INT + addi) — zero
#     exec, zero C ops (the zero-recompile "Shape 1" design);
#   - the 3 string commands (Name/Nickname/Profile) compile via TEMPLATE
#     exec wrappers calling the real mr_actor_string.+x op ("Shape 2" —
#     prisc's kv ecalls are integer-only, so a small events-hq op is the
#     honest way to set a STRING; no shared-engine/renderer edits).
#
# Why a sandbox under the house root: same convention as the Task 1
# harness — cmd_N.sh wrappers walk up for `xyzfs` to resolve $D, so the
# sandbox entity lives UNDER the real house root (pals/cursword/
# harnesses/.task2b_sandbox/<ts>). Non-blocking only (no popups -> no
# clock coupling, no auto-picker needed) — matches the design note that
# these commands never wait on the player.
#
# No visuals. No renderer. No PNG. Text-state-dump only.

set -u

HERE="$(cd "$(dirname "$0")" && pwd)"

find_house_root() {
  local d="$HERE"
  while [ "$d" != "/" ]; do
    case "$(basename "$d")" in
      44.xyz*) echo "$d"; return 0 ;;
    esac
    d="$(dirname "$d")"
  done
  echo "task2b_harness: could not find house root above $HERE" >&2
  exit 1
}

HOUSE="${HOUSE:-$(find_house_root)}"
PAL="$HOUSE/xyzfs/users/0a9558a7-7c74-4358-833c-2d5b21edc421/home/livedesk/pals/cursword"
EVHQQPS="$HOUSE/&.widgits/events-hq/ops"
MGR="$EVHQQPS/+x/khtpm_events_hq_manager.+x"
ASTR="$EVHQQPS/+x/mr_actor_string.+x"
PRISC="$(ls -d "$HOUSE"/101.mutaclsym*+18.0G/system/prisc+x 2>/dev/null | head -1)"
if [ -z "$PRISC" ] || [ ! -x "$PRISC" ]; then
  PRISC="$(ls -d "$HOUSE"/101.mutaclsym*+*/system/prisc+x 2>/dev/null | head -1)"
fi
PRISC_DIR="$(dirname "$PRISC")"
# prisc+x resolves its ops file (default_op.txt) against the CWD FIRST —
# the game's ops file lives at the mutaclsym ROOT, so run from there.
PRISC_CWD="$(dirname "$PRISC_DIR")"

TS=$(date '+%Y%m%d-%H%M%S')
SAND_ROOT="$PAL/harnesses/.task2b_sandbox"
SAND="$SAND_ROOT/$TS"
EVENT_PKG="$SAND/event_pkg"
IR="$EVENT_PKG/pages/page_1/event.ir.pdl"
PAL_FILE="$EVENT_PKG/pages/page_1/event.pal"
MGR_ACTION="$EVENT_PKG/.hq_manager/action.txt"

RESULTS="$PAL/presentations/events-hq-task2b-party-actor-commands-$TS"
mkdir -p "$RESULTS"
SUMMARY="$RESULTS/summary.txt"

log() { echo "[$(date '+%H:%M:%S')] $*" | tee -a "$RESULTS/log.txt"; }
pass() { log "PASS: $*"; echo "PASS: $*" >> "$SUMMARY"; }
fail() { log "FAIL: $*"; echo "FAIL: $*" >> "$SUMMARY"; }

cleanup() {
  for pat in "khtpm_events_hq_manager\.\+x" "task2b_picker" "prisc\+x"; do
    pids="$(pgrep -f "$pat" 2>/dev/null || true)"
    [ -n "$pids" ] && echo "$pids" | xargs -r kill -TERM 2>/dev/null
  done
  sleep 0.5
  [ -d "$SAND" ] && rm -rf "$SAND"
  echo "cleanup done" >> "$RESULTS/log.txt"
}
trap cleanup EXIT

send_action() {
  echo "$1" > "$MGR_ACTION"
  log "action sent: $1"
  sleep 1.6
}

# =========================================================================
log "=== step 0: preflight ==="
if [ -z "$PRISC" ] || [ ! -x "$PRISC" ]; then fail "prisc+x not found"; exit 1; fi
if [ ! -x "$MGR" ]; then fail "manager binary not found: $MGR"; exit 1; fi
if [ ! -x "$ASTR" ]; then fail "mr_actor_string.+x not built: $ASTR"; exit 1; fi
log "prisc=$PRISC"

log "=== clean stray processes ==="
for pat in "khtpm_events_hq_manager\.\+x" "prisc\+x"; do
  existing="$(pgrep -f "$pat" 2>/dev/null || true)"
  [ -n "$existing" ] && echo "$existing" | xargs -r kill -KILL 2>/dev/null
done
sleep 0.5

# =========================================================================
log "=== step 1: sandbox package + IR with all 18 Party/Actor commands ==="
mkdir -p "$EVENT_PKG/pages/page_1"

# Pre-seeded real actor state so the delta commands have a starting point
# (GET_KV_INT defaults to x13=0 when a key is missing, so only real
# deltas end up being exercised here).
cat > "$SAND/actor_1_stats.txt" <<'EOF'
hp=20
mp=5
tp=8
EOF

# Pre-seed an item so Change Items / Change Weapons / Change Armors apply
# a REAL delta on an existing key too (items/weapons/armors start empty
# otherwise — both are valid; the seeded one proves addi-on-existing).
cat > "$SAND/items.txt" <<'EOF'
item_1=1
EOF

cat > "$IR" <<'EOF'
SECTION      | KEY                | VALUE
----------------------------------------
META         | piece_id           | task2b-sandbox
STATE        | source             | events-hq
NODE         | id=1 type=change_items | item_id=1|amount=5
NODE         | id=2 type=change_weapons | weapon_id=2|amount=3
NODE         | id=3 type=change_armors | armor_id=3|amount=7
NODE         | id=4 type=change_party_member | actor_id=1|in_party=1
NODE         | id=5 type=change_hp | actor_id=1|amount=-10
NODE         | id=6 type=change_mp | actor_id=1|amount=25
NODE         | id=7 type=change_tp | actor_id=1|amount=-5
NODE         | id=8 type=change_exp | actor_id=1|amount=120
NODE         | id=9 type=change_level | actor_id=1|amount=2
NODE         | id=10 type=change_parameter | actor_param=1:2|amount=5
NODE         | id=11 type=recover_all | actor_id=1
NODE         | id=12 type=change_state | actor_state=1:4|add_remove=1
NODE         | id=13 type=change_skill | actor_skill=1:3|learn_forget=1
NODE         | id=14 type=change_equipment | actor_slot=1:0|item_id=8
NODE         | id=15 type=change_class | actor_id=1|class_id=2
NODE         | id=16 type=change_name | actor_id=1|name=Officer Helena
NODE         | id=17 type=change_nickname | actor_id=1|nickname=Hel
NODE         | id=18 type=change_profile | actor_id=1|profile=Guardian of the North
EOF

# =========================================================================
log "=== step 2: launch events-hq manager (standalone, real house root) ==="
mkdir -p "$EVENT_PKG/.hq_manager"
setsid nohup "$MGR" "$HOUSE" "$EVENT_PKG" "task2b-sandbox" >"$RESULTS/mgr.log" 2>&1 </dev/null &
disown 2>/dev/null || true
sleep 2

# The manager only compiles on a REAL action (IR is pre-written with all
# 18 nodes, so an identity edit: rewrites node 1 identically yet forces a
# full recompile of the whole page through the live action path).
send_action "edit:1|change_items|item_id=1|amount=5"
sleep 2

pids="$(pgrep -f 'khtpm_events_hq_manager\.\+x' 2>/dev/null | grep -c . || true)"
if [ "$pids" != "1" ]; then
  fail "expected exactly 1 manager process, got $pids"
  cat "$RESULTS/mgr.log" 2>/dev/null
  exit 1
fi
pass "manager launched"

# =========================================================================
log "=== step 3: compile proof ==="
if [ ! -f "$PAL_FILE" ]; then
  fail "event.pal not created"
  cat "$RESULTS/mgr.log" 2>/dev/null
  exit 1
else
  pass "event.pal compiled"
  cat "$PAL_FILE" >> "$RESULTS/log.txt"
fi

NODES=$(grep -c '^NODE' "$IR" 2>/dev/null || echo 0)
if [ "$NODES" = "18" ]; then
  pass "IR has 18 NODEs (15 numeric + 3 string Party/Actor commands)"
else
  fail "IR expected 18 NODEs, got $NODES"
fi

if grep -q 'li x15, 6' "$PAL_FILE" && grep -q 'li x15, 7' "$PAL_FILE" && grep -q 'addi x12, x12, -10' "$PAL_FILE"; then
  pass "event.pal has inline PAL kv bytecode (GET/SET_KV_INT + negative addi delta)"
else
  fail "event.pal missing inline PAL kv bytecode"
fi

if grep -q 'item_1' "$PAL_FILE" && grep -q 'param_1:2' "$PAL_FILE" && grep -q 'state_1:4' "$PAL_FILE" && grep -q 'equip_1:0' "$PAL_FILE"; then
  pass "event.pal has compound-id keys (param_/state_/equip_) and item_ key verbatim"
else
  fail "event.pal missing compound-id key literals"
fi

blocks=$(ls "$EVENT_PKG/pages/page_1"/cmd_*.sh 2>/dev/null | wc -l | tr -d ' ')
if [ "$blocks" = "3" ]; then
  pass "3 cmd_N.sh wrappers generated (change_name/nickname/profile string ops)"
else
  fail "expected 3 wrappers for the string ops, got $blocks"
fi

if [ "$(grep -l 'mr_actor_string' "$EVENT_PKG/pages/page_1"/cmd_*.sh 2>/dev/null | wc -l | tr -d ' ')" = "3" ]; then
  pass "all 3 string-command wrappers route to mr_actor_string.+x"
else
  fail "string wrapper routing to mr_actor_string incorrect"
fi

for f in "$EVENT_PKG/pages/page_1"/cmd_*.sh; do
  [ -f "$f" ] && { echo "### $f"; cat "$f"; } >> "$RESULTS/log.txt"
done
cp "$PAL_FILE" "$RESULTS/event.pal.txt"

# =========================================================================
log "=== step 4: runtime proof - run the compiled bytecode under prisc+x ==="
(
  cd "$PRISC_CWD" || exit 1
  "$PRISC" "$PAL_FILE" >> "$RESULTS/prisc.log" 2>&1
)
PRISC_RC=$?
copy_rc=0
if [ "$PRISC_RC" = "0" ]; then
  pass "prisc+x ran the compiled event to halt (exit 0)"
else
  fail "prisc+x exit $PRISC_RC (non-zero)"
  copy_rc=1
fi
cat "$RESULTS/prisc.log" >> "$RESULTS/log.txt"

# =========================================================================
log "=== step 5: text-state-dump verification ==="

# 5a: inventory-family files (items/weapons/armors/party)
if grep -q '^item_1=6$' "$SAND/items.txt" 2>/dev/null; then
  pass "change_items: item_1 1->6 (seeded delta)"
else
  fail "change_items: item_1 expected 6, got '$(cat "$SAND/items.txt" 2>/dev/null)'"
fi
if grep -q '^weapon_2=3$' "$SAND/weapons.txt" 2>/dev/null; then
  pass "change_weapons: weapon_2=3"
else
  fail "change_weapons missing weapon_2=3"
fi
if grep -q '^armor_3=7$' "$SAND/armors.txt" 2>/dev/null; then
  pass "change_armors: armor_3=7"
else
  fail "change_armors missing armor_3=7"
fi
if grep -q '^member_1=1$' "$SAND/party.txt" 2>/dev/null; then
  pass "change_party_member: member_1=1"
else
  fail "change_party_member missing member_1=1"
fi

# 5b: actor stats file — deltas then Recover All heal-to-max
STATF="$SAND/actor_1_stats.txt"
state="$(cat "$STATF" 2>/dev/null)"
for kv in "hp=9999" "mp=9999" "tp=9999" "exp=120" "level=2" "class_id=2" \
          "name=Officer Helena" "nickname=Hel" "profile=Guardian of the North"; do
  if grep -q "^$kv$" "$STATF" 2>/dev/null; then
    pass "actor_1_stats: $kv"
  else
    fail "actor_1_stats missing '$kv'; got: $state"
  fi
done

# 5c: compound-key files
for t in "actor_params.txt:param_1:2=5" "actor_states.txt:state_1:4=1" \
         "actor_skills.txt:skill_1:3=1" "actor_equipment.txt:equip_1:0=8"; do
  f="${t%%:*}"; kv="${t#*:}"
  if grep -q "^$kv$" "$SAND/$f" 2>/dev/null; then
    pass "$f: $kv"
  else
    fail "$f missing '$kv'; got: '$(cat "$SAND/$f" 2>/dev/null)'"
  fi
done

# 5d: history.txt audit (string ops log through the real mr_log path)
if grep -qF 'ACTOR actor=1 name=Officer Helena' "$SAND/history.txt" 2>/dev/null &&
   grep -qF 'ACTOR actor=1 nickname=Hel' "$SAND/history.txt" 2>/dev/null &&
   grep -qF 'ACTOR actor=1 profile=Guardian of the North' "$SAND/history.txt" 2>/dev/null; then
  pass "history.txt: ACTOR string command audits present"
else
  fail "history.txt missing ACTOR audits: '$(cat "$SAND/history.txt" 2>/dev/null)'"
fi

# 5e: state dumps + presentation copies
cp "$SAND/items.txt"        "$RESULTS/01_items.txt"        2>/dev/null
cp "$SAND/weapons.txt"      "$RESULTS/02_weapons.txt"      2>/dev/null
cp "$SAND/armors.txt"       "$RESULTS/03_armors.txt"       2>/dev/null
cp "$SAND/party.txt"        "$RESULTS/04_party.txt"        2>/dev/null
cp "$SAND/actor_1_stats.txt" "$RESULTS/05_actor_1_stats.txt" 2>/dev/null
cp "$SAND/actor_params.txt" "$RESULTS/06_actor_params.txt"  2>/dev/null
cp "$SAND/actor_states.txt" "$RESULTS/07_actor_states.txt"  2>/dev/null
cp "$SAND/actor_skills.txt" "$RESULTS/08_actor_skills.txt"  2>/dev/null
cp "$SAND/actor_equipment.txt" "$RESULTS/09_actor_equipment.txt" 2>/dev/null
cp "$SAND/history.txt"      "$RESULTS/10_history.txt"      2>/dev/null
log "state dumps copied into $RESULTS"

echo "" >> "$SUMMARY"
echo "harness done TS=$TS - see $RESULTS" >> "$SUMMARY"
echo "$SUMMARY"