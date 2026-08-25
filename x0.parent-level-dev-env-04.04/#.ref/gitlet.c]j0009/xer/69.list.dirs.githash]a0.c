#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <time.h>

#define MAX_PATH_LEN PATH_MAX

FILE* output_file = NULL;

// Hash function from gitlet.c
unsigned long hash(unsigned char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
}

// Hash file contents or metadata
unsigned long hash_contents(const char* filepath, long long size, int is_regular_file) {
    if (is_regular_file) {
        FILE* file = fopen(filepath, "r");
        if (!file) {
            // Fallback to size-based hash if file can't be read
            char size_str[32];
            snprintf(size_str, sizeof(size_str), "%lld", size);
            return hash((unsigned char*)size_str);
        }
        fseek(file, 0, SEEK_END);
        long length = ftell(file);
        fseek(file, 0, SEEK_SET);
        char *buffer = malloc(length + 1);
        if (!buffer) {
            fclose(file);
            char size_str[32];
            snprintf(size_str, sizeof(size_str), "%lld", size);
            return hash((unsigned char*)size_str);
        }
        fread(buffer, 1, length, file);
        buffer[length] = '\0';
        fclose(file);
        unsigned long result = hash((unsigned char*)buffer);
        free(buffer);
        return result;
    } else {
        // For directories/symlinks, hash the size
        char size_str[32];
        snprintf(size_str, sizeof(size_str), "%lld", size);
        return hash((unsigned char*)size_str);
    }
}

void print_usage(const char* program_name) {
    fprintf(stderr, "Usage: %s <directory_path>\n", program_name);
    fprintf(stderr, "Displays directory structure with sizes and hashes.\n");
}

int is_valid_path(const char* path) {
    if (!path || strlen(path) == 0) return 0;
    if (strlen(path) >= MAX_PATH_LEN) {
        fprintf(stderr, "Path too long: %s\n", path);
        return 0;
    }
    return 1;
}

void print_to_both(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    if (output_file) {
        va_start(args, format);
        vfprintf(output_file, format, args);
        va_end(args);
    }
}

long long list_dir(const char* path, int depth, char* parent_index, int* index) {
    DIR* dir = opendir(path);
    long long total_size = 0;
    if (!dir) {
        fprintf(stderr, "Error opening %s: %s\n", path, strerror(errno));
        if (output_file) fprintf(output_file, "Error opening %s: %s\n", path, strerror(errno));
        return 0;
    }

    struct dirent* entry;
    char full_path[MAX_PATH_LEN];
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        if (strcmp(entry->d_name, "directory_map.txt") == 0) continue;

        if (snprintf(full_path, MAX_PATH_LEN, "%s/%s", path, entry->d_name) >= MAX_PATH_LEN) {
            fprintf(stderr, "Path too long: %s/%s\n", path, entry->d_name);
            if (output_file) fprintf(output_file, "Path too long: %s/%s\n", path, entry->d_name);
            continue;
        }

        struct stat st;
        if (lstat(full_path, &st) != 0) {
            fprintf(stderr, "Error getting stats for %s: %s\n", full_path, strerror(errno));
            if (output_file) fprintf(output_file, "Error getting stats for %s: %s\n", full_path, strerror(errno));
            continue;
        }

        char current_index[256];
        if (depth == 1) {
            snprintf(current_index, sizeof(current_index), "%d", *index);
        } else {
            snprintf(current_index, sizeof(current_index), "%s.%d", parent_index, *index);
        }

        for (int i = 0; i < depth; i++) print_to_both("  ");
        print_to_both("[%s] %s", current_index, entry->d_name);
        if (S_ISDIR(st.st_mode)) print_to_both("/");
        if (S_ISLNK(st.st_mode)) print_to_both(" -> symlink");
        unsigned long file_hash = hash_contents(full_path, st.st_size, S_ISREG(st.st_mode) && !S_ISLNK(st.st_mode));
        print_to_both(" (%lld bytes, hash: %lu)", (long long)st.st_size, file_hash);
        total_size += st.st_size;
        print_to_both("\n");

        if (S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode)) {
            total_size += list_dir(full_path, depth + 1, current_index, index);
        }
        (*index)++;
    }
    closedir(dir);
    return total_size;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (!is_valid_path(argv[1])) {
        fprintf(stderr, "Invalid directory path: %s\n", argv[1]);
        print_usage(argv[0]);
        return 1;
    }

    output_file = fopen("directory_map.txt", "w");
    if (!output_file) {
        fprintf(stderr, "Error opening directory_map.txt: %s\n", strerror(errno));
        return 1;
    }

    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);
    print_to_both("File content hashes]Generated on: %s\n", time_str);

    struct stat st;
    if (lstat(argv[1], &st) != 0) {
        fprintf(stderr, "Error getting stats for %s: %s\n", argv[1], strerror(errno));
        if (output_file) fprintf(output_file, "Error getting stats for %s: %s\n", argv[1], strerror(errno));
        fclose(output_file);
        output_file = NULL;
        return 1;
    }

    print_to_both("Directory Contents: %s/", argv[1]);
    unsigned long dir_hash = hash_contents("", st.st_size, 0);
    print_to_both(" (%lld bytes, hash: %lu)", (long long)st.st_size, dir_hash);
    print_to_both("\n");

    int index = 1;
    long long total_size = st.st_size;
    total_size += list_dir(argv[1], 1, "", &index);
    print_to_both("Total recursive size: %lld bytes\n", total_size);

    if (output_file) {
        fflush(output_file);
        fclose(output_file);
        output_file = NULL;
    }
    return 0;
}
