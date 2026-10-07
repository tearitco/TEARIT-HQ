#!/bin/bash
# button.sh - $.crypts real house-wide autostart control.
# Direct instruction: "i want the ability thru button.sh to run this
# script manually also, and the .pdl with the app paths 2 run, should
# also have an 'on/off' option incase i want it to stop auto running."
#
# 2026-08-11: legacy tp_taskbar.c retired (archived to
# _.monads/_.livedesk-taskbar/ops/LEGACY-ARCHIVE-20260811.zip, originals
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
TB_DIR="$HOUSE/_.monads/_.livedesk-taskbar/ops"
# REAL FIX 2026-09-01 - khtpm_strip_parser.+x retired as a separate
# binary (folded verbatim into khtpm_core_render.c as strip_main(),
# phase 1) and this build-sanity check was never updated to match -
# stale since that merge, always checking for a binary that no longer
# gets built by build_khtpm_strip.sh at all.
KHTPM_PARSER="$TB_DIR/+x/khtpm_core_render.+x"

# Single shared kill pattern for every action (quit/reset/status) - was
# duplicated three times with khtpm_hq_render present in some spots and
# missing in others (drift). One source of truth. Includes both the _rgb
# entity binary (the real one on Linux) and the bare name (legacy safety).
# REAL FIX 2026-09-01 - khtpm_strip_parser.+x AND tp_desktop_window_rgb.+x
# both retired as separate binaries, folded verbatim into
# khtpm_core_render.c (strip_main()/tp_main() - phase 1/phase 2 of this
# house's own consolidation). khtpm_core_render added; the two retired
# names kept, harmless, in case an old build is somehow still running
# mid-transition.
# 2026-10-05: khtpm_entity.+x added - entity windows run that binary (split out of
# khtpm_core_render 2026-09-27); without it quit/reset left every entity running.
KHTPM_PAT="khtpm_core_render\.\+x|khtpm_strip_parser\.\+x|khtpm_taskbar_manager_main\.\+x|khtpm_hq_render\.\+x|khtpm_entity\.\+x|tp_desktop_window_rgb\.\+x|tp_desktop_window\.\+x"
khtpm_pids() { pgrep -f "$KHTPM_PAT" 2>/dev/null; }

# REAL FIX 2026-09-23, direct live report ("reset should be killing
# x11-hq windows as well"): khtpm_pids() above already covers every
# x11-hq window's own RENDERER (they all share khtpm_core_render.+x,
# confirmed live - chat-hai/network-browser/co-lab-hai/etc. all launch
# it, matching this pattern already). The real gap is each app's own
# MANAGER child (colab_hai_manager.+x, network_browser_manager.+x, every
# other &.hq-apps/*/+x/*_manager.+x) - a separate binary, tied to its
# renderer's lifetime only through the renderer's own graceful
# window-close path. quit/reset here bypass that (a raw external
# kill -TERM/-KILL on the renderer, not a real window-close event), so
# those managers were silently left running/orphaned - exactly what
# happened to Cursword's own entity process earlier tonight, same real
# shape. Scoped generically by PATH (any live process whose cmdline
# runs a real +x/ binary from under this house's own &.hq-apps/), not a
# hardcoded per-app binary-name list - matches this house's own stated
# preference (see reset's own comment below: "no hardcoded entity list
# duplicated here") and needs zero edits when a new HQ app is added.
hq_app_manager_pids() {
    for p in /proc/[0-9]*; do
        pid="${p#/proc/}"
        [ -r "$p/cmdline" ] || continue
        args="$(tr '\0' ' ' < "$p/cmdline" 2>/dev/null)"
        [ -z "$args" ] && continue
        case "$args" in
            *"$HOUSE/"*"&.hq-apps/"*"/+x/"*) echo "$pid" ;;
        esac
    done
}
all_khtpm_and_hq_pids() { { khtpm_pids; hq_app_manager_pids; } 2>/dev/null | sort -u; }

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
        sh "$SCRIPT_DIR/livedesk-icon-refresh.sh" </dev/null >/dev/null 2>&1 &   # icon follows the current theme colors
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
        all_khtpm_and_hq_pids | xargs -r kill -TERM
        sleep 1
        # REAL, MERGED 2026-09-28: all_khtpm_and_hq_pids() (ee6afba47,
        # main) generically catches every HQ app manager compiled to
        # .../ops/+x/*.+x (colab_hai_manager, network_browser_manager,
        # etc.) by path pattern - but NOT world_manager's own persistent
        # loop, since that's `prisc+x` interpreting world_manager.pal,
        # and the binary is literally named "prisc+x" (no "/+x/"
        # directory segment in its path for the generic pattern to
        # match). Both kills are needed for full coverage; neither
        # alone is a superset of the other.
        all_khtpm_and_hq_pids | xargs -r kill -KILL 2>/dev/null || true
        [ -x "$HOUSE/&.hq-apps/world-manager/button.sh" ] && \
            "$HOUSE/&.hq-apps/world-manager/button.sh" kill 2>/dev/null || true
        echo "closed all toolbars, entities, HQ app managers, and world_manager"
        ;;
    build|rebuild)
        # Compile EVERY house program (compile-runner.sh: each project's own
        # build script, with the +x output folders recreated first). Use after a
        # wipe, a fresh clone or a branch switch - compiled programs are in no git
        # branch, so windows come up empty until this has run. An optional 2nd
        # argument limits it to scripts whose path contains that text, e.g.
        #   sh button.sh build board-viewer
        shift
        DISPLAY="${DISPLAY:-:0}" nice -n 15 bash "$SCRIPT_DIR/compile-runner.sh" "$@"
        ;;
    reset)
        sh "$SCRIPT_DIR/livedesk-icon-refresh.sh" </dev/null >/dev/null 2>&1 &   # icon follows the current theme colors
        # Guaranteed-clean kill-everything-then-relaunch — for when the
        # normal autostart sweep (crypt_autostart's own /proc scan, which
        # only matches known taskbar/entity process names) isn't enough,
        # e.g. a genuinely stuck/orphaned process. Rebuilds khtpm fresh,
        # then delegates the actual launch to crypt_autostart against
        # autostart.pdl — same single source of truth as `run` (the pdl
        # LAUNCH rows own the tool-bar AND all entity paths, no hardcoded
        # entity list duplicated here).
        all_khtpm_and_hq_pids | xargs -r kill -TERM
        sleep 1
        all_khtpm_and_hq_pids | xargs -r kill -KILL 2>/dev/null || true
        # world_manager's own prisc+x loop isn't caught by the generic
        # scan above (see the `quit|close` case's own comment) - killed
        # explicitly here too.
        [ -x "$HOUSE/&.hq-apps/world-manager/button.sh" ] && \
            "$HOUSE/&.hq-apps/world-manager/button.sh" kill 2>/dev/null || true
        # REAL FIX 2026-09-21, direct instruction ("i dont want it to run
        # the old binaries if theres a compile fail or it may mislead me
        # into thinking things are ok, when they aren't"): this used to
        # run build_khtpm_strip.sh and ignore its exit status, then only
        # check the binary EXISTS - which a STALE binary from a prior
        # successful build also satisfies, so a compile failure here
        # silently relaunched old code with no sign anything was wrong.
        # We already just killed every running process above, so on a
        # build failure leave the desktop DOWN and say so, the same
        # already-proven pattern run_khtpm_strip.sh's own `new` mode uses
        # (`|| { echo "BUILD FAILED — not launching"; exit 1; }`) - never
        # fall through to launching whatever binary happens to exist.
        if [ -x "$TB_DIR/build_khtpm_strip.sh" ]; then
            # REAL FIX 2026-09-21, direct follow-up ("yes, log it"): a
            # reset triggered from the desktop runs with no visible
            # terminal, so a plain "BUILD FAILED" message had nowhere
            # real to point to. `tee` to a durable log next to the
            # build's own +x/ output; exit code captured via a temp
            # file since dash has no `set -o pipefail` (a pipeline's
            # own $? would reflect `tee`, not the build).
            _blog="$TB_DIR/+x/build_error.log"
            _brc="$(mktemp 2>/dev/null || echo "/tmp/khtpm_build_rc.$$")"
            mkdir -p "$TB_DIR/+x"
            { sh "$TB_DIR/build_khtpm_strip.sh"; echo $? > "$_brc"; } 2>&1 | tee "$_blog"
            _brc_val="$(cat "$_brc" 2>/dev/null || echo 1)"
            rm -f "$_brc"
            [ "$_brc_val" = 0 ] || { echo "BUILD FAILED — desktop left stopped, not relaunched with a stale binary (full output: $_blog)"; exit 1; }
        fi
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
        # the desktop start button (start-temp -> this ELF). Had no build
        # automation, went stale across the strip refactor.
        gcc -Wall -O2 -o "$SCRIPT_DIR/ops/livedesk-start-button" "$SCRIPT_DIR/ops/livedesk-start-button.c" \
            && echo "OK livedesk-start-button" || echo "FAIL livedesk-start-button"
        ;;
    check)
        [ -x "$BIN" ] && echo "OK $BIN" || echo "MISSING $BIN"
        [ -f "$PDL" ] && echo "OK $PDL" || echo "MISSING $PDL"
        ;;
    install-app)
        # A normal Linux app + desktop launcher for livedesk (replaces the bare start-temp ELF): PNG icon, Terminal=false, shows in
        # the app grid and on the Desktop. Generated for THIS checkout's location, never committed (no absolute paths in git);
        # re-run after moving the checkout.
        _icon_dir="$HOME/.local/share/icons/hicolor/256x256/apps"
        _df="$HOME/.local/share/applications/livedesk.desktop"
        mkdir -p "$_icon_dir" "$HOME/.local/share/applications"
        # the icon is drawn from the CURRENT livedesk_theme.pdl colors (re-run install-app after changing the theme);
        # no Pillow -> the shipped livedesk-icon-256.png
        python3 "$SCRIPT_DIR/livedesk-icon-gen.py" "$HOUSE" "$_icon_dir/livedesk.png" 256 \
            || cp "$SCRIPT_DIR/livedesk-icon-256.png" "$_icon_dir/livedesk.png"
        # Exec must be quoted, with $ ` " \ escaped (the house folder is literally named "$.crypts"; the keyfile also doubles the backslash)
        _exec="$(printf '%s' "$SCRIPT_DIR/livedesk-launch.sh" | sed 's/[$`"\\]/\\\\&/g')"
        printf '%s\n' '[Desktop Entry]' 'Version=1.0' 'Type=Application' 'Name=Livedesk' \
            'Comment=Start the livedesk desktop (taskbar, entities, autostart)' \
            "Exec=\"$_exec\"" "Icon=$_icon_dir/livedesk.png" \
            'Terminal=false' 'StartupNotify=false' 'Categories=Utility;' > "$_df"
        chmod +x "$_df"
        echo "installed app entry: $_df"
        if [ -d "$HOME/Desktop" ]; then
            cp "$_df" "$HOME/Desktop/Livedesk.desktop"; chmod +x "$HOME/Desktop/Livedesk.desktop"
            command -v gio >/dev/null 2>&1 && gio set "$HOME/Desktop/Livedesk.desktop" metadata::trusted true 2>/dev/null
            echo "installed desktop launcher: $HOME/Desktop/Livedesk.desktop (right-click > Allow Launching if it shows a gear icon)"
        fi
        command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$HOME/.local/share/applications" 2>/dev/null
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
    install-desktop)
        # REAL, NEW 2026-10-07: pre-compile Ubuntu app-library shortcut.
        # No build needed (pure shell + a text SVG icon), so this runs on
        # a fresh clone before anything is compiled. Reads the user's own
        # color prefs (#.desktop/livedesk_theme.pdl COLOR bg/fg - the same
        # rows the color picker writes) and bakes them into the icon, so
        # the launcher in the GNOME Activities/apps grid matches the desk.
        # Writes $HOME/.local/share/applications/<name>.desktop (the real
        # per-user "apps lib") with Exec pointing at this script's own
        # `run` action. Usage: sh button.sh install-desktop [name]
        NAME="${2:-muchiverse}"
        THEME="$HOUSE/#.desktop/livedesk_theme.pdl"
        BG="#1c1c1c"; FG="#cccccc"
        if [ -f "$THEME" ]; then
            V="$(sed -n 's/^COLOR[ |]*bg[ |]*|//p' "$THEME" | head -1 | tr -d ' |')"
            [ -n "$V" ] && BG="$V"
            V="$(sed -n 's/^COLOR[ |]*fg[ |]*|//p' "$THEME" | head -1 | tr -d ' |')"
            [ -n "$V" ] && FG="$V"
        fi
        APPS="$HOME/.local/share/applications"
        ICONS="$HOME/.local/share/icons"
        mkdir -p "$APPS" "$ICONS"
        # .desktop Exec reserves `$` (our own `$.crypts` dir trips it) -
        # escape as `\$`; validated with desktop-file-validate on Debian.
        ESCAPED_DIR="$(printf '%s' "$SCRIPT_DIR" | sed 's/\$/\\$/g')"
        ICON="$ICONS/$NAME.svg"
        cat > "$ICON" << EOF
<svg xmlns="http://www.w3.org/2000/svg" width="128" height="128" viewBox="0 0 128 128">
<rect x="4" y="4" width="120" height="120" rx="24" fill="$BG" stroke="$FG" stroke-width="6"/>
<text x="64" y="88" font-family="sans-serif" font-size="72" font-weight="bold" text-anchor="middle" fill="$FG">M</text>
</svg>
EOF
        cat > "$APPS/$NAME.desktop" << EOF
[Desktop Entry]
Type=Application
Name=Muchiverse
Comment=Launch the Muchiverse livedesk (house theme $BG/$FG)
Exec=sh "$ESCAPED_DIR/button.sh" run
Icon=$ICON
Terminal=false
Categories=Utility;
EOF
        command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$APPS" >/dev/null 2>&1 || true
        if command -v desktop-file-validate >/dev/null 2>&1; then
            desktop-file-validate "$APPS/$NAME.desktop" && echo "desktop file valid."
        fi
        echo "installed: $APPS/$NAME.desktop (icon $ICON, theme $BG/$FG)"
        ;;
    help|h|-h|--help|*)
        cat <<EOF
\$.crypts — house-wide autostart control

  sh button.sh run            # quit current livedesk, then mount+launch (autostart.pdl)
  sh button.sh restart        # same as run (clean restart for $ shortcut / focus tests)
  sh button.sh quit | close   # kill all running toolbars and entities (no relaunch)
  sh button.sh build [text]   # compile every house program (after a wipe/clone/branch switch); text filters by path
  sh button.sh reset          # harder: guaranteed kill-everything + rebuild + relaunch via autostart.pdl
  sh button.sh on | off       # toggle STATE|enabled in autostart.pdl
  sh button.sh status         # show current enabled state + running processes
  sh button.sh compile        # rebuild ops/+x/crypt_autostart.+x
  sh button.sh check          # verify binary + pdl exist
  sh button.sh install-app    # Linux app + Desktop launcher 'Livedesk' with a PNG icon (runs livedesk-launch.sh)
  sh button.sh install-xdg    # install the real XDG autostart .desktop file
                               # (real login-time autostart - one-time setup)
  sh button.sh install-desktop [name]  # themed app-grid shortcut in
                               # ~/.local/share/applications (pre-compile safe)
EOF
        ;;
esac
