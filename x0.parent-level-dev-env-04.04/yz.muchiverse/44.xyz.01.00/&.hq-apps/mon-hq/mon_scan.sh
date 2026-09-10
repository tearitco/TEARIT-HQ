#!/bin/sh
# mon_scan.sh - house "session monitor": find every board/engine/hq
# process this house can spawn, classify each one (is it the live
# desktop? a child of something alive? an orphan? a leaked detached
# stack?), and optionally kill everything that is stray.
#
# Born 2026-09-10 after a real leak: closing a piececraft-hq window left
# its whole engine stack (button.sh -> orchestrator -> chtpm_parser_pal
# -> prisc+x, + pc_clock_daemon + pchq_board_projector) running for 10h,
# unregistered in the proc-ledger, burning ~10% of a core in its diamond
# loop with no window attached. Two older piececraft-xyz stacks had been
# up 23h the same way.
#
# Usage:
#   mon_scan.sh [list]                 - print the table (default)
#   mon_scan.sh publish <ui.txt>       - write key=value rows for mon-hq.xhtpm
#   mon_scan.sh kill-all [--with-pals] - TERM->KILL every STRAY proc
#                                        (--with-pals also reaps idle pal windows)
#
# NEVER kills: the livedesk taskbar itself (strip renderer + taskbar
# manager + swatch/entity managers that share its process-group), or
# this script's own shell. Those are the desktop shell, not a session.
#
# "Stray" = matches a house pattern AND is one of:
#   orphan   - reparented to init (ppid 1)
#   detached - child of `systemd --user` but NOT part of the live
#              taskbar's process-group and NOT in the proc-ledger
#   dead-parent - its nearest house ancestor is gone
#   stale-unreg - a full engine stack (prisc/orchestrator/parser/button.sh)
#                 not in the proc-ledger and older than STALE_MIN minutes
set -u

STALE_MIN=3
INIT_PID=1

# ---- locate house root -------------------------------------------------
SELF_DIR=$(cd "$(dirname "$0")" && pwd)
HOUSE_ROOT="${MON_HOUSE:-${KHTPM_HOUSE:-}}"
if [ -z "${HOUSE_ROOT:-}" ] || [ ! -d "$HOUSE_ROOT" ]; then
    # &.hq-apps/mon-hq -> up 2 = 44.xyz.01.00 (the house root)
    HOUSE_ROOT=$(cd "$SELF_DIR/../.." && pwd)
fi
LEDGER="$HOUSE_ROOT/#.desktop/livedesk_proc_list.txt"

# ---- what counts as a "house session" process ------------------------
# extended-regex, matched against the full command line (ps -o args)
PATTERNS='
prisc\+x(\.\+x)? .*\.pal
/system/orchestrator( |$)
chtpm_parser_pal
button\.sh (run|run-widget)
main_module\.pal
bv_render_3d
bv_gpu_raymarch
bv_dispatch
bv_menu_input
pc_menu_input
pc_clock_daemon
pchq_board_projector
pc_phymoji
_projector\.pal
khtpm_core_render\.\+x
khtpm_entity_menu_render
khtpm_hq_manager\.\+x
_hq_manager\.\+x
swatch_picker_manager
'

# processes that ARE the desktop shell - never stray, never killed
SHELL_PATTERNS='
khtpm_taskbar_manager_main
khtpm_strip_parser
run_khtpm_strip\.sh
khtpm_strip_header\.xhtpm
khtpm_strip_bottom\.xhtpm
livedesk_splash
'

MYPID=$$
ppid_of() { ps -o ppid= -p "$1" 2>/dev/null | tr -d ' '; }
# walk my own ancestry so we never list/kill a proc we are running under
my_ancestry() {
    p=$MYPID
    _g=0
    while [ -n "${p:-}" ] && [ "$p" -gt 1 ] 2>/dev/null; do
        echo "$p"
        p=$(ppid_of "$p")
        _g=$((_g+1)); [ "$_g" -gt 40 ] && break
    done
}
MY_ANCESTRY=" $(my_ancestry | tr '\n' ' ') "

# ---- helpers ---------------------------------------------------------
ps_field() { awk -v p="$1" -v f="$2" 'NR>1 && $1==p {print $f}' "$PS_SNAP"; }

# is pid (or its pgid) written in the proc-ledger?
in_ledger() {
    [ -f "$LEDGER" ] || return 1
    _pid=$1; _pgid=$2
    while read -r f1 f2 _rest; do
        case "$f1" in ''|\#*) continue ;; esac
        case "$f1" in *[!0-9]*) continue ;; esac
        [ "$f1" = "$_pid" ] && return 0
        [ -n "${f2:-}" ] && [ "$f2" = "$_pgid" ] && return 0
        [ -n "${f2:-}" ] && [ "$f2" = "$_pid" ]  && return 0
    done < "$LEDGER"
    return 1
}

# nearest ancestor pid that is itself in MATCHED (the house stack it hangs off)
house_ancestor() {
    _p=$(ppid_of "$1")
    _guard=0
    while [ -n "${_p:-}" ] && [ "$_p" -gt 1 ] 2>/dev/null; do
        case " $MATCHED_PIDS " in *" $_p "*) echo "$_p"; return 0 ;; esac
        _p=$(ppid_of "$_p")
        _guard=$((_guard+1)); [ "$_guard" -gt 40 ] && break
    done
    return 1
}

human_age() {
    s=$1
    if   [ "$s" -lt 3600 ];  then echo "$((s/60))m"
    elif [ "$s" -lt 86400 ]; then echo "$((s/3600))h$(( (s%3600)/60 ))m"
    else echo "$((s/86400))d$(( (s%86400)/3600 ))h"
    fi
}

# ---- take one ps snapshot everything else reads ---------------------
PS_SNAP=$(mktemp "${TMPDIR:-/tmp}/mon_scan.XXXXXX")
trap 'rm -f "$PS_SNAP"' EXIT
# pid ppid pgid etimes pcpu args...
ps -eo pid=,ppid=,pgid=,etimes=,pcpu=,args= 2>/dev/null \
  | awk '{print}' > /dev/null
ps -eo pid,ppid,pgid,etimes,pcpu,args 2>/dev/null > "$PS_SNAP"

# taskbar process-group (the whole desktop shell shares it)
TASKBAR_PGID=$(awk 'NR>1 && $6 ~ /khtpm_taskbar_manager_main/ {print $3; exit}' "$PS_SNAP")
[ -z "${TASKBAR_PGID:-}" ] && TASKBAR_PGID=-1

# ---- build MATCHED set ---------------------------------------------
# a line matches if args hits PATTERNS and does NOT hit SHELL_PATTERNS
# POSIX-ERE for awk: turn \+ \. into bracket classes so mawk/gawk don't warn
awk_ere() { printf '%s' "$1" | sed '/^$/d' | sed 's/\\+/[+]/g; s/\\\./[.]/g' | paste -sd'|' -; }
PAT_RE=$(awk_ere "$PATTERNS")
SHELL_RE=$(awk_ere "$SHELL_PATTERNS")

MATCHED_PIDS=$(awk -v pat="$PAT_RE" -v shl="$SHELL_RE" '
    NR>1 {
        args=""; for(i=6;i<=NF;i++) args=args (i>6?" ":"") $i
        if (args ~ shl) next
        if (args ~ pat) print $1
    }' "$PS_SNAP" | tr '\n' ' ')

# drop our own ancestry / self
_clean=""
for pid in $MATCHED_PIDS; do
    case "$MY_ANCESTRY" in *" $pid "*) continue ;; esac
    [ "$pid" = "$MYPID" ] && continue
    _clean="$_clean $pid"
done
MATCHED_PIDS=$(echo $_clean)

# ---- classify -----------------------------------------------------
# emits, per pid, a pipe row:
#   pid|ppid|pgid|age_s|pcpu|class|stray(0/1)|ledger|note|cmd
classify_all() {
    for pid in $MATCHED_PIDS; do
        ppid=$(ps_field "$pid" 2)
        pgid=$(ps_field "$pid" 3)
        age=$(ps_field  "$pid" 4)
        cpu=$(ps_field  "$pid" 5)
        cmd=$(awk -v p="$pid" 'NR>1 && $1==p {for(i=6;i<=NF;i++) printf "%s%s", (i>6?" ":""), $i; exit}' "$PS_SNAP")
        [ -z "${ppid:-}" ] && continue

        # a bare shell whose argv merely mentions an engine binary name is
        # NOT an engine - only count it if it is a real `button.sh run`
        # stack host. Prevents killing an unrelated shell (incl. an agent
        # shell) that happens to echo "bv_render_3d" etc.
        comm=$(cat "/proc/$pid/comm" 2>/dev/null || echo "")
        case "$comm" in
            sh|bash|dash|zsh|ksh)
                case "$cmd" in
                    *"button.sh run"*|*"button.sh run-widget"*|*"/system/orchestrator"*) : ;;
                    *) continue ;;
                esac
                ;;
        esac
        age=${age:-0}; cpu=${cpu:-0}; pgid=${pgid:-0}

        short=$(printf '%s' "$cmd" \
          | sed -E "s#$HOUSE_ROOT/##g; s#[^ ]*/##; s/\.\+x//; s/ +\-\-daemon//" \
          | cut -c1-46)

        led=unreg
        in_ledger "$pid" "$pgid" && led=reg

        is_pal=0
        case "$cmd" in *"/home/livedesk/pals/"*) is_pal=1 ;; esac
        is_singleton=0
        case "$cmd" in
            *swatch_picker_manager*|*_hq_manager.+x*|*stats_hq_manager*|*bookmarks_manager*|*terms_hq_manager*)
                is_singleton=1 ;;
        esac
        # an "engine" = a game/board session stack member. ONLY these get
        # the "detached from the taskbar => leaked" verdict; a lone
        # khtpm_core_render / *_manager detached via setsid is just a
        # normal -hq window, not a leak.
        is_engine=0
        case "$cmd" in
            *"main_module.pal"*|*"/system/orchestrator"*|*chtpm_parser_pal*|\
            *"button.sh run"*|*"button.sh run-widget"*|*bv_render_3d*|*bv_gpu_raymarch*|\
            *bv_dispatch*|*bv_menu_input*|*pc_menu_input*|*pc_clock_daemon*|\
            *pchq_board_projector*|*pc_phymoji*|*prisc+x*pal/*)
                is_engine=1 ;;
        esac

        class=""; stray=0; note=""

        if [ "$pgid" = "$TASKBAR_PGID" ]; then
            class=shell; note="taskbar process-group - protected"
        elif [ "$ppid" = "$INIT_PID" ]; then
            class=orphan; stray=1; note="reparented to init(1) - no live owner"
        elif house_ancestor "$pid" >/dev/null; then
            anc=$(house_ancestor "$pid")
            class=child; note="child of pid $anc"
            # a child is stray iff its stack root is stray (resolved in pass 2)
        else
            # parent is not another matched house proc
            pcmd=$(awk -v p="$ppid" 'NR>1 && $1==p {for(i=6;i<=NF;i++) printf "%s ", $i; exit}' "$PS_SNAP")
            pshort=$(printf %s "$pcmd" | sed -E 's#[^ ]*/##;s/ .*//')
            case "$pcmd" in
                *"systemd --user"*|*"/lib/systemd/systemd"*|"")
                    if [ "$is_singleton" = 1 ]; then
                        class=singleton
                        note="house-wide manager (relaunched on demand); $led"
                    elif [ "$is_pal" = 1 ] && [ "$led" = reg ]; then
                        class=pal; note="registered pal window"
                    elif [ "$is_pal" = 1 ]; then
                        class=pal-unreg; stray=1; note="pal window, not in ledger"
                    elif [ "$is_engine" = 1 ]; then
                        class=detached; stray=1
                        note="engine stack detached from the taskbar, $led - leak"
                    else
                        class=hq-window
                        note="detached -hq window (normal); $led"
                    fi
                    ;;
                *)
                    if [ "$is_engine" = 1 ]; then
                        class=detached; stray=1
                        note="engine stack under non-house parent '$pshort' (pid $ppid)"
                    else
                        class=hq-window
                        note="parent '$pshort' (pid $ppid) not a house proc"
                    fi
                    ;;
            esac
        fi

        # a full engine-stack member that isn't registered and is old = stray
        if [ "$is_engine" = 1 ] && [ "$led" = unreg ] \
           && [ "$age" -ge "$((STALE_MIN*60))" ] && [ "$class" != shell ]; then
            stray=1
            [ -z "$note" ] && note="engine stack, unregistered, ${STALE_MIN}m+ old"
        fi

        printf '%s|%s|%s|%s|%s|%s|%s|%s|%s|%s\n' \
            "$pid" "$ppid" "$pgid" "$age" "$cpu" "$class" "$stray" "$led" "$note" "$short"
    done
}

ROWS=$(classify_all)

# pass 2: a 'child' whose house-ancestor chain ends in a stray root is stray
resolve_children() {
    printf '%s\n' "$ROWS" | while IFS='|' read -r pid ppid pgid age cpu class stray led note short; do
        [ -z "$pid" ] && continue
        if [ "$class" = child ] && [ "$stray" = 0 ]; then
            root=$pid; guard=0
            while :; do
                r=$(printf '%s\n' "$ROWS" | awk -F'|' -v p="$root" '$1==p{print $2; exit}')
                [ -z "$r" ] && break
                match=$(printf '%s\n' "$ROWS" | awk -F'|' -v p="$r" '$1==p{print $1"|"$7; exit}')
                [ -z "$match" ] && break
                rootstray=$(printf '%s' "$match" | cut -d'|' -f2)
                root=$r
                [ "$rootstray" = 1 ] && { stray=1; note="$note; stack root pid $root is stray"; break; }
                guard=$((guard+1)); [ "$guard" -gt 40 ] && break
            done
        fi
        printf '%s|%s|%s|%s|%s|%s|%s|%s|%s|%s\n' \
            "$pid" "$ppid" "$pgid" "$age" "$cpu" "$class" "$stray" "$led" "$note" "$short"
    done
}
ROWS=$(resolve_children)

STRAY_PIDS=$(printf '%s\n' "$ROWS" | awk -F'|' '$7==1 {print $1}' | tr '\n' ' ')
STRAY_PGIDS=$(printf '%s\n' "$ROWS" | awk -F'|' '$7==1 {print $3}' | sort -u | tr '\n' ' ')

# ================= modes ==========================================
MODE="${1:-list}"

print_section() {   # $1 = want-stray (0/1), $2 = heading
    _any=0
    printf '%s\n' "$ROWS" | sort -t'|' -k4,4nr | while IFS='|' read -r pid ppid pgid age cpu class stray led note short; do
        [ -z "$pid" ] && continue
        [ "$stray" = "$1" ] || continue
        if [ "$_any" = 0 ]; then printf '\n== %s ==\n' "$2"; _any=1; fi
        printf '  %-7s %-11s %-6s %6s  %s\n' "$pid" "[$class]" "$(human_age "$age")" "$cpu" "$note"
        printf '          %s\n' "$short"
    done
}

case "$MODE" in
list)
    n=$(printf '%s\n' "$ROWS" | awk -F'|' '$1!=""' | wc -l | tr -d ' ')
    ns=$(echo $STRAY_PIDS | wc -w | tr -d ' ')
    ng=$((n - ns))
    print_section 0 "GOOD  ($ng) - owned / accounted for"
    print_section 1 "BAD   ($ns) - stray / leaked (no live owner)"
    echo
    echo "matched $n house proc(s): $ng good, $ns bad."
    if [ "$ns" -gt 0 ]; then
        echo "bad PIDs  : $STRAY_PIDS"
        echo "reap them : sh '$SELF_DIR/mon_scan.sh' kill-all"
    else
        echo "nothing stray - clean."
    fi
    ;;

publish)
    OUT="${2:-$SELF_DIR/state/ui.txt}"
    tmp="$OUT.tmp.$$"
    emit_rows() {   # $1 = want-stray, $2 = key prefix  -> lines "<prefix>_N_text=..."
        _i=0
        printf '%s\n' "$ROWS" | sort -t'|' -k4,4nr | while IFS='|' read -r pid ppid pgid age cpu class stray led note short; do
            [ -z "$pid" ] && continue
            [ "$stray" = "$1" ] || continue
            printf '%s_%s_text=%-7s %-11s %-6s cpu:%-4s %-5s  %s\n' \
                "$2" "$_i" "$pid" "[$class]" "$(human_age "$age")" "$cpu" "$led" "$short"
            _i=$((_i+1))
        done
    }
    {
        echo "scan_time=$(date '+%H:%M:%S')"
        echo "house_root=$HOUSE_ROOT"
        emit_rows 0 good
        emit_rows 1 bad
        rc=$(printf '%s\n' "$ROWS" | awk -F'|' '$1!=""' | wc -l | tr -d ' ')
        sc=$(echo $STRAY_PIDS | wc -w | tr -d ' ')
        gc=$((rc - sc))
        echo "rows_count=$rc"
        echo "good_count=$gc"
        echo "bad_count=$sc"
        echo "stray_count=$sc"
        echo "bad_pids=$STRAY_PIDS"
        echo "stray_pids=$STRAY_PIDS"
        [ "$gc" -eq 0 ] && echo "no_good=1" || echo "no_good=0"
        [ "$sc" -eq 0 ] && echo "no_bad=1"  || echo "no_bad=0"
        [ "$rc" -eq 0 ] && echo "clean=1"   || echo "clean=0"
    } > "$tmp"
    mv -f "$tmp" "$OUT"
    echo "mon_scan: wrote $OUT ($gc good, $sc bad)"
    ;;

kill-all)
    WITH_PALS=0
    [ "${2:-}" = "--with-pals" ] && WITH_PALS=1
    victims="$STRAY_PIDS"
    if [ "$WITH_PALS" = 1 ]; then
        extra=$(printf '%s\n' "$ROWS" | awk -F'|' '$6=="pal" {print $1}' | tr '\n' ' ')
        victims="$victims $extra"
    fi
    victims=$(echo $victims | tr ' ' '\n' | sort -un | tr '\n' ' ')
    if [ -z "$(echo $victims | tr -d ' ')" ]; then
        echo "mon_scan: nothing stray to kill"
        exit 0
    fi
    echo "mon_scan: TERM -> $victims"
    for p in $victims; do kill -TERM "$p" 2>/dev/null; done
    # also TERM the whole process-group of each stray stack
    for g in $STRAY_PGIDS; do
        case "$g" in ''|*[!0-9]*|0) continue ;; esac
        [ "$g" = "$TASKBAR_PGID" ] && continue
        kill -TERM "-$g" 2>/dev/null || true
    done
    sleep 2
    left=""
    for p in $victims; do kill -0 "$p" 2>/dev/null && left="$left $p"; done
    if [ -n "$left" ]; then
        echo "mon_scan: KILL ->$left"
        for p in $left; do kill -KILL "$p" 2>/dev/null; done
        for g in $STRAY_PGIDS; do
            case "$g" in ''|*[!0-9]*|0) continue ;; esac
            [ "$g" = "$TASKBAR_PGID" ] && continue
            kill -KILL "-$g" 2>/dev/null || true
        done
    fi
    sleep 1
    still=""
    for p in $victims; do kill -0 "$p" 2>/dev/null && still="$still $p"; done
    if [ -n "$still" ]; then
        echo "mon_scan: STILL ALIVE ->$still (permission? re-run)"
    else
        echo "mon_scan: all stray procs reaped"
    fi
    # refresh the published view if it exists
    [ -f "$SELF_DIR/state/ui.txt" ] && sh "$0" publish "$SELF_DIR/state/ui.txt" >/dev/null 2>&1 || true
    ;;

*)
    echo "usage: mon_scan.sh [list|publish <ui.txt>|kill-all [--with-pals]]" >&2
    exit 2
    ;;
esac
