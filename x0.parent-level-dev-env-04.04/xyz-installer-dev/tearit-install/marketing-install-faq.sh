#!/bin/sh
###############################################################################
#                                                                             #
#   tearit-hq — INSTALL FAQ                                                     #
#   =====================                                                      #
#                                                                             #
#   This file is BOTH a readable FAQ and a runnable helper.                     #
#                                                                             #
#     * Just want to read it?   Open it in any text editor.                     #
#     * Want it to do the work?  Run:   sh marketing-install-faq.sh            #
#                                                                             #
#   Hand this one file to someone. They can read it, or run it, or both.       #
#                                                                             #
# --------------------------------------------------------------------------- #
#                                                                             #
#   Q. What is tearit-hq?                                                      #
#   A. A small, self-contained desktop: a taskbar with a login/signup         #
#      system and a helper character ("cursword"). It installs into a         #
#      folder in your home directory and runs on top of your normal Linux     #
#      desktop. It does not touch system files and needs no root.             #
#                                                                             #
#   Q. What do I need first?                                                   #
#   A. A Linux machine with an X11 display, plus a compiler and a few dev     #
#      packages (it builds from source on your machine):                      #
#                                                                             #
#        Debian / Ubuntu / Mint:                                              #
#          sudo apt install build-essential pkg-config \                      #
#               libx11-dev libxext-dev libxft-dev curl                        #
#                                                                             #
#        Fedora:                                                              #
#          sudo dnf install gcc pkgconf-pkg-config \                          #
#               libX11-devel libXext-devel libXft-devel curl                  #
#                                                                             #
#        Arch:                                                                #
#          sudo pacman -S base-devel libx11 libxext libxft curl              #
#                                                                             #
#      (Windows and macOS are not supported yet.)                             #
#                                                                             #
#   Q. How do I install it?                                                    #
#   A. One command:                                                           #
#                                                                             #
#        curl -fsSL https://raw.githubusercontent.com/tearitco/tearit-install/main/install.sh | sh -s -- tearit-hq
#                                                                             #
#      That downloads the desktop, compiles it in ~/tearit-hq, and adds a     #
#      "tearit-hq" command to ~/.local/bin.                                    #
#                                                                             #
#      Or, if you have THIS folder already:   sh install.sh tearit-hq         #
#                                                                             #
#   Q. How do I start it after installing?                                     #
#   A.   tearit-hq                 (if ~/.local/bin is on your PATH)           #
#      or sh ~/tearit-hq/start.sh                                             #
#                                                                             #
#      A yellow bar with red text will appear near the top-left. Click the    #
#      user cell (second from the left) to create an account or log in.       #
#      The yellow/red colours mean "this is an install/test build" — that's   #
#      expected.                                                              #
#                                                                             #
#   Q. How do I stop it?                                                       #
#   A.   tearit-hq stop     (or: sh ~/tearit-hq/start.sh stop)                 #
#                                                                             #
#   Q. Can I put it somewhere else / call it something else?                   #
#   A. Yes. The first argument is the name; env vars move the location:       #
#                                                                             #
#        PREFIX=/opt/mydesk BINDIR=~/bin \                                    #
#          sh install.sh mydesk                                              #
#                                                                             #
#      Other knobs: FORCE=1 (overwrite an existing folder),                    #
#      NO_BUILD=1 (download only), NO_LAUNCHER=1 (don't touch PATH).           #
#                                                                             #
#   Q. How do I uninstall?                                                     #
#   A.   tearit-hq uninstall           (add -y to skip the prompt)            #
#      or sh ~/tearit-hq/start.sh uninstall                                   #
#      or curl -fsSL https://raw.githubusercontent.com/tearitco/tearit-install/main/uninstall.sh | sh -s -- tearit-hq
#      or by hand:  rm -rf ~/tearit-hq ~/.local/bin/tearit-hq                  #
#      Nothing else is written anywhere. Reinstall = uninstall then the       #
#      install one-liner again (or add FORCE=1 to install over the top).      #
#                                                                             #
#   Q. It didn't build — now what?                                            #
#   A. The build stops with the exact missing package name. Install that      #
#      (see the lists above) and re-run. If it still fails, keep the output   #
#      of:   sh ~/tearit-hq/bootstrap.sh                                      #
#                                                                             #
# --------------------------------------------------------------------------- #
#                                                                             #
#   RUNNING THIS FILE                                                          #
#                                                                             #
#     sh marketing-install-faq.sh            print this FAQ, then check your   #
#                                            machine and offer to install     #
#     sh marketing-install-faq.sh --check    only check prerequisites         #
#     sh marketing-install-faq.sh --install  check, then install without      #
#                                            asking (for scripts)             #
#     sh marketing-install-faq.sh --faq      print the FAQ text and stop      #
#                                                                             #
#   Env: PRODUCT (default tearit-hq) — passed through to the installer.        #
#                                                                             #
###############################################################################

set -u

PRODUCT="${PRODUCT:-tearit-hq}"
INSTALL_ONELINER="curl -fsSL https://raw.githubusercontent.com/tearitco/tearit-install/main/install.sh | sh -s -- $PRODUCT"
HERE="$(cd "$(dirname "$0")" 2>/dev/null && pwd || echo .)"

b() { printf '\033[1m%s\033[0m\n' "$*"; }
ok()   { printf '  \033[32m OK \033[0m %s\n' "$*"; }
bad()  { printf '  \033[31mMISS\033[0m %s\n' "$*"; }
info() { printf '  \033[36m -- \033[0m %s\n' "$*"; }

print_faq() {
    # Reprint the comment banner above (everything between the first two
    # lines of #### and the RUNNING THIS FILE section) without the leading "# ".
    sed -n '2,/^#   RUNNING THIS FILE/p' "$0" \
      | sed -e 's/^#\{1,\}//' -e 's/^ //' \
      | sed -e 's/[[:space:]]*#$//' \
      | grep -v '^#\{5,\}$'
}

check_prereqs() {
    b "Checking this machine…"
    miss=0

    if [ "$(uname -s)" = "Linux" ]; then ok "OS: Linux"; else bad "OS: $(uname -s) — only Linux is supported"; miss=1; fi

    if [ -n "${DISPLAY:-}" ]; then ok "X11 display: \$DISPLAY=$DISPLAY"
    else bad "no \$DISPLAY set — you need a graphical (X11) session"; miss=1; fi

    if command -v gcc >/dev/null 2>&1; then ok "compiler: $(gcc --version | head -1)"
    else bad "gcc not found"; miss=1; fi

    if command -v pkg-config >/dev/null 2>&1; then ok "pkg-config"
    else bad "pkg-config not found"; miss=1; fi

    for p in x11 xext xft; do
        if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists "$p" 2>/dev/null; then
            ok "dev headers: $p"
        else
            bad "dev headers: $p  (lib${p}-dev / lib${p}-devel)"; miss=1
        fi
    done

    if command -v curl >/dev/null 2>&1; then ok "curl"
    elif command -v git >/dev/null 2>&1; then info "no curl, but git present — installer will clone instead"
    else bad "need curl or git to download"; miss=1; fi

    if command -v tar >/dev/null 2>&1; then ok "tar"
    elif command -v git >/dev/null 2>&1; then info "no tar, but git present — installer will clone instead"
    else bad "need tar or git to unpack"; miss=1; fi

    free_kb=$(df -Pk "$HOME" 2>/dev/null | awk 'NR==2{print $4}')
    if [ -n "${free_kb:-}" ] && [ "$free_kb" -gt 102400 ]; then ok "disk: $((free_kb/1024)) MB free in \$HOME"
    else info "could not confirm free disk space in \$HOME (need ~50 MB)"; fi

    echo
    if [ "$miss" = 0 ]; then
        b "Ready to install."
        return 0
    else
        b "Not ready — install the MISS items above, then re-run."
        printf '\n%s\n' "Package hints:"
        printf '%s\n' "  Debian/Ubuntu : sudo apt install build-essential pkg-config libx11-dev libxext-dev libxft-dev curl"
        printf '%s\n' "  Fedora        : sudo dnf install gcc pkgconf-pkg-config libX11-devel libXext-devel libXft-devel curl"
        printf '%s\n' "  Arch          : sudo pacman -S base-devel libx11 libxext libxft curl"
        return 1
    fi
}

do_install() {
    if [ -f "$HERE/install.sh" ]; then
        b "Running local installer: $HERE/install.sh $PRODUCT"
        sh "$HERE/install.sh" "$PRODUCT"
    else
        b "Running: $INSTALL_ONELINER"
        curl -fsSL "https://raw.githubusercontent.com/tearitco/tearit-install/main/install.sh" | sh -s -- "$PRODUCT"
    fi
}

case "${1:-}" in
    --faq|faq)
        print_faq
        exit 0
        ;;
    --check|check)
        check_prereqs
        exit $?
        ;;
    --install|install|--yes|-y)
        check_prereqs || exit 1
        do_install
        exit $?
        ;;
    ""|--help|-h|help)
        print_faq
        echo
        echo "═══════════════════════════════════════════════════════════════════"
        echo
        check_prereqs || exit 1
        # Only offer the interactive prompt when we have a real terminal.
        if [ -t 0 ] && [ -t 1 ]; then
            echo
            printf "Install %s now? [y/N] " "$PRODUCT"
            read ans
            case "$ans" in
                y|Y|yes|YES) echo; do_install ;;
                *) echo "Nothing installed. When ready, run:  sh $0 --install"; ;;
            esac
        else
            echo
            echo "To install:  sh $0 --install    (or run the one-liner above)"
        fi
        ;;
    *)
        echo "unknown option: $1" >&2
        echo "usage: sh $0 [--faq | --check | --install]" >&2
        exit 1
        ;;
esac
