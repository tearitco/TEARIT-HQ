/* update_history_cursor - persist the history read cursor to player_app
 * state.txt so it survives a process restart. Called after each
 * read_history in main_loop.pal to avoid replaying the entire history
 * on restart (e.g. after a crash).
 * Reads the current cursor from state.txt, increments it by 1 (one keycode
 * read per call), and writes it back. */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {
	char *cwd = getcwd(NULL, 0);
	if (!cwd) return 1;

	char path[4096];
	snprintf(path, sizeof(path), "%s/pieces/apps/player_app/state.txt", cwd);
	free(cwd);

	/* Read current cursor value */
	FILE *f = fopen(path, "r");
	int cursor = 0;
	if (f) {
		char line[256];
		while (fgets(line, sizeof(line), f)) {
			if (sscanf(line, "history_cursor=%d", &cursor) == 1) {
				break;
			}
		}
		fclose(f);
	}

	/* Increment and write back */
	cursor++;
	f = fopen(path, "w");
	if (!f) return 1;

	fprintf(f, "history_cursor=%d\n", cursor);
	fclose(f);
	return 0;
}
