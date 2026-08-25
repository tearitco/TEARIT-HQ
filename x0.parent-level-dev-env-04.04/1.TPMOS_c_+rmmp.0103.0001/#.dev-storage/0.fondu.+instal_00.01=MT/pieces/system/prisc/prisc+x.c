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
#define REALPATH(path, resolved) realpath(path, resolved)
#define SETENV(name, value, overwrite) setenv(name, value, overwrite)
#endif
#include <sys/stat.h>

void handle_sigint(int sig) {
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

typedef enum { OP_ADDI, OP_BEQ, OP_LW, OP_SW, OP_JALR, OP_J, OP_HALT, OP_CUSTOM, OP_READ_HISTORY, OP_EXEC, OP_HIT_FRAME, OP_READ_STATE, OP_READ_ACTIVE_TARGET, OP_READ_ENV_KEY, OP_SLEEP, OP_READ_LAYOUT, OP_READ_POS } OpBase;

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
                /* old style or literal path? */
                if (sscanf(args, "%255s %255s", arg1, arg2) == 2) {
                    strncpy(i->literal_arg, arg1, 255);
                    if (sscanf(arg2, "r%d", &i->rd) != 1) sscanf(arg2, "x%d", &i->rd);
                } else if (sscanf(args, "%255s", arg1) == 1) {
                    if (sscanf(arg1, "r%d", &i->rd) != 1 && sscanf(arg1, "x%d", &i->rd) != 1) strncpy(i->literal_arg, arg1, 255);
                }
            }
        } else if (strcmp(part, "read_pos") == 0) {
            i->op = OP_READ_POS;
            char *args = strstr(original, part) + strlen(part);
            while (*args == ' ' || *args == '\t') args++;
            if (sscanf(args, "r%d", &i->rd) != 1) sscanf(args, "x%d", &i->rd);
        } else if (strcmp(part, "read_layout") == 0) {
            i->op = OP_READ_LAYOUT;
            char *args = strstr(original, part) + strlen(part);
            while (*args == ' ' || *args == '\t') args++;
            char arg1[256] = "", arg2[256] = "";
            if (sscanf(args, "r%d, \"%[^\"]\"", &i->rd, arg2) == 2 || sscanf(args, "x%d, \"%[^\"]\"", &i->rd, arg2) == 2) {
                strncpy(i->literal_arg, arg2, 255);
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
            if (count >= 1) strncpy(i->literal_arg, arg1, 255);
            if (count >= 2) {
                if (sscanf(arg2, "r%d", &i->rs1) != 1 && sscanf(arg2, "x%d", &i->rs1) != 1) {
                    strncpy(i->literal_arg2, arg2, 255);
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
                strncpy(i->literal_arg, arg1, 255); // piece_id
                strncpy(i->literal_arg2, arg2, 255); // key
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
                    asprintf(&full_script_path, "%s/%s", base_abs, clean_script_path);
                } else {
                    asprintf(&full_script_path, "%s/%s", project_root_env, clean_script_path);
                }
            } else {
                full_script_path = strdup(clean_script_path);
            }

            /* For layout_op, use system() for interactive stdin */
            if (strcmp(i->custom_name, "layout_op") == 0) {
                /* Pass menu_id from rs1, result goes to shared file */
                if (asprintf(&cmd, "%s %d > /tmp/prisc_layout_result.txt",
                         full_script_path, regs[i->rs1]) != -1) {
                    system(cmd);
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
                asprintf(&alt_path, "%s/default_op.txt", binary_path);
                parse_ops_file(alt_path);
                free(alt_path);
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
        strncpy(trimmed, line, 255);
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
                if (proj_root) asprintf(&path, "%s/pieces/apps/player_app/history.txt", proj_root);
                else path = strdup("pieces/apps/player_app/history.txt");
            }
            FILE *hf = fopen(path, "r");
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
                if (proj_root) asprintf(&path, "%s/pieces/apps/player_app/history.txt", proj_root);
                else path = strdup("pieces/apps/player_app/history.txt");
            }
            struct stat st;
            if (stat(path, &st) == 0) regs[i.rd] = st.st_size;
            else regs[i.rd] = 0;
            free(path);
        } else if (i.op == OP_READ_LAYOUT) {
            /* Check layout: read_layout rd, [literal_pattern] */
            char *proj_root = getenv("PRISC_PROJECT_ROOT");
            char *path = NULL;
            if (proj_root) asprintf(&path, "%s/pieces/display/current_layout.txt", proj_root);
            else path = strdup("pieces/display/current_layout.txt");
            
            FILE *f = fopen(path, "r");
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
            
            system(cmd);
        } else if (i.op == OP_HIT_FRAME) {
            char *proj_root = getenv("PRISC_PROJECT_ROOT");
            char *path = NULL;
            if (proj_root) asprintf(&path, "%s/pieces/display/frame_changed.txt", proj_root);
            else path = strdup("pieces/display/frame_changed.txt");
            FILE *f = fopen(path, "a");
            if (f) { fprintf(f, "X\n"); fclose(f); }
            free(path);
        } else if (i.op == OP_READ_STATE) {
            char *proj_root = getenv("PRISC_PROJECT_ROOT");
            char *proj_id = getenv("PRISC_PROJECT_ID");
            char *path = NULL;
            if (proj_root && proj_id) asprintf(&path, "%s/projects/%s/pieces/%s/state.txt", proj_root, proj_id, i.literal_arg);
            else asprintf(&path, "projects/template/pieces/%s/state.txt", i.literal_arg);
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
            free(path);
        } else if (i.op == OP_READ_ACTIVE_TARGET) {
            char *target = getenv("PRISC_ACTIVE_TARGET");
            if (target && (strcmp(target, "selector") == 0 || strcmp(target, "1") == 0)) regs[i.rd] = 1;
            else regs[i.rd] = 0;
        } else if (i.op == OP_READ_ENV_KEY) {
            char *key_env = getenv("PRISC_KEY");
            if (key_env) regs[i.rd] = atoi(key_env);
            else regs[i.rd] = 0;
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
