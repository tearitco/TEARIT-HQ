// tools/gemini_payload_builder.c - Converts flat context JSON into Gemini REST JSON payload
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char* role;
    char* content;
} Message;

// Write raw string escaping JSON control characters
void fprintf_json_escaped(FILE* f, const char* s) {
    while (*s) {
        if (*s == '"') fputs("\\\"", f);
        else if (*s == '\\') fputs("\\\\", f);
        else if (*s == '\n') fputs("\\n", f);
        else if (*s == '\r') fputs("\\r", f);
        else if (*s == '\t') fputs("\\t", f);
        else fputc(*s, f);
        s++;
    }
}

// Unescape JSON string helper
char* json_unescape(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char* out = malloc(len + 1);
    if (!out) return NULL;
    char* p = out;
    while (*s) {
        if (*s == '\\' && *(s+1)) {
            s++;
            if (*s == 'n') *p++ = '\n';
            else if (*s == 't') *p++ = '\t';
            else if (*s == 'r') *p++ = '\r';
            else if (*s == '"') *p++ = '"';
            else if (*s == '\\') *p++ = '\\';
            else *p++ = *s;
        } else {
            *p++ = *s;
        }
        s++;
    }
    *p = '\0';
    return out;
}

// Simple JSON parser for unescaped content to extract function name, args, and id
static char* parse_key_obj(const char* json, const char* key, int is_object) {
    const char* p = json;
    char k_buf[256];
    while ((p = strchr(p, '"')) != NULL) {
        p++;
        size_t i = 0;
        while (*p && *p != '"' && i < sizeof(k_buf) - 1) {
            if (*p == '\\' && *(p+1)) { p++; k_buf[i++] = *p++; }
            else k_buf[i++] = *p++;
        }
        k_buf[i] = '\0';
        if (*p == '"') p++;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (*p == ':') {
            p++;
            while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
            if (strcmp(k_buf, key) == 0) {
                if (is_object) {
                    if (*p == '{') {
                        int depth = 0;
                        const char* start = p;
                        while (*p) {
                            if (*p == '"') {
                                p++;
                                while (*p && (*p != '"' || (*(p-1) == '\\' && *(p-2) != '\\'))) p++;
                            } else if (*p == '{') {
                                depth++;
                            } else if (*p == '}') {
                                depth--;
                                if (depth == 0) { p++; break; }
                            }
                            if (*p) p++;
                        }
                        size_t len = p - start;
                        char* res = malloc(len + 1);
                        memcpy(res, start, len);
                        res[len] = '\0';
                        return res;
                    }
                } else {
                    if (*p == '"') {
                        p++;
                        const char* start = p;
                        while (*p && *p != '"') {
                            if (*p == '\\' && *(p+1)) p += 2;
                            else p++;
                        }
                        size_t len = p - start;
                        char* res = malloc(len + 1);
                        memcpy(res, start, len);
                        res[len] = '\0';
                        return res;
                    }
                }
            }
        }
    }
    return NULL;
}

char* read_file(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = malloc(size + 1);
    if (buf) {
        size_t n = fread(buf, 1, size, f);
        buf[n] = '\0';
    }
    fclose(f);
    return buf;
}

int parse_messages(const char* json, Message* messages, int max_messages) {
    int count = 0;
    const char* p = json;
    
    while (count < max_messages) {
        p = strstr(p, "\"role\"");
        if (!p) break;
        p = strchr(p, ':');
        if (!p) break;
        p++;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (*p != '"') break;
        p++;
        const char* role_start = p;
        const char* role_end = strchr(p, '"');
        if (!role_end) break;
        size_t role_len = role_end - role_start;
        char* role = malloc(role_len + 1);
        memcpy(role, role_start, role_len);
        role[role_len] = '\0';
        
        p = role_end + 1;
        p = strstr(p, "\"content\"");
        if (!p) { free(role); break; }
        p = strchr(p, ':');
        if (!p) { free(role); break; }
        p++;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (*p != '"') { free(role); break; }
        p++;
        
        const char* content_start = p;
        const char* q = p;
        while (*q) {
            if (*q == '\\' && *(q+1)) {
                q += 2;
            } else if (*q == '"') {
                break;
            } else {
                q++;
            }
        }
        if (!*q) { free(role); break; }
        size_t content_len = q - content_start;
        char* content = malloc(content_len + 1);
        memcpy(content, content_start, content_len);
        content[content_len] = '\0';
        
        messages[count].role = role;
        messages[count].content = content;
        count++;
        p = q + 1;
    }
    return count;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input_messages_json> <output_gemini_json> [enable_tools_flag] [override_system_instruction]\n", argv[0]);
        return 1;
    }
    
    const char* input_path = argv[1];
    const char* output_path = argv[2];
    int enable_tools = (argc >= 4 && strcmp(argv[3], "0") == 0) ? 0 : 1;
    const char* override_sys = (argc >= 5) ? argv[4] : NULL;
    
    char* json_data = read_file(input_path);
    if (!json_data) {
        fprintf(stderr, "Error: Could not read file %s\n", input_path);
        return 1;
    }
    
    Message messages[1024];
    int message_count = parse_messages(json_data, messages, 1024);
    free(json_data);
    
    FILE* out = fopen(output_path, "w");
    if (!out) {
        perror("fopen output");
        return 1;
    }
    
    char* system_instruction = (char*)override_sys;
    if (!system_instruction) {
        for (int i = 0; i < message_count; i++) {
            if (strcmp(messages[i].role, "system") == 0) {
                system_instruction = messages[i].content;
            }
        }
    }
    
    fprintf(out, "{\n");
    if (system_instruction) {
        fprintf(out, "  \"systemInstruction\": {\n");
        fprintf(out, "    \"parts\": [{\n");
        if (override_sys) {
            fprintf(out, "      \"text\": \"");
            fprintf_json_escaped(out, system_instruction);
            fprintf(out, "\"\n");
        } else {
            fprintf(out, "      \"text\": \"%s\"\n", system_instruction);
        }
        fprintf(out, "    }]\n");
        fprintf(out, "  },\n");
    }
    
    fprintf(out, "  \"contents\": [\n");
    int first_content = 1;
    char* last_function_name = NULL;
    char* last_function_id = NULL;
    
    for (int i = 0; i < message_count; i++) {
        if (strcmp(messages[i].role, "system") == 0) continue;
        
        if (!first_content) {
            fprintf(out, ",\n");
        }
        first_content = 0;
        
        if (strcmp(messages[i].role, "user") == 0) {
            fprintf(out, "    {\n");
            fprintf(out, "      \"role\": \"user\",\n");
            fprintf(out, "      \"parts\": [{\n");
            fprintf(out, "        \"text\": \"%s\"\n", messages[i].content);
            fprintf(out, "      }]\n");
            fprintf(out, "    }");
        } 
        else if (strcmp(messages[i].role, "assistant") == 0) {
            char* unescaped = json_unescape(messages[i].content);
            char* func_name = NULL;
            char* func_args = NULL;
            char* func_id = NULL;
            
            if (unescaped) {
                const char* start_p = unescaped;
                while (*start_p == ' ' || *start_p == '\t' || *start_p == '\n' || *start_p == '\r') start_p++;
                if (*start_p == '{') {
                    func_name = parse_key_obj(unescaped, "name", 0);
                    if (!func_name) func_name = parse_key_obj(unescaped, "tool", 0);
                    func_args = parse_key_obj(unescaped, "args", 1);
                    if (!func_args) func_args = parse_key_obj(unescaped, "arguments", 1);
                    func_id = parse_key_obj(unescaped, "id", 0);
                }
            }
            
            if (func_name) {
                if (last_function_name) free(last_function_name);
                last_function_name = strdup(func_name);
                if (last_function_id) { free(last_function_id); last_function_id = NULL; }
                if (func_id) last_function_id = strdup(func_id);
                
                fprintf(out, "    {\n");
                fprintf(out, "      \"role\": \"model\",\n");
                fprintf(out, "      \"parts\": [{\n");
                fprintf(out, "        \"functionCall\": {\n");
                fprintf(out, "          \"name\": \"%s\"", func_name);
                if (func_args) {
                    fprintf(out, ",\n          \"args\": %s", func_args);
                }
                if (func_id) {
                    fprintf(out, ",\n          \"id\": \"%s\"", func_id);
                }
                fprintf(out, "\n        }\n");
                fprintf(out, "      }]\n");
                fprintf(out, "    }");
                free(func_name);
                if (func_args) free(func_args);
                if (func_id) free(func_id);
            } else {
                fprintf(out, "    {\n");
                fprintf(out, "      \"role\": \"model\",\n");
                fprintf(out, "      \"parts\": [{\n");
                fprintf(out, "        \"text\": \"%s\"\n", messages[i].content);
                fprintf(out, "      }]\n");
                fprintf(out, "    }");
            }
            if (unescaped) free(unescaped);
        }
        else if (strcmp(messages[i].role, "tool") == 0) {
            char* tool_name = last_function_name ? last_function_name : "unknown_tool";
            fprintf(out, "    {\n");
            fprintf(out, "      \"role\": \"function\",\n");
            fprintf(out, "      \"parts\": [{\n");
            fprintf(out, "        \"functionResponse\": {\n");
            fprintf(out, "          \"name\": \"%s\"", tool_name);
            if (last_function_id) {
                fprintf(out, ",\n          \"id\": \"%s\"", last_function_id);
            }
            fprintf(out, ",\n          \"response\": {\n");
            fprintf(out, "            \"result\": \"%s\"\n", messages[i].content);
            fprintf(out, "          }\n");
            fprintf(out, "        }\n");
            fprintf(out, "      }]\n");
            fprintf(out, "    }");
        }
    }
    fprintf(out, "\n  ]");
    
    if (enable_tools) {
        fprintf(out, ",\n  \"tools\": [{\n");
        fprintf(out, "    \"functionDeclarations\": [\n");
        
        // Tool 1: exec_cmd
        fprintf(out, "      {\n");
        fprintf(out, "        \"name\": \"exec_cmd\",\n");
        fprintf(out, "        \"description\": \"Run a shell command on the host system\",\n");
        fprintf(out, "        \"parameters\": {\n");
        fprintf(out, "          \"type\": \"object\",\n");
        fprintf(out, "          \"properties\": {\n");
        fprintf(out, "            \"cmd\": {\n");
        fprintf(out, "              \"type\": \"string\",\n");
        fprintf(out, "              \"description\": \"The shell command line string to execute\"\n");
        fprintf(out, "            }\n");
        fprintf(out, "          },\n");
        fprintf(out, "          \"required\": [\"cmd\"]\n");
        fprintf(out, "        }\n");
        fprintf(out, "      },\n");
        
        // Tool 2: read_file
        fprintf(out, "      {\n");
        fprintf(out, "        \"name\": \"read_file\",\n");
        fprintf(out, "        \"description\": \"Read the contents of a file from the local filesystem\",\n");
        fprintf(out, "        \"parameters\": {\n");
        fprintf(out, "          \"type\": \"object\",\n");
        fprintf(out, "          \"properties\": {\n");
        fprintf(out, "            \"path\": {\n");
        fprintf(out, "              \"type\": \"string\",\n");
        fprintf(out, "              \"description\": \"The absolute or relative path to the file to read\"\n");
        fprintf(out, "            }\n");
        fprintf(out, "          },\n");
        fprintf(out, "          \"required\": [\"path\"]\n");
        fprintf(out, "        }\n");
        fprintf(out, "      },\n");
        
        // Tool 3: write_file
        fprintf(out, "      {\n");
        fprintf(out, "        \"name\": \"write_file\",\n");
        fprintf(out, "        \"description\": \"Create a new file or overwrite an existing file with content\",\n");
        fprintf(out, "        \"parameters\": {\n");
        fprintf(out, "          \"type\": \"object\",\n");
        fprintf(out, "          \"properties\": {\n");
        fprintf(out, "            \"path\": {\n");
        fprintf(out, "              \"type\": \"string\",\n");
        fprintf(out, "              \"description\": \"The path to write the file to\"\n");
        fprintf(out, "            },\n");
        fprintf(out, "            \"content\": {\n");
        fprintf(out, "              \"type\": \"string\",\n");
        fprintf(out, "              \"description\": \"The full string content to write to the file\"\n");
        fprintf(out, "            }\n");
        fprintf(out, "          },\n");
        fprintf(out, "          \"required\": [\"path\", \"content\"]\n");
        fprintf(out, "        }\n");
        fprintf(out, "      },\n");
        
        // Tool 4: list_dir
        fprintf(out, "      {\n");
        fprintf(out, "        \"name\": \"list_dir\",\n");
        fprintf(out, "        \"description\": \"List all files and subdirectories in a directory\",\n");
        fprintf(out, "        \"parameters\": {\n");
        fprintf(out, "          \"type\": \"object\",\n");
        fprintf(out, "          \"properties\": {\n");
        fprintf(out, "            \"path\": {\n");
        fprintf(out, "              \"type\": \"string\",\n");
        fprintf(out, "              \"description\": \"The directory path to list (defaults to . if empty)\"\n");
        fprintf(out, "            }\n");
        fprintf(out, "          }\n");
        fprintf(out, "        }\n");
        fprintf(out, "      },\n");
        
        // Tool 5: search_in_files
        fprintf(out, "      {\n");
        fprintf(out, "        \"name\": \"search_in_files\",\n");
        fprintf(out, "        \"description\": \"Search local files recursively for a specific text query/pattern\",\n");
        fprintf(out, "        \"parameters\": {\n");
        fprintf(out, "          \"type\": \"object\",\n");
        fprintf(out, "          \"properties\": {\n");
        fprintf(out, "            \"query\": {\n");
        fprintf(out, "              \"type\": \"string\",\n");
        fprintf(out, "              \"description\": \"The text query to search for\"\n");
        fprintf(out, "            }\n");
        fprintf(out, "          },\n");
        fprintf(out, "          \"required\": [\"query\"]\n");
        fprintf(out, "        }\n");
        fprintf(out, "      },\n");
        
        // Tool 6: edit_file
        fprintf(out, "      {\n");
        fprintf(out, "        \"name\": \"edit_file\",\n");
        fprintf(out, "        \"description\": \"Surgically search and replace a block of text in an existing file\",\n");
        fprintf(out, "        \"parameters\": {\n");
        fprintf(out, "          \"type\": \"object\",\n");
        fprintf(out, "          \"properties\": {\n");
        fprintf(out, "            \"path\": {\n");
        fprintf(out, "              \"type\": \"string\",\n");
        fprintf(out, "              \"description\": \"The file path to edit\"\n");
        fprintf(out, "            },\n");
        fprintf(out, "            \"search\": {\n");
        fprintf(out, "              \"type\": \"string\",\n");
        fprintf(out, "              \"description\": \"The exact contiguous block of code/text to match\"\n");
        fprintf(out, "            },\n");
        fprintf(out, "            \"replace\": {\n");
        fprintf(out, "              \"type\": \"string\",\n");
        fprintf(out, "              \"description\": \"The new replacement text block\"\n");
        fprintf(out, "            }\n");
        fprintf(out, "          },\n");
        fprintf(out, "          \"required\": [\"path\", \"search\", \"replace\"]\n");
        fprintf(out, "        }\n");
        fprintf(out, "      },\n");
        
        // Tool 7: web_search
        fprintf(out, "      {\n");
        fprintf(out, "        \"name\": \"web_search\",\n");
        fprintf(out, "        \"description\": \"Search the internet using DuckDuckGo for up-to-date information\",\n");
        fprintf(out, "        \"parameters\": {\n");
        fprintf(out, "          \"type\": \"object\",\n");
        fprintf(out, "          \"properties\": {\n");
        fprintf(out, "            \"query\": {\n");
        fprintf(out, "              \"type\": \"string\",\n");
        fprintf(out, "              \"description\": \"The search query\"\n");
        fprintf(out, "            }\n");
        fprintf(out, "          },\n");
        fprintf(out, "          \"required\": [\"query\"]\n");
        fprintf(out, "        }\n");
        fprintf(out, "      }\n");
        
        fprintf(out, "    ]\n");
        fprintf(out, "  }]\n");
    }
    
    fprintf(out, "\n}\n");
    fclose(out);
    
    if (last_function_name) free(last_function_name);
    for (int i = 0; i < message_count; i++) {
        free(messages[i].role);
        free(messages[i].content);
    }
    
    return 0;
}
