#!/bin/sh
# actions.sh <verb> [arg] <package_dir> - store-hq MOCKUP actions. Nothing is installed: "get" only records the id in state/owned.txt
# so the Library tab can be demonstrated. Verbs: tab <library|store|toys>, sel <id>, get.
set -u
VERB="${1:-}"; shift || true
case "$VERB" in tab|sel) ARG="${1:-}"; shift || true ;; *) ARG="" ;; esac
PKG="${1:-$(cd "$(dirname "$0")" && pwd)}"
mkdir -p "$PKG/state"
case "$VERB" in
    tab)  case "$ARG" in library|store|toys) printf '%s\n' "$ARG" > "$PKG/state/tab.txt"; : > "$PKG/state/sel.txt" ;; *) echo "bad tab" >&2; exit 2 ;; esac
          msg="tab $ARG" ;;
    sel)  case "$ARG" in *[!a-z0-9-]*|"") echo "bad id" >&2; exit 2 ;; esac
          printf '%s\n' "$ARG" > "$PKG/state/sel.txt"; msg="selected $ARG" ;;
    get)  id=$(cat "$PKG/state/sel.txt" 2>/dev/null)
          [ -n "$id" ] || exit 0
          grep -qx "$id" "$PKG/state/owned.txt" 2>/dev/null || printf '%s\n' "$id" >> "$PKG/state/owned.txt"
          msg="MOCK: marked $id owned (nothing was downloaded or installed)" ;;
    *)    echo "actions.sh: unknown verb '$VERB'" >&2; exit 2 ;;
esac
printf '%s\n' "$msg" > "$PKG/state/last_action.txt"
sh "$PKG/refresh.sh" --once "$PKG" >/dev/null 2>&1 || true
