/* prisc+x.c - SHARED (yz.muchiverse/2.muchi-verse/shared-ops/, see
 * shared-ops-refactor-plan.txt for the why) - the pal VM itself, moved
 * here after confirming byte-identical copies across all 4 pal
 * projects (mutaclsym/zoo_0000/muchipal-editor-0.0/wsr-pal). Unlike
 * shared-ops/chtpm_parser_pal.c (a tracked FORK of real 1.TPMOS code we
 * deliberately keep minimal-diff against), this file is OUR OWN VM -
 * genuine fixes belong here directly, not worked around elsewhere
 * (direct user framing: "its better to change prisc+x... since prisc
 * is newer").
 *
 * REAL FIX (see chtpm-to-pal-layout-plan.txt §8's own interact+module
 * work): OP_READ_HISTORY's literal-path parsing used to capture ONLY
 * a destination register (rd), silently leaving the position/cursor
 * register (rs1) at its zero-initialized default - register 0, which
 * this VM's own main loop unconditionally resets to 0 every single
 * instruction (`regs[0] = 0`, RISC-V-style hardwired zero - confirmed
 * by reading this file's own main loop). Any literal-path
 * `read_history` call would therefore re-fseek to byte 0 and re-read
 * only the very first entry, forever - permanently broken incremental
 * reads. Fixed by adding a real 3-argument form,
 * `read_history <path> xD, xS` (path, destination register, AND a
 * genuine position register), parsed BEFORE the old 1-register
 * fallback so it only ever activates for scripts that actually use it -
 * every existing pal script's own `read_history xD, xS` (no path) is
 * untouched, byte-for-byte the same parse path as before. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <signal.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <process.h>
#include <io.h>
#define usleep(us) Sleep((us)/1000)
#define REALPATH(path, resolved) _fullpath(resolved, path, 4096)
#define SETENV(name, value, overwrite) _putenv_s(name, value)
#else
#include <unistd.h>
#include <dirent.h>
#define REALPATH(path, resolved) realpath(path, resolved)
#define SETENV(name, value, overwrite) setenv(name, value, overwrite)
#endif
#include <sys/stat.h>

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif

/* mutaclsym patch: world/map-aware piece state resolution, per
 * !.world_architecture+1=rusindol.txt sec. 3 resolution order:
 *   1. world_<id>/map_<id>/<piece_id>/state.txt (canonical, scanned)
 *   2. root/global fallback: pieces/<piece_id>/state.txt
 * Upstream prisc+x only ever did (2). Windows build keeps upstream
 * behavior (no dirent.h there) rather than growing a second codepath. */
static int resolve_piece_state_path(const char *proj_root, const char *proj_id,
                                     const char *piece_id, char *out, size_t out_sz) {
#ifndef _WIN32
    /* Headroom beyond MAX_PATH so gcc can prove these concatenations
     * (proj_root/proj_id, then a dirent name, then another dirent name
     * plus piece_id) can't truncate, instead of just happening not to. */
    char pieces_dir[MAX_PATH + 512];
    if (proj_root && proj_id)
        snprintf(pieces_dir, sizeof(pieces_dir), "%s/projects/%s/pieces", proj_root, proj_id);
    else
        snprintf(pieces_dir, sizeof(pieces_dir), "projects/template/pieces");

    DIR *worlds = opendir(pieces_dir);
    if (worlds) {
        struct dirent *w;
        while ((w = readdir(worlds)) != NULL) {
            if (strncmp(w->d_name, "world_", 6) != 0) continue;
            char maps_dir[MAX_PATH + 768];
            snprintf(maps_dir, sizeof(maps_dir), "%s/%s", pieces_dir, w->d_name);
            DIR *maps = opendir(maps_dir);
            if (!maps) continue;
            struct dirent *m;
            while ((m = readdir(maps)) != NULL) {
                if (strncmp(m->d_name, "map_", 4) != 0) continue;
                char candidate[MAX_PATH + 1024];
                /* piece_id/out_sz are caller-supplied and unbounded from
                 * gcc's static view, so -Wformat-truncation can't be
                 * fully satisfied by sizing alone here; snprintf already
                 * truncates safely (null-terminated, no overflow) in the
                 * extreme case, so the warning is suppressed narrowly
                 * rather than restructuring a vendored VM's control flow. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
                snprintf(candidate, sizeof(candidate), "%s/%s/%s/state.txt",
                         maps_dir, m->d_name, piece_id);
                struct stat st;
                if (stat(candidate, &st) == 0) {
                    closedir(maps);
                    closedir(worlds);
                    snprintf(out, out_sz, "%s", candidate);
                    return 1;
                }
#pragma GCC diagnostic pop
            }
            closedir(maps);
        }
        closedir(worlds);
    }
#endif
    /* Fallback: flat root pieces dir (matches upstream behavior). */
    if (proj_root && proj_id)
        snprintf(out, out_sz, "%s/projects/%s/pieces/%s/state.txt", proj_root, proj_id, piece_id);
    else
        snprintf(out, out_sz, "projects/template/pieces/%s/state.txt", piece_id);
    return 0;
}

void handle_sigint(int sig) {
    (void)sig;
    _exit(130);  /* Immediate exit, no cleanup */
}

#define MAX_INST 1024
#define MAX_LABELS 128
#define MAX_VARS 256
#define MAX_OPS 64
#define MAX_INCLUDES 16
#ifndef MAX_PATH
#define MAX_PATH 4096
#endif
#define MEM_SIZE 4096
#define STRING_POOL_START 0xF00

typedef enum { OP_ADDI, OP_BEQ, OP_LW, OP_SW, OP_JALR, OP_J, OP_HALT, OP_CUSTOM, OP_READ_HISTORY, OP_EXEC, OP_HIT_FRAME, OP_READ_STATE, OP_READ_ACTIVE_TARGET, OP_READ_ENV_KEY, OP_SLEEP, OP_READ_LAYOUT, OP_READ_POS, OP_ECALL } OpBase;

typedef struct {
    OpBase op;
    int rd, rs1, rs2, imm;
    char label_ref[32];
    char custom_name[32];
    char literal_arg[256];
    char literal_arg2[256];
} Inst;

typedef struct {
    char name[32];
    int addr;
} Label;

typedef struct {
    char name[32];
    int addr;
    char type[16];
    int value;
    int size;
} Variable;

typedef struct {
    char name[32];
    char type[16];
    char handler[256];
    char desc[128];
} CustomOp;

int32_t regs[16] = {0};
int32_t mem[MEM_SIZE] = {0};
Inst program[MAX_INST];
Label labels[MAX_LABELS];
Variable variables[MAX_VARS];
CustomOp custom_ops[MAX_OPS];
int label_count = 0, inst_count = 0, var_count = 0, op_count = 0;
int next_var_addr = 0;
char included_files[MAX_INCLUDES][64];
int include_count = 0;

int find_custom_op(const char *name);

void trim(char *str) {
    char *start = str, *end;
    while (isspace(*start)) start++;
    end = start + strlen(start) - 1;
    while (end > start && isspace(*end)) end--;
    end[1] = '\0';
    memmove(str, start, strlen(start) + 1);
}

int is_included(const char *path) {
    for (int i = 0; i < include_count; i++)
        if (strcmp(included_files[i], path) == 0) return 1;
    return 0;
}

void add_variable(const char *name, const char *type, const char *value_str) {
    if (var_count >= MAX_VARS) return;
    Variable *v = &variables[var_count++];
    strncpy(v->name, name, 31);
    strncpy(v->type, type, 15);
    v->addr = next_var_addr;
    
    if (strcmp(type, "int") == 0) {
        v->value = atoi(value_str);
        v->size = 1;
        mem[next_var_addr++] = v->value;
    } else if (strcmp(type, "char") == 0 || strcmp(type, "string") == 0) {
        char *p = strchr(value_str, '"');
        if (p) {
            p++;
            char *end = strchr(p, '"');
            if (end) *end = '\0';
            v->value = STRING_POOL_START + var_count;
            v->size = 1;
            mem[STRING_POOL_START + var_count] = (int32_t)(uintptr_t)p;
            next_var_addr = STRING_POOL_START + 1;
        }
    } else if (strcmp(type, "array") == 0) {
        v->size = 0;
        char *copy = strdup(value_str);
        char *tok = strtok(copy, ",");
        while (tok && v->size < 16) {
            mem[next_var_addr + v->size] = atoi(tok);
            v->size++;
            tok = strtok(NULL, ",");
        }
        free(copy);
        next_var_addr += v->size;
    }
}

int find_variable(const char *name) {
    for (int i = 0; i < var_count; i++)
        if (strcmp(variables[i].name, name) == 0) return i;
    return -1;
}

void add_custom_op(const char *name, const char *type, const char *handler, const char *desc) {
    int existing = find_custom_op(name);
    CustomOp *c;
    if (existing >= 0) {
        c = &custom_ops[existing];
    } else {
        if (op_count >= MAX_OPS) return;
        c = &custom_ops[op_count++];
    }
    strncpy(c->name, name, 31);
    strncpy(c->type, type, 15);
    strncpy(c->handler, handler, 255);
    strncpy(c->desc, desc, 127);
}

int find_custom_op(const char *name) {
    for (int i = 0; i < op_count; i++)
        if (strcmp(custom_ops[i].name, name) == 0) return i;
    return -1;
}

void parse_ops_file(const char *path) {
    if (is_included(path)) return;
    strncpy(included_files[include_count++], path, 63);
    
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "[Prisc Error] Could not open ops file: %s\n", path);
        return;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        trim(line);
        if (line[0] == '#' || line[0] == '\0') continue;
        
        if (strncmp(line, "#include", 8) == 0) {
            char inc_path[64];
            if (sscanf(line + 8, " \"%63[^\"]\"", inc_path) == 1) {
                parse_ops_file(inc_path);
            }
            continue;
        }
        
        char name[32], type[16], handler[256], desc[128] = "";
        char *brace = strchr(line, '{');
        if (brace) {
            *brace = '\0';
            strncpy(desc, brace + 1, 126);
            char *end = strchr(desc, '}');
            if (end) *end = '\0';
        }
        
        if (sscanf(line, "%31s %15s %255s", name, type, handler) >= 3) {
            /* If path was truncated by sscanf, try a more robust approach */
            char *h_start = strstr(line, type) + strlen(type);
            while (*h_start == ' ' || *h_start == '\t') h_start++;
            char *h_end = h_start;
            while (*h_end && !isspace((unsigned char)*h_end) && *h_end != '{') h_end++;
            int h_len = h_end - h_start;
            if (h_len > 0 && h_len < 256) {
                strncpy(handler, h_start, h_len);
                handler[h_len] = '\0';
            }
            // fprintf(stderr, "[Prisc Debug] Parsed Op: %s -> %s\n", name, handler);
            add_custom_op(name, type, handler, desc);
        }
    }
    fclose(f);
}

int parse_vars_line(char *line) {
    trim(line);
    if (line[0] == '#' || line[0] == '\0') return 0;
    if (strncmp(line, "#include", 8) == 0) return 0;
    
    char name[32], type[16], value[128];
    if (sscanf(line, "%31s %15s %127[^\n]", name, type, value) == 3) {
        add_variable(name, type, value);
        return 1;
    }
    return 0;
}

int is_variable_line(char *line) {
    char trimmed[128];
    strncpy(trimmed, line, 127);
    trim(trimmed);
    if (trimmed[0] == '#' || trimmed[0] == '\0') return 0;
    if (strncmp(trimmed, "#include", 8) == 0) return 0;
    
    char name[32], type[16], value[128];
    if (sscanf(trimmed, "%31s %15s %127[^\n]", name, type, value) == 3) {
        if (strcmp(type, "int") == 0 || strcmp(type, "char") == 0 || 
            strcmp(type, "string") == 0 || strcmp(type, "array") == 0) {
            return 1;
        }
    }
    return 0;
}

void parse_line(char *line, int pass) {
    char original[128];
    strncpy(original, line, 127);
    trim(line);
    
    if (line[0] == '#' || line[0] == '\0') return;
    if (strncmp(line, "#include", 8) == 0) return;
    if (is_variable_line(line)) return;
    
    char part[32], *p = line;
    if (sscanf(p, "%s", part) != 1) return;
    
    if (part[strlen(part) - 1] == ':') {
        if (pass == 1) {
            part[strlen(part) - 1] = '\0';
            strcpy(labels[label_count].name, part);
            labels[label_count++].addr = inst_count;
        }
        return;
    }
    
    if (pass == 2) {
        Inst *i = &program[inst_count];
        memset(i, 0, sizeof(Inst));
        
        int op_idx = find_custom_op(part);
        if (op_idx >= 0) {
            i->op = OP_CUSTOM;
            strcpy(i->custom_name, part);
            
            /* Parse arguments: layout_op x2, x1 (rd, menu_id) or move_player x1 */
            char *args = strstr(original, part) + strlen(part);
            while (*args == ' ' || *args == '\t') args++;
            
            char arg_copy[64];
            strncpy(arg_copy, args, 63);
            arg_copy[63] = '\0';
            
            /* Check for comma-separated args (rd, rs1) */
            char *comma = strchr(arg_copy, ',');
            if (comma && strcmp(i->custom_name, "layout_op") == 0) {
                /* layout_op rd, menu_id */
                *comma = '\0';
                trim(arg_copy);
                int reg;
                if (sscanf(arg_copy, "x%d", &reg) == 1) {
                    i->rd = reg;
                }
                
                /* Parse second arg (menu_id) */
                char *arg2 = comma + 1;
                while (*arg2 == ' ' || *arg2 == '\t') arg2++;
                char arg2_copy[64];
                strncpy(arg2_copy, arg2, 63);
                char *c = strchr(arg2_copy, ' ');
                if (c) *c = '\0';
                c = strchr(arg2_copy, '\n');
                if (c) *c = '\0';
                trim(arg2_copy);
                
                if (sscanf(arg2_copy, "x%d", &reg) == 1) {
                    i->rs1 = reg;
                }
            } else {
                /* Check for literal (quoted string or single word) */
                char *quote = strchr(args, '"');
                if (quote) {
                    quote++;
                    char *end_q = strchr(quote, '"');
                    if (end_q) {
                        int len = end_q - quote;
                        if (len > 255) len = 255;
                        strncpy(i->literal_arg, quote, len);
                        i->literal_arg[len] = '\0';
                    }
                } else {
                    /* Single arg: move_player x1 or move_player hero */
                    char *c = strchr(arg_copy, ',');
                    if (c) *c = '\0';
                    c = strchr(arg_copy, ' ');
                    if (c) *c = '\0';
                    c = strchr(arg_copy, '\n');
                    if (c) *c = '\0';
                    c = strchr(arg_copy, '\r');
                    if (c) *c = '\0';
                    trim(arg_copy);
                    
                    int reg;
                    if (sscanf(arg_copy, "x%d", &reg) == 1) {
                        i->rs1 = reg;
                    } else {
                        /* Treat as literal piece name/arg */
                        strncpy(i->literal_arg, arg_copy, 255);
                    }
                }
            }
        } else if (strcmp(part, "read_history") == 0) {
            i->op = OP_READ_HISTORY;
            char *args = strstr(original, part) + strlen(part);
            while (*args == ' ' || *args == '\t') args++;
            char arg1[256] = "", arg2[256] = "";
            if (sscanf(args, "r%d, r%d", &i->rd, &i->rs1) == 2 || sscanf(args, "x%d, x%d", &i->rd, &i->rs1) == 2) {
                /* read_history rd, rs1 */
            } else if (sscanf(args, "r%d, x%d", &i->rd, &i->rs1) == 2 || sscanf(args, "x%d, r%d", &i->rd, &i->rs1) == 2) {
                /* mixed */
            } else {
                /* PAL-NATIVE FIX (see this file's own top-of-file
                 * comment): a real 3-arg literal-path form, checked
                 * FIRST here so it only ever activates when the plain
                 * register forms above didn't already match - captures
                 * a genuine position register (rs1_lit), unlike the
                 * older 1-register fallback below it, which silently
                 * defaulted the position register to 0 (permanently
                 * broken incremental reads, since register 0 is reset
                 * every instruction). */
                int rd_lit = -1, rs1_lit = -1;
                char path_tok[256] = "";
                if (sscanf(args, "%255s r%d, r%d", path_tok, &rd_lit, &rs1_lit) == 3 ||
                    sscanf(args, "%255s x%d, x%d", path_tok, &rd_lit, &rs1_lit) == 3 ||
                    sscanf(args, "%255s r%d, x%d", path_tok, &rd_lit, &rs1_lit) == 3 ||
                    sscanf(args, "%255s x%d, r%d", path_tok, &rd_lit, &rs1_lit) == 3) {
                    snprintf(i->literal_arg, sizeof(i->literal_arg), "%s", path_tok);
                    i->rd = rd_lit;
                    i->rs1 = rs1_lit;
                }
                /* old style: <path> <single-register> - kept, but note
                 * this form still leaves rs1 at 0 (register 0, reset
                 * every instruction) - only correct for a ONE-SHOT read,
                 * never for incremental tailing. Use the 3-arg form
                 * above for anything that needs to advance a cursor. */
                else if (sscanf(args, "%255s %255s", arg1, arg2) == 2) {
                    snprintf(i->literal_arg, sizeof(i->literal_arg), "%s", arg1);
                    if (sscanf(arg2, "r%d", &i->rd) != 1) sscanf(arg2, "x%d", &i->rd);
                } else if (sscanf(args, "%255s", arg1) == 1) {
                    if (sscanf(arg1, "r%d", &i->rd) != 1 && sscanf(arg1, "x%d", &i->rd) != 1) {
                        snprintf(i->literal_arg, sizeof(i->literal_arg), "%s", arg1);
                    }
                }
            }
        } else if (strcmp(part, "read_pos") == 0) {
            /* read_pos rd, [literal_path] - the literal-path form this
             * op's own runtime handler (OP_READ_POS's own comment: "Get
             * file size: read_pos rd, [literal_path]") has always
             * accepted was never actually parsed here - only rd was
             * ever extracted, so i->literal_arg stayed empty and every
             * call silently fell back to the hardcoded default
             * (pieces/apps/player_app/history.txt), regardless of what
             * a caller wrote. Completing it here, mirroring
             * read_layout's own already-correct optional-literal
             * parsing immediately below - a real gap, not a new
             * feature: any op that needs to cheaply detect "did some
             * OTHER file grow" (a marker file, not history.txt) had no
             * way to express that until now. */
            i->op = OP_READ_POS;
            char *args = strstr(original, part) + strlen(part);
            while (*args == ' ' || *args == '\t') args++;
            char arg2[256] = "";
            if (sscanf(args, "r%d, \"%[^\"]\"", &i->rd, arg2) == 2 || sscanf(args, "x%d, \"%[^\"]\"", &i->rd, arg2) == 2) {
                snprintf(i->literal_arg, sizeof(i->literal_arg), "%s", arg2);
            } else if (sscanf(args, "r%d", &i->rd) != 1) {
                sscanf(args, "x%d", &i->rd);
            }
        } else if (strcmp(part, "read_layout") == 0) {
            i->op = OP_READ_LAYOUT;
            char *args = strstr(original, part) + strlen(part);
            while (*args == ' ' || *args == '\t') args++;
            char arg2[256] = "";
            if (sscanf(args, "r%d, \"%[^\"]\"", &i->rd, arg2) == 2 || sscanf(args, "x%d, \"%[^\"]\"", &i->rd, arg2) == 2) {
                snprintf(i->literal_arg, sizeof(i->literal_arg), "%s", arg2);
            }
        } else if (strcmp(part, "sleep") == 0) {
            i->op = OP_SLEEP;
            char *args = strstr(original, part) + strlen(part);
            while (*args == ' ' || *args == '\t') args++;
            if (sscanf(args, "r%d", &i->rs1) != 1 && sscanf(args, "x%d", &i->rs1) != 1) {
                i->imm = atoi(args);
            }
        } else if (strcmp(part, "exec") == 0) {
            i->op = OP_EXEC;
            char *args = strstr(original, part) + strlen(part);
            while (*args == ' ' || *args == '\t') args++;
            char arg1[256] = "", arg2[256] = "", arg3[256] = "";
            int count = sscanf(args, "%255s %255s %255s", arg1, arg2, arg3);
            if (count >= 1) {
                snprintf(i->literal_arg, sizeof(i->literal_arg), "%s", arg1);
            }
            if (count >= 2) {
                if (sscanf(arg2, "r%d", &i->rs1) != 1 && sscanf(arg2, "x%d", &i->rs1) != 1) {
                    snprintf(i->literal_arg2, sizeof(i->literal_arg2), "%s", arg2);
                    i->rs1 = -1; // Flag as literal
                }
            }
            if (count >= 3) {
                if (sscanf(arg3, "r%d", &i->rs2) != 1 && sscanf(arg3, "x%d", &i->rs2) != 1) {
                    // We don't have literal_arg3 yet, so just ignore or add more complexity if needed
                    // But usually exec op arg1 arg2 is enough for now.
                }
            }
        } else if (strcmp(part, "hit_frame") == 0) {
            i->op = OP_HIT_FRAME;
        } else if (strcmp(part, "read_state") == 0) {
            i->op = OP_READ_STATE;
            char *args = strstr(original, part) + strlen(part);
            while (*args == ' ' || *args == '\t') args++;
            char arg1[256] = "", arg2[256] = "", arg3[256] = "";
            if (sscanf(args, "%255s %255s %255s", arg1, arg2, arg3) == 3) {
                snprintf(i->literal_arg, sizeof(i->literal_arg), "%s", arg1); // piece_id
                snprintf(i->literal_arg2, sizeof(i->literal_arg2), "%s", arg2); // key
                if (sscanf(arg3, "r%d", &i->rd) != 1) sscanf(arg3, "x%d", &i->rd);
            }
        } else if (strcmp(part, "read_active_target") == 0) {
            i->op = OP_READ_ACTIVE_TARGET;
            char *args = strstr(original, part) + strlen(part);
            while (*args == ' ' || *args == '\t') args++;
            if (sscanf(args, "r%d", &i->rd) != 1) sscanf(args, "x%d", &i->rd);
        } else if (strcmp(part, "read_env_key") == 0) {
            i->op = OP_READ_ENV_KEY;
            char *args = strstr(original, part) + strlen(part);
            while (*args == ' ' || *args == '\t') args++;
            if (sscanf(args, "r%d", &i->rd) != 1) sscanf(args, "x%d", &i->rd);
        } else if (strcmp(part, "addi") == 0) {
            int r_rd, r_rs1;
            if (sscanf(line, "%*s r%d, r%d, %d", &r_rd, &r_rs1, &i->imm) == 3) { i->rd = r_rd; i->rs1 = r_rs1; }
            else if (sscanf(line, "%*s x%d, x%d, %d", &i->rd, &i->rs1, &i->imm) == 3) {}
            i->op = OP_ADDI;
        } else if (strcmp(part, "beq") == 0) {
            int r_rs1, r_rs2;
            if (sscanf(line, "%*s r%d, r%d, %s", &r_rs1, &r_rs2, i->label_ref) == 3) { i->rs1 = r_rs1; i->rs2 = r_rs2; }
            else if (sscanf(line, "%*s x%d, x%d, %s", &i->rs1, &i->rs2, i->label_ref) == 3) {}
            i->op = OP_BEQ;
        } else if (strcmp(part, "li") == 0) {
            /* li rd, imm -> addi rd, x0, imm */
            int r_rd;
            if (sscanf(line, "%*s r%d, %d", &r_rd, &i->imm) == 2) { i->rd = r_rd; }
            else if (sscanf(line, "%*s x%d, %d", &i->rd, &i->imm) == 2) {}
            i->rs1 = 0;
            i->op = OP_ADDI;
        } else if (strcmp(part, "lw") == 0) {
            sscanf(line, "%*s x%d, %d(x%d)", &i->rd, &i->imm, &i->rs1);
            i->op = OP_LW;
        } else if (strcmp(part, "sw") == 0) {
            sscanf(line, "%*s x%d, %d(x%d)", &i->rs2, &i->imm, &i->rs1);
            i->op = OP_SW;
        } else if (strcmp(part, "jalr") == 0) {
            sscanf(line, "%*s x%d, x%d, %d", &i->rd, &i->rs1, &i->imm);
            i->op = OP_JALR;
        } else if (strcmp(part, "j") == 0) {
            sscanf(line, "%*s %s", i->label_ref);
            i->op = OP_J;
        } else if (strcmp(part, "halt") == 0) {
            i->op = OP_HALT;
        } else if (strcmp(part, "ecall") == 0) {
            /* Bare `ecall`, `ecall "text"` (SYS_OPEN's path /
             * SYS_WRITE_LINE's text), or `ecall "path" "key"`
             * (SYS_GET_KV_INT/SYS_SET_KV_INT) - see exec_ecall()'s own
             * header comment for why literal string args, not
             * registers, carry these operations' string data. */
            char *args = strstr(original, part) + strlen(part);
            while (*args == ' ' || *args == '\t') args++;
            if (sscanf(args, "\"%255[^\"]\" \"%255[^\"]\"", i->literal_arg, i->literal_arg2) != 2) {
                sscanf(args, "\"%255[^\"]\"", i->literal_arg);
            }
            i->op = OP_ECALL;
        }
    }
    inst_count++;
}

void load_mem(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return;
    int addr, val;
    while (fscanf(f, "%d %d", &addr, &val) == 2)
        if (addr >= 0 && addr < MEM_SIZE) mem[addr] = val;
    fclose(f);
}

void save_mem(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return;
    for (int i = 0; i < MEM_SIZE; i++)
        if (mem[i] != 0) fprintf(f, "%d %d\n", i, mem[i]);
    fclose(f);
}

/* ECALL - real syscall-style instruction, direct user instruction
 * ("implement the ecall or w/e riscv uses for syscalls"). This is the
 * foundation the long-term "compile pal+ops directly to real RISC-V"
 * goal needs (see PAL-VS-C-ARCHITECTURE.txt) - file I/O is the one
 * thing every op in this family actually does, and the user confirmed
 * that's fine to keep as a real syscall boundary ("i write and read
 * from file all the time in assembly, just think of that as our file
 * system") rather than something that has to become pure register/
 * memory instructions.
 *
 * Real RISC-V ecall takes NO operands - the syscall number and args
 * are pre-loaded into fixed ABI registers (a7 = number, a0-a6 = args,
 * a0 = return value) before the bare `ecall` executes. Same shape
 * here, adapted to this VM's own 16-register file (x0-x15, no a0-a7
 * aliases): x15 = syscall number, x12 = arg0/return value, x13 = arg1.
 * A script sets these with `li`/`addi` then issues a bare `ecall`.
 *
 * ONE deliberate departure from pure-register ecall: SYS_OPEN's path
 * and SYS_WRITE_LINE's text are fundamentally STRINGS, and this VM's
 * `mem[]` is a flat int32 word array with no safe byte/string
 * addressing mode (the existing string-variable mechanism already
 * truncates real pointers into int32, a pre-existing fragility this
 * doesn't try to fix or build further on). Passing them as a literal
 * argument on the `ecall` instruction line itself (`ecall "text"`) -
 * the SAME mechanism read_history's own literal-path form already
 * uses - keeps this memory-safe without a full byte-addressable-
 * memory VM redesign. SYS_READ_INT deliberately only supports reading
 * back a single INTEGER (not an arbitrary string) for the same reason
 * - a real, named future extension, not silently glossed over: full
 * string read-back needs that memory-model upgrade first.
 *
 * This VM's own syscall table (NOT Linux/RISC-V ABI syscall numbers -
 * there is no real kernel underneath, this VM implements each one
 * directly in C, so reusing Linux's numbering would be a false
 * equivalence, not real compatibility). */
#define SYS_OPEN       1  /* ecall "path", x15=1, x13=mode(0=r,1=w,2=a) -> x12=fd or -1 */
#define SYS_CLOSE      2  /* x15=2, x12=fd -> x12=0 */
#define SYS_WRITE_LINE 3  /* ecall "text", x15=3, x12=fd -> x12=bytes written or -1 */
#define SYS_WRITE_INT  4  /* x15=4, x12=fd, x13=value -> x12=bytes written or -1 (writes "%d\n") */
#define SYS_READ_INT   5  /* x15=5, x12=fd -> x12=parsed int (atoi of next line), or -1 at EOF */
/* SYS_GET_KV_INT/SYS_SET_KV_INT - added while prototyping the first
 * real op conversion (see PAL-VS-C-ARCHITECTURE.txt sec. 4/6): almost
 * every real op in this family (pet_pet.c/feed_pet.c/move_player.c/
 * compose_frame.c/...) duplicates the exact same read_kv_int()/
 * set_kv_int() idiom - read one "key=value" line out of a small
 * multi-key state file, or rewrite ONE key's value while preserving
 * every other line. That "preserve other lines" part needs reading
 * arbitrary line CONTENT, which plain byte-addressable ecall (sec. 3's
 * own SYS_READ_INT limitation) can't do yet without a real memory-
 * model upgrade. Rather than block the whole conversion effort on
 * that, these two syscalls do the read-all/rewrite-one/write-back
 * work INTERNALLY in C (same as set_kv_int() already does) and expose
 * it as ONE syscall - the right abstraction level for what THIS
 * family's own software actually needs, not a slavish imitation of a
 * general-purpose OS's raw byte-buffer syscalls. A script provides
 * path+key as TWO literal strings on the ecall line itself:
 * `ecall "path" "key"` (extends the same literal-arg mechanism sec. 3
 * already uses, now using BOTH literal_arg and literal_arg2). */
#define SYS_GET_KV_INT 6  /* ecall "path" "key", x15=6, x13=default -> x12=int value (or default if key/file missing) */
#define SYS_SET_KV_INT 7  /* ecall "path" "key", x15=7, x12=value -> x12=1 on success, 0 on failure (preserves other lines, appends key if new) */

#define MAX_ECALL_FDS 8
static FILE *ecall_fds[MAX_ECALL_FDS] = {0};

static int ecall_alloc_fd(FILE *f) {
    for (int i = 0; i < MAX_ECALL_FDS; i++) {
        if (!ecall_fds[i]) { ecall_fds[i] = f; return i; }
    }
    return -1;
}

void exec_ecall(Inst *i) {
    int sysnum = regs[15];
    switch (sysnum) {
        case SYS_OPEN: {
            const char *mode = (regs[13] == 1) ? "w" : (regs[13] == 2) ? "a" : "r";
            FILE *f = fopen(i->literal_arg, mode);
            regs[12] = f ? ecall_alloc_fd(f) : -1;
            break;
        }
        case SYS_CLOSE: {
            int fd = regs[12];
            if (fd >= 0 && fd < MAX_ECALL_FDS && ecall_fds[fd]) {
                fclose(ecall_fds[fd]);
                ecall_fds[fd] = NULL;
            }
            regs[12] = 0;
            break;
        }
        case SYS_WRITE_LINE: {
            int fd = regs[12];
            if (fd >= 0 && fd < MAX_ECALL_FDS && ecall_fds[fd]) {
                regs[12] = fprintf(ecall_fds[fd], "%s\n", i->literal_arg);
                fflush(ecall_fds[fd]);
            } else regs[12] = -1;
            break;
        }
        case SYS_WRITE_INT: {
            int fd = regs[12];
            int value = regs[13];
            if (fd >= 0 && fd < MAX_ECALL_FDS && ecall_fds[fd]) {
                regs[12] = fprintf(ecall_fds[fd], "%d\n", value);
                fflush(ecall_fds[fd]);
            } else regs[12] = -1;
            break;
        }
        case SYS_READ_INT: {
            int fd = regs[12];
            if (fd >= 0 && fd < MAX_ECALL_FDS && ecall_fds[fd]) {
                char line[64];
                if (fgets(line, sizeof(line), ecall_fds[fd])) regs[12] = atoi(line);
                else regs[12] = -1;
            } else regs[12] = -1;
            break;
        }
        case SYS_GET_KV_INT: {
            int def = regs[13];
            FILE *f = fopen(i->literal_arg, "r");
            int val = def;
            if (f) {
                char line[512];
                size_t klen = strlen(i->literal_arg2);
                while (fgets(line, sizeof(line), f)) {
                    if (strncmp(line, i->literal_arg2, klen) == 0 && line[klen] == '=') {
                        val = atoi(line + klen + 1);
                        break;
                    }
                }
                fclose(f);
            }
            regs[12] = val;
            break;
        }
        case SYS_SET_KV_INT: {
            int value = regs[12];
            char lines[64][512];
            int nlines = 0;
            FILE *f = fopen(i->literal_arg, "r");
            if (f) {
                while (nlines < 64 && fgets(lines[nlines], sizeof(lines[0]), f)) nlines++;
                fclose(f);
            }
            size_t klen = strlen(i->literal_arg2);
            int found = 0;
            f = fopen(i->literal_arg, "w");
            if (!f) { regs[12] = 0; break; }
            for (int n = 0; n < nlines; n++) {
                if (!found && strncmp(lines[n], i->literal_arg2, klen) == 0 && lines[n][klen] == '=') {
                    fprintf(f, "%s=%d\n", i->literal_arg2, value);
                    found = 1;
                    continue;
                }
                fputs(lines[n], f);
            }
            if (!found) fprintf(f, "%s=%d\n", i->literal_arg2, value);
            fclose(f);
            regs[12] = 1;
            break;
        }
        default: regs[12] = -1; break;
    }
}

void exec_custom_op(Inst *i) {
    int op_idx = find_custom_op(i->custom_name);
    if (op_idx < 0) return;
    
    CustomOp *op = &custom_ops[op_idx];
    
    if (strncmp(op->handler, "+x/", 3) == 0 || strstr(op->handler, "/+x/")) {
        char *cmd = NULL;
        char *ext = strstr(op->handler, ".+x");
        if (ext) {
            char script_path[256];
            strncpy(script_path, op->handler, ext - op->handler + 3);
            script_path[ext - op->handler + 3] = '\0';
            
            const char *clean_script_path = script_path;
            if (strncmp(clean_script_path, "./", 2) == 0) clean_script_path += 2;

            /* Resolve absolute path relative to project root env */
            char *project_root_env = getenv("PRISC_PROJECT_ROOT");
            char *full_script_path = NULL;
            if (project_root_env) {
                char base_abs[MAX_PATH];
                if (REALPATH(project_root_env, base_abs)) {
                    if (asprintf(&full_script_path, "%s/%s", base_abs, clean_script_path) < 0)
                        full_script_path = NULL;
                } else {
                    if (asprintf(&full_script_path, "%s/%s", project_root_env, clean_script_path) < 0)
                        full_script_path = NULL;
                }
            } else {
                full_script_path = strdup(clean_script_path);
            }

            /* For layout_op, use system() for interactive stdin */
            if (strcmp(i->custom_name, "layout_op") == 0) {
                /* Pass menu_id from rs1, result goes to shared file */
                if (asprintf(&cmd, "%s %d > /tmp/prisc_layout_result.txt",
                         full_script_path, regs[i->rs1]) != -1) {
                    int sys_rc = system(cmd);
                    (void)sys_rc; /* op's own exit status isn't consulted here */
                    free(cmd);
                }
                
                /* Read result from shared file */
                FILE *rf = fopen("/tmp/prisc_layout_result.txt", "r");
                if (rf) {
                    char result[16];
                    if (fgets(result, sizeof(result), rf)) {
                        regs[i->rd] = atoi(result);
                    }
                    fclose(rf);
                }
            } else {
                /* Pass literal arg if present, else pass register value */
                if (strlen(i->literal_arg) > 0) {
                    if (asprintf(&cmd, "%s \"%s\"",
                             full_script_path, i->literal_arg) != -1) {
                        FILE *pipe = popen(cmd, "r");
                        if (pipe) {
                            char result[256];
                            while (fgets(result, sizeof(result), pipe)) printf("%s", result);
                            pclose(pipe);
                        }
                        free(cmd);
                    }
                } else {
                    if (asprintf(&cmd, "%s %d",
                             full_script_path, regs[i->rs1]) != -1) {
                        FILE *pipe = popen(cmd, "r");
                        if (pipe) {
                            char result[256];
                            while (fgets(result, sizeof(result), pipe)) printf("%s", result);
                            pclose(pipe);
                        }
                        free(cmd);
                    }
                }
            }
            free(full_script_path);
        }
    } else if (strcmp(op->handler, "builtin_out") == 0) {
        printf("OUT: %d\n", regs[i->rs1]);
    } else if (strcmp(op->handler, "builtin_halt") == 0) {
    }
}

int main(int argc, char **argv) {
    if (argc < 2) return printf("Usage: %s <prog> [mem_in] [mem_out] [ops_file]\n", argv[0]), 1;
    
    /* Set up Ctrl+C handler */
    signal(SIGINT, handle_sigint);
    
    /* Try local first, then relative to binary */
    FILE *base_check = fopen("default_op.txt", "r");
    if (base_check) {
        fclose(base_check);
        parse_ops_file("default_op.txt");
    } else {
        char binary_path[MAX_PATH];
#ifndef _WIN32
        ssize_t len = readlink("/proc/self/exe", binary_path, sizeof(binary_path)-1);
        if (len != -1) {
            binary_path[len] = '\0';
            char *last_slash = strrchr(binary_path, '/');
#else
        DWORD len = GetModuleFileName(NULL, binary_path, sizeof(binary_path));
        if (len > 0) {
            char *last_slash = strrchr(binary_path, '\\');
#endif
            if (last_slash) {
                *last_slash = '\0';
                char *alt_path = NULL;
                if (asprintf(&alt_path, "%s/default_op.txt", binary_path) >= 0) {
                    parse_ops_file(alt_path);
                    free(alt_path);
                }
            }
        }
    }
    if (argc > 4) parse_ops_file(argv[4]);
    
    FILE *f = fopen(argv[1], "r");
    if (!f) {
        fprintf(stderr, "[Prisc Error] Could not open program file: %s\n", argv[1]);
        return 1;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char trimmed[256];
        strncpy(trimmed, line, sizeof(trimmed) - 1);
        trimmed[sizeof(trimmed) - 1] = '\0';
        trim(trimmed);
        if (is_variable_line(trimmed)) {
            char name[32], type[16], value[256];
            if (sscanf(trimmed, "%31s %15s %255[^\n]", name, type, value) == 3) {
                add_variable(name, type, value);
            }
        }
    }
    
    rewind(f);
    inst_count = 0;
    
    while (fgets(line, sizeof(line), f)) parse_line(line, 1);
    rewind(f);
    inst_count = 0;
    
    while (fgets(line, sizeof(line), f)) parse_line(line, 2);
    fclose(f);
    
    if (argc > 2) load_mem(argv[2]);
    
    int pc = 0;
    while (pc < inst_count) {
        Inst i = program[pc];
        regs[0] = 0;
        int next_pc = pc + 1;
        
        if (i.op == OP_CUSTOM) {
            exec_custom_op(&i);
            if (find_custom_op(i.custom_name) >= 0 &&
                strcmp(custom_ops[find_custom_op(i.custom_name)].handler, "builtin_halt") == 0) {
                break;
            }
        } else if (i.op == OP_READ_HISTORY) {
            /* Incremental Read: read_history rd, rs1 (pos), [literal_path] */
            char *path = NULL;
            if (strlen(i.literal_arg) > 0) path = strdup(i.literal_arg);
            else {
                char *proj_root = getenv("PRISC_PROJECT_ROOT");
                if (proj_root) {
                    if (asprintf(&path, "%s/pieces/apps/player_app/history.txt", proj_root) < 0) path = NULL;
                } else path = strdup("pieces/apps/player_app/history.txt");
            }
            FILE *hf = path ? fopen(path, "r") : NULL;
            if (hf) {
                fseek(hf, regs[i.rs1], SEEK_SET);
                int key;
                if (fscanf(hf, "%d", &key) == 1) {
                    regs[i.rd] = key;
                    regs[i.rs1] = ftell(hf);
                } else {
                    regs[i.rd] = 0;
                }
                fclose(hf);
            } else regs[i.rd] = 0;
            free(path);
        } else if (i.op == OP_READ_POS) {
            /* Get file size: read_pos rd, [literal_path] */
            char *path = NULL;
            if (strlen(i.literal_arg) > 0) path = strdup(i.literal_arg);
            else {
                char *proj_root = getenv("PRISC_PROJECT_ROOT");
                if (proj_root) {
                    if (asprintf(&path, "%s/pieces/apps/player_app/history.txt", proj_root) < 0) path = NULL;
                } else path = strdup("pieces/apps/player_app/history.txt");
            }
            struct stat st;
            if (path && stat(path, &st) == 0) regs[i.rd] = st.st_size;
            else regs[i.rd] = 0;
            free(path);
        } else if (i.op == OP_READ_LAYOUT) {
            /* Check layout: read_layout rd, [literal_pattern] */
            char *proj_root = getenv("PRISC_PROJECT_ROOT");
            char *path = NULL;
            if (proj_root) {
                if (asprintf(&path, "%s/pieces/display/current_layout.txt", proj_root) < 0) path = NULL;
            } else path = strdup("pieces/display/current_layout.txt");

            FILE *f = path ? fopen(path, "r") : NULL;
            if (f) {
                char line[256];
                if (fgets(line, sizeof(line), f)) {
                    if (strstr(line, i.literal_arg)) regs[i.rd] = 1;
                    else regs[i.rd] = 0;
                } else regs[i.rd] = 0;
                fclose(f);
            } else regs[i.rd] = 0;
            free(path);
        } else if (i.op == OP_SLEEP) {
            /* Sleep: sleep rs1 (micros) or imm */
            if (i.imm > 0) usleep(i.imm);
            else if (regs[i.rs1] > 0) usleep(regs[i.rs1]);
        } else if (i.op == OP_EXEC) {
            char cmd[1024];
            char arg1[256] = "", arg2[256] = "";
            if (i.rs1 >= 0) sprintf(arg1, "%d", regs[i.rs1]);
            else strcpy(arg1, i.literal_arg2);
            
            if (i.rs2 >= 0) sprintf(arg2, "%d", regs[i.rs2]);
            
            if (strlen(arg2) > 0) sprintf(cmd, "%s %s %s > /dev/null 2>&1", i.literal_arg, arg1, arg2);
            else if (strlen(arg1) > 0) sprintf(cmd, "%s %s > /dev/null 2>&1", i.literal_arg, arg1);
            else sprintf(cmd, "%s > /dev/null 2>&1", i.literal_arg);

            int exec_rc = system(cmd);
            (void)exec_rc; /* exec op's own exit status isn't consulted here */
        } else if (i.op == OP_HIT_FRAME) {
            char *proj_root = getenv("PRISC_PROJECT_ROOT");
            char *path = NULL;
            if (proj_root) {
                if (asprintf(&path, "%s/pieces/display/frame_changed.txt", proj_root) < 0) path = NULL;
            } else path = strdup("pieces/display/frame_changed.txt");
            FILE *f = path ? fopen(path, "a") : NULL;
            if (f) { fprintf(f, "X\n"); fclose(f); }
            free(path);
        } else if (i.op == OP_READ_STATE) {
            char *proj_root = getenv("PRISC_PROJECT_ROOT");
            char *proj_id = getenv("PRISC_PROJECT_ID");
            char path[MAX_PATH];
            resolve_piece_state_path(proj_root, proj_id, i.literal_arg, path, sizeof(path));
            FILE *f = fopen(path, "r");
            if (f) {
                char line[256];
                int found = 0;
                while (fgets(line, sizeof(line), f)) {
                    char *eq = strchr(line, '=');
                    if (eq) {
                        *eq = '\0';
                        char *k = line;
                        while(isspace(*k)) k++;
                        if (strcmp(k, i.literal_arg2) == 0) {
                            regs[i.rd] = atoi(eq + 1);
                            found = 1; break;
                        }
                    }
                }
                if (!found) regs[i.rd] = 0;
                fclose(f);
            } else regs[i.rd] = 0;
        } else if (i.op == OP_READ_ACTIVE_TARGET) {
            char *target = getenv("PRISC_ACTIVE_TARGET");
            if (target && (strcmp(target, "selector") == 0 || strcmp(target, "1") == 0)) regs[i.rd] = 1;
            else regs[i.rd] = 0;
        } else if (i.op == OP_READ_ENV_KEY) {
            char *key_env = getenv("PRISC_KEY");
            if (key_env) regs[i.rd] = atoi(key_env);
            else regs[i.rd] = 0;
        } else if (i.op == OP_ECALL) {
            exec_ecall(&i);
        } else {
            switch (i.op) {
                case OP_ADDI: regs[i.rd] = regs[i.rs1] + i.imm; break;
                case OP_BEQ: {
                    int target = -1;
                    for (int l = 0; l < label_count; l++)
                        if (strcmp(labels[l].name, i.label_ref) == 0) target = labels[l].addr;
                    if (regs[i.rs1] == regs[i.rs2]) next_pc = target;
                    break;
                }
                case OP_J: {
                    int target = -1;
                    for (int l = 0; l < label_count; l++)
                        if (strcmp(labels[l].name, i.label_ref) == 0) target = labels[l].addr;
                    next_pc = target;
                    break;
                }
                case OP_LW:  regs[i.rd] = mem[(regs[i.rs1] + i.imm) % MEM_SIZE]; break;
                case OP_SW:  mem[(regs[i.rs1] + i.imm) % MEM_SIZE] = regs[i.rs2]; break;
                case OP_JALR: { int tmp = pc + 1; next_pc = regs[i.rs1] + i.imm; regs[i.rd] = tmp; break; }
                case OP_HALT: pc = inst_count; continue;
                default: break;
            }
        }
        pc = next_pc;
    }
    
    if (argc > 3) save_mem(argv[3]);
    exit(0);
}
