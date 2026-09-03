#!/bin/sh
# toy-install — download and install a toy from the tearit-hq app store.
#
#   curl -fsSL https://raw.githubusercontent.com/tearitco/tearit-store-demo/main/toy-install.sh | sh -s -- hello-toy
#
# Positional arg 1 : TOY NAME (default: hello-toy). Names the toy dir.
# Env overrides:
#   PREFIX       tearit-hq install root       (default: $HOME/tearit-hq)
#   STORE_REPO   owner/repo of store          (default: tearitco/tearit-store-demo)
#   STORE_REF    branch/tag/sha               (default: main)
#   FORCE=1      overwrite an existing toy

set -eu

TOY_NAME="${1:-hello-toy}"
PREFIX="${PREFIX:-$HOME/tearit-hq}"
STORE_REPO="${STORE_REPO:-tearitco/tearit-store-demo}"
STORE_REF="${STORE_REF:-main}"

say() { printf '\033[1m[toy-install]\033[0m %s\n' "$*"; }
die() { printf '\033[1;31m[toy-install] FATAL:\033[0m %s\n' "$*" >&2; exit 1; }

say "toy       : $TOY_NAME"
say "prefix    : $PREFIX"
say "store     : $STORE_REPO@$STORE_REF"

# --- preflight ----------------------------------------------------------
[ -d "$PREFIX" ] || die "$PREFIX does not exist (is tearit-hq installed?)"
[ -f "$PREFIX/start.sh" ] || die "$PREFIX/start.sh not found (wrong PREFIX?)"

command -v curl >/dev/null 2>&1 || command -v git >/dev/null 2>&1 \
  || die "need either curl or git on PATH"
command -v tar >/dev/null 2>&1 || command -v git >/dev/null 2>&1 \
  || die "need either tar or git on PATH"

TOY_DIR="$PREFIX/@.toys/$TOY_NAME"

if [ -e "$TOY_DIR" ]; then
    [ "${FORCE:-0}" = "1" ] || die "$TOY_DIR exists (set FORCE=1 to overwrite)"
    say "FORCE=1 — removing existing $TOY_DIR"
    rm -rf "$TOY_DIR"
fi

mkdir -p "$PREFIX/@.toys"

# --- fetch store tarball ----------------------------------------------------------
TMP="$(mktemp -d "${TMPDIR:-/tmp}/toy-install.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT
fetched=""

if command -v curl >/dev/null 2>&1 && command -v tar >/dev/null 2>&1; then
    URL="https://codeload.github.com/$STORE_REPO/tar.gz/refs/heads/$STORE_REF"
    say "downloading tarball: $URL"
    if curl -fsSL "$URL" -o "$TMP/store.tar.gz"; then
        mkdir -p "$TMP/x"
        tar -xzf "$TMP/store.tar.gz" -C "$TMP/x"
        inner="$(find "$TMP/x" -mindepth 1 -maxdepth 1 -type d | head -1)"
        [ -n "$inner" ] || die "tarball had no top-level dir"
        [ -d "$inner/$TOY_NAME" ] || die "toy '$TOY_NAME' not found in tarball"
        cp -r "$inner/$TOY_NAME" "$TOY_DIR"
        fetched="tarball"
    else
        say "tarball fetch failed — will try git clone"
    fi
fi

if [ -z "$fetched" ]; then
    command -v git >/dev/null 2>&1 || die "tarball route failed and git not available"
    say "git clone https://github.com/$STORE_REPO (ref $STORE_REF)"
    git clone --depth 1 --branch "$STORE_REF" \
        "https://github.com/$STORE_REPO.git" "$TMP/clone" \
      || git clone --depth 1 "https://github.com/$STORE_REPO.git" "$TMP/clone"
    [ -d "$TMP/clone/$TOY_NAME" ] || die "toy '$TOY_NAME' not found in repo"
    cp -r "$TMP/clone/$TOY_NAME" "$TOY_DIR"
    fetched="git"
fi

say "toy installed ($fetched) at $TOY_DIR"

say "run it with:  sh $TOY_DIR/run.sh"
say "done."
