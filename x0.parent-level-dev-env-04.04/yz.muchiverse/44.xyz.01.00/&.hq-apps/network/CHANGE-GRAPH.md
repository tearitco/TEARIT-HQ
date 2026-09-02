# Change graph vs github.com/tearitco/co-work (main zip, 2026-09-02)
Compared GitHub `tearitco/co-work` main to the live house copies we actually ran.
Parser: `khtpm_css_parser.c` and `khtpm_chtpm_loader.c` were **not** edited.
No `network_browser_render.c`. No `g_is_network_browser`.

## Modified (existed on GitHub)
| GitHub path | +lines | -lines | what changed |
|---|---:|---:|---|
| `network-browser-demo/&.hq-apps/network/network_browser_manager.c` | 1111 | 55 | History/Back/Reload/Bookmark, Duktape JS op, article-body extract, img/video sprites, 4chan catalog.json, toolbar/address/status chtpm, sprite-grid-row wrap, Home. |
| `network-browser-demo/&.hq-apps/network/network-browser-hq.css` | 12 | 9 | Window 960x640, toolbar/address/status/content/sidebar colors, solid fill. |
| `network-browser-demo/&.hq-apps/network/ops/nb_write_go.sh` | 0 | 0 | Check if path still has ops/ and house_root argv. |
| `network-browser-demo/*.monads/*.livedesk-taskbar/ops/khtpm_core_render.c` | 211 | 17 | from-top scroll, sprite row height, sprite-grid wrap, toolbar row, cli_io class=top, CSS window/sidebar size, MAX_ELEMS, opacity default 1.0. Generic, no per-app flag. |
| `khtpm-core/khtpm_draw_core.c` | 34 | 4 | Label under tall 64px sprite. Nav [ ]N. badges stay visible. |
| `khtpm-core/khtpm_css_parser.c` | 0 | 0 | (expect unchanged or tiny) |
| `khtpm-core/khtpm_render_core.c` | 1 | 1 | MAX_CHILDREN raised 64 to 256. |

## Added (not on GitHub co-work)
| path | why |
|---|---|
| `&.hq-apps/network/ops/nb_write_back.sh` | Back history |
| `&.hq-apps/network/ops/nb_write_reload.sh` | Reload current URL |
| `&.hq-apps/network/ops/nb_write_bookmark.sh` | Append bookmark table |
| `&.hq-apps/network/ops/nb_media_to_sprite.c` | JPEG/PNG/GIF/WEBP to 64px sprite.csv (stb_image), C11 |
| `&.hq-apps/network/ops/nb_js_eval.c` | Isolated Duktape eval op, 3s timeout, window/self/globalThis |
| notes .md files | notes on every fix |
| `#.desktop/livedesk_theme.pdl` | COLOR|opacity|1.00 so the window is solid |

## Unchanged
- `khtpm_css_parser.c` identical to GitHub khtpm-core
- `khtpm_chtpm_loader.c` untouched
- No second renderer. Shared khtpm_core_render still only reads .chtpm.
