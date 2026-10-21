#!/bin/sh
# df_event.sh <entity_dir> <script> [arg] - dispatch a game script via prisc+x VM
# with the relay event bus wired for read_relay_row/relay_write.
# $1 = ENT (entity dir, e.g. .../projects/dwarf-fortress/pieces/hero)
# $2 = script name without .asm (mine, move, rest, look, build, combat, etc.)
# $3 = optional relay arg (e.g. "N" for move direction -> writes MOVE_DIR|N to relay)
#
# Resolves HOUSE_ROOT by walking up from ENT looking for either:
#   a) a xyzfs/ dir (yz.muchiverse layout) -> then prisc+x at HOUSE_ROOT/101.mutaclsym*/system/prisc+x
#   b) a pieces/system/prisc dir (TPMOS layout) -> prisc+x at <that>/pieces/system/prisc/+x/prisc+x
# Exit 0 on success, 1 on bad input/missing prisc+x.

ENT="${1:-}"
SCRIPT="${2:-}"
ARG="${3:-}"
[ -z "$ENT" ] && { echo "df_event: need entity dir" >&2; exit 1; }
[ -z "$SCRIPT" ] && { echo "df_event: need script name" >&2; exit 1; }

# Resolve project + piece from entity dir
# ENT layout: .../projects/<project_id>/pieces/<piece_id>
DF_PROJECT_ID=""
DF_PIECE_ID=""
piece_basename="$(basename "$ENT")"
ENT_PARENT="$(dirname "$ENT")"
parent_basename="$(basename "$ENT_PARENT")"
if [ "$parent_basename" = "pieces" ]; then
    DF_PIECE_ID="$piece_basename"
    GRANDPARENT="$(dirname "$ENT_PARENT")"
    DF_PROJECT_ID="$(basename "$GRANDPARENT")"
else
    DF_PIECE_ID="${DF_PIECE_ID_OVERRIDE:-hero}"
    DF_PROJECT_ID="${DF_PROJECT_ID_OVERRIDE:-dwarf-fortress}"
fi
# Allow env overrides
DF_PROJECT_ID="${DF_PROJECT_ID_OVERRIDE:-$DF_PROJECT_ID}"
DF_PIECE_ID="${DF_PIECE_ID_OVERRIDE:-$DF_PIECE_ID}"

# Walk up from ENT to find HOUSE_ROOT
# Strategy A: look for xyzfs/ marker (yz.muchiverse layout)
D="$ENT"
HOUSE_ROOT=""
while [ "$D" != "/" ]; do
    if [ -d "$D/xyzfs" ]; then HOUSE_ROOT="$D"; break; fi
    D="$(dirname "$D")"
done

PRISC=""
if [ -n "$HOUSE_ROOT" ]; then
    # Found xyzfs marker - try the 101.mutaclsym*/system/prisc+x path
    for cand in "$HOUSE_ROOT"/101.mutaclsym*/system; do
        if [ -d "$cand" ] && [ -x "$cand/prisc+x" ]; then
            PRISC="$cand/prisc+x"
            break
        fi
    done
fi

# Strategy B: if prisc+x not found yet, walk up looking for pieces/system/prisc dir
if [ -z "$PRISC" ]; then
    D="$ENT"
    while [ "$D" != "/" ]; do
        CAND="$D/pieces/system/prisc/+x/prisc+x"
        if [ -x "$CAND" ]; then
            PRISC="$CAND"
            # HOUSE_ROOT is D (the dir containing both projects/ and pieces/)
            HOUSE_ROOT="$D"
            break
        fi
        D="$(dirname "$D")"
    done
fi

[ -x "$PRISC" ] || { echo "df_event: missing prisc+x binary" >&2; exit 1; }

# Verify script exists under HOUSE_ROOT/projects/<project_id>/scripts/<script>.asm
SCRIPT_PATH="projects/${DF_PROJECT_ID}/scripts/${SCRIPT}.asm"
SCRIPT_FULL="${HOUSE_ROOT}/${SCRIPT_PATH}"
[ -f "$SCRIPT_FULL" ] || { echo "df_event: script not found: $SCRIPT_FULL" >&2; exit 1; }

# Ensure the relay file exists (game engine's relay event bus)
RELAY="${HOUSE_ROOT}/pieces/apps/player_app/interact_relay.txt"
mkdir -p "$(dirname "$RELAY")"
touch "$RELAY"
# If an ARG is provided, write it as MOVE_DIR|ARG for scripts that read direction
if [ -n "$ARG" ]; then
    echo "MOVE_DIR|${ARG}" > "$RELAY"
fi

# Ensure frame_changed.txt dir exists
FRAME_DIR="${HOUSE_ROOT}/pieces/display"
mkdir -p "$FRAME_DIR"

# Run prisc+x with the right env vars
# cd to HOUSE_ROOT so relative script paths resolve, and so the exec opcode
# (which uses system() with ./ops/+x/rng_op.+x relative paths) works
cd "$HOUSE_ROOT" || { echo "df_event: cannot cd to $HOUSE_ROOT" >&2; exit 1; }

PRISC_PROJECT_ROOT="$HOUSE_ROOT" \
PRISC_PROJECT_ID="$DF_PROJECT_ID" \
PRISC_PIECE_ID="$DF_PIECE_ID" \
"$PRISC" "$SCRIPT_PATH" > "${HOUSE_ROOT}/projects/${DF_PROJECT_ID}/pieces/${DF_PIECE_ID}/output.log" 2>&1
exit 0
