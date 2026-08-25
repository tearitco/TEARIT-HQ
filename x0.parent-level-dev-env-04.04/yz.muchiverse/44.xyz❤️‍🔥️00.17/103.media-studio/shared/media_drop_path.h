/* media_drop_path.h — resolve drag-drop / clipboard file URIs for this house.
 *
 * House paths contain emoji, ZWJ, VS16, &, spaces — Nautilus sends long
 * percent-encoded file:// URLs. Callers must use PATH_MAX-sized buffers
 * (or MEDIA_PATH_MAX). Never 512-byte path stacks.
 */
#ifndef MEDIA_DROP_PATH_H
#define MEDIA_DROP_PATH_H

#include <stddef.h>

#ifndef MEDIA_PATH_MAX
#define MEDIA_PATH_MAX 4096
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Percent-decode UTF-8 %XX sequences in place. */
void media_url_decode_inplace(char *s);

/* Convert one URI/line (file://… or plain path) to a filesystem path.
 * Returns 1 on success, 0 on failure. Writes reason into err (optional). */
int media_uri_to_path(const char *uri, char *out, size_t out_sz,
                      char *err, size_t err_sz);

/* True if path exists and is a regular readable file. */
int media_path_is_readable_file(const char *path);

/* Classify: 0 unknown, 1 video, 2 audio, 3 image, 4 mesh/3d (by extension). */
int media_kind_from_path(const char *path);

/* Append a debug line (creates parent dirs best-effort). */
void media_drop_debug_log(const char *project_root, const char *line);

/* Parse text/uri-list (multi-line). Calls on_path(path, user) for each
 * resolved readable file. Returns count accepted. */
typedef void (*media_drop_on_path_fn)(const char *path, void *user);
int media_import_uri_list(const char *data, const char *project_root,
                          media_drop_on_path_fn on_path, void *user,
                          char *status, size_t status_sz);

#ifdef __cplusplus
}
#endif
#endif
