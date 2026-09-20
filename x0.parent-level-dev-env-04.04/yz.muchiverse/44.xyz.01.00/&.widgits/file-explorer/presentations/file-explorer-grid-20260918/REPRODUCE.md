# Reproduce file-explorer grid PNG proof (2026-09-18)

House: `44.xyz.01.00/`. Relay only. `'p'` is KEY_PRESSED 112.

```
HOUSE=<44.xyz.01.00>
FE="$HOUSE/&.widgits/file-explorer"
sh "$FE/button.sh" run
# wait until one khtpm_core_render ... file-explorer-pal.xhtpm
PID=$(pgrep -n -f 'khtpm_core_render[.][+]x .*file-explorer-pal[.]xhtpm')
H="$HOUSE/#.desktop/entity_menu_history/${PID}.txt"
# dump list
printf '%s\n' 'KEY_PRESSED: 112' >> "$H"
# Grid View was nav 13 on the 2026-09-18 list dump — READ THE LIVE
# dump/frame before copying these digits next time
sleep 1
printf '%s\n' 'KEY_PRESSED: 49' >> "$H"   # 1
sleep 0.4
printf '%s\n' 'KEY_PRESSED: 51' >> "$H"   # 3
sleep 0.8
printf '%s\n' 'KEY_PRESSED: 13' >> "$H"   # Enter
sleep 1
printf '%s\n' 'KEY_PRESSED: 112' >> "$H"
```

Outputs: `/tmp/entity-menu-frame.png` plus `.png.receipt.txt` and
`.png.frame.txt`. Copy into `snapshots/` if you want a dated keep.

Check `file_explorer_ui.txt`: `is_grid_view=1` before trusting the PNG.
