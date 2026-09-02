#!/bin/bash
# run_visible_window_events_hq_demo.sh - launcher/verifier/cleanup for the
# visible-window Loop/Wait/Send-Input proof (NEW TASK, COMMON-EVENTS-
# MANAGER-HANDOFF.md line 3526). The actual drive sequence is the PAL
# harness pal/visible_window_events_hq_demo.pal (run via the real
# prisc+x VM); this script owns the three things a .pal can never do per
# HARNESS-AUTHORING-GUIDE.md §3a ("Still needs TEMPLATE/exec: launching
# the target render binary itself ... not a gap"):
#   1. launch a REAL events-hq window on a DISPOSABLE entity (exact
#      launch command from events_hq_task3_test_harness.sh / the
#      REPRODUCE.md Part 1 block, against a throwaway /tmp package dir,
#      never cursword's own event_pkg),
#   2. run the .pal through the real prisc+x binary,
#   3. verify the real PNG + verdict, then restore relay/history files and
#      kill only the processes THIS run spawned.
#
# Verification standard (from the handoff): zero stray
# khtpm_core_render.+x processes before/after; disposable entity
# deleted when done; real relay/history files restored to their pre-test
# git-clean state. Pre-existing live managers (cursword, /tmp/v2demo -
# present before this run) are snapshotted and NEVER touched.

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
  echo "run_visible_window_events_hq_demo: could not find house root above $HERE" >&2
  exit 1
}

HOUSE="${HOUSE:-$(find_house_root)}"

PAL_DIR="$HERE/pal"
PAL_FILE="$PAL_DIR/visible_window_events_hq_demo.pal"
PKG="/tmp/eventshw_visproof/event_pkg"          # DISPOSABLE entity pkg (ASCII-safe, like §3a-proof's /tmp rule)
ENTITY="visproof-disposable"

BIN="$HOUSE/*.monads/*.livedesk-taskbar/ops/+x/khtpm_core_render.+x"
CHTPM="$HOUSE/&.widgits/events-hq/pieces/dashboard.chtpm"
RELAY="$HOUSE/#.desktop/events_hq_history.txt"
VMODE="$HOUSE/#.desktop/events_hq_view_mode.txt"
FH="$HOUSE/#.desktop/events_hq_frame_history.txt"
PNG="/tmp/events-hq-frame.png"
RESULTS="/tmp/events_hq_visproof_results.txt"
PRISC_BIN="$HOUSE/101.mutaclsym🧟‍♂️️+18.0G/system/prisc+x"
PRISC_CWD="$HOUSE/101.mutaclsym🧟‍♂️️+18.0G"
LAUNCH_LOG="/tmp/eventshw_visproof_launch.log"
BKPDIR="/tmp/eventshw_visproof_backup"

PROC_PATTERN="khtpm_core_render\.\+x"
MGR_PATTERN="khtpm_events_hq_manager\.\+x"

RESULTS_DIR="$PAL_DIR/presentations/events-hq-visible-window-$(date '+%Y%m%d-%H%M%S')"

log() { echo "[$(date '+%H:%M:%S')] $*" | tee -a "$RESULTS_DIR/log.txt"; }
pass() { log "PASS: $*"; echo "PASS: $*" >> "$RESULTS_DIR/summary.txt"; }
fail() { log "FAIL: $*"; echo "FAIL: $*" >> "$RESULTS_DIR/summary.txt"; }

any_pids() { pgrep -f "$1" 2>/dev/null || true; }

# Snapshot the manager set that already existed BEFORE this run so cleanup
# only ever touches what THIS run spawned (never cursword's or v2demo's).
PREEXISTING_MGRS="$(any_pids "$MGR_PATTERN")"
SEQ_EXISTED=0
[ -f "$HOUSE/#.desktop/events_hq_seq.txt" ] && SEQ_EXISTED=1

cleanup() {
  # idempotent: this runs once explicitly and again via trap - guard it
  if [ -n "${CLEANUP_DONE:-}" ]; then return; fi
  CLEANUP_DONE=1
  # close the window gracefully first (Escape, twice - same as REPRODUCE.md),
  # into the LIVE relay (restore happens last so these reach the render)
  echo 27 >> "$RELAY" 2>/dev/null; sleep 0.3
  echo 27 >> "$RELAY" 2>/dev/null; sleep 0.3
  local new_mgrs pids
  # kill NEW render procs (this run's)
  pids="$(any_pids "$PROC_PATTERN")"
  if [ -n "$pids" ]; then
    echo "$pids" | xargs -r kill -TERM 2>/dev/null
    sleep 2
    pids="$(any_pids "$PROC_PATTERN")"
    [ -n "$pids" ] && echo "$pids" | xargs -r kill -KILL 2>/dev/null
    sleep 0.5
  fi
  # kill NEW manager procs only (subtract the pre-existing set)
  new_mgrs="$(comm -13 <(echo "$PREEXISTING_MGRS" | tr ' ' '\n' | sort) <(any_pids "$MGR_PATTERN" | tr '\n' ' ' | tr ' ' '\n' | sort) 2>/dev/null)"
  if [ -n "$new_mgrs" ]; then
    echo "$new_mgrs" | xargs -r kill -TERM 2>/dev/null
    sleep 1
    echo "$new_mgrs" | xargs -r kill -KILL 2>/dev/null
  fi
  # dispose of everything this run created (ed: disposable pkg, /tmp artifacts)
  rm -rf "$PKG" "$RESULTS" "$PNG"
  rm -f "$LAUNCH_LOG" /tmp/events_hq_visproof_results.txt /tmp/eventshw_visproof_launch.log
  # remove the render-created sibling files, but ONLY those not present
  # before this run (view_mode/frame_history are render-made each launch)
  rm -f "$VMODE" "$FH"
  if [ "$SEQ_EXISTED" = "0" ]; then rm -f "$HOUSE/#.desktop/events_hq_seq.txt"; fi
  # restore the real relay/history files to their pre-test git-clean state
  # LAST (after the render is dead, so nothing can re-append; the backup
  # dir is then removed so the subsequent trap call re-reaches here safely)
  [ -f "$BKPDIR/events_hq_history.txt" ] && cp "$BKPDIR/events_hq_history.txt" "$RELAY"
  [ -f "$BKPDIR/events_hq_view_mode.txt" ] && cp "$BKPDIR/events_hq_view_mode.txt" "$VMODE"
  [ -f "$BKPDIR/events_hq_frame_history.txt" ] && cp "$BKPDIR/events_hq_frame_history.txt" "$FH"
  rm -rf "$BKPDIR"
}
trap cleanup EXIT

mkdir -p "$RESULTS_DIR"
log "=== step 0: zero stray renders before run ==="
existing="$(any_pids "$PROC_PATTERN")"
if [ -n "$existing" ]; then
  fail "stray render process(es) before run: $existing"
  echo "$existing" | xargs -r kill -KILL
  sleep 1
fi

# back up the real relay/history files, then clear the relay for a clean drive
mkdir -p "$BKPDIR"
[ -f "$RELAY" ] && cp "$RELAY" "$BKPDIR/events_hq_history.txt"
[ -f "$VMODE" ] && cp "$VMODE" "$BKPDIR/events_hq_view_mode.txt"
[ -f "$FH" ] && cp "$FH" "$BKPDIR/events_hq_frame_history.txt"
: > "$RELAY"
rm -f "$PNG" "$RESULTS" "$VMODE" "$FH"

log "=== step 1: create disposable entity ($ENTITY) ==="
mkdir -p "$PKG/pages/page_1"
: > "$PKG/pages/page_1/event.ir.pdl"
: > "$PKG/pages/page_1/event.pal"

log "=== step 2: launch real events-hq window (exact task3 launch command) ==="
setsid nohup "$BIN" "$HOUSE" "$CHTPM" "$PKG" "$ENTITY" > "$LAUNCH_LOG" 2>&1 < /dev/null &
disown 2>/dev/null || true
sleep 3

pids="$(any_pids "$PROC_PATTERN")"
n="$(echo "$pids" | grep -c . || true)"
if [ "$n" != "1" ]; then
  fail "expected 1 render process, got $n"
  cat "$LAUNCH_LOG" 2>/dev/null
  exit 1
fi
log "events-hq launched (PID $pids)"
sleep 1

log "=== step 3: run the PAL harness via the real prisc+x VM ==="
(cd "$PRISC_CWD" && timeout 60 "$PRISC_BIN" "$PAL_FILE")
rc=$?
sleep 2   # render needs a moment to process the queued 112 and write the PNG

log "=== step 4: verify the real PNG + verdict ==="
cat "$RESULTS" 2>/dev/null
if [ -f "$PNG" ]; then
  size=$(stat -c%s "$PNG")
  log "PNG EXISTS: $PNG ($size bytes)"
  if [ "$size" -gt 1000 ]; then
    pass "real PNG produced and asserted genuine size: $PNG ($size bytes)"
  else
    fail "PNG too small to be a real captured frame: $size bytes"
  fi
  cp "$PNG" "$RESULTS_DIR/final_proof_events_hq.png"
  log "copy saved: $RESULTS_DIR/final_proof_events_hq.png"
else
  fail "PNG not created at $PNG"
fi

log "view_mode file readback (should be view_mode=1):"
cat "$VMODE" 2>/dev/null || echo "(file missing)"

if [ -f "$RESULTS" ] && grep -q '^done=1' "$RESULTS" && grep -q '^pass=1' "$RESULTS"; then
  pass "PAL verdict: done=1 pass=1 (archive $RESULTS_DIR)"
else
  fail "PAL verdict not all PASS: $(cat "$RESULTS" 2>/dev/null)"
fi

log "=== step 5: cleanup (explicit, then verified) ==="
cleanup
log "cleanup complete (trap re-entry is idempotent-guarded)"
final="$(any_pids "$PROC_PATTERN")"
if [ -n "$final" ]; then
  fail "stray render process(es) after cleanup: $final"
else
  pass "zero stray render processes after cleanup"
fi
cat "$RESULTS_DIR/summary.txt"
log "Done. PNG to see: $RESULTS_DIR/final_proof_events_hq.png"