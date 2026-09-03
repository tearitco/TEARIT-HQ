#!/bin/sh
# evhq_action.sh - the ONLY write path from events-hq.xhtpm clicks/typing.
# Never compiles IR: writes a selection / request file the manager
# (khtpm_events_hq_manager.+x) already polls (action.txt), or a small UI
# state file the projector reads (picker.txt / editor.txt / pending_fields.txt).
#
# Item actions arrive as:  evhq_action.sh <verb> <arg> <event_pkg> <xhtpm_pkg> <house>
# cli_io (field) actions:  evhq_action.sh field <type> <name> <event_pkg> <xhtpm_pkg> <house> <typed_value>
set -e
VERB="${1:-}"

if [ "$VERB" = "field" ]; then
    FTYPE="${2:-}"; FNAME="${3:-}"; PKG_DIR="${4:-}"; VALUE="${7:-}"
else
    ARG="${2:-}"; PKG_DIR="${3:-}"
fi
[ -n "$PKG_DIR" ] && [ -d "$PKG_DIR" ] || { echo "evhq_action: bad event_pkg dir '$PKG_DIR'" >&2; exit 1; }
MGR="$PKG_DIR/.hq_manager"
mkdir -p "$MGR"

# upsert "key=value" into a k=v file
kv_put() {  # kv_put <file> <key> <value>
    _f="$1"; _k="$2"; _v="$3"
    _tmp="$_f.tmp.$$"
    { [ -f "$_f" ] && grep -v "^$_k=" "$_f" || true; printf '%s=%s\n' "$_k" "$_v"; } > "$_tmp"
    mv "$_tmp" "$_f"
}

# build "k1=v1|k2=v2" from pending_fields.txt in registry PARAMS order
build_params() {  # build_params <type>
    _t="$1"
    _reg="$(dirname "$PKG_DIR")"   # unused; registry is house-wide
    _pend="$MGR/pending_fields.txt"
    [ -f "$_pend" ] || { printf ''; return; }
    # emit in file order (good enough - manager re-derives on compile)
    _first=1
    while IFS='=' read -r _k _v; do
        [ -n "$_k" ] || continue
        if [ "$_first" = 1 ]; then printf '%s=%s' "$_k" "$_v"; _first=0
        else printf '|%s=%s' "$_k" "$_v"; fi
    done < "$_pend"
}

case "$VERB" in
  view)   printf '%s\n' "$ARG" > "$MGR/view.txt" ;;
  page)   printf '%s\n' "$ARG" > "$MGR/selected_page.txt" ;;

  picker)
    case "$ARG" in
      open)  printf '1\n' > "$MGR/picker.txt" ;;
      close) printf '0\n' > "$MGR/picker.txt" ;;
      *) echo "evhq_action: picker needs open|close" >&2; exit 1 ;;
    esac ;;

  pick)
    # ARG = registry command type. Open the field editor (not a bare append).
    : > "$MGR/pending_fields.txt"
    printf 'mode=fields\ntype=%s\nedit_id=-1\n' "$ARG" > "$MGR/editor.txt"
    printf '0\n' > "$MGR/picker.txt" ;;

  edit)
    # command-list row click: ARG = command id; $6 (after pkg/house) unused.
    # the template passes the type as arg-after-arg -> re-read from page.state.txt
    EID="$ARG"
    ETYPE="$(awk -F'|' -v id="$EID" '$1=="CMD" && $2==id {print $3; exit}' "$MGR/page.state.txt" 2>/dev/null)"
    [ -n "$ETYPE" ] || { echo "evhq_action: edit - no CMD id $EID" >&2; exit 1; }
    : > "$MGR/pending_fields.txt"
    printf 'mode=fields\ntype=%s\nedit_id=%s\n' "$ETYPE" "$EID" > "$MGR/editor.txt"
    printf '0\n' > "$MGR/picker.txt" ;;

  field)
    kv_put "$MGR/pending_fields.txt" "$FNAME" "$VALUE" ;;

  commit)
    # read editor.txt for type + edit_id, assemble params, hand to manager
    ETYPE="$(sed -n 's/^type=//p' "$MGR/editor.txt" 2>/dev/null | head -1)"
    EID="$(sed -n 's/^edit_id=//p' "$MGR/editor.txt" 2>/dev/null | head -1)"
    [ -n "$ETYPE" ] || { echo "evhq_action: commit - no editor type" >&2; exit 1; }
    PARAMS="$(build_params "$ETYPE")"
    if [ -n "$EID" ] && [ "$EID" != "-1" ]; then
        printf 'edit:%s|%s|%s\n' "$EID" "$ETYPE" "$PARAMS" > "$MGR/action.txt"
    else
        printf 'append:%s|%s\n' "$ETYPE" "$PARAMS" > "$MGR/action.txt"
    fi
    : > "$MGR/pending_fields.txt"
    : > "$MGR/editor.txt" ;;

  cancel-fields)
    : > "$MGR/pending_fields.txt"
    : > "$MGR/editor.txt" ;;

  del)
    printf 'delete:%s\n' "$ARG" > "$MGR/action.txt" ;;

  play)
    printf 'play\n' > "$MGR/action.txt" ;;

  *)
    echo "evhq_action: unknown verb '$VERB'" >&2
    exit 1 ;;
esac
