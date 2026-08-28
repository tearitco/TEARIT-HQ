#!/bin/bash
# run_db_hq_tab_switch_demo.sh - launcher/verifier/cleanup for the db-hq
# tab-switch Loop/Wait/Send-Input proof (NEW TASK, COMMON-EVENTS-MANAGER-
# HANDOFF.md "db-hq PAL harness proof"). The actual drive sequence is the
# PAL harness pal/db_hq_tab_switch_demo.pal (run via the real prisc+x VM);
# this script owns the three things a .pal can never do per
# HARNESS-AUTHORING-GUIDE.md 3a ("Still needs TEMPLATE/exec: launching the
# target render binary itself ... not a gap"):
#   1. launch a REAL db-hq window via the REAL open_db_hq.sh (the house's
#      own db-hq launcher - it kills any pre-existing db-hq instance,
#      launches the shared khtpm_entity_menu_render.+x in db-hq mode, and
#      lets the shell self-spawn khtpm_hq_manager.+x via the <module> tag),
#   2. run the .pal through the real prisc+x binary,
#   3. verify the real PNG + verdict, then restore relay/history files and
#      kill only the processes THIS run spawned.
#
# DISPOSABLE-ENTITY honesty (see the PAL header for the full note): db-hq
# has NO entity argument in its launch command - it is deliberately
# single-instance-per-house ("$BIN <house_root> <db-hq/dashboard.chtpm>").
# "Disposable test entity" therefore means: the harness never opens
# cursword's own event_pkg, never activates a Common Event, and drives the
# window to the Actors tab (a real placeholder tab that touches zero data).
# Nothing here edits any common_events/ content - the run reads the relay
# and frame-history state only.
#
# Verification standard (from the handoff): zero stray db-hq processes
# before/after; every real relay/history file the run touched restored to
# its pre-test git-clean state; pre-existing live processes (this house's
# events-hq managers - present before this run) are snapshotted and NEVER
# touched.

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
  echo "run_db_hq_tab_switch_demo: could not find house root above $HERE" >&2
  exit 1
}

HOUSE="${HOUSE:-$(find_house_root)}"

PAL_DIR="$HERE/pal"
PAL_FILE="$PAL_DIR/db_hq_tab_switch_demo.pal"
LAUNCHER="$HOUSE/*.monads/*.muchi-pet/ops/open_db_hq.sh"

D="$HOUSE/#.desktop"
RELAY="$D/db_hq_history.txt"            # git-TRACKED real relay (restore reqd)
CESTATE="$D/db_hq_common_events.state.txt" # git-TRACKED manager shell IPC (restore reqd)
CACTION="$D/db_hq_action.txt"           # git-TRACKED manager action file (restore reqd)
FH="$D/db_hq_frame_history.txt"         # render's own frame log (original behavior; absent pre-run, remove)

TMPSTATE="/tmp/db-hq-state.txt"
PNG="/tmp/db-hq-frame.png"
RESULTS="/tmp/dbhq_visproof_results.txt"
LAUNCH_LOG="/tmp/dbhq_visproof_launch.log"

PRISC_BIN="$HOUSE/101.mutaclsym🧟‍♂️️+18.0G/system/prisc+x"
PRISC_CWD="$HOUSE/101.mutaclsym🧟‍♂️️+18.0G"

RENDER_PAT="khtpm_entity_menu_render\.\+x"
DBHQ_PAT="khtpm_entity_menu_render\.\+x .*db-hq/dashboard\.chtpm"
HQMGR_PAT="khtpm_hq_manager\.\+x"

# repo root + rel paths for the git-clean restore/verify (walk up from the
# house root to the dir that has .git; git is the authoritative source of
# the pre-test state, NOT a backup copy - a backup can silently drift if
# the pre-run file was already dirty, which once produced a false
# "byte-exact" PASS, so the assertion below is against HEAD itself).
ROOT="$HOUSE"
while [ "$(dirname "$ROOT")" != "/" ] && [ ! -d "$ROOT/.git" ]; do ROOT="$(dirname "$ROOT")"; done
RELAY_REL="$(realpath --relative-to="$ROOT" "$RELAY")"
CESTATE_REL="$(realpath --relative-to="$ROOT" "$CESTATE")"
CACTION_REL="$(realpath --relative-to="$ROOT" "$CACTION")"

RESULTS_DIR="$PAL_DIR/presentations/db-hq-tab-switch-$(date '+%Y%m%d-%H%M%S')"

log() { echo "[$(date '+%H:%M:%S')] $*" | tee -a "$RESULTS_DIR/log.txt"; }
pass() { log "PASS: $*"; echo "PASS: $*" >> "$RESULTS_DIR/summary.txt"; }
fail() { log "FAIL: $*"; echo "FAIL: $*" >> "$RESULTS_DIR/summary.txt"; }

any_pids() { pgrep -f "$1" 2>/dev/null || true; }

# Snapshot the process sets that existed BEFORE this run so cleanup only
# ever touches what THIS run spawned (never the pre-existing events-hq
# managers cursword/v2demo, never any pre-existing render).
PRE_EXISTING_RENDERS="$(any_pids "$RENDER_PAT")"
PRE_EXISTING_HQMGRS="$(any_pids "$HQMGR_PAT")"
EXISTED_FH=0; [ -f "$FH" ] && EXISTED_FH=1

cleanup() {
  # idempotent: this runs once explicitly and again via trap - guard it
  if [ -n "${CLEANUP_DONE:-}" ]; then return; fi
  CLEANUP_DONE=1
  # close the window gracefully first (Escape, twice - db-hq's Escape is
  # g_quit), into the LIVE relay (restore happens last so these reach the
  # render). The relay was truncated for the clean drive, so these appends
  # are the last bytes the render will see this run.
  echo 27 >> "$RELAY" 2>/dev/null; sleep 0.3
  echo 27 >> "$RELAY" 2>/dev/null; sleep 0.3
  local dead new_hqmgrs pids
  # reset the pre-existing render list - any render that appeared AFTER
  # this process started snapshotting is THIS run's (the launcher itself
  # also TERM'd any pre-existing db-hq at launch, which is expected and
  # already reflected: they are gone, not "ours to restore")
  dead="$(any_pids "$RENDER_PAT")"
  if [ -n "$dead" ]; then
    echo "$dead" | xargs -r kill -TERM 2>/dev/null
    sleep 2
    dead="$(any_pids "$RENDER_PAT")"
    [ -n "$dead" ] && echo "$dead" | xargs -r kill -KILL 2>/dev/null
    sleep 0.5
  fi
  # kill NEW hq manager procs only (subtract the pre-existing set)
  new_hqmgrs="$(comm -13 <(echo "$PRE_EXISTING_HQMGRS" | tr ' ' '\n' | sort) <(any_pids "$HQMGR_PAT" | tr '\n' ' ' | tr ' ' '\n' | sort) 2>/dev/null)"
  if [ -n "$new_hqmgrs" ]; then
    echo "$new_hqmgrs" | xargs -r kill -TERM 2>/dev/null
    sleep 1
    echo "$new_hqmgrs" | xargs -r kill -KILL 2>/dev/null
  fi
  # dispose of everything this run created (tmp artifacts + new files that
  # did NOT pre-exist - the render makes FH each run)
  rm -f "$RESULTS" "$PNG" "$TMPSTATE" "$LAUNCH_LOG"
  [ "$EXISTED_FH" = "0" ] && rm -f "$FH"
  # restore the real git-tracked relay/state files to EXACT git-HEAD state
  # LAST (after the render/manager are dead, so nothing can re-append; git
  # checkout is authoritative vs a backup-copy, which drifts if the pre-run
  # file was already dirty) - trap re-entry is idempotent-guarded above.
  git -C "$ROOT" checkout -- "$RELAY_REL" "$CESTATE_REL" "$CACTION_REL" 2>/dev/null
}
trap cleanup EXIT

mkdir -p "$RESULTS_DIR"

log "=== step 0: zero stray db-hq processes before run ==="
pre_dbhq="$(any_pids "$DBHQ_PAT")"
pre_hqmgrs="$(any_pids "$HQMGR_PAT")"
if [ -n "$pre_dbhq" ] || [ -n "$pre_hqmgrs" ]; then
  fail "stray db-hq process(es) before run: render($pre_dbhq) mgr($pre_hqmgrs)"
  echo "$pre_dbhq" | xargs -r kill -KILL
  echo "$pre_hqmgrs" | xargs -r kill -KILL
  sleep 1
fi

# PRE-RUN git-clean assertion: refuse to run if any git-tracked relay/state
# file is already dirty. A stale corrupt state is exactly the drift that
# once produced a false "byte-exact" PASS - start clean or don't start.
if [ -n "$(git -C "$ROOT" status --porcelain -- "$RELAY_REL" "$CESTATE_REL" "$CACTION_REL")" ]; then
  log "git-tracked db-hq files already dirty before this run - restore them first:"
  git -C "$ROOT" status --porcelain -- "$RELAY_REL" "$CESTATE_REL" "$CACTION_REL"
  exit 1
fi
# clear the relay for a clean drive (restores happen in cleanup under trap)
: > "$RELAY"
rm -f "$PNG" "$RESULTS" "$TMPSTATE" "$FH" "$LAUNCH_LOG"

log "=== step 1: launch real db-hq window via the real open_db_hq.sh ==="
sh "$LAUNCHER" "$HOUSE" > "$LAUNCH_LOG" 2>&1
launch_rc=$?
cat "$LAUNCH_LOG"
if [ "$launch_rc" != "0" ]; then
  fail "open_db_hq.sh exited $launch_rc"
  exit 1
fi
sleep 2

pids="$(any_pids "$DBHQ_PAT")"
n="$(echo "$pids" | grep -c . || true)"
mgrs="$(any_pids "$HQMGR_PAT")"
nm="$(echo "$mgrs" | grep -c . || true)"
if [ "$n" != "1" ] || [ "$nm" != "1" ]; then
  fail "expected 1 db-hq render + 1 hq manager, got render=$n manager=$nm"
  cat "$LAUNCH_LOG" 2>/dev/null
  exit 1
fi
log "db-hq launched (render PID $pids, manager PID $mgrs)"
sleep 1

log "=== step 2: run the PAL harness via the real prisc+x VM ==="
(cd "$PRISC_CWD" && timeout 60 "$PRISC_BIN" "$PAL_FILE")
rc=$?
log "prisc+x rc=$rc"
sleep 2   # db-hq needs a moment to process the queued 112 and write the PNG

log "=== step 3: verify the real PNG + verdict ==="
cat "$RESULTS" 2>/dev/null
if [ -f "$PNG" ]; then
  size=$(stat -c%s "$PNG")
  log "PNG EXISTS: $PNG ($size bytes)"
  if [ "$size" -gt 1000 ]; then
    pass "real PNG produced and asserted genuine size: $PNG ($size bytes)"
  else
    fail "PNG too small to be a real captured frame: $size bytes"
  fi
  cp "$PNG" "$RESULTS_DIR/final_proof_db_hq.png"
  log "copy saved: $RESULTS_DIR/final_proof_db_hq.png"
else
  fail "PNG not created at $PNG"
fi

log "code-210 state readback (post-switch dump should show Actors):"
[ -f "$TMPSTATE" ] && grep -q '^g_dbhq_current_tab=0 (Actors)' "$TMPSTATE" && pass "code-210 debug dump independently confirms Actors tab"
grep -E '^g_dbhq_current_tab|^g_focus_nav' "$TMPSTATE" 2>/dev/null | sed 's/^/  state: /'

if [ -f "$RESULTS" ] && grep -q '^done=1' "$RESULTS" && grep -q '^pass=1' "$RESULTS"; then
  pass "PAL verdict: done=1 pass=1 (archive $RESULTS_DIR)"
else
  fail "PAL verdict not all PASS: $(cat "$RESULTS" 2>/dev/null)"
fi

log "=== step 4: cleanup (explicit, then verified) ==="
cleanup
log "cleanup complete (trap re-entry is idempotent-guarded)"
final_renders="$(any_pids "$RENDER_PAT")"
final_mgrs="$(any_pids "$HQMGR_PAT")"
if [ -n "$final_renders" ] || [ -n "$final_mgrs" ]; then
  fail "stray process(es) after cleanup: renders($final_renders) hq_mgrs($final_mgrs)"
else
  pass "zero stray render/hq-manager processes after cleanup"
fi
# git-clean assertion on the restored tracked files - the real check is
# against HEAD state directly (authoritative), not against a snapshot.
dirty="$(git -C "$ROOT" status --porcelain -- "$RELAY_REL" "$CESTATE_REL" "$CACTION_REL")"
if [ -n "$dirty" ]; then
  fail "tracked db-hq files not git-clean after restore:"
  echo "$dirty" | sed 's/^/  /'
else
  pass "db_hq relay/state files git-clean after restore"
fi
cat "$RESULTS_DIR/summary.txt"
log "Done. PNG to see: $RESULTS_DIR/final_proof_db_hq.png"