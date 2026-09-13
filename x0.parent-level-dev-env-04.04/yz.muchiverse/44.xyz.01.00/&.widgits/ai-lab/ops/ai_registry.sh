#!/bin/sh
# ai_registry.sh - the real, house-wide AI-instance registry.
#
# H-AI-LAB-DESIGN.md Part 1 ("build this first"): any real FSM/
# attention-net/decision-pal in this house can register itself here
# once, so h-ai-lab's own sidebar has something real to list instead
# of a hardcoded scan. Deliberately a standalone shell tool, not a
# linked C function - callers are genuinely different binaries (C
# pals, a Python http_server, shell-driven FSMs), and this house's own
# standing rule is no cross-.c linking to share behavior; a small,
# separate, shell-invokable tool (system()/subprocess/`sh` from
# anywhere) is the real, portable answer here, same spirit as
# mon_scan.sh.
#
# Registry file: #.desktop/ai_instances_registry.txt (house-root
# scoped, same tier as livedesk_open.txt). One line per instance,
# pipe-delimited, matching this house's existing .pdl-adjacent
# convention:
#   NAME=<display name>|KIND=<fsm|attention-net|decision-pal|other>|PATH=<real path>|IFACE=<how to reach it, or ->
#
# Locking: a real cross-process flock() (via the `flock` coreutil),
# not a bespoke mkdir-lock - the read-modify-write critical section
# below must never interleave with a concurrent add/remove, same
# reason khtpm_core_render.c's registry_lock_acquire() exists for
# livedesk_open.txt.
#
# Pruning (v0, direct decision - H-AI-LAB-DESIGN.md left this open,
# decided here): an entry is dropped from the registry the moment its
# own PATH no longer exists on disk, regardless of KIND. A real,
# liveness-checked prune (HTTP health-check for attention-net's IFACE,
# a PID check for anything process-backed) is real future work once a
# KIND actually needs it - PATH-existence is the honest, simple,
# always-true-for-every-KIND baseline every real instance already
# satisfies (an FSM's state-table file, an attention-net's project
# dir, a decision-pal's state.txt all have a real PATH).
#
# Usage:
#   ai_registry.sh add    <house_root> <name> <kind> <path> [iface]
#   ai_registry.sh remove <house_root> <name>
#   ai_registry.sh list   <house_root>
#   ai_registry.sh prune  <house_root>
set -u

SELF_DIR="$(cd "$(dirname "$0")" && pwd)"
ACTION="${1:-}"
HOUSE_ROOT="${2:-}"

[ -n "$HOUSE_ROOT" ] && [ -d "$HOUSE_ROOT" ] || {
    echo "ai_registry.sh: need a real house_root as argv[2]" >&2
    exit 1
}
HOUSE_ROOT="$(cd "$HOUSE_ROOT" && pwd)"
REG="$HOUSE_ROOT/#.desktop/ai_instances_registry.txt"
LOCK="$HOUSE_ROOT/#.desktop/ai_instances_registry.lock"
mkdir -p "$HOUSE_ROOT/#.desktop"
touch "$LOCK"

# real KINDs, checked against a fixed list so a typo doesn't silently
# create a new, unintended category - "other" is the deliberate escape
# hatch (H-AI-LAB-DESIGN.md Part 1: "add a new KIND when a real
# instance needs one, don't pre-design for hypotheticals").
valid_kind() {
    case "$1" in
        fsm|attention-net|decision-pal|other) return 0 ;;
        *) return 1 ;;
    esac
}

# rewrite the registry under lock: read every existing line, keep it
# unless (a) its own PATH is gone (real prune) or (b) it's about to be
# replaced by $KEEP_OUT_NAME (a re-register / explicit remove) - then
# optionally append $APPEND_LINE. All three verbs below share this one
# critical section so a concurrent add/remove/prune can never interleave.
rewrite_registry() {
    # $1 = NAME to drop from the OLD content (re-register or remove),
    #      empty string = don't drop any name (plain prune)
    # $2 = line to append after pruning, empty = don't append (remove/prune)
    drop_name="$1"
    append_line="$2"
    tmp="$REG.tmp.$$"
    : > "$tmp"
    if [ -f "$REG" ]; then
        while IFS= read -r line || [ -n "$line" ]; do
            [ -z "$line" ] && continue
            line_name=$(printf '%s\n' "$line" | awk -F'|' '{for(i=1;i<=NF;i++) if ($i ~ /^NAME=/) print substr($i,6)}')
            line_path=$(printf '%s\n' "$line" | awk -F'|' '{for(i=1;i<=NF;i++) if ($i ~ /^PATH=/) print substr($i,6)}')
            [ -n "$drop_name" ] && [ "$line_name" = "$drop_name" ] && continue
            [ -n "$line_path" ] && [ ! -e "$line_path" ] && continue
            printf '%s\n' "$line" >> "$tmp"
        done < "$REG"
    fi
    [ -n "$append_line" ] && printf '%s\n' "$append_line" >> "$tmp"
    mv -f "$tmp" "$REG"
}

case "$ACTION" in
add)
    NAME="${3:-}"; KIND="${4:-}"; PATHV="${5:-}"; IFACE="${6:--}"
    if [ -z "$NAME" ] || [ -z "$KIND" ] || [ -z "$PATHV" ]; then
        echo "usage: ai_registry.sh add <house_root> <name> <kind> <path> [iface]" >&2
        exit 2
    fi
    case "$NAME" in *'|'*|*'='*) echo "ai_registry.sh: name must not contain | or =" >&2; exit 2 ;; esac
    valid_kind "$KIND" || {
        echo "ai_registry.sh: unknown kind '$KIND' - real KINDs are fsm|attention-net|decision-pal|other" >&2
        exit 2
    }
    [ -e "$PATHV" ] || {
        echo "ai_registry.sh: refusing to register '$NAME' - PATH does not exist: $PATHV" >&2
        exit 1
    }
    LINE="NAME=$NAME|KIND=$KIND|PATH=$PATHV|IFACE=$IFACE"
    (
        flock -x 9
        rewrite_registry "$NAME" "$LINE"
    ) 9>"$LOCK"
    echo "ai_registry.sh: registered '$NAME' ($KIND)"
    ;;

remove)
    NAME="${3:-}"
    [ -n "$NAME" ] || { echo "usage: ai_registry.sh remove <house_root> <name>" >&2; exit 2; }
    (
        flock -x 9
        rewrite_registry "$NAME" ""
    ) 9>"$LOCK"
    echo "ai_registry.sh: removed '$NAME' (if it was present)"
    ;;

prune)
    (
        flock -x 9
        rewrite_registry "" ""
    ) 9>"$LOCK"
    echo "ai_registry.sh: pruned dead entries"
    ;;

list)
    (
        flock -s 9
        [ -f "$REG" ] && cat "$REG"
    ) 9>"$LOCK"
    ;;

*)
    echo "usage: ai_registry.sh [add|remove|prune|list] <house_root> ..." >&2
    exit 2
    ;;
esac
