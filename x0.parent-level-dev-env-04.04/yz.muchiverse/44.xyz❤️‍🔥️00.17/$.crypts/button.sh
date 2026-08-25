#!/bin/bash
# button.sh - $.crypts real house-wide autostart control.
# Direct instruction: "i want the ability thru button.sh to run this
# script manually also, and the .pdl with the app paths 2 run, should
# also have an 'on/off' option incase i want it to stop auto running."
#
# 2026-08-11: legacy tp_taskbar.c retired (archived to
# *.monads/*.livedesk-taskbar/ops/LEGACY-ARCHIVE-20260811.zip, originals
# deleted). `run` below is UNCHANGED — it still just triggers
# crypt_autostart against autostart.pdl, whose tool-bar LAUNCH row now
# points at the real khtpm binaries — so "button.sh run" already does the
# right thing with no logic change needed here. The prior "button_khtpm.sh"
# (a narrower, khtpm-only script written earlier the same session before
# autostart.pdl itself was updated) is retired — its one genuinely useful
# piece (a harder, guaranteed-clean kill-everything-then-relaunch, for
# when the normal autostart sweep isn't enough) is folded in below as
# `reset`. Direct instruction: "we should still use button r to run
# things. so that should be renamed [to be the canonical button.sh]...
# eadd that stuff [the on/off/compile/status/install-xdg subcommands]."
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ACTION="${1:-help}"
PDL="$SCRIPT_DIR/autostart.pdl"
BIN="$SCRIPT_DIR/ops/+x/crypt_autostart.+x"
RESTORE="$SCRIPT_DIR/restore-list.txt"
HOUSE="$(cd "$SCRIPT_DIR/.." && pwd)"
TB_DIR="$HOUSE/*.monads/*.livedesk-taskbar/ops"
KHTPM_PARSER="$TB_DIR/+x/khtpm_strip_parser.+x"

# Single shared kill pattern for every action (quit/reset/status) - was
# duplicated three times with khtpm_hq_render present in some spots and
# missing in others (drift). One source of truth. Includes both the _rgb
# entity binary (the real one on Linux) and the bare name (legacy safety).
KHTPM_PAT="khtpm_strip_parser\.\+x|khtpm_taskbar_manager_main\.\+x|khtpm_hq_render\.\+x|tp_desktop_window_rgb\.\+x|tp_desktop_window\.\+x"
khtpm_pids() { pgrep -f "$KHTPM_PAT" 2>/dev/null; }

read_restore_mode() {
    awk -F'|' '
        $1 ~ /^STATE[[:space:]]*$/ {
            gsub(/[[:space:]]+/, "", $2);
            gsub(/[[:space:]]+/, "", $3);
            if ($2 == "restore-last-open") { print $3; found=1; exit }
        }
        END { if (!found) print "0" }
    ' "$PDL"
}

case "$ACTION" in
    run|r|start|restart)
        # restart == run: use the restore feature only when explicitly enabled in autostart.pdl
        if [ "$(read_restore_mode)" = "1" ] && [ -f "$RESTORE" ] && [ -x "$SCRIPT_DIR/scrypts/openall/run.sh" ]; then
            "$SCRIPT_DIR/scrypts/openall/run.sh"
        else
            mkdir -p "$SCRIPT_DIR/ops/+x"
            [ -x "$BIN" ] || gcc -Wall -O2 -o "$BIN" "$SCRIPT_DIR/ops/crypt_autostart.c"
            "$BIN" "$PDL"
        fi
        ;;
    quit|close)
        # Kill all running toolbars and entities (no relaunch)
        khtpm_pids | xargs -r kill -TERM
        sleep 1
        khtpm_pids | xargs -r kill -KILL 2>/dev/null || true
        echo "closed all toolbars and entities"
        ;;
    reset)
        # Guaranteed-clean kill-everything-then-relaunch — for when the
        # normal autostart sweep (crypt_autostart's own /proc scan, which
        # only matches known taskbar/entity process names) isn't enough,
        # e.g. a genuinely stuck/orphaned process. Rebuilds khtpm fresh,
        # then delegates the actual launch to crypt_autostart against
        # autostart.pdl — same single source of truth as `run` (the pdl
        # LAUNCH rows own the tool-bar AND all entity paths, no hardcoded
        # entity list duplicated here).
        khtpm_pids | xargs -r kill -TERM
        sleep 1
        khtpm_pids | xargs -r kill -KILL 2>/dev/null || true
        [ -x "$TB_DIR/build_khtpm_strip.sh" ] && sh "$TB_DIR/build_khtpm_strip.sh"
        [ -x "$KHTPM_PARSER" ] || { echo "MISSING $KHTPM_PARSER (build failed?)"; exit 1; }
        mkdir -p "$SCRIPT_DIR/ops/+x"
        [ -x "$BIN" ] || gcc -Wall -O2 -o "$BIN" "$SCRIPT_DIR/ops/crypt_autostart.c"
        DISPLAY="${DISPLAY:-:0}" "$BIN" "$PDL"
        sleep 2
        echo "--- status ---"
        pgrep -af "$KHTPM_PAT" 2>/dev/null
        ;;
    on)
        sed -i 's/^STATE        | enabled              | 0/STATE        | enabled              | 1/' "$PDL"
        echo "autostart: ON"
        ;;
    off)
        sed -i 's/^STATE        | enabled              | 1/STATE        | enabled              | 0/' "$PDL"
        echo "autostart: OFF"
        ;;
    status)
        grep "enabled" "$PDL"
        pgrep -af "$KHTPM_PAT" 2>/dev/null
        ;;
    compile|c|build)
        mkdir -p "$SCRIPT_DIR/ops/+x"
        gcc -Wall -O2 -o "$BIN" "$SCRIPT_DIR/ops/crypt_autostart.c" && echo "OK crypt_autostart" || echo "FAIL crypt_autostart"
        ;;
    check)
        [ -x "$BIN" ] && echo "OK $BIN" || echo "MISSING $BIN"
        [ -f "$PDL" ] && echo "OK $PDL" || echo "MISSING $PDL"
        ;;
    install-xdg)
        mkdir -p "$HOME/.config/autostart"
        cat > "$HOME/.config/autostart/muchiverse-autostart.desktop" << EOF
[Desktop Entry]
Type=Application
Name=Muchiverse Autostart
Exec=$SCRIPT_DIR/button.sh run
X-GNOME-Autostart-enabled=true
EOF
        echo "installed: $HOME/.config/autostart/muchiverse-autostart.desktop"
        ;;
    help|h|-h|--help|*)
        cat <<EOF
\$.crypts — house-wide autostart control

  sh button.sh run            # quit current livedesk, then mount+launch (autostart.pdl)
  sh button.sh restart        # same as run (clean restart for $ shortcut / focus tests)
  sh button.sh quit | close   # kill all running toolbars and entities (no relaunch)
  sh button.sh reset          # harder: guaranteed kill-everything + rebuild + relaunch via autostart.pdl
  sh button.sh on | off       # toggle STATE|enabled in autostart.pdl
  sh button.sh status         # show current enabled state + running processes
  sh button.sh compile        # rebuild ops/+x/crypt_autostart.+x
  sh button.sh check          # verify binary + pdl exist
  sh button.sh install-xdg    # install the real XDG autostart .desktop file
                               # (real login-time autostart - one-time setup)
EOF
        ;;
esac
