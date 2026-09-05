/*
 * text_edit_manager.c
 *
 * Manager process for text-edit-hq plain-text editor app.
 * Owns file_path, content_buffer, and status; publishes to text_edit_ui.txt.
 * Polls text_edit_action.txt for commands (NEW, SAVE, SAVEAS) and
 * file-explorer widget for file picks (LOAD via result_action).
 * No GUI; runs as a background state manager.
 */

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PATH_MAX_LOCAL 4096
#define CONTENT_BUFFER_SIZE 65536
#define ACTION_BUFFER_SIZE 2048
#define STATUS_SIZE 256

static char house_root[PATH_MAX_LOCAL];
static char package_dir[PATH_MAX_LOCAL];
static char file_path[PATH_MAX_LOCAL];
static char content_buffer[CONTENT_BUFFER_SIZE];
static int content_size = 0;
static char status[STATUS_SIZE];
static char last_fe_result[PATH_MAX_LOCAL];
/* REAL, NEW 2026-09-05 - Save As no longer uses a typed <cli_io> path
 * (removed from the sidebar, direct live request). SAVEAS_ARM sets
 * this; the next File Explorer pick is then treated as a save TARGET
 * (write the buffer to it) rather than a file to load. */
static int save_as_armed = 0;

/*
 * write_escaped_content: Write buffer to file with escaping.
 * Replaces \n with \n (two chars), \\ with \\\\ (two chars), others as-is.
 */
static void write_escaped_content(FILE *f, const char *buf, int size) {
	for (int i = 0; i < size; i++) {
		switch (buf[i]) {
			case '\n':
				fputc('\\', f);
				fputc('n', f);
				break;
			case '\\':
				fputc('\\', f);
				fputc('\\', f);
				break;
			default:
				fputc(buf[i], f);
				break;
		}
	}
}

/*
 * write_ui_file: Write current state to text_edit_ui.txt.
 * Format: file_path=<path>\nstatus=<status>\ncontent=<escaped>\n
 */
static void write_ui_file(void) {
	char ui_path[PATH_MAX_LOCAL];
	snprintf(ui_path, sizeof(ui_path), "%s/text_edit_ui.txt", package_dir);

	FILE *f = fopen(ui_path, "w");
	if (!f) return;

	fprintf(f, "file_path=%s\n", file_path);
	fprintf(f, "status=%s\n", status);
	fprintf(f, "content=");
	write_escaped_content(f, content_buffer, content_size);
	fprintf(f, "\n");

	fclose(f);
}

/*
 * read_kv_line: Read a key=value line from a file.
 * key includes the trailing "=". Returns empty string if not found or file missing.
 * Strips trailing \r\n from the value.
 */
static void read_kv_line(const char *path, const char *key, char *out, size_t outsz) {
	out[0] = '\0';

	FILE *f = fopen(path, "r");
	if (!f) return;

	size_t keylen = strlen(key);
	char line[2048];

	while (fgets(line, sizeof(line), f)) {
		if (strncmp(line, key, keylen) == 0) {
			const char *val = line + keylen;
			size_t vlen = strlen(val);

			/* Strip trailing \r and \n */
			if (vlen > 0 && val[vlen - 1] == '\n') vlen--;
			if (vlen > 0 && val[vlen - 1] == '\r') vlen--;

			if (vlen >= outsz) vlen = outsz - 1;
			strncpy(out, val, vlen);
			out[vlen] = '\0';

			fclose(f);
			return;
		}
	}

	fclose(f);
}

/*
 * handle_action: Dispatch on command from action file.
 * Commands: NEW, SAVE, SAVEAS:<path>
 */
static void handle_action(const char *cmd) {
	if (strcmp(cmd, "NEW") == 0) {
		content_buffer[0] = '\0';
		content_size = 0;
		file_path[0] = '\0';
		strncpy(status, "New file.", sizeof(status) - 1);
		status[sizeof(status) - 1] = '\0';
		write_ui_file();
	}
	else if (strcmp(cmd, "SAVE") == 0) {
		if (file_path[0] == '\0') {
			strncpy(status, "No file yet — use Save As (or Open) first.", sizeof(status) - 1);
			status[sizeof(status) - 1] = '\0';
		} else {
			char save_buffer_path[PATH_MAX_LOCAL];
			snprintf(save_buffer_path, sizeof(save_buffer_path), "%s/text_edit_save_buffer.txt", package_dir);

			/* Read from save_buffer (raw bytes, no unescaping) */
			FILE *src = fopen(save_buffer_path, "rb");
			if (!src) {
				snprintf(status, sizeof(status), "Save failed: %s", file_path);
				write_ui_file();
				return;
			}

			int bytes_read = fread(content_buffer, 1, sizeof(content_buffer) - 1, src);
			fclose(src);
			content_buffer[bytes_read] = '\0';
			content_size = bytes_read;

			/* Write to file_path */
			FILE *dst = fopen(file_path, "wb");
			if (!dst) {
				snprintf(status, sizeof(status), "Save failed: %s", file_path);
				write_ui_file();
				return;
			}

			fwrite(content_buffer, 1, content_size, dst);
			fclose(dst);

			snprintf(status, sizeof(status), "Saved: %s", file_path);
		}
		write_ui_file();
	}
	else if (strcmp(cmd, "SAVEAS_ARM") == 0) {
		save_as_armed = 1;
		strncpy(status, "Save As: pick a location in the File Explorer window.", sizeof(status) - 1);
		status[sizeof(status) - 1] = '\0';
		write_ui_file();
	}
	else if (strncmp(cmd, "SAVEAS:", 7) == 0) {
		const char *path = cmd + 7;

		if (path[0] == '\0') {
			strncpy(status, "Save As needs a path — type one first.", sizeof(status) - 1);
			status[sizeof(status) - 1] = '\0';
		} else {
			char save_buffer_path[PATH_MAX_LOCAL];
			snprintf(save_buffer_path, sizeof(save_buffer_path), "%s/text_edit_save_buffer.txt", package_dir);

			/* Read from save_buffer (raw bytes, no unescaping) */
			FILE *src = fopen(save_buffer_path, "rb");
			if (!src) {
				snprintf(status, sizeof(status), "Save failed: %s", path);
				write_ui_file();
				return;
			}

			int bytes_read = fread(content_buffer, 1, sizeof(content_buffer) - 1, src);
			fclose(src);
			content_buffer[bytes_read] = '\0';
			content_size = bytes_read;

			/* Write to path */
			FILE *dst = fopen(path, "wb");
			if (!dst) {
				snprintf(status, sizeof(status), "Save failed: %s", path);
				write_ui_file();
				return;
			}

			fwrite(content_buffer, 1, content_size, dst);
			fclose(dst);

			/* Update file_path for subsequent plain Save commands */
			strncpy(file_path, path, sizeof(file_path) - 1);
			file_path[sizeof(file_path) - 1] = '\0';

			snprintf(status, sizeof(status), "Saved as: %s", path);
		}
		write_ui_file();
	}
	/* Unknown command: ignore */
}

int main(int argc, char *argv[]) {
	if (argc != 4) {
		fprintf(stderr, "Usage: %s <house_root> <package_dir> <unused>\n", argv[0]);
		return 1;
	}

	/* Store argv */
	strncpy(house_root, argv[1], sizeof(house_root) - 1);
	house_root[sizeof(house_root) - 1] = '\0';

	strncpy(package_dir, argv[2], sizeof(package_dir) - 1);
	package_dir[sizeof(package_dir) - 1] = '\0';

	/* Initialize state */
	content_buffer[0] = '\0';
	content_size = 0;
	file_path[0] = '\0';
	last_fe_result[0] = '\0';
	strncpy(status, "Ready.", sizeof(status) - 1);
	status[sizeof(status) - 1] = '\0';

	/* Create/clear action file */
	char action_path[PATH_MAX_LOCAL];
	snprintf(action_path, sizeof(action_path), "%s/text_edit_action.txt", package_dir);

	FILE *f = fopen(action_path, "w");
	if (f) {
		fprintf(f, "seq=0\ncmd=\n");
		fclose(f);
	}

	/* Write initial UI file */
	write_ui_file();

	/* Main loop */
	static int last_seq = 0;

	while (1) {
		usleep(50000); /* 50ms */

		/* Poll action file */
		char action_buffer[ACTION_BUFFER_SIZE];
		read_kv_line(action_path, "seq=", action_buffer, sizeof(action_buffer));
		int seq = atoi(action_buffer);

		if (seq > last_seq) {
			last_seq = seq;

			read_kv_line(action_path, "cmd=", action_buffer, sizeof(action_buffer));
			handle_action(action_buffer);
		}

		/* Poll File Explorer widget */
		char fe_ui_path[PATH_MAX_LOCAL];
		snprintf(fe_ui_path, sizeof(fe_ui_path), "%s/&.widgits/file-explorer/file_explorer_ui.txt", house_root);

		char result_action[256];
		read_kv_line(fe_ui_path, "result_action=", result_action, sizeof(result_action));

		if (strcmp(result_action, "LOAD") == 0) {
			char result[PATH_MAX_LOCAL];
			read_kv_line(fe_ui_path, "result=", result, sizeof(result));

			if (result[0] != '\0' && strcmp(result, last_fe_result) != 0) {
				/* New file pick from File Explorer */
				strncpy(last_fe_result, result, sizeof(last_fe_result) - 1);
				last_fe_result[sizeof(last_fe_result) - 1] = '\0';

				if (save_as_armed) {
					/* Save As: the pick is a save TARGET, not a file to
					 * load. Write the current buffer (dumped to
					 * text_edit_save_buffer.txt by TXT_SAVEAS) to it. */
					save_as_armed = 0;
					char save_buffer_path[PATH_MAX_LOCAL];
					snprintf(save_buffer_path, sizeof(save_buffer_path), "%s/text_edit_save_buffer.txt", package_dir);
					FILE *src = fopen(save_buffer_path, "rb");
					if (src) {
						int n = fread(content_buffer, 1, sizeof(content_buffer) - 1, src);
						fclose(src);
						content_buffer[n] = '\0';
						content_size = n;
					}
					FILE *dst = fopen(result, "wb");
					if (dst) {
						fwrite(content_buffer, 1, content_size, dst);
						fclose(dst);
						strncpy(file_path, result, sizeof(file_path) - 1);
						file_path[sizeof(file_path) - 1] = '\0';
						snprintf(status, sizeof(status), "Saved as: %s", result);
					} else {
						snprintf(status, sizeof(status), "Save As failed: %s", result);
					}
					write_ui_file();
				} else {
					FILE *file = fopen(result, "rb");
					if (file) {
						int bytes_read = fread(content_buffer, 1, sizeof(content_buffer) - 1, file);
						fclose(file);
						content_buffer[bytes_read] = '\0';
						content_size = bytes_read;

						strncpy(file_path, result, sizeof(file_path) - 1);
						file_path[sizeof(file_path) - 1] = '\0';

						snprintf(status, sizeof(status), "Opened: %s", result);
					} else {
						snprintf(status, sizeof(status), "Failed to open: %s", result);
					}

					write_ui_file();
				}
			}
		}
	}

	return 0;
}
