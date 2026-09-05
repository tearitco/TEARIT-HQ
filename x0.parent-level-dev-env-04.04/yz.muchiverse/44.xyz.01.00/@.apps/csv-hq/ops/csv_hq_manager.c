/*
 * csv_hq_manager.c - CSV spreadsheet editor manager process
 *
 * Stateful manager for a simple 2D grid editor. Owns the grid data, handles load/save,
 * cell operations, and spreadsheet functions (SUM/AVG/MIN/MAX/COUNT). Publishes UI
 * state to a plain-text file read by a separate renderer, polls an action file for
 * commands, and monitors File Explorer for file picks.
 *
 * argv[1] = house_root (absolute path to app house root)
 * argv[2] = package_dir (absolute path to this app's directory: .../csv-hq)
 * argv[3] = unused (module extra-arg slot, required but ignored)
 *
 * Data model: 2D grid of cells, each cell a plain string, no quoted-CSV support (v1).
 * Max 26 columns (A-Z, single letter), 500 rows, 256 bytes per cell.
 */

#define _BSD_SOURCE
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <float.h>

#define MAX_ROWS 500
#define MAX_COLS 26
#define MAX_CELL_LEN 256
#define MAX_PATH_LEN 4096
#define MAX_STATUS_LEN 256
#define MAX_FUNC_RESULT_LEN 256

/* Grid data and state */
static char grid[MAX_ROWS][MAX_COLS][MAX_CELL_LEN];
static int n_rows = 0;
static int n_cols_used = 1;
static char file_path[MAX_PATH_LEN] = "";
static char status[MAX_STATUS_LEN] = "Ready.";
static char func_result[MAX_FUNC_RESULT_LEN] = "";
static char last_fe_result[MAX_PATH_LEN] = "";

/* Paths */
static char g_house_root[MAX_PATH_LEN] = "";
static char g_package_dir[MAX_PATH_LEN] = "";

/* Track last action sequence number processed */
static int last_seq = 0;

/*
 * Parse a cell reference like "B3" or "b12".
 * Returns 1 on success, 0 on invalid reference.
 * On success, *col_out is 0-25 (A-Z) and *row_out is 0-indexed.
 */
static int parse_cell_ref(const char *ref, int *col_out, int *row_out) {
    const char *p = ref;

    /* Skip leading whitespace */
    while (*p && isspace((unsigned char)*p)) p++;

    /* First char must be a letter */
    if (!*p || !isalpha((unsigned char)*p)) return 0;

    int col = toupper((unsigned char)*p) - 'A';
    if (col < 0 || col >= MAX_COLS) return 0;
    p++;

    /* Rest must be digits */
    if (!*p || !isdigit((unsigned char)*p)) return 0;

    char *endptr;
    long row_num = strtol(p, &endptr, 10);
    if (*endptr != '\0') return 0;
    if (row_num < 1 || row_num > MAX_ROWS) return 0;

    *col_out = col;
    *row_out = (int)(row_num - 1);
    return 1;
}

/*
 * Read a single key=value line from a file.
 * key should include trailing "=" (e.g., "result_action=").
 * Stores value in out (null-terminated), up to outsz-1 chars.
 * Sets out[0]='\0' if file not found or key not found.
 */
static void read_kv_line(const char *path, const char *key, char *out, size_t outsz) {
    FILE *f = fopen(path, "r");
    if (!f) {
        out[0] = '\0';
        return;
    }

    char line[2048];
    size_t keylen = strlen(key);

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, keylen) == 0) {
            /* Found it; extract value and strip \r\n */
            const char *val = line + keylen;
            size_t vlen = strlen(val);
            while (vlen > 0 && (val[vlen-1] == '\r' || val[vlen-1] == '\n')) {
                vlen--;
            }

            if (vlen >= outsz) vlen = outsz - 1;
            strncpy(out, val, vlen);
            out[vlen] = '\0';
            fclose(f);
            return;
        }
    }

    out[0] = '\0';
    fclose(f);
}

/*
 * Write the current grid state to the UI file.
 * This is called after every state change.
 */
/* REAL, NEW 2026-09-05 (08-roadmap/design-docs/GRID-ELEMENT-DESIGN.md,
 * step 5 - the real <grid> element replacing the old cellref/cellval/
 * Set-Cell trio, direct live request: "it doesn't allow user editing
 * the cell live in place like a real spreadsheet"). The grid element
 * itself now draws all the header/row-number/border chrome and reads
 * per-cell content on demand via a var-name convention
 * (cell_<row>_<col>, matching the "cell_" prefix csv-hq-pal.xhtpm's
 * own <grid target_id="cell_"> declares) - this manager's only real
 * job is to publish n_rows (how many real rows the grid should size
 * itself for) and one cell_R_C var per published cell. Always publish
 * at least DISPLAY_MIN_ROWS so a fresh/empty sheet still sizes as a
 * real spreadsheet, not a 1-row sliver (same real reason the earlier
 * formatted-scrolllist version needed this, direct live request: "i
 * expect to see a grid... even if no file opened"). */
#define DISPLAY_MIN_ROWS 12
#define DISPLAY_MAX_COLS 26

static void write_ui_file(void) {
    char ui_path[MAX_PATH_LEN];
    snprintf(ui_path, sizeof(ui_path), "%s/csv_hq_ui.txt", g_package_dir);

    FILE *f = fopen(ui_path, "w");
    if (!f) return;

    fprintf(f, "file_path=%s\n", file_path);
    fprintf(f, "status=%s\n", status);
    fprintf(f, "func_result=%s\n", func_result);

    /* Display cap: only publish min(n_rows, 200) real data rows - a
     * deliberate v1 display cap, not a bug. Always show at least
     * DISPLAY_MIN_ROWS so an empty sheet still reads as a real
     * spreadsheet, not a sliver. */
    int real_rows = n_rows > 200 ? 200 : n_rows;
    int show_rows = real_rows > DISPLAY_MIN_ROWS ? real_rows : DISPLAY_MIN_ROWS;
    int show_cols = n_cols_used > DISPLAY_MAX_COLS ? DISPLAY_MAX_COLS : n_cols_used;
    if (show_cols < 1) show_cols = 1;

    fprintf(f, "n_rows=%d\n", show_rows);

    for (int i = 0; i < show_rows; i++) {
        for (int j = 0; j < show_cols; j++) {
            const char *cell = (i < n_rows && j < n_cols_used) ? grid[i][j] : "";
            fprintf(f, "cell_%d_%d=%s\n", i, j, cell);
        }
    }

    fclose(f);
}

/*
 * Clear the grid and reset state (for NEW command).
 */
static void clear_grid(void) {
    n_rows = 0;
    n_cols_used = 1;
    file_path[0] = '\0';
    grid[0][0][0] = '\0';
    snprintf(status, sizeof(status), "New sheet.");
    func_result[0] = '\0';
}

/*
 * Handle SAVE command: write grid to CSV file.
 */
static void handle_save(void) {
    if (file_path[0] == '\0') {
        snprintf(status, sizeof(status),
                 "No file open — Open a file first (Save As not built in v1).");
        write_ui_file();
        return;
    }

    FILE *f = fopen(file_path, "wb");
    if (!f) {
        snprintf(status, sizeof(status), "Save failed: %s", file_path);
        write_ui_file();
        return;
    }

    for (int i = 0; i < n_rows; i++) {
        for (int j = 0; j < n_cols_used; j++) {
            if (j > 0) fprintf(f, ",");
            fprintf(f, "%s", grid[i][j]);
        }
        fprintf(f, "\n");
    }

    fclose(f);
    snprintf(status, sizeof(status), "Saved: %s", file_path);
    write_ui_file();
}

/*
 * Handle SETCELL:<ref> command: set a single cell value.
 * The value comes from csv_setcell_buffer.txt written by the frontend.
 */
static void handle_setcell(const char *ref_str) {
    int col, row;
    if (!parse_cell_ref(ref_str, &col, &row)) {
        snprintf(func_result, sizeof(func_result), "Bad cell ref.");
        write_ui_file();
        return;
    }

    if (row >= MAX_ROWS || col >= MAX_COLS) {
        snprintf(func_result, sizeof(func_result),
                 "Cell out of range (max 26 cols A-Z, 500 rows).");
        write_ui_file();
        return;
    }

    /* Read cell value from buffer file written by frontend */
    char buffer_path[MAX_PATH_LEN];
    snprintf(buffer_path, sizeof(buffer_path), "%s/csv_setcell_buffer.txt", g_package_dir);

    FILE *f = fopen(buffer_path, "r");
    char cell_value[MAX_CELL_LEN] = "";
    if (f) {
        if (fgets(cell_value, sizeof(cell_value), f)) {
            /* Strip trailing \r\n */
            size_t len = strlen(cell_value);
            while (len > 0 && (cell_value[len-1] == '\r' || cell_value[len-1] == '\n')) {
                cell_value[--len] = '\0';
            }
        }
        fclose(f);
    }

    /* Copy into grid */
    size_t clen = strlen(cell_value);
    size_t cpylen = clen < MAX_CELL_LEN ? clen : MAX_CELL_LEN - 1;
    memcpy(grid[row][col], cell_value, cpylen);
    grid[row][col][cpylen] = '\0';

    /* Update grid bounds */
    if (row + 1 > n_rows) n_rows = row + 1;
    if (col + 1 > n_cols_used) n_cols_used = col + 1;

    snprintf(status, sizeof(status), "Set %c%d.", 'A' + col, row + 1);
    func_result[0] = '\0';
    write_ui_file();
}

/*
 * Handle FUNC:<name>:<colletter> command.
 * Compute SUM, AVG, MIN, MAX, or COUNT for a column.
 */
static void handle_func(const char *func_spec) {
    /* Parse "SUM:B" format */
    char *colon = strchr(func_spec, ':');
    if (!colon) {
        snprintf(func_result, sizeof(func_result), "Bad function syntax.");
        write_ui_file();
        return;
    }

    size_t namelen = colon - func_spec;
    char func_name[32] = "";
    if (namelen >= sizeof(func_name)) namelen = sizeof(func_name) - 1;
    strncpy(func_name, func_spec, namelen);
    func_name[namelen] = '\0';

    /* Uppercase function name */
    for (char *p = func_name; *p; p++) *p = toupper((unsigned char)*p);

    /* Parse and validate column letter */
    const char *col_str = colon + 1;
    while (*col_str && isspace((unsigned char)*col_str)) col_str++;

    if (!*col_str || !isalpha((unsigned char)*col_str)) {
        snprintf(func_result, sizeof(func_result), "Bad column letter.");
        write_ui_file();
        return;
    }

    int col = toupper((unsigned char)*col_str) - 'A';
    col_str++;

    /* Check that only whitespace remains (no junk after column letter) */
    while (*col_str && isspace((unsigned char)*col_str)) col_str++;
    if (*col_str) {
        snprintf(func_result, sizeof(func_result), "Bad column letter.");
        write_ui_file();
        return;
    }

    if (col >= n_cols_used) {
        snprintf(func_result, sizeof(func_result), "Bad column letter.");
        write_ui_file();
        return;
    }

    /* Compute statistics for this column */
    double sum = 0.0;
    double min_val = DBL_MAX;
    double max_val = -DBL_MAX;
    int count = 0;

    for (int i = 0; i < n_rows; i++) {
        char *endptr;
        double v = strtod(grid[i][col], &endptr);

        /* Treat only fully-numeric non-empty cells as numeric */
        int is_numeric = (endptr != grid[i][col] && *endptr == '\0' &&
                          grid[i][col][0] != '\0');

        if (is_numeric) {
            sum += v;
            if (v < min_val) min_val = v;
            if (v > max_val) max_val = v;
            count++;
        }
    }

    /* Generate result string based on function */
    if (strcmp(func_name, "SUM") == 0) {
        snprintf(func_result, sizeof(func_result), "SUM(%c) = %.2f", 'A' + col, sum);
    } else if (strcmp(func_name, "AVG") == 0) {
        if (count == 0) {
            snprintf(func_result, sizeof(func_result), "AVG(%c): no numeric cells", 'A' + col);
        } else {
            snprintf(func_result, sizeof(func_result), "AVG(%c) = %.2f", 'A' + col, sum / count);
        }
    } else if (strcmp(func_name, "MIN") == 0) {
        if (count == 0) {
            snprintf(func_result, sizeof(func_result), "MIN(%c): no numeric cells", 'A' + col);
        } else {
            snprintf(func_result, sizeof(func_result), "MIN(%c) = %.2f", 'A' + col, min_val);
        }
    } else if (strcmp(func_name, "MAX") == 0) {
        if (count == 0) {
            snprintf(func_result, sizeof(func_result), "MAX(%c): no numeric cells", 'A' + col);
        } else {
            snprintf(func_result, sizeof(func_result), "MAX(%c) = %.2f", 'A' + col, max_val);
        }
    } else if (strcmp(func_name, "COUNT") == 0) {
        snprintf(func_result, sizeof(func_result), "COUNT(%c) = %d", 'A' + col, count);
    } else {
        snprintf(func_result, sizeof(func_result), "Unknown function.");
    }

    write_ui_file();
}

/*
 * Handle File Explorer file picks.
 * Reads file_explorer_ui.txt and loads selected files.
 */
static void handle_fe_pick(void) {
    char fe_ui_path[MAX_PATH_LEN];
    snprintf(fe_ui_path, sizeof(fe_ui_path),
             "%s/&.widgits/file-explorer/file_explorer_ui.txt", g_house_root);

    char result_action[256] = "";
    char result[MAX_PATH_LEN] = "";

    read_kv_line(fe_ui_path, "result_action=", result_action, sizeof(result_action));
    read_kv_line(fe_ui_path, "result=", result, sizeof(result));

    /* Only act on LOAD action, non-empty path, and new paths */
    if (strcmp(result_action, "LOAD") != 0) return;
    if (result[0] == '\0') return;
    if (strcmp(result, last_fe_result) == 0) return;

    /* Track this file so we don't re-load it */
    snprintf(last_fe_result, sizeof(last_fe_result), "%s", result);

    /* Try to open and load the CSV file */
    FILE *f = fopen(result, "r");
    if (!f) {
        snprintf(status, sizeof(status), "Failed to open: %s", result);
        write_ui_file();
        return;
    }

    /* Reset grid */
    n_rows = 0;
    n_cols_used = 1;

    /* Read file line by line, parse CSV */
    char line[2048];
    while (fgets(line, sizeof(line), f) && n_rows < MAX_ROWS) {
        /* Strip trailing \r\n */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\r' || line[len-1] == '\n')) {
            line[--len] = '\0';
        }

        /* Split line on commas */
        int col = 0;
        const char *field_start = line;

        for (const char *p = line; *p && col < MAX_COLS; p++) {
            if (*p == ',') {
                size_t field_len = p - field_start;
                size_t cpylen = field_len < MAX_CELL_LEN ? field_len : MAX_CELL_LEN - 1;
                memcpy(grid[n_rows][col], field_start, cpylen);
                grid[n_rows][col][cpylen] = '\0';
                col++;
                field_start = p + 1;
            }
        }

        /* Copy last field */
        if (col < MAX_COLS) {
            size_t field_len = strlen(field_start);
            size_t cpylen = field_len < MAX_CELL_LEN ? field_len : MAX_CELL_LEN - 1;
            memcpy(grid[n_rows][col], field_start, cpylen);
            grid[n_rows][col][cpylen] = '\0';
            col++;
        }

        if (col > n_cols_used) n_cols_used = col;
        n_rows++;
    }

    fclose(f);
    snprintf(file_path, sizeof(file_path), "%s", result);
    snprintf(status, sizeof(status), "Opened: %s", result);
    func_result[0] = '\0';
    write_ui_file();
}

/*
 * Poll and process action commands from csv_hq_action.txt.
 * Dispatches to appropriate handler based on command type.
 */
static void process_actions(void) {
    char action_path[MAX_PATH_LEN];
    snprintf(action_path, sizeof(action_path), "%s/csv_hq_action.txt", g_package_dir);

    char line[2048];
    FILE *f = fopen(action_path, "r");
    if (!f) return;

    int seq = 0;
    char cmd[2048] = "";

    /* Parse seq= and cmd= lines */
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "seq=", 4) == 0) {
            seq = atoi(line + 4);
        } else if (strncmp(line, "cmd=", 4) == 0) {
            snprintf(cmd, sizeof(cmd), "%s", line + 4);
            /* Strip trailing \r\n */
            size_t len = strlen(cmd);
            while (len > 0 && (cmd[len-1] == '\r' || cmd[len-1] == '\n')) {
                cmd[--len] = '\0';
            }
        }
    }
    fclose(f);

    /* Only process if seq is new */
    if (seq <= last_seq) return;
    last_seq = seq;

    /* Dispatch command */
    if (strcmp(cmd, "NEW") == 0) {
        clear_grid();
        write_ui_file();
    } else if (strcmp(cmd, "SAVE") == 0) {
        handle_save();
    } else if (strncmp(cmd, "SETCELL:", 8) == 0) {
        handle_setcell(cmd + 8);
    } else if (strncmp(cmd, "FUNC:", 5) == 0) {
        handle_func(cmd + 5);
    }
    /* Unknown commands are silently ignored */
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <house_root> <package_dir> <unused>\n", argv[0]);
        return 1;
    }

    /* Store paths */
    snprintf(g_house_root, sizeof(g_house_root), "%s", argv[1]);
    snprintf(g_package_dir, sizeof(g_package_dir), "%s", argv[2]);

    /* Initialize state */
    n_rows = 0;
    n_cols_used = 1;
    file_path[0] = '\0';
    snprintf(status, sizeof(status), "Ready.");
    func_result[0] = '\0';
    last_seq = 0;

    /* Create/clear action file with initial state */
    char action_path[MAX_PATH_LEN];
    snprintf(action_path, sizeof(action_path), "%s/csv_hq_action.txt", g_package_dir);
    FILE *f = fopen(action_path, "w");
    if (f) {
        fprintf(f, "seq=0\ncmd=\n");
        fclose(f);
    }

    /* Write initial UI file */
    write_ui_file();

    /* Main event loop: poll for actions and file picks */
    while (1) {
        usleep(50000);  /* 50 milliseconds */
        process_actions();
        handle_fe_pick();
    }

    return 0;
}
