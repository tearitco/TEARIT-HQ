#!/usr/bin/env bash
# install-duk.sh — build the NB-JS engine and expose it as the `duk` command
# plus the $duk environment variable. No khtpm/chtpm/GUI dependency.
#
# Usage:
#   ./install-duk.sh            # build + install into $HOME/.local/bin
#   PREFIX=/usr/local ./install-duk.sh
#   RC_FILE=~/.zshrc ./install-duk.sh
#
# After it runs: open a new shell (or `source ~/.bashrc`) and run
#   duk /tmp/script.js              (node mode — like node, no browser;
#                                    require() + require('fs') + ESM import/export)
#   duk --browser /tmp/page.js      (DOM page runner + render-back)
#   duk -i                          (interactive REPL, even when piped)
#   $duk /tmp/script.js             (always works via the env var)
#
# Exit codes: 0 installed, 1 build/install failure.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
prefix="${PREFIX:-$HOME/.local}"
bindir="$prefix/bin"
dukpath="$bindir/duk"
rcfile="${RC_FILE:-$HOME/.bashrc}"

if [ ! -x "$here/nbjs" ]; then
  echo "building nbjs..."
  make -C "$here" nbjs
fi

mkdir -p "$bindir"
install -m 0755 "$here/nbjs" "$dukpath"
echo "installed engine -> $dukpath"

# persist `export duk=...` in the shell rc files so it survives shells/reloads.
# ~/.bashrc covers interactive shells; ~/.profile covers login/non-interactive
# shells (bash -l does not read ~/.bashrc).
exline="export duk='$dukpath'"
for rcf in "$rcfile" "$HOME/.profile"; do
  [ "$rcf" = "$rcfile" ] || [ -f "$rcf" ] || continue
  if [ -w "$rcf" ] || [ ! -f "$rcf" ]; then
    if ! grep -qs '^export duk=' "$rcf"; then
      { echo; echo "# NB-JS engine CLI as 'duk'"; echo "$exline"; } >> "$rcf"
      echo "added 'export duk=...' to $rcf"
    fi
  fi
done

if [ "${BASH_SOURCE[0]}" != "$0" ]; then
  # sourced: make $duk (and PATH) live in THIS shell immediately
  export duk="$dukpath"
  case ":$PATH:" in
    *":$bindir:"*) ;;
    *) export PATH="$bindir:$PATH" ;;
  esac
fi

echo
echo "Run it as:  duk /tmp/script.js [args...]       # node mode (require() + fs + ESM)"
echo "             duk --browser /tmp/page.js         # DOM page runner + render-back"
echo "             duk -i                              # interactive REPL, even piped"
echo "             duk                                 # bare on a terminal: REPL"
echo "             \$duk /tmp/script.js                 # the env var works identically"
if [ "${BASH_SOURCE[0]}" = "$0" ]; then
  echo "Then open a new shell (or run: source ~/$(basename "$rcfile"))"
fi