Widgets under &.widgits/<name>/

UI LAW: GL is primary. Widgets launch as run-widget (GL on, ASCII off).
Do not open a second terminal for widgets.

Roadmap: WIDGETS_ROADMAP.txt

Packages (ops-layer; full GL chrome still proposal):
  file-menu/      — FILE menu ops; focus-adaptive (editor docs | mutaclysm saves)
  tile-picker/    — PLACE_TILE on muta + **tp_place_desktop** → #.desktop/tiles/
  map-picker/     — list maps + SWITCH_MAP (hero map_id + xlector teleport)
  proc-monitor/   — register / list / focus / soft / kill / gc
  event-editor/   — like file-menu for muta: headless GL widget (ASCII optional);
                    CHTPM frame → rgb → gl_mirror; desktop packages + drop into muta

House desktop tray:
  ../#.desktop/   — portable packages (events/ entities/ tiles/ inbox/)

Harnesses:
  ../%.harnesses/file-menu+editor/
  ../%.harnesses/file-menu+mutaclysm/
  ../%.harnesses/map-picker+mutaclysm/
  ../%.harnesses/tile-picker+mutaclysm/
  ../%.harnesses/proc-monitor/
  ../%.harnesses/event-editor+desktop/

Dev status:
  file-menu/USER_REPORT.txt
  file-menu/widget+plan.txt
  GL_CHROME_WIDGETS_PROPOSAL.md  (not code)
  ../ARCHI_TEST_SUM-J28.txt §10 coding-complete stamp

Standards: ❤️.XYZOS_README.md , #.haiku+/!.xyzos-standards+1.txt §35–36
