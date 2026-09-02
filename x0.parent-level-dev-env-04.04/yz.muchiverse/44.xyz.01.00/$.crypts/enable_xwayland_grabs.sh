#!/bin/bash
# enable_xwayland_grabs.sh - real fix, 2026-08-06, direct report
# ("keystrokes went INTO the terminal" instead of the popup that just
# opened). Root cause: this is a Wayland session (GNOME Shell/Mutter),
# and every popup here is an X11 window running under XWayland. Mutter
# deliberately restricts XGrabKeyboard from XWayland clients by default
# (org.gnome.mutter.wayland xwayland-allow-grabs = false) - a real,
# intentional Wayland security policy, not a bug in tp_desktop_window.c's
# own grab code. No amount of retrying XGrabKeyboard can override this;
# it has to be allowlisted at the compositor level.
#
# xwayland-grab-access-rules matches by WM_CLASS (see
# `gsettings describe org.gnome.mutter.wayland xwayland-grab-access-rules`)
# - every popup/text-popup window this house creates now sets a real
# WM_CLASS of "MuchiverseLivedesk" (tp_desktop_window.c's own
# open_context_menu()/SHOW_TEXT_FILE handler), so allowlisting that one
# class covers every popup, house-wide, without opening the door to
# every other X11 app on the system (that's the difference from just
# flipping the global xwayland-allow-grabs switch on).
#
# Re-runnable any time - direct instruction ("lets make a script to do
# that so we could always run it if needed"), e.g. after a GNOME
# update/reset wipes the setting, or on a fresh account.

CLASS="MuchiverseLivedesk"

echo "Current xwayland-grab-access-rules: $(gsettings get org.gnome.mutter.wayland xwayland-grab-access-rules)"

gsettings set org.gnome.mutter.wayland xwayland-grab-access-rules "['$CLASS']"

echo "Set to: $(gsettings get org.gnome.mutter.wayland xwayland-grab-access-rules)"
echo "(global xwayland-allow-grabs left untouched: $(gsettings get org.gnome.mutter.wayland xwayland-allow-grabs) - not needed, access-rules allowlists '$CLASS' directly)"
