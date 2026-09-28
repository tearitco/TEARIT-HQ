#!/bin/sh
# build_khtpm_strip.sh — build the two-process strip architecture
# (khtpm_taskbar_manager_main.c manager driver + khtpm_strip_parser.c
# outer parser), per khtpm-strip-parser-design.md.
#
# PRODUCTION BINARY NAMES (2026-08-11): legacy tp_taskbar.c has been
# retired (archived to
# *.monads/*.livedesk-taskbar/ops/LEGACY-ARCHIVE-20260811.zip, originals
# deleted — direct instruction: "id like to deprecate the old toolbar
# system now"). This khtpm pair is now the real, only taskbar — dropped
# the "_test" suffix these binaries carried through the whole build-out
# session, now that there's no live legacy binary left to avoid
# clobbering.
set -e
cd "$(dirname "$0")"
mkdir -p +x

# ── freshness gate ─────────────────────────────────────────────────────
# REAL FIX 2026-09-28 (direct instruction: "extend gate and not ignore
# it" - EVENT-MODULARITY-AND-BUILD-SPEED.md §2). The old gate here
# (2026-09-09) was a single, coarse, ALL-OR-NOTHING mtime check: if
# anything under this dir or the shared-lib was newer than the two main
# binaries, the ENTIRE script recompiled every binary it builds,
# unconditionally - and `run_khtpm_strip.sh new` set KHTPM_FORCE_BUILD=1
# specifically to bypass even that, because mtime alone can't tell "a
# real edit" from "a touch/checkout with no real content change," so a
# dev asking for a real rebuild had no finer-grained way to get one.
#
# hash_gate.sh (§2, already proven: 24x speedup, content-hash not
# mtime-based) removes that whole tension: every individual compile
# below now gates on its OWN real source content, per binary, so a
# real edit to one file rebuilds exactly the binaries that depend on
# it and nothing else - no coarse whole-script bail-out needed, and
# nothing for `new` to need to force past any more. `run_khtpm_strip.sh`
# no longer sets KHTPM_FORCE_BUILD=1 for this reason (see its own
# updated comment). This script now always runs to completion; each
# hash_gate_stale check below is what makes a no-op run fast again.
SHARED_DIR="$(cd "$(dirname "$0")/../../../&.widgits/_shared-lib" 2>/dev/null && pwd || echo /nonexistent)"
MANIFEST="$(dirname "$0")/.build_hashes.pdl"
. "$SHARED_DIR/hash_gate.sh"

# ── build-failure marker (2026-09-21) ─────────────────────────────────
# Direct instruction: "i actually dont want it to run the old binaries
# if theres a compile fail... could [alert] go [in the splash]." A
# dead-man's-switch marker, not an ERR trap (this is a plain #!/bin/sh
# script - dash has no `trap ERR`): written BEFORE the real build starts,
# removed on the LAST line of this file (reached only if every command
# above it succeeded, since `set -e` aborts immediately on the first
# failure). If the script exits for ANY reason - a failed compile, a
# killed process - without reaching that last line, the marker is still
# there. livedesk_splash.c polls for it and switches to a persistent
# "BUILD FAILED" state instead of quietly closing; the EXIT trap below
# checks the SAME file to decide whether it's safe to kill the splash.
mkdir -p +x
echo "BUILD IN PROGRESS - build_khtpm_strip.sh started $(date +%H:%M:%S) and has not finished yet. If this file still exists after the build should be done, it FAILED partway (check the terminal/log for the real gcc error) - the splash/caller must not treat a stale binary as success." > +x/.build_failed.txt

# ── "Building livedesk…" splash ───────────────────────────────────────
# Only when invoked from the desktop start button / $.restart (which
# export LIVEDESK_START_SPLASH=1). REAL, NEW 2026-09-28: since the old
# coarse whole-script freshness gate (which used to `exit 0` before ever
# reaching here on a no-op run) is gone in favor of per-binary hash
# gating below, a fully-up-to-date run now shows this splash too - but
# only very briefly, since every hash_gate_stale check below returns
# false immediately and the script finishes in a fraction of a second.
# Traded a near-instant flash for correctness (a real edit always
# rebuilds exactly what changed, with no force flag needed anywhere) -
# not treated as worth the complexity of a separate stale-precheck.
#
# 2026-09-09, direct instruction ("the popup should be x11 layout style,
# not gl ... watching compile is most accurate"): a house-style X11
# window (livedesk_splash.c) that themes off livedesk_theme.pdl and
# fills a real progress bar as each build output binary lands in +x/.
# zenity/xmessage stay only as the fallback for the first-ever build
# (before livedesk_splash.+x itself exists) or a headless box.
if [ -n "${LIVEDESK_START_SPLASH:-}" ]; then
    _HOUSE_DIR="$(cd ../../.. 2>/dev/null && pwd || echo .)"
    _XDIR="$(pwd)/+x"
    # build the splash binary itself first (tiny, ~1s, X11+Xft only) so
    # it exists for THIS build and every later one.
    if [ ! -x "+x/livedesk_splash.+x" ]; then
        _sx="$(pkg-config --cflags --libs x11 xft 2>/dev/null)"
        [ -n "$_sx" ] || _sx="-I/usr/include/freetype2 -lX11 -lXft"
        "${CC:-gcc}" -std=c11 -O2 -o "+x/livedesk_splash.+x" livedesk_splash.c $_sx >/dev/null 2>&1 || true
    fi
    if [ -x "+x/livedesk_splash.+x" ] && [ -n "${DISPLAY:-}" ]; then
        "+x/livedesk_splash.+x" "$_HOUSE_DIR" "$_XDIR" >/dev/null 2>&1 &
        _splash_pid=$!
        _real_splash=1
    elif command -v xmessage >/dev/null 2>&1; then
        xmessage -center -timeout 120 "Building livedesk…  (~30s)" >/dev/null 2>&1 &
        _splash_pid=$!
    elif command -v zenity >/dev/null 2>&1; then
        zenity --info --width=320 --timeout=180 --title="livedesk" \
               --text="Building livedesk…  (~30s the first time)" >/dev/null 2>&1 &
        _splash_pid=$!
    fi
    # Only the real X11 splash (livedesk_splash.+x) knows how to poll
    # +x/.build_failed.txt and switch to a "BUILD FAILED" state that
    # waits for a real dismissal - so it alone is left running on
    # failure. xmessage/zenity can't be told anything after they're
    # launched, so a failure there is no worse than before (their own
    # --timeout still closes them; the terminal/log has the real error).
    if [ -n "${_splash_pid:-}" ]; then
        if [ "${_real_splash:-}" = 1 ]; then
            trap 'if [ -f "+x/.build_failed.txt" ]; then :; else kill "$_splash_pid" 2>/dev/null || true; fi' EXIT INT TERM
        else
            trap 'kill "$_splash_pid" 2>/dev/null || true' EXIT INT TERM
        fi
    fi
fi

set -e
CC=${CC:-gcc}
CFLAGS="-std=c11 -Wall -O2"

# macOS leg (2026-08-22): XQuartz owns X11/Xft under /opt/X11 — brew's
# pkg-config doesn't search there by default, and clang won't find
# Xlib headers/libs without explicit -I/-L. Prebuilt emoji-helper
# binaries are Linux ELF, so on Darwin they build from wsr-pal source
# instead of being copied. Guarded: on Linux every added var stays
# empty and behavior is unchanged.
OS_TYPE="$(uname -s)"
X11_FLAGS=""
FT_CFLAGS=""
FT_LIBS=""
if [ "$OS_TYPE" = "Darwin" ]; then
    PKG_CONFIG_PATH="/opt/X11/lib/pkgconfig:/usr/local/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
    export PKG_CONFIG_PATH
    X11_FLAGS="-I/opt/X11/include -L/opt/X11/lib"
    if command -v pkg-config >/dev/null 2>&1; then
        FT_CFLAGS=$(pkg-config --cflags freetype2)
        FT_LIBS=$(pkg-config --libs freetype2)
    fi
fi

# SHARED-SOURCE-COMPILE-IN-PLACE.md (2026-09-09): the two `cp
# "$SHARED"/khtpm_css_parser.{c,h}` lines here were vestigial — the
# manager-driver compile below uses neither, and the renderer is built
# by build_core_render.sh (which now compiles the canonical in place
# via -I). Nothing in this script needs a local copy any more.
SHARED="$(cd "$(dirname "$0")/../../../&.widgits/_shared-lib" && pwd)"

# -I "$SHARED": khtpm_taskbar_manager.c now #includes kh_proc_registry.h
# (PROC-LIFECYCLE-ORCHESTRATOR-TEARDOWN.md). Hash-gated (2026-09-28,
# "extend gate and not ignore it") - also depends on kh_proc_registry.h
# itself, listed as a real input so a change there correctly rebuilds
# this binary too (same fan-out handling -I'd shared files already get
# in build_core_render.sh).
MGR_SRCS="khtpm_taskbar_manager_main.c khtpm_taskbar_manager.c $SHARED/kh_proc_registry.h"
if hash_gate_stale "$MANIFEST" +x/khtpm_taskbar_manager_main.+x $MGR_SRCS; then
    echo "-- khtpm manager driver (pure logic, no Xlib) -> +x/khtpm_taskbar_manager_main.+x"
    $CC $CFLAGS -I "$SHARED" -o +x/khtpm_taskbar_manager_main.+x \
      khtpm_taskbar_manager_main.c khtpm_taskbar_manager.c
    hash_gate_commit "$MANIFEST" +x/khtpm_taskbar_manager_main.+x $MGR_SRCS
else
    echo "-- khtpm_taskbar_manager_main.+x up to date (hash unchanged), skipping compile"
fi

# REAL FIX 2026-09-01 - khtpm_strip_parser.+x AND tp_desktop_window_rgb.+x
# both retired as separate binaries. khtpm_strip_parser.c/khtpm_strip_
# layout.c/.h/khtpm_strip_codes.h (phase 1) and tp_desktop_window_rgb.c
# (phase 2, including its own real 3D raymarch/phymoji engine) were
# folded verbatim into khtpm_core_render.c as two new modes
# (strip_main()/tp_main(), dispatched on argc==2 - see that file's own
# big merged-block comments). All 5 source files above have been
# deleted (real house-standard consolidation: no cross-.c linking to
# share behavior within one binary, and no dead source left lying
# around once nothing compiles or launches it) - see git history if
# the old, pre-consolidation versions are ever needed again.
# run_khtpm_strip.sh launches +x/khtpm_core_render.+x (built by
# build_core_render.sh, invoked below) for both roles now.
echo "-- shared khtpm_core_render.+x (now includes strip mode + entity/tile mode) -> +x/khtpm_core_render.+x"
sh build_core_render.sh

if hash_gate_stale "$MANIFEST" +x/tp_asset_to_sprite.+x tp_asset_to_sprite.c; then
    echo "-- emoji->sprite helper tp_asset_to_sprite.c -> +x/tp_asset_to_sprite.+x"
    $CC $CFLAGS -o +x/tp_asset_to_sprite.+x tp_asset_to_sprite.c -lm
    hash_gate_commit "$MANIFEST" +x/tp_asset_to_sprite.+x tp_asset_to_sprite.c
else
    echo "-- tp_asset_to_sprite.+x up to date (hash unchanged), skipping compile"
fi

echo "-- emoji atlas helpers emoji_gen_atlas/emoji_xtract (copied from wsr-pal)"
# emoji_gen_atlas.+x + emoji_xtract.+x ship as prebuilt binaries from the
# 014.wsr-pal toolchain (same source every other widget copies them from)
# - the entity calls them via ops_dir at runtime, so they must sit in the
# same +x/ dir as the entity binary.
WSR="$(cd "$(dirname "$0")/../../../014.wsr-pal💸️📌️+2" 2>/dev/null && pwd)"
for t in emoji_gen_atlas emoji_xtract; do
    if [ ! -x "+x/$t.+x" ]; then
        if [ "$OS_TYPE" = "Darwin" ]; then
            # macOS: prebuilt copies are Linux ELF — compile from source.
            # emoji_gen_atlas needs freetype; emoji_xtract doesn't.
            if [ -n "$WSR" ] && [ -f "$WSR/ops/$t.c" ]; then
                if $CC $CFLAGS -I"$WSR/ops" $FT_CFLAGS -o "+x/$t.+x" "$WSR/ops/$t.c" $FT_LIBS -lm 2>/dev/null; then
                    echo "    $t.+x built from wsr-pal source"
                else
                    echo "WARN: $t.+x failed to build from source"
                fi
            else
                echo "WARN: +x/$t.+x missing (no wsr-pal source to build)"
            fi
        elif [ -n "$WSR" ] && [ -x "$WSR/ops/+x/$t.+x" ]; then
            cp "$WSR/ops/+x/$t.+x" "+x/$t.+x"
            chmod +x "+x/$t.+x"
            echo "    $t.+x copied from wsr-pal"
        else
            echo "WARN: +x/$t.+x missing (wsr-pal copy unavailable)"
        fi
    fi
done

echo "-- real, shared sprite-driven phymoji generator (build in shared, copy binary)"
# Direct instruction 2026-08-30 ("make a script to do phymoji of all
# entities. save it locally in shared. and all new entities will use
# it as well") - one real compiled binary, built from the shared
# source, copied locally same as x11_mirror.+x/emoji_gen_atlas.+x
# already are - tp_desktop_window_rgb.c's own real load_entity_phymoji()
# shells out to this copy at runtime (ops_dir-relative, same real
# lookup pattern apply_asset_override() already uses).
if [ -f "$SHARED/ops/build_sprite_phymoji_gen.sh" ]; then
    (cd "$SHARED/ops" && bash build_sprite_phymoji_gen.sh >/dev/null 2>&1) || echo "WARN: sprite_phymoji_gen.+x failed to build"
    if [ -x "$SHARED/ops/+x/sprite_phymoji_gen.+x" ]; then
        cp "$SHARED/ops/+x/sprite_phymoji_gen.+x" "+x/sprite_phymoji_gen.+x"
        chmod +x "+x/sprite_phymoji_gen.+x"
    fi
fi

# 2026-08-18: taskbar's terminal ASCII mirror (HQ menu "cli" row) - two
# binaries, matching TPMOS's real renderer.c/keyboard_input.c split
# (never combined - see khtpm_strip_render_ascii.c's own header comment
# for the real \r\n/staircase bug this split fixes).
if hash_gate_stale "$MANIFEST" +x/khtpm_strip_render_ascii.+x khtpm_strip_render_ascii.c; then
    echo "-- taskbar ASCII renderer (no termios) -> +x/khtpm_strip_render_ascii.+x"
    $CC $CFLAGS -o +x/khtpm_strip_render_ascii.+x khtpm_strip_render_ascii.c
    hash_gate_commit "$MANIFEST" +x/khtpm_strip_render_ascii.+x khtpm_strip_render_ascii.c
else
    echo "-- khtpm_strip_render_ascii.+x up to date (hash unchanged), skipping compile"
fi

if hash_gate_stale "$MANIFEST" +x/khtpm_strip_keyboard_ascii.+x khtpm_strip_keyboard_ascii.c; then
    echo "-- taskbar ASCII keyboard input (raw termios only, never prints) -> +x/khtpm_strip_keyboard_ascii.+x"
    $CC $CFLAGS -o +x/khtpm_strip_keyboard_ascii.+x khtpm_strip_keyboard_ascii.c
    hash_gate_commit "$MANIFEST" +x/khtpm_strip_keyboard_ascii.+x khtpm_strip_keyboard_ascii.c
else
    echo "-- khtpm_strip_keyboard_ascii.+x up to date (hash unchanged), skipping compile"
fi

# 2026-09-06: GENERIC (any-window) siblings of the two above -
# TERMINAL-MIRROR-PARITY-all-windows.md steps 2 & 3. Same renderer/
# keyboard split, path templated on a target PID.
if hash_gate_stale "$MANIFEST" +x/khtpm_render_ascii.+x khtpm_render_ascii.c; then
    echo "-- generic window ASCII presenter -> +x/khtpm_render_ascii.+x"
    $CC $CFLAGS -o +x/khtpm_render_ascii.+x khtpm_render_ascii.c
    hash_gate_commit "$MANIFEST" +x/khtpm_render_ascii.+x khtpm_render_ascii.c
else
    echo "-- khtpm_render_ascii.+x up to date (hash unchanged), skipping compile"
fi
if hash_gate_stale "$MANIFEST" +x/khtpm_kbd_ascii.+x khtpm_kbd_ascii.c; then
    echo "-- generic window ASCII keyboard relay -> +x/khtpm_kbd_ascii.+x"
    $CC $CFLAGS -o +x/khtpm_kbd_ascii.+x khtpm_kbd_ascii.c
    hash_gate_commit "$MANIFEST" +x/khtpm_kbd_ascii.+x khtpm_kbd_ascii.c
else
    echo "-- khtpm_kbd_ascii.+x up to date (hash unchanged), skipping compile"
fi

if hash_gate_stale "$MANIFEST" +x/livedesk_splash.+x livedesk_splash.c; then
    echo "-- build/restart splash (X11) -> +x/livedesk_splash.+x"
    _sx="$(pkg-config --cflags --libs x11 xft 2>/dev/null)"
    [ -n "$_sx" ] || _sx="-I/usr/include/freetype2 -lX11 -lXft"
    $CC $CFLAGS -o +x/livedesk_splash.+x livedesk_splash.c $_sx
    hash_gate_commit "$MANIFEST" +x/livedesk_splash.+x livedesk_splash.c
else
    echo "-- livedesk_splash.+x up to date (hash unchanged), skipping compile"
fi

# 2026-09-15: house-wide joystick arrow input, v1 (JOYSTICK-INPUT-
# HOUSE-WIDE-DESIGN.md). Standalone, no X11 deps - reads the raw Linux
# joystick API and appends to a shared relay file every window's own
# poll_agent_history() already tail-polls.
if hash_gate_stale "$MANIFEST" +x/khtpm_joystick_daemon.+x khtpm_joystick_daemon.c; then
    echo "-- house-wide joystick arrow-input daemon -> +x/khtpm_joystick_daemon.+x"
    $CC $CFLAGS -o +x/khtpm_joystick_daemon.+x khtpm_joystick_daemon.c
    hash_gate_commit "$MANIFEST" +x/khtpm_joystick_daemon.+x khtpm_joystick_daemon.c
else
    echo "-- khtpm_joystick_daemon.+x up to date (hash unchanged), skipping compile"
fi

echo "OK +x/khtpm_taskbar_manager_main.+x and +x/khtpm_core_render.+x (strip mode + entity/tile mode, plus helpers)"
# Every real step above succeeded (set -e would have aborted otherwise) -
# clear the build-failure marker so the splash/caller know it's real.
rm -f +x/.build_failed.txt
