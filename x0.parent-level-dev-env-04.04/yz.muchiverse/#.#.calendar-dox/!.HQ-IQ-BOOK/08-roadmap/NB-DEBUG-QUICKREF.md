# NETWORK-BROWSER DEBUG / AGENT QUICK REFERENCE

Files to tail when you drive the manager:

- `#.desktop/network_browser_status.state.txt` — idle/loading/ready/stopped/error
- `#.desktop/network_browser_page.state.txt` — parsed page rows (URL|, TITLE|, TEXT|, LINK|, IMG|, MEDIA|, VIDEO|)
- `#.desktop/network_browser_console.txt` — eval results / console.* capture
- `#.desktop/network-browser-hq_ui.txt` — full rendered projection fed to the xhtpm
- `&.hq-apps/network/tmp/nb_video0/video.state`, `surface.receipt.txt` — V3 playback
- `&.hq-apps/network/tmp/fetch.dom`, `tmp/fetch.html`, `tmp/page.js` — raw fetch + extracted DOM/script
- Relay (inject key events): `#.desktop/entity_menu_history/<pid>.txt`, lines like `KEY_PRESSED: 112` = dump frame, `KEY_PRESSED: 13` = Enter. `p` dumps PNG to `/tmp/entity-menu-frame.png` only via `112`.
- Request (inject go/back): `#.desktop/network_browser_request.txt`, lines `go:<url>`, `back:`, `forward:`, `reload:`.

Run a page: `printf 'go:<url>\n' > <house>/#.desktop/network_browser_request.txt`
Dump the live frame: `printf '112\n' >> <house>/#.desktop/entity_menu_history/<pid>.txt` then read `/tmp/entity-menu-frame.png` + `.txt`.

Fixtures: `&.hq-apps/network/tests/fixtures/`, snapshot harness
`&.hq-apps/network/tests/nb_layout_test.sh` (NB_SNAPSHOT_UPDATE=1 to
record).
